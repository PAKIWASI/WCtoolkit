#include "hashmap.h"
#include "common.h"

#include <stddef.h>
#include <stdlib.h>



#define GET_SLOT(map, i) (map->keys + ((u64)(1 + map->key_size) * (i))) // psl byte
#define GET_PSL(map, i)  (GET_SLOT(map, i))                             // same — psl is byte 0
#define GET_KEY(map, i)  (GET_SLOT(map, i) + 1)                         // key starts at byte 1
#define GET_VAL(map, i)  (map->vals + ((u64)(map->val_size) * (i)))

// psl sentient val to indicate empty bucket
#define BUCKET_EMPTY 0

typedef enum {
    NOT_FOUND = 0,
    FOUND,
    ROBINHOOD_EXIT,
} MAP_LOOKUP_RES;




/*
====================PRIVATE DECLERATIONS====================
*/

#define INIT_BUCKETS(map, start, num)                                        \
    ({                                                                       \
        memset(GET_SLOT(map, start), 0, ((u64)1 + (map)->key_size) * (num)); \
        memset(GET_VAL(map, start), 0, ((u64)(map)->val_size) * (num));      \
    })

static u64  map_lookup(const hashmap* map, const u8* key, MAP_LOOKUP_RES* res, u8* out_psl);
static void map_insert(hashmap* map, u8* key, u8* val, u8 psl, u64 idx);

static void        map_resize(hashmap* map, u64 new_capacity);
static inline void map_maybe_resize(hashmap* map);

/*
====================PUBLIC FUNCTIONS====================
*/

hashmap* hashmap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* key_ops, const container_ops* val_ops)
{
    CHECK_FATAL(key_size == 0, "key_size can't be 0");
    CHECK_FATAL(val_size == 0, "val_size can't be 0");

    hashmap* map = malloc(sizeof(hashmap));
    CHECK_FATAL(!map, "map malloc failed");

    map->keys = calloc(HASHMAP_INIT_CAPACITY, 1 + key_size);
    CHECK_FATAL(!map->keys, "keys malloc failed");
    map->vals = calloc(HASHMAP_INIT_CAPACITY, val_size);
    CHECK_FATAL(!map->vals, "vals malloc failed");

    map->scratch = malloc(key_size + val_size);
    CHECK_FATAL(!map->scratch, "scratch malloc failed");

    map->size     = 0;
    map->capacity = HASHMAP_INIT_CAPACITY;

    map->key_size = key_size;
    map->val_size = val_size;

    map->hash_fn = hash_fn ? hash_fn : fnv1a_hash;
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
    free(map->vals);
    free(map);
}


// copy semantics - deep copy key, val into hashmap
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

    // New key — make a deep copy into scratch first, then insert
    // map_insert will memcpy from these into the actual slots
    u8* k_buf = map->scratch;
    u8* v_buf = map->scratch + map->key_size;

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

b8 hashmap_put_move(hashmap* map, u8** key, u8** val)
{
    CHECK_FATAL(!map || !key || !val, "null arg");

    map_maybe_resize(map);

    MAP_LOOKUP_RES res;
    u8  out_psl;
    u64 slot = map_lookup(map, *key, &res, &out_psl);

    delete_fn v_del = MAP_DEL(map->val_ops);

    if (res == FOUND) {

        if (v_del) { 
            v_del(GET_VAL(map, slot));
        }

        memcpy(GET_VAL(map, slot), *val, map->val_size);

        // Also replace key bytes (same key value, caller owns old key)
        delete_fn k_del = MAP_DEL(map->key_ops);
        if (k_del) { 
            k_del(GET_KEY(map, slot));
        }

        memcpy(GET_KEY(map, slot), *key, map->key_size);

        *key = NULL;
        *val = NULL;
        return 1;
    }

    // map_insert memcpy's from these — ownership transfers into the map
    map_insert(map, *key, *val, out_psl, slot);
    *key = NULL;
    *val = NULL;
    return 0;
}


/*
====================PRIVATE FUNCTIONS====================
*/

/*


  1 1 2 3 2 0
[ a b c d e   ]
  0 1 2 3 4 5

f -> 1
psl: 1 2 3 4 -> robinhood exit idx 4

*/
static u64 map_lookup(const hashmap* map, const u8* key, MAP_LOOKUP_RES* res, u8* out_psl)
{
    u64 idx = map->hash_fn(key, map->key_size) % map->capacity;
    u8  psl = 1; // stored value for PSL=0 is 1

    for (u64 i = idx;; i = (i + 1) % map->capacity) {
        u8 slot_psl = *GET_PSL(map, i);
        *out_psl    = psl;

        if (slot_psl == 0) {
            *res = NOT_FOUND;
            return i; // empty — key not here
        }

        if (slot_psl < psl) {
            *res = ROBINHOOD_EXIT;
            return i; // Robin Hood early exit
        }

        if (map->cmp_fn(GET_KEY(map, i), key, map->key_size) == 0) {
            *res = FOUND;
            return i; // found
        }

        psl++;
    }
}

static void map_insert(hashmap* map, u8* key, u8* val, u8 psl, u64 idx)
{
    // key/val passed in are ALREADY owned by the caller
    // either a fresh copy (put) or moved pointer (put_move)
    // We only memcpy-shuffle ownership between slots, no copy_fn needed here

    for (u64 i = idx;; i = (i + 1) % map->capacity) {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == BUCKET_EMPTY) {
            // Empty slot — take it. Ownership transfers in.
            *GET_PSL(map, i) = psl;
            memcpy(GET_KEY(map, i), key, map->key_size);
            memcpy(GET_VAL(map, i), val, map->val_size);
            map->size++;
            return;
        }

        // Robin Hood: evict the "rich" resident
        if (slot_psl < psl) {
            // Swap psl
            *GET_PSL(map, i) = psl;
            psl              = slot_psl;

            // Swap key/val via scratch — pure ownership transfer, no copy_fn
            memcpy(map->scratch, GET_KEY(map, i), map->key_size);
            memcpy(map->scratch + map->key_size, GET_VAL(map, i), map->val_size);

            memcpy(GET_KEY(map, i), key, map->key_size);
            memcpy(GET_VAL(map, i), val, map->val_size);

            key = map->scratch;
            val = map->scratch + map->key_size;
        }

        psl++;
    }
}



