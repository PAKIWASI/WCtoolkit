#include "hashmap.h"
#include "common.h"
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

// scratch layout: [0 .. key_size) = slot A,  [key_size .. key_size+val_size) = slot B
// We need two independent regions:
//   - "stage"  : where hashmap_put copies the incoming key/val before calling map_insert
//   - "swap"   : where map_insert saves a displaced resident during Robin Hood eviction
// They must NOT overlap because map_insert is called with a pointer INTO scratch.
// scratch is therefore 2 * (key_size + val_size) bytes.
#define STAGE_KEY(map) ((map)->scratch)
#define STAGE_VAL(map) ((map)->scratch + ALIGN8((map)->key_size))
#define SWAP_BUF(map)  ((map)->scratch + (map)->key_size + (map)->val_size)
#define SWAP_KEY(map)  ((map)->scratch + ALIGN8((map)->key_size) + (map)->val_size)
#define SWAP_VAL(map)  ((map)->scratch + ALIGN8((map)->key_size) + (map)->val_size + ALIGN8((map)->key_size))

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
    CHECK_FATAL(!map || !key || !val, "args null");

    copy_fn   k_cp  = MAP_COPY(map->key_ops);
    copy_fn   v_cp  = MAP_COPY(map->val_ops);
    delete_fn v_del = MAP_DEL(map->val_ops);

    MAP_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = map_lookup(map, key, &res, &out_psl);

    // key exists, update val and delete prev val
    if (res == FOUND) {
        // del deletes owned resourses, not the buffer
        if (v_del) {
            v_del(GET_VAL(map, slot));
        }
        // update value (copy semantics)
        if (v_cp) {
            v_cp(GET_VAL(map, slot), val);
        } else {
            memcpy(GET_VAL(map, slot), val, map->val_size);
        }

        return 1; // found
    }

    // key doesn't exist, copy both key and val and insert into map

    //
}


// Insert or update — MOVE semantics (key and val are u8**, both nulled on success).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put_move(hashmap* map, u8** key, u8** val) {}


// Mixed: key copied, val moved.
b8 hashmap_put_val_move(hashmap* map, const u8* key, u8** val) {}


// Mixed: key moved, val copied.
b8 hashmap_put_key_move(hashmap* map, u8** key, const u8* val) {}


// Get value for key — copies into val. Returns 1 if found, 0 if not.
b8 hashmap_get(const hashmap* map, const u8* key, u8* val)
{
    CHECK_FATAL(!map || !key || !val, "null arg");

    MAP_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = map_lookup(map, key, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    copy_fn v_copy = MAP_COPY(map->val_ops);
    if (v_copy) {
        v_copy(val, GET_VAL(map, slot));
    } else {
        memcpy(val, GET_VAL(map, slot), map->val_size);
    }
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
b8 hashmap_del(hashmap* map, const u8* key, u8* out) {}


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
void hashmap_clear(hashmap* map) {}


void hashmap_copy(hashmap* dest, const hashmap* src) {}


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

    for (u64 i = idx;; i = MAP_NEXT(map, i)) {
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
    // This loop only shuffles ownership between slots — no copy/move fn
    // Uses SWAP_BUF (second half of scratch) to avoid aliasing the staged data.

    for (u64 i = idx;; i = MAP_NEXT(map, i)) {
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
