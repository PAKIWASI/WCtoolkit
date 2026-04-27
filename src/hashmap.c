#include "hashmap.h"

#include <string.h>


#define GET_KEY(map, i) ((map)->keys + ((u64)(map)->key_size * (i)))
#define GET_PSL(map, i) ((map)->psls + (i))
#define GET_VAL(map, i) ((map)->vals + ((u64)(map)->val_size * (i)))

// capacity is always power-of-2 — use bitmask instead of %
#define MAP_MASK(map)     ((map)->capacity - 1)
#define MAP_IDX(map, key) ((map)->hash_fn((key), (map)->key_size) & MAP_MASK(map))
#define MAP_NEXT(map, i)  (((i) + 1) & MAP_MASK(map))

// PSL 0 == empty bucket; stored PSL is (real_psl + 1), starting at 1
#define BUCKET_EMPTY 0

// scratch layout:
//   STAGE region (first half)  — used by hashmap_put to copy incoming key/val
//     [0                         ..  ALIGN8(key_size))             = STAGE_KEY
//     [ALIGN8(key_size)          ..  ALIGN8(key_size) + val_size)  = STAGE_VAL
//   SWAP region (second half)  — used by map_insert for Robin Hood evictions
//     [ALIGN8(key_size)+val_size ..  +ALIGN8(key_size))            = SWAP_KEY
//     [+ALIGN8(key_size)         ..  +val_size)                    = SWAP_VAL
//
// The two regions must NOT overlap: map_insert is called with pointers INTO
// the STAGE region, and it writes displaced residents into the SWAP region.
// Total: 2 * (ALIGN8(key_size) + val_size) bytes.
#define STAGE_KEY(map) ((map)->scratch)
#define STAGE_VAL(map) ((map)->scratch + ALIGN8((map)->key_size))
#define SWAP_KEY(map)  ((map)->scratch + ALIGN8((map)->key_size) + (map)->val_size)
#define SWAP_VAL(map)  ((map)->scratch + ALIGN8((map)->key_size) + (map)->val_size + ALIGN8((map)->key_size))


/*
====================PRIVATE DECLARATIONS====================
*/

static u64         map_lookup(const hashmap* map, const u8* key, LOOKUP_RES* res, u8* out_psl);
static void        map_insert(hashmap* map, u8* key, u8* val, u8 psl, u64 idx);
static inline void map_maybe_resize(hashmap* map);
static void        map_resize(hashmap* map, u64 new_capacity);


/*
====================PUBLIC FUNCTIONS====================
*/

hashmap* hashmap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* key_ops, const container_ops* val_ops)
{
    CHECK_FATAL(key_size == 0 || val_size == 0, "key/val size can't be 0");

    hashmap* map = malloc(sizeof(hashmap));
    CHECK_FATAL(!map, "map malloc failed");

    // map->keys = calloc(HASHMAP_INIT_CAPACITY, key_size);
    map->keys = malloc((u64)HASHMAP_INIT_CAPACITY * key_size);
    CHECK_FATAL(!map->keys, "keys calloc failed");
    map->psls = calloc(HASHMAP_INIT_CAPACITY, sizeof(u8));
    CHECK_FATAL(!map->psls, "psls calloc failed");
    // map->vals = calloc(HASHMAP_INIT_CAPACITY, val_size);
    map->vals = malloc((u64)HASHMAP_INIT_CAPACITY * val_size);
    CHECK_FATAL(!map->vals, "vals calloc failed");

    map->scratch = malloc(2 * (ALIGN8(key_size) + val_size));
    CHECK_FATAL(!map->scratch, "scratch malloc failed");

    map->size     = 0;
    map->capacity = HASHMAP_INIT_CAPACITY;
    map->key_size = key_size;
    map->val_size = val_size;

    map->hash_fn = hash_fn ? hash_fn : wyhash;
    map->cmp_fn  = cmp_fn ? cmp_fn : default_compare;

    map->key_ops = key_ops;
    map->val_ops = val_ops;
    map->key_pod = (key_ops == NULL);
    map->val_pod = (val_ops == NULL);

    return map;
}


void hashmap_destroy(hashmap* map)
{
    CHECK_FATAL(!map, "map is null");

    if (!map->key_pod || !map->val_pod) {
        delete_fn k_del = map->key_pod ? NULL : map->key_ops->del_fn;
        delete_fn v_del = map->val_pod ? NULL : map->val_ops->del_fn;
        if (k_del || v_del) {
            for (u64 i = 0; i < map->capacity; i++) {
                if (*GET_PSL(map, i) == BUCKET_EMPTY) {
                    continue;
                }
                if (k_del) {
                    k_del(GET_KEY(map, i));
                }
                if (v_del) {
                    v_del(GET_VAL(map, i));
                }
            }
        }
    }

    free(map->keys);
    free(map->psls);
    free(map->vals);
    free(map->scratch);
    free(map);
}

static void hashmap_destroy_stk(hashmap* map)
{
    CHECK_FATAL(!map, "map is null");

    if (!map->key_pod || !map->val_pod) {
        delete_fn k_del = map->key_pod ? NULL : map->key_ops->del_fn;
        delete_fn v_del = map->val_pod ? NULL : map->val_ops->del_fn;
        if (k_del || v_del) {
            for (u64 i = 0; i < map->capacity; i++) {
                if (*GET_PSL(map, i) == BUCKET_EMPTY) {
                    continue;
                }
                if (k_del) {
                    k_del(GET_KEY(map, i));
                }
                if (v_del) {
                    v_del(GET_VAL(map, i));
                }
            }
        }
    }

    free(map->keys);
    free(map->psls);
    free(map->vals);
    free(map->scratch);
}


// Insert or update — COPY semantics.
// Ownership: map takes a deep copy of key and val via ops->copy_fn (or memcpy for POD).
// The caller retains ownership of its key/val and is responsible for freeing them.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put(hashmap* map, const u8* key, const u8* val)
{
    CHECK_FATAL(!map || !key || !val, "args null");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res == FOUND) {
        if (map->val_pod) {
            memcpy(GET_VAL(map, slot), val, map->val_size);
        } else {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
            copy_fn v_cp = map->val_ops->copy_fn;
            if (v_cp) {
                v_cp(GET_VAL(map, slot), val);
            } else {
                memcpy(GET_VAL(map, slot), val, map->val_size);
            }
        }
        return 1;
    }

    if (map->key_pod) {
        memcpy(STAGE_KEY(map), key, map->key_size);
    } else {
        copy_fn k_cp = map->key_ops->copy_fn;
        if (k_cp) {
            k_cp(STAGE_KEY(map), key);
        } else {
            memcpy(STAGE_KEY(map), key, map->key_size);
        }
    }
    if (map->val_pod) {
        memcpy(STAGE_VAL(map), val, map->val_size);
    } else {
        copy_fn v_cp = map->val_ops->copy_fn;
        if (v_cp) {
            v_cp(STAGE_VAL(map), val);
        } else {
            memcpy(STAGE_VAL(map), val, map->val_size);
        }
    }

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Insert or update — MOVE semantics.
// Ownership: the map takes ownership of *key and *val directly (no copy made).
// On success both pointers are nulled. Requires move_fn for both key and val.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put_move(hashmap* map, u8** key, u8** val)
{
    CHECK_FATAL(!map || !key || !val || !*key || !*val, "args null");

    move_fn k_mv = MAP_MOVE(map->key_ops);
    move_fn v_mv = MAP_MOVE(map->val_ops);

    // move_fn is mandatory: it must transfer the heap resource and null the source.
    // For by-value types with no heap resources, use hashmap_put (copy semantics) instead.
    CHECK_FATAL(!k_mv || !v_mv, "key/val move funcs required");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, *key, &res, &out_psl);

    if (res == FOUND) {
        if (!map->val_pod) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
        }
        v_mv(GET_VAL(map, slot), val);
        if (!map->key_pod) {
            delete_fn k_del = map->key_ops->del_fn;
            if (k_del) {
                k_del(*key);
            }
        }
        free(*key);
        *key = NULL;
        return 1;
    }

    // Stage: move key into STAGE_KEY, move val into STAGE_VAL.
    // move_fn transfers the heap resource pointer into the dest slot and nulls src.
    k_mv(STAGE_KEY(map), key); // nulls *key
    v_mv(STAGE_VAL(map), val); // nulls *val

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Insert or update — mixed: key is COPIED, val is MOVED.
// Ownership: map deep-copies the key (caller retains it); map takes ownership of *val (*val nulled).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put_val_move(hashmap* map, const u8* key, u8** val)
{
    CHECK_FATAL(!map || !key || !val || !*val, "args null");

    move_fn v_mv = MAP_MOVE(map->val_ops);

    CHECK_FATAL(!v_mv, "val move func required");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res == FOUND) {
        if (!map->val_pod) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
        }
        v_mv(GET_VAL(map, slot), val);
        return 1;
    }

    if (map->key_pod) {
        memcpy(STAGE_KEY(map), key, map->key_size);
    } else {
        copy_fn k_cp = map->key_ops->copy_fn;
        if (k_cp) {
            k_cp(STAGE_KEY(map), key);
        } else {
            memcpy(STAGE_KEY(map), key, map->key_size);
        }
    }
    v_mv(STAGE_VAL(map), val);

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Insert or update — mixed: key is MOVED, val is COPIED.
// Ownership: map takes ownership of *key (*key nulled); map deep-copies val (caller retains it).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put_key_move(hashmap* map, u8** key, const u8* val)
{
    CHECK_FATAL(!map || !key || !*key || !val, "args null");

    move_fn k_mv = MAP_MOVE(map->key_ops);

    CHECK_FATAL(!k_mv, "key move func required for hashmap_put_key_move");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, *key, &res, &out_psl);

    if (res == FOUND) {
        if (!map->val_pod) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
            copy_fn v_cp = map->val_ops->copy_fn;
            if (v_cp) {
                v_cp(GET_VAL(map, slot), val);
            } else {
                memcpy(GET_VAL(map, slot), val, map->val_size);
            }
        } else {
            memcpy(GET_VAL(map, slot), val, map->val_size);
        }
        // Key already in map — consume (and discard) the incoming duplicate.
        if (!map->key_pod) {
            delete_fn k_del = map->key_ops->del_fn;
            if (k_del) {
                k_del(*key);
            }
        }
        free(*key);
        *key = NULL;
        return 1;
    }

    // Stage key (move) and val (copy).
    k_mv(STAGE_KEY(map), key);
    if (map->val_pod) {
        memcpy(STAGE_VAL(map), val, map->val_size);
    } else {
        copy_fn v_cp = map->val_ops->copy_fn;
        if (v_cp) {
            v_cp(STAGE_VAL(map), val);
        } else {
            memcpy(STAGE_VAL(map), val, map->val_size);
        }
    }

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Get value for key — COPIES into val. Returns 1 if found, 0 if not.
// Caller owns the copy returned in val and must free it when done.
b8 hashmap_get(const hashmap* map, const u8* key, u8* val)
{
    CHECK_FATAL(!map || !key || !val, "null arg");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    if (map->val_pod) {
        memcpy(val, GET_VAL(map, slot), map->val_size);
    } else {
        copy_fn v_copy = map->val_ops->copy_fn;
        if (v_copy) {
            v_copy(val, GET_VAL(map, slot));
        } else {
            memcpy(val, GET_VAL(map, slot), map->val_size);
        }
    }
    return 1;
}


// Get pointer to value in-place. Returns NULL if not found.
// The pointer is valid until the next mutation (put/del/resize).
// Do NOT free the returned pointer — the map owns it.
u8* hashmap_get_ptr(hashmap* map, const u8* key)
{
    CHECK_FATAL(!map || !key, "null arg");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    return (res == FOUND) ? GET_VAL(map, slot) : NULL;
}


// Delete key.
// If out != NULL, the value is MOVED into it before deletion (caller takes ownership).
// If out == NULL, the value is destroyed via del_fn (or simply discarded for POD).
// Returns 1 if found and deleted, 0 if not found.
//
// Uses Robin Hood backward-shift deletion to maintain the probe-sequence invariant
// without tombstones: after removing a slot, we shift subsequent entries back one
// position as long as they have PSL > 1 (i.e. they are not sitting at their home slot).
b8 hashmap_del(hashmap* map, const u8* key, u8* out)
{
    CHECK_FATAL(!map || !key, "null arg");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    if (out) {
        memcpy(out, GET_VAL(map, slot), map->val_size);
    } else {
        if (!map->val_pod) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
        }
    }

    if (!map->key_pod) {
        delete_fn k_del = map->key_ops->del_fn;
        if (k_del) {
            k_del(GET_KEY(map, slot));
        }
    }

    // Backward-shift deletion: pull subsequent entries one slot back as long as
    // they have PSL > 1.  Entries at their home slot (PSL == 1) must not move.
    // This restores the Robin Hood invariant without tombstones.
    u64 cur = slot;
    for (;;) {
        u64 next     = MAP_NEXT(map, cur);
        u8  next_psl = *GET_PSL(map, next);

        // Stop if next slot is empty or the next entry is already at its home slot.
        if (next_psl <= 1) {
            *GET_PSL(map, cur) = BUCKET_EMPTY;
            break;
        }

        // Shift next entry one slot back; its PSL decreases by 1.
        *GET_PSL(map, cur) = next_psl - 1;
        memcpy(GET_KEY(map, cur), GET_KEY(map, next), map->key_size);
        memcpy(GET_VAL(map, cur), GET_VAL(map, next), map->val_size);

        cur = next;
    }

    map->size--;
    return 1;
}


// Check if key exists.
b8 hashmap_has(const hashmap* map, const u8* key)
{
    CHECK_FATAL(!map || !key, "null arg");

    LOOKUP_RES res;
    u8         out_psl;
    map_lookup(map, key, &res, &out_psl);
    return res == FOUND;
}


// Print all key-value pairs.
void hashmap_print(const hashmap* map, print_fn key_print, print_fn val_print)
{
    CHECK_FATAL(!map || !key_print || !val_print, "null arg");

    printf("\t=========\n");
    printf("\tSize: %lu / Capacity: %lu\n", map->size, map->capacity);
    printf("\t=========\n");

    for (u64 i = 0; i < map->capacity; i++) {
        if (*GET_PSL(map, i) == BUCKET_EMPTY) {
            continue;
        }
        putchar('\t');
        key_print(GET_KEY(map, i));
        printf(" => ");
        val_print(GET_VAL(map, i));
        putchar('\n');
    }

    printf("\t=========\n");
}


// Remove all elements, keep capacity.
// Destroys all keys and values via their del_fn callbacks, then zeroes the arrays.
void hashmap_clear(hashmap* map)
{
    CHECK_FATAL(!map, "map is null");

    if (!map->key_pod || !map->val_pod) {
        delete_fn k_del = map->key_pod ? NULL : map->key_ops->del_fn;
        delete_fn v_del = map->val_pod ? NULL : map->val_ops->del_fn;
        for (u64 i = 0; i < map->capacity; i++) {
            if (*GET_PSL(map, i) == BUCKET_EMPTY) {
                continue;
            }
            if (k_del) {
                k_del(GET_KEY(map, i));
            }
            if (v_del) {
                v_del(GET_VAL(map, i));
            }
        }
    }

    memset(map->psls, 0, map->capacity * sizeof(u8));
    map->size = 0;
}


// TODO: test
// Deep copy src into dest.
// Ownership: dest gets independently owned copies of all keys and values.
void hashmap_copy(hashmap* dest, const hashmap* src)
{
    CHECK_FATAL(!dest || !src, "null arg");

    hashmap_destroy_stk(dest);

    dest->keys = malloc(src->capacity * src->key_size);
    CHECK_FATAL(!dest->keys, "copy keys calloc failed");
    dest->psls = calloc(src->capacity, sizeof(u8));
    CHECK_FATAL(!dest->psls, "copy psls calloc failed");
    dest->vals = malloc(src->capacity * src->val_size);
    CHECK_FATAL(!dest->vals, "copy vals calloc failed");
    dest->scratch = malloc(2 * (ALIGN8(src->key_size) + src->val_size));
    CHECK_FATAL(!dest->scratch, "copy scratch malloc failed");

    dest->size     = src->size;
    dest->capacity = src->capacity;
    dest->key_size = src->key_size;
    dest->val_size = src->val_size;
    dest->hash_fn  = src->hash_fn;
    dest->cmp_fn   = src->cmp_fn;
    dest->key_ops  = src->key_ops;
    dest->val_ops  = src->val_ops;
    dest->key_pod  = src->key_pod;
    dest->val_pod  = src->val_pod;

    copy_fn k_cp = src->key_pod ? NULL : src->key_ops->copy_fn;
    copy_fn v_cp = src->val_pod ? NULL : src->val_ops->copy_fn;

    for (u64 i = 0; i < src->capacity; i++) {
        u8 psl = *GET_PSL(src, i);
        if (psl == BUCKET_EMPTY) {
            continue;
        }

        *GET_PSL(dest, i) = psl;

        if (k_cp) {
            k_cp(GET_KEY(dest, i), GET_KEY(src, i));
        } else {
            memcpy(GET_KEY(dest, i), GET_KEY(src, i), src->key_size);
        }

        if (v_cp) {
            v_cp(GET_VAL(dest, i), GET_VAL(src, i));
        } else {
            memcpy(GET_VAL(dest, i), GET_VAL(src, i), src->val_size);
        }
    }
}


/*
====================PRIVATE FUNCTIONS====================
*/

static inline void map_maybe_resize(hashmap* map)
{
    // integer multiply avoids float — equivalent to load > 0.75
    if (map->size * 4 >= map->capacity * 3) {
        map_resize(map, map->capacity * 2);
    }
}

static u64 map_lookup(const hashmap* map, const u8* key, LOOKUP_RES* res, u8* out_psl)
{
    u64        idx = MAP_IDX(map, key);
    u8         psl = 1; // stored PSL=1 means real probe distance 0 (home slot)
    compare_fn cmp = map->cmp_fn;

    for (u64 i = idx;; i = MAP_NEXT(map, i)) {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == BUCKET_EMPTY) {
            *res     = NOT_FOUND;
            *out_psl = psl;
            return i;
        }

        if (slot_psl < psl) {
            // The resident has a lower PSL — it's closer to its home than we are.
            // Under Robin Hood, our key would have displaced this resident on insert,
            // so our key cannot exist at or beyond this slot.
            *res     = ROBINHOOD_EXIT;
            *out_psl = psl;
            return i;
        }

        if (cmp(GET_KEY(map, i), key, map->key_size) == 0) {
            *res     = FOUND;
            *out_psl = psl;
            return i;
        }

        psl++;
    }
}



// Insert key/val with the given starting psl at slot idx.
// key and val are already OWNED by the caller (staged copy or moved pointer).
// This function never calls copy/del — it only shuffles raw bytes between slots.
// Displaced residents are temporarily buffered in SWAP_KEY/SWAP_VAL (second half
// of scratch), which is disjoint from the STAGE region where key/val came from.
static void map_insert(hashmap* map, u8* key, u8* val, u8 psl, u64 idx)
{
    // Use two alternating scratch halves to avoid aliasing
    u8* cur_key = STAGE_KEY(map);
    u8* cur_val = STAGE_VAL(map);
    u8* swp_key = SWAP_KEY(map);
    u8* swp_val = SWAP_VAL(map);

    // key/val may already be STAGE — only copy if not already there
    if (key != cur_key) {
        memcpy(cur_key, key, map->key_size);
    }
    if (val != cur_val) {
        memcpy(cur_val, val, map->val_size);
    }
    key = cur_key;
    val = cur_val;

    for (u64 i = idx;; i = MAP_NEXT(map, i)) {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == BUCKET_EMPTY) {
            *GET_PSL(map, i) = psl;
            memcpy(GET_KEY(map, i), key, map->key_size);
            memcpy(GET_VAL(map, i), val, map->val_size);
            map->size++;
            return;
        }

        if (slot_psl < psl) {
            u8 tmp_psl = slot_psl;
            // Evict into swp (disjoint from key which is in cur)
            memcpy(swp_key, GET_KEY(map, i), map->key_size);
            memcpy(swp_val, GET_VAL(map, i), map->val_size);

            *GET_PSL(map, i) = psl;
            memcpy(GET_KEY(map, i), key, map->key_size);
            memcpy(GET_VAL(map, i), val, map->val_size);

            // Swap roles: evicted becomes current, current becomes swap buffer
            u8* tmp = cur_key;
            cur_key = swp_key;
            swp_key = tmp;
            tmp     = cur_val;
            cur_val = swp_val;
            swp_val = tmp;
            key     = cur_key;
            val     = cur_val;
            psl     = tmp_psl + 1;
            continue;
        }

        psl++;
    }
}


// Rehash into a new array of new_capacity (must be power-of-2).
// Ownership transfers as raw bytes — no copy/del callbacks are invoked.
// This is safe because the data itself doesn't move, only the slot positions.
static void map_resize(hashmap* map, u64 new_capacity)
{
    if (new_capacity < HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    u8* old_keys = map->keys;
    u8* old_psls = map->psls;
    u8* old_vals = map->vals;
    u64 old_cap  = map->capacity;

    // map->keys = calloc(new_capacity, map->key_size);
    map->keys = malloc(new_capacity * map->key_size);
    CHECK_FATAL(!map->keys, "resize keys calloc failed");
    map->psls = calloc(new_capacity, sizeof(u8));
    CHECK_FATAL(!map->psls, "resize psls calloc failed");
    // map->vals = calloc(new_capacity, map->val_size);
    map->vals = malloc(new_capacity * map->val_size);
    CHECK_FATAL(!map->vals, "resize vals calloc failed");

    map->capacity = new_capacity;
    map->size     = 0;


    for (u64 i = 0; i < old_cap; i++) {

        if (old_psls[i] == BUCKET_EMPTY) {
            continue;
        }

        u8* old_key = old_keys + ((u64)map->key_size * i);
        u8* old_val = old_vals + ((u64)map->val_size * i);

        // Stage into scratch first — map_insert uses SWAP region of scratch
        // and would clobber old_key/old_val if they happened to alias it
        memcpy(STAGE_KEY(map), old_key, map->key_size);
        memcpy(STAGE_VAL(map), old_val, map->val_size);

        LOOKUP_RES res;
        u8         out_psl;
        u64        slot = map_lookup(map, STAGE_KEY(map), &res, &out_psl);

        map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    }

    free(old_keys);
    free(old_psls);
    free(old_vals);
}


