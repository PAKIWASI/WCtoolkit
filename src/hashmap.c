#include "hashmap.h"



#define GET_SLOT(map, i) (map->keys + ((1 + map->key_size) * (i))) // psl byte
#define GET_PSL(map, i)  (GET_SLOT(map, i))                        // same — psl is byte 0
#define GET_KEY(map, i)  (GET_SLOT(map, i) + 1)                    // key starts at byte 1
#define GET_VAL(map, i)  (map->vals + ((map->val_size) * (i)))

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

static inline void init_buckets(hashmap* map, u64 start, u64 num);
static u64         map_lookup(const hashmap* map, const u8* key, MAP_LOOKUP_RES* res);
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
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");
    CHECK_FATAL(!val, "val is null");

    MAP_LOOKUP_RES res;
    u64 slot = map_lookup(map, key, &res);

    switch (res) {
        // key found - update the val, delete old one
        case FOUND: {

            break;
        }
        // not found and slot empty - new key/val insert at slot
        case NOT_FOUND: {

            break;
        }
        // not found and slot occupied - do robin hood hashing
        case ROBINHOOD_EXIT: {

            break;
        }

    }
}


/*
====================PRIVATE FUNCTIONS====================
*/

/* 1 2 3 4 5 6 7 8 9
  s = 1
  n = 3
     2 3 4
*/
static inline void init_buckets(hashmap* map, u64 start, u64 num)
{
    // psl val 0 indicates empty bucket
    memset(GET_SLOT(map, start), 0, (1 + map->key_size) * num);
    memset(GET_VAL(map, start), 0, (map->val_size) * num);
}
/*


  1 1 2 3 2 0
[ a b c d e   ]
  0 1 2 3 4 5

f -> 1
psl: 1 2 3 4 -> robinhood exit idx 4

*/
static u64 map_lookup(const hashmap* map, const u8* key, MAP_LOOKUP_RES* res)
{
    u64 idx = map->hash_fn(key, map->key_size) % map->capacity;
    u8  psl = 1; // stored value for PSL=0 is 1

    for (u64 i = idx;; i = (i + 1) % map->capacity) {
        u8 slot_psl = *GET_PSL(map, i);

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

static void map_insert(hashmap* map, u8* key, u8* val, u8 psl)
{

}

