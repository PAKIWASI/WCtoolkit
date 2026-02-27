#include "hashmap.h"
#include "common.h"
#include "gen_vector.h"
#include "map_setup.h"
#include "wc_macros.h"
#include <string.h>


#define GET_KV(map, i) (((map)->buckets + i))


// TODO: will it be better to pass kv around by val instead of ptr ?


/*
====================PRIVATE FUNCTIONS====================
*/

static void memset_buckets(KV* buckets, u64 size);
static void kv_destroy(KV* kv, const genVec_ops* key_ops, const genVec_ops* val_ops);

static u64  find_slot(const hashmap* map, const u8* key, b8* found, int* tombstone);

static void hashmap_resize(hashmap* map, u64 new_capacity);
static void hashmap_maybe_resize(hashmap* map);



/*
====================PUBLIC FUNCTIONS====================
*/

hashmap* hashmap_create(u32 key_size, u32 val_size,
                        custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const genVec_ops* key_ops, const genVec_ops* val_ops)
{
    CHECK_FATAL(key_size == 0, "key size can't be zero");
    CHECK_FATAL(val_size == 0, "val size can't be zero");

    hashmap* map = malloc(sizeof(hashmap));
    CHECK_FATAL(!map, "map malloc failed");

    map->buckets = malloc(HASHMAP_INIT_CAPACITY * sizeof(KV));
    CHECK_FATAL(!map->buckets, "map bucket init failed");

    memset_buckets(map->buckets, HASHMAP_INIT_CAPACITY);

    map->capacity = HASHMAP_INIT_CAPACITY;
    map->size     = 0;
    map->key_size = key_size;
    map->val_size = val_size;

    map->hash_fn = hash_fn ? hash_fn : fnv1a_hash;
    map->cmp_fn  = cmp_fn  ? cmp_fn  : default_compare;

    map->key_ops = key_ops;
    map->val_ops = val_ops;

    return map;
}

void hashmap_destroy(hashmap* map)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!map->buckets, "map bucket is null");

    for (u64 i = 0; i < map->capacity; i++) {
        KV* kv = GET_KV(map, i);
        if (kv->state == FILLED) {
            kv_destroy(kv, map->key_ops, map->val_ops);
        }
    }
    // TODO: 
    // MAP_FOREACH_BUCKET(map, kv) {
    //     if (kv->state == FILLED) {
    //         kv_destroy(kv, map->key_ops, map->val_ops);
    //     }
    // }

    free(map->buckets);
    free(map);
}


// COPY semantics
b8 hashmap_put(hashmap* map, const u8* key, const u8* val)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");
    CHECK_FATAL(!val, "val is null");

    hashmap_maybe_resize(map);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = find_slot(map, key, &found, &tombstone);

    copy_fn k_copy = MAP_COPY(map->key_ops);
    copy_fn v_copy = MAP_COPY(map->val_ops);
    delete_fn k_del = MAP_DEL(map->key_ops);
    delete_fn v_del = MAP_DEL(map->val_ops);

    if (found) {
        KV* kv = GET_KV(map, slot);

        // Free old value's resources
        if (v_del) {
            v_del(kv->val);
        }

        // Update value
        if (v_copy) {
            v_copy(kv->val, val);
        } else {
            memcpy(kv->val, val, map->val_size);
        }

        return 1;
    }

    // New key — insert
    u8* k = malloc(map->key_size);
    CHECK_FATAL(!k, "key malloc failed");
    u8* v = malloc(map->val_size);
    CHECK_FATAL(!v, "val malloc failed");

    if (k_copy) {
        k_copy(k, key);
    } else {
        memcpy(k, key, map->key_size);
    }

    if (v_copy) {
        v_copy(v, val);
    } else {
        memcpy(v, val, map->val_size);
    }

    KV* old_kv   = GET_KV(map, slot);
    old_kv->key   = k;
    old_kv->val   = v;
    old_kv->state = FILLED;

    map->size++;

    return 0;
}

// MOVE semantics
b8 hashmap_put_move(hashmap* map, u8** key, u8** val)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");
    CHECK_FATAL(!*key, "*key is null");
    CHECK_FATAL(!val, "val is null");
    CHECK_FATAL(!*val, "*val is null");

    hashmap_maybe_resize(map);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = find_slot(map, *key, &found, &tombstone);

    if (found) {
        KV* kv = GET_KV(map, slot);

        if (map->val_ops && map->val_ops->del_fn) {
            map->val_ops->del_fn(kv->val);
        }

        if (map->val_ops && map->val_ops->move_fn) {
            map->val_ops->move_fn(kv->val, val);
        } else {
            memcpy(kv->val, *val, map->val_size);
            *val = NULL;
        }

        // Key already exists — clean up the passed key
        if (map->key_ops && map->key_ops->del_fn) {
            map->key_ops->del_fn(*key);
        }
        free(*key);
        *key = NULL;

        return 1;
    }

    u8* k = malloc(map->key_size);
    CHECK_FATAL(!k, "key malloc failed");
    u8* v = malloc(map->val_size);
    CHECK_FATAL(!v, "val malloc failed");

    if (map->key_ops && map->key_ops->move_fn) {
        map->key_ops->move_fn(k, key);
    } else {
        memcpy(k, *key, map->key_size);
        *key = NULL;
    }

    if (map->val_ops && map->val_ops->move_fn) {
        map->val_ops->move_fn(v, val);
    } else {
        memcpy(v, *val, map->val_size);
        *val = NULL;
    }

    KV* old_kv   = GET_KV(map, slot);
    old_kv->key   = k;
    old_kv->val   = v;
    old_kv->state = FILLED;

    map->size++;

    return 0;
}

// Mixed: key copy, val move
b8 hashmap_put_val_move(hashmap* map, const u8* key, u8** val)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");
    CHECK_FATAL(!val, "val is null");
    CHECK_FATAL(!*val, "*val is null");

    hashmap_maybe_resize(map);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = find_slot(map, key, &found, &tombstone);

    if (found) {
        KV* kv = GET_KV(map, slot);

        if (map->val_ops && map->val_ops->del_fn) {
            map->val_ops->del_fn(kv->val);
        }

        if (map->val_ops && map->val_ops->move_fn) {
            map->val_ops->move_fn(kv->val, val);
        } else {
            memcpy(kv->val, *val, map->val_size);
            *val = NULL;
        }

        return 1;
    }

    u8* k = malloc(map->key_size);
    CHECK_FATAL(!k, "key malloc failed");
    u8* v = malloc(map->val_size);
    CHECK_FATAL(!v, "val malloc failed");

    if (map->key_ops && map->key_ops->copy_fn) {
        map->key_ops->copy_fn(k, key);
    } else {
        memcpy(k, key, map->key_size);
    }

    if (map->val_ops && map->val_ops->move_fn) {
        map->val_ops->move_fn(v, val);
    } else {
        memcpy(v, *val, map->val_size);
        *val = NULL;
    }

    KV* old_kv   = GET_KV(map, slot);
    old_kv->key   = k;
    old_kv->val   = v;
    old_kv->state = FILLED;

    map->size++;

    return 0;
}

// Mixed: key move, val copy
b8 hashmap_put_key_move(hashmap* map, u8** key, const u8* val)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");
    CHECK_FATAL(!*key, "*key is null");
    CHECK_FATAL(!val, "val is null");

    hashmap_maybe_resize(map);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = find_slot(map, *key, &found, &tombstone);

    if (found) {
        KV* kv = GET_KV(map, slot);

        if (map->val_ops && map->val_ops->del_fn) {
            map->val_ops->del_fn(kv->val);
        }

        if (map->val_ops && map->val_ops->copy_fn) {
            map->val_ops->copy_fn(kv->val, val);
        } else {
            memcpy(kv->val, val, map->val_size);
        }

        if (map->key_ops && map->key_ops->del_fn) {
            map->key_ops->del_fn(*key);
        }
        free(*key);
        *key = NULL;

        return 1;
    }

    u8* k = malloc(map->key_size);
    CHECK_FATAL(!k, "key malloc failed");
    u8* v = malloc(map->val_size);
    CHECK_FATAL(!v, "val malloc failed");

    if (map->key_ops && map->key_ops->move_fn) {
        map->key_ops->move_fn(k, key);
    } else {
        memcpy(k, *key, map->key_size);
        *key = NULL;
    }

    if (map->val_ops && map->val_ops->copy_fn) {
        map->val_ops->copy_fn(v, val);
    } else {
        memcpy(v, val, map->val_size);
    }

    KV* old_kv   = GET_KV(map, slot);
    old_kv->key   = k;
    old_kv->val   = v;
    old_kv->state = FILLED;

    map->size++;

    return 0;
}

b8 hashmap_get(const hashmap* map, const u8* key, u8* val)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");
    CHECK_FATAL(!val, "val is null");

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = find_slot(map, key, &found, &tombstone);

    if (found) {
        const KV* kv = GET_KV(map, slot);

        if (map->val_ops && map->val_ops->copy_fn) {
            map->val_ops->copy_fn(val, kv->val);
        } else {
            memcpy(val, kv->val, map->val_size);
        }

        return 1;
    }

    return 0;
}

u8* hashmap_get_ptr(hashmap* map, const u8* key)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = find_slot(map, key, &found, &tombstone);

    if (found) {
        return (GET_KV(map, slot))->val;
    }

    return NULL;
}

KV* hashmap_get_bucket(hashmap* map, u64 i)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(i >= map->capacity, "index out of bounds");

    return (map->buckets + i);
}

b8 hashmap_del(hashmap* map, const u8* key, u8* out)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");

    if (map->size == 0) { return 0; }

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = find_slot(map, key, &found, &tombstone);

    if (found) {
        KV* kv = GET_KV(map, slot);

        if (out) {
            if (map->val_ops && map->val_ops->copy_fn) {
                map->val_ops->copy_fn(out, kv->val);
            } else {
                memcpy(out, kv->val, map->val_size);
            }
        }

        kv_destroy(kv, map->key_ops, map->val_ops);

        kv->key   = NULL;
        kv->val   = NULL;
        kv->state = TOMBSTONE;

        map->size--;

        hashmap_maybe_resize(map);

        return 1;
    }

    return 0;
}

b8 hashmap_has(const hashmap* map, const u8* key)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");

    b8  found     = 0;
    int tombstone = -1;
    find_slot(map, key, &found, &tombstone);

    return found;
}

void hashmap_print(const hashmap* map, print_fn key_print, print_fn val_print)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key_print, "key_print is null");
    CHECK_FATAL(!val_print, "val_print is null");

    printf("\t=========\n");
    printf("\tSize: %lu / Capacity: %lu\n", map->size, map->capacity);
    printf("\t=========\n");

    for (u64 i = 0; i < map->capacity; i++) {
        const KV* kv = GET_KV(map, i);
        if (kv->state == FILLED) {
            putchar('\t');
            key_print(kv->key);
            printf(" => ");
            val_print(kv->val);
            putchar('\t');
        }
    }

    printf("\t=========\n");
}

void hashmap_clear(hashmap* map)
{
    CHECK_FATAL(!map, "map is null");

    for (u64 i = 0; i < map->capacity; i++) {
        KV* kv = GET_KV(map, i);
        
    }

}

void hashmap_copy(hashmap* dest, const hashmap* src)
{

}


/*
====================PRIVATE FUNCTION IMPLEMENTATIONS====================
*/

static void kv_destroy(KV* kv, const genVec_ops* key_ops, const genVec_ops* val_ops)
{
    CHECK_FATAL(!kv, "kv is null");

    if (kv->key) {
        if (key_ops && key_ops->del_fn) {
            key_ops->del_fn(kv->key);
        }
        free(kv->key);
    }

    if (kv->val) {
        if (val_ops && val_ops->del_fn) {
            val_ops->del_fn(kv->val);
        }
        free(kv->val);
    }
}

// we can memset so key, val will be effectively NULL
// state will be = 0, which is EMPTY
// externally, if someone does hashmap_get_bucket(map, i) and we 
// haven't set it, it will be NULL
static void memset_buckets(KV* buckets, u64 size)
{
    memset(buckets, 0, sizeof(KV) * size);
}


static u64 find_slot(const hashmap* map, const u8* key, b8* found, int* tombstone)
{
    u64 index = map->hash_fn(key, map->key_size) % map->capacity;

    *found     = 0;
    *tombstone = -1;

    for (u64 x = 0; x < map->capacity; x++) {
        u64      i  = (index + x) % map->capacity;
        const KV* kv = GET_KV(map, i);

        switch (kv->state) {
            case EMPTY:
                return i;
            case FILLED:
                if (map->cmp_fn(kv->key, key, map->key_size) == 0) {
                    *found = 1;
                    return i;
                }
                break;
            case TOMBSTONE:
                if (*tombstone == -1) {
                    *tombstone = (int)i;
                }
                break;
        }
    }

    return (*tombstone != -1) ? (u64)*tombstone : 0;
}

static void hashmap_resize(hashmap* map, u64 new_capacity)
{
    if (new_capacity <= HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    KV* old_vec = map->buckets;
    u64 old_cap = map->capacity;

    map->buckets = malloc(new_capacity * sizeof(KV));
    memset_buckets(map->buckets, new_capacity);

    map->capacity = new_capacity;
    map->size     = 0;

    for (u64 i = 0; i < old_cap; i++) {
        const KV* old_kv = (old_vec + i);

        if (old_kv->state == FILLED) {
            b8  found     = 0;
            int tombstone = -1;
            u64 slot      = find_slot(map, old_kv->key, &found, &tombstone);

            KV* new_kv   = GET_KV(map, slot);
            new_kv->key   = old_kv->key;
            new_kv->val   = old_kv->val;
            new_kv->state = FILLED;

            map->size++;
        }
    }

    free(old_vec);
}

static void hashmap_maybe_resize(hashmap* map)
{
    CHECK_FATAL(!map, "map is null");

    double load_factor = (double)map->size / (double)map->capacity;

    if (load_factor > LOAD_FACTOR_GROW) {
        u64 new_cap = next_prime(map->capacity);
        hashmap_resize(map, new_cap);
    } else if (load_factor < LOAD_FACTOR_SHRINK && map->capacity > HASHMAP_INIT_CAPACITY) {
        u64 new_cap = prev_prime(map->capacity);
        if (new_cap >= HASHMAP_INIT_CAPACITY) {
            hashmap_resize(map, new_cap);
        }
    }
}

