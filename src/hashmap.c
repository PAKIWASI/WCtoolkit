#include "hashmap.h"



#define GET_KEY(map, i) ((map)->keys + ((u64)(map)->key_size * (i)))
#define GET_PSL(map, i) ((map)->psls + (i))
#define GET_VAL(map, i) ((map)->vals + ((u64)(map)->val_size * (i)))

// capacity is always power-of-2 — use bitmask instead of %
#define MAP_MASK(map)     ((map)->capacity - 1)
#define MAP_IDX(map, key) ((map)->hash_fn((key), (map)->key_size) & MAP_MASK(map))
#define MAP_NEXT(map, i)  (((i) + 1) & MAP_MASK(map))

// PSL 0 == empty bucket; stored PSL is (real_psl + 1), starting at 1
#define BUCKET_EMPTY 0

// scratch layout: [0 .. key_size) = slot A,  [key_size .. key_size+val_size) = slot B
// We need two independent regions:
//   - "stage"  : where hashmap_put copies the incoming key/val before calling map_insert
//   - "swap"   : where map_insert saves a displaced resident during Robin Hood eviction
// They must NOT overlap because map_insert is called with a pointer INTO scratch.
// scratch is therefore 2 * (key_size + val_size) bytes.
#define STAGE_KEY(map) ((map)->scratch)
#define STAGE_VAL(map) ((map)->scratch + (map)->key_size)
#define SWAP_BUF(map)  ((map)->scratch + (map)->key_size + (map)->val_size)
#define SWAP_KEY(map)  (SWAP_BUF(map))
#define SWAP_VAL(map)  (SWAP_BUF(map) + (map)->key_size)

typedef enum {
    NOT_FOUND = 0,
    FOUND,
    ROBINHOOD_EXIT,
} MAP_LOOKUP_RES;


/*
====================PRIVATE DECLARATIONS====================
*/

static u64         map_lookup(const hashmap* map, const u8* key, MAP_LOOKUP_RES* res, u8* out_psl);
static void        map_insert(hashmap* map, u8* key, u8* val, u8 psl, u64 idx);
static void        map_resize(hashmap* map, u64 new_capacity);
static inline void map_maybe_resize(hashmap* map);


/*
====================PUBLIC FUNCTIONS====================
*/

hashmap* hashmap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* key_ops, const container_ops* val_ops)
{
    CHECK_FATAL(key_size == 0 || val_size == 0, "key/val size can't be 0");

    hashmap* map = malloc(sizeof(hashmap));
    CHECK_FATAL(!map, "map malloc failed");

    map->keys = calloc(HASHMAP_INIT_CAPACITY, key_size);
    CHECK_FATAL(!map->keys, "keys calloc failed");
    map->psls = calloc(HASHMAP_INIT_CAPACITY, sizeof(u8));
    CHECK_FATAL(!map->psls, "psls calloc failed");
    map->vals = calloc(HASHMAP_INIT_CAPACITY, val_size);
    CHECK_FATAL(!map->vals, "vals calloc failed");

    // 2 * (key_size + val_size): first half = staging, second half = RH swap buffer
    map->scratch = malloc(2 * ((u64)key_size + val_size));
    CHECK_FATAL(!map->scratch, "scratch malloc failed");

    map->size     = 0;
    map->capacity = HASHMAP_INIT_CAPACITY;
    map->key_size = key_size;
    map->val_size = val_size;

    map->hash_fn = hash_fn ? hash_fn : wyhash;
    map->cmp_fn  = cmp_fn ? cmp_fn : default_compare;

    map->key_ops = key_ops;
    map->val_ops = val_ops;

    return map;
}


void hashmap_destroy(hashmap* map)
{
    CHECK_FATAL(!map, "map is null");

    delete_fn k_del = MAP_DEL(map->key_ops);
    delete_fn v_del = MAP_DEL(map->val_ops);

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

    free(map->keys);
    free(map->psls);
    free(map->vals);
    free(map->scratch);
    free(map);
}


// Insert or update — COPY semantics.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put(hashmap* map, const u8* key, const u8* val)
{
    CHECK_FATAL(!map || !key || !val, "null arg");

    map_maybe_resize(map);

    MAP_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = map_lookup(map, key, &res, &out_psl);

    copy_fn   k_copy = MAP_COPY(map->key_ops);
    copy_fn   v_copy = MAP_COPY(map->val_ops);
    delete_fn v_del  = MAP_DEL(map->val_ops);

    if (res == FOUND) {
        // Update value in-place — delete old, copy new
        if (v_del) {
            v_del(GET_VAL(map, slot));
        }
        if (v_copy) {
            v_copy(GET_VAL(map, slot), val);
        } else {
            memcpy(GET_VAL(map, slot), val, map->val_size);
        }
        return 1;
    }

    // New key — stage a deep copy into the first half of scratch, then hand off
    // to map_insert which uses the second half (SWAP_BUF) for Robin Hood evictions.
    u8* k_buf = STAGE_KEY(map);
    u8* v_buf = STAGE_VAL(map);

    if (k_copy) {
        k_copy(k_buf, key);
    } else {
        memcpy(k_buf, key, map->key_size);
    }

    if (v_copy) {
        v_copy(v_buf, val);
    } else {
        memcpy(v_buf, val, map->val_size);
    }

    map_insert(map, k_buf, v_buf, out_psl, slot);
    return 0;
}


// Insert or update — MOVE semantics (key and val are u8**, both nulled on success).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put_move(hashmap* map, u8** key, u8** val)
{
    CHECK_FATAL(!map || !key || !val, "null arg");

    map_maybe_resize(map);

    MAP_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = map_lookup(map, *key, &res, &out_psl);

    delete_fn k_del = MAP_DEL(map->key_ops);
    delete_fn v_del = MAP_DEL(map->val_ops);

    if (res == FOUND) {
        if (v_del)
            v_del(GET_VAL(map, slot));
        memcpy(GET_VAL(map, slot), *val, map->val_size);

        if (k_del)
            k_del(GET_KEY(map, slot));
        memcpy(GET_KEY(map, slot), *key, map->key_size);

        *key = NULL;
        *val = NULL;
        return 1;
    }

    // map_insert memcpy's these into slots — ownership transfers in
    map_insert(map, *key, *val, out_psl, slot);
    *key = NULL;
    *val = NULL;
    return 0;
}


// Mixed: key copied, val moved.
b8 hashmap_put_val_move(hashmap* map, const u8* key, u8** val)
{
    CHECK_FATAL(!map || !key || !val, "null arg");

    map_maybe_resize(map);

    MAP_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = map_lookup(map, key, &res, &out_psl);

    copy_fn   k_copy = MAP_COPY(map->key_ops);
    delete_fn v_del  = MAP_DEL(map->val_ops);

    if (res == FOUND) {
        if (v_del)
            v_del(GET_VAL(map, slot));
        memcpy(GET_VAL(map, slot), *val, map->val_size);
        *val = NULL;
        return 1;
    }

    u8* k_buf = STAGE_KEY(map);
    u8* v_buf = STAGE_VAL(map);

    if (k_copy)
        k_copy(k_buf, key);
    else
        memcpy(k_buf, key, map->key_size);

    memcpy(v_buf, *val, map->val_size);
    *val = NULL;

    map_insert(map, k_buf, v_buf, out_psl, slot);
    return 0;
}


// Mixed: key moved, val copied.
b8 hashmap_put_key_move(hashmap* map, u8** key, const u8* val)
{
    CHECK_FATAL(!map || !key || !val, "null arg");

    map_maybe_resize(map);

    MAP_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = map_lookup(map, *key, &res, &out_psl);

    copy_fn   v_copy = MAP_COPY(map->val_ops);
    delete_fn k_del  = MAP_DEL(map->key_ops);
    delete_fn v_del  = MAP_DEL(map->val_ops);

    if (res == FOUND) {
        if (v_del)
            v_del(GET_VAL(map, slot));
        if (v_copy)
            v_copy(GET_VAL(map, slot), val);
        else
            memcpy(GET_VAL(map, slot), val, map->val_size);

        if (k_del)
            k_del(GET_KEY(map, slot));
        memcpy(GET_KEY(map, slot), *key, map->key_size);
        *key = NULL;
        return 1;
    }

    u8* k_buf = STAGE_KEY(map);
    u8* v_buf = STAGE_VAL(map);

    memcpy(k_buf, *key, map->key_size);
    *key = NULL;

    if (v_copy)
        v_copy(v_buf, val);
    else
        memcpy(v_buf, val, map->val_size);

    map_insert(map, k_buf, v_buf, out_psl, slot);
    return 0;
}


// Get value for key — copies into val. Returns 1 if found, 0 if not.
b8 hashmap_get(const hashmap* map, const u8* key, u8* val)
{
    CHECK_FATAL(!map || !key || !val, "null arg");

    MAP_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = map_lookup(map, key, &res, &out_psl);

    if (res != FOUND)
        return 0;

    copy_fn v_copy = MAP_COPY(map->val_ops);
    if (v_copy)
        v_copy(val, GET_VAL(map, slot));
    else
        memcpy(val, GET_VAL(map, slot), map->val_size);
    return 1;
}


// Get pointer to value in-place. Returns NULL if not found.
u8* hashmap_get_ptr(hashmap* map, const u8* key)
{
    CHECK_FATAL(!map || !key, "null arg");

    MAP_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = map_lookup(map, key, &res, &out_psl);

    return (res == FOUND) ? GET_VAL(map, slot) : NULL;
}


// Delete key. If out != NULL, value is copied into it before deletion.
// Returns 1 if found and deleted, 0 if not found.
b8 hashmap_del(hashmap* map, const u8* key, u8* out)
{
    CHECK_FATAL(!map || !key, "null arg");

    if (map->size == 0)
        return 0;

    MAP_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = map_lookup(map, key, &res, &out_psl);

    if (res != FOUND)
        return 0;

    // Optionally hand the value to the caller before destroying it
    if (out) {
        copy_fn v_copy = MAP_COPY(map->val_ops);
        if (v_copy)
            v_copy(out, GET_VAL(map, slot));
        else
            memcpy(out, GET_VAL(map, slot), map->val_size);
    }

    delete_fn k_del = MAP_DEL(map->key_ops);
    delete_fn v_del = MAP_DEL(map->val_ops);
    if (k_del)
        k_del(GET_KEY(map, slot));
    if (v_del)
        v_del(GET_VAL(map, slot));

    // Robin Hood backward shift deletion — no tombstones needed.
    // Walk forward and pull each subsequent element back one slot
    // as long as its PSL > 1 (it's not in its ideal slot).
    for (;;) {
        u64 next     = MAP_NEXT(map, slot);
        u8  next_psl = *GET_PSL(map, next);

        // Stop if next slot is empty or already at its ideal position
        if (next_psl <= 1) {
            *GET_PSL(map, slot) = BUCKET_EMPTY;
            break;
        }

        // Pull neighbour back one slot, reducing its PSL by 1
        memcpy(GET_KEY(map, slot), GET_KEY(map, next), map->key_size);
        memcpy(GET_VAL(map, slot), GET_VAL(map, next), map->val_size);
        *GET_PSL(map, slot) = next_psl - 1;

        slot = next;
    }

    map->size--;
    return 1;
}


// Check if key exists.
b8 hashmap_has(const hashmap* map, const u8* key)
{
    CHECK_FATAL(!map || !key, "null arg");

    MAP_LOOKUP_RES res;
    u8             out_psl;
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
        if (*GET_PSL(map, i) == BUCKET_EMPTY)
            continue;
        putchar('\t');
        key_print(GET_KEY(map, i));
        printf(" => ");
        val_print(GET_VAL(map, i));
        putchar('\n');
    }

    printf("\t=========\n");
}


// Remove all elements, keep capacity.
void hashmap_clear(hashmap* map)
{
    CHECK_FATAL(!map, "map is null");

    delete_fn k_del = MAP_DEL(map->key_ops);
    delete_fn v_del = MAP_DEL(map->val_ops);

    for (u64 i = 0; i < map->capacity; i++) {
        if (*GET_PSL(map, i) == BUCKET_EMPTY)
            continue;
        if (k_del)
            k_del(GET_KEY(map, i));
        if (v_del)
            v_del(GET_VAL(map, i));
    }

    memset(map->psls, 0, map->capacity);
    memset(map->keys, 0, (u64)map->key_size * map->capacity);
    memset(map->vals, 0, (u64)map->val_size * map->capacity);

    map->size = 0;
}


// Deep copy src into dest (dest must be uninitialised or already destroyed).
void hashmap_copy(hashmap* dest, const hashmap* src)
{
    CHECK_FATAL(!dest || !src, "null arg");

    copy_fn k_copy = MAP_COPY(src->key_ops);
    copy_fn v_copy = MAP_COPY(src->val_ops);

    dest->keys = calloc(src->capacity, src->key_size);
    CHECK_FATAL(!dest->keys, "keys calloc failed");
    dest->psls = calloc(src->capacity, sizeof(u8));
    CHECK_FATAL(!dest->psls, "psls calloc failed");
    dest->vals = calloc(src->capacity, src->val_size);
    CHECK_FATAL(!dest->vals, "vals calloc failed");
    dest->scratch = malloc(2 * ((u64)src->key_size + src->val_size));
    CHECK_FATAL(!dest->scratch, "scratch malloc failed");

    dest->size     = 0;
    dest->capacity = src->capacity;
    dest->key_size = src->key_size;
    dest->val_size = src->val_size;
    dest->hash_fn  = src->hash_fn;
    dest->cmp_fn   = src->cmp_fn;
    dest->key_ops  = src->key_ops;
    dest->val_ops  = src->val_ops;

    for (u64 i = 0; i < src->capacity; i++) {
        u8 psl = *GET_PSL(src, i);
        if (psl == BUCKET_EMPTY)
            continue;

        // Deep-copy key/val into the staging region, then insert
        u8* k_buf = STAGE_KEY(dest);
        u8* v_buf = STAGE_VAL(dest);

        if (k_copy)
            k_copy(k_buf, GET_KEY(src, i));
        else
            memcpy(k_buf, GET_KEY(src, i), src->key_size);

        if (v_copy)
            v_copy(v_buf, GET_VAL(src, i));
        else
            memcpy(v_buf, GET_VAL(src, i), src->val_size);

        // Re-insert at the correct position for dest's layout.
        // We can't just copy the PSL/slot directly — same capacity but
        // the hash fn + capacity haven't changed, so positions are identical.
        // Still go through map_insert for correctness with any future changes.
        MAP_LOOKUP_RES res;
        u8             out_psl;
        u64            slot = map_lookup(dest, k_buf, &res, &out_psl);
        map_insert(dest, k_buf, v_buf, out_psl, slot);
    }
}


/*
====================PRIVATE FUNCTIONS====================
*/

static inline void map_maybe_resize(hashmap* map)
{
    // integer multiply avoids float — equivalent to load > 0.75
    if (map->size * 4 >= map->capacity * 3) {
        map_resize(map, map->capacity * 2); // power-of-2 doubles stay power-of-2
    }
}


static u64 map_lookup(const hashmap* map, const u8* key, MAP_LOOKUP_RES* res, u8* out_psl)
{
    u64 idx = MAP_IDX(map, key);
    u8  psl = 1; // stored PSL=1 means real probe distance 0 (home slot)

    for (u64 i = idx;; i = MAP_NEXT(map, i)) 
    {
        u8 slot_psl = *GET_PSL(map, i);
        *out_psl    = psl;

        if (slot_psl == BUCKET_EMPTY) {
            *res = NOT_FOUND;
            return i;
        }

        if (slot_psl < psl) {
            // The resident was inserted closer to home than we are
            // our key can't be further ahead (Robin Hood invariant).
            *res = ROBINHOOD_EXIT;
            return i;
        }

        if (map->cmp_fn(GET_KEY(map, i), key, map->key_size) == 0) {
            *res = FOUND;
            return i;
        }

        psl++;
    }
}


static void map_insert(hashmap* map, u8* key, u8* val, u8 psl, u64 idx)
{
    // key/val are already owned (either staged copy or moved pointer).
    // This loop only shuffles ownership between slots — no copy_fn ever.
    // Uses SWAP_BUF (second half of scratch) to avoid aliasing the staged data.

    for (u64 i = idx;; i = MAP_NEXT(map, i)) 
    {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == BUCKET_EMPTY) {
            *GET_PSL(map, i) = psl;
            memcpy(GET_KEY(map, i), key, map->key_size);
            memcpy(GET_VAL(map, i), val, map->val_size);
            map->size++;
            return;
        }

        // Robin Hood: evict the "rich" resident (lower PSL = closer to home)
        if (slot_psl < psl) {
            // Save displaced resident into swap buffer
            memcpy(SWAP_KEY(map), GET_KEY(map, i), map->key_size);
            memcpy(SWAP_VAL(map), GET_VAL(map, i), map->val_size);
            u8 tmp_psl = slot_psl;

            // Place incoming element
            *GET_PSL(map, i) = psl;
            memcpy(GET_KEY(map, i), key, map->key_size);
            memcpy(GET_VAL(map, i), val, map->val_size);

            // Continue inserting the displaced element
            key = SWAP_KEY(map);
            val = SWAP_VAL(map);
            psl = tmp_psl;
        }

        psl++;
    }
}


static void map_resize(hashmap* map, u64 new_capacity)
{
    if (new_capacity < HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    u8* old_keys = map->keys;
    u8* old_psls = map->psls;
    u8* old_vals = map->vals;
    u64 old_cap  = map->capacity;

    map->keys = calloc(new_capacity, map->key_size);
    CHECK_FATAL(!map->keys, "resize keys calloc failed");
    map->psls = calloc(new_capacity, sizeof(u8));
    CHECK_FATAL(!map->psls, "resize psls calloc failed");
    map->vals = calloc(new_capacity, map->val_size);
    CHECK_FATAL(!map->vals, "resize vals calloc failed");

    // Scratch size scales with key/val sizes but not capacity — no realloc needed.

    map->capacity = new_capacity;
    map->size     = 0;

    // Rehash — ownership transfers as-is, no copy/del callbacks
    for (u64 i = 0; i < old_cap; i++) {
        if (old_psls[i] == BUCKET_EMPTY) {
            continue;
        }

        u8* old_key = old_keys + ((u64)map->key_size * i);
        u8* old_val = old_vals + ((u64)map->val_size * i);

        MAP_LOOKUP_RES res;
        u8             out_psl;
        u64            slot = map_lookup(map, old_key, &res, &out_psl);
        map_insert(map, old_key, old_val, out_psl, slot);
    }

    free(old_keys);
    free(old_psls);
    free(old_vals);
}


