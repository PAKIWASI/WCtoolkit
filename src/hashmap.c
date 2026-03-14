#include "hashmap.h"
#include "common.h"



#define GET_SLOT(map, i) (map->keys + ((1 + map->key_size) * (i))) // psl byte
#define GET_PSL(map, i)  (GET_SLOT(map, i))                        // same — psl is byte 0
#define GET_KEY(map, i)  (GET_SLOT(map, i) + 1)                    // key starts at byte 1
#define GET_VAL(map, i)  (map->vals + ((map->val_size) * (i)))

// psl sentient val to indicate empty bucket
#define BUCKET_EMPTY 0



/*
====================PRIVATE DECLERATIONS====================
*/

static inline void init_buckets(hashmap* map, u64 start, u64 num);
static u64         map_lookup(const hashmap* map, const u8* key);
static void        map_insert(hashmap* map, u8* key, u8* val, u8 psl);
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


b8 hashmap_put(hashmap* map, const u8* key, const u8* val) {}


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

// returns slot index if found, (u64)-1 if not
static u64 map_lookup(const hashmap* map, const u8* key)
{
    u64 idx = map->hash_fn(key, map->key_size) % map->capacity;
    u8  psl = 1; // stored value for PSL=0 is 1

    for (u64 i = idx;; i = (i + 1) % map->capacity) {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == 0) {
            return (u64)-1; // empty — key not here
        }
        if (slot_psl < psl) {
            return (u64)-1; // Robin Hood early exit
        }
        if (map->cmp_fn(GET_KEY(map, i), key, map->key_size) == 0) {
            return i; // found
        }
        psl++;
    }
}

static void map_insert(hashmap* map, u8* key, u8* val, u8 psl)
{

}

