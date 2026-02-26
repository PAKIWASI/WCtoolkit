#include "hashmap.h"



#define GET_KV(data, i) ((KV*)(data) + i)


typedef struct {
    u8*   key;
    u8*   val;
    STATE state;
} KV;

/*
====================KV HANDLERS====================
*/

static void kv_destroy(const genVec_ops* key_ops, const genVec_ops* val_ops, const KV* kv)
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

/*
====================PRIVATE FUNCTIONS====================
*/

static void reset_buckets(u8* buckets, u64 size)
{
    KV kv = {
        .key   = NULL,
        .val   = NULL,
        .state = EMPTY
    };

    for (u64 i = 0; i < size; i++) {
        memcpy(GET_KV(buckets, i), &kv, sizeof(KV));
    }
}


static u64 find_slot(const hashmap* map, const u8* key, b8* found, int* tombstone)
{
    u64 index = map->hash_fn(key, map->key_size) % map->capacity;

    *found     = 0;
    *tombstone = -1;

    for (u64 x = 0; x < map->capacity; x++) {
        u64      i  = (index + x) % map->capacity;
        const KV* kv = GET_KV(map->buckets, i);

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

    u8* old_vec = map->buckets;
    u64 old_cap = map->capacity;

    map->buckets = malloc(new_capacity * sizeof(KV));
    reset_buckets(map->buckets, new_capacity);

    map->capacity = new_capacity;
    map->size     = 0;

    for (u64 i = 0; i < old_cap; i++) {
        const KV* old_kv = GET_KV(old_vec, i);

        if (old_kv->state == FILLED) {
            b8  found     = 0;
            int tombstone = -1;
            u64 slot      = find_slot(map, old_kv->key, &found, &tombstone);

            KV* new_kv   = GET_KV(map->buckets, slot);
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

    reset_buckets(map->buckets, HASHMAP_INIT_CAPACITY);

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
        const KV* kv = GET_KV(map->buckets, i);
        if (kv->state == FILLED) {
            kv_destroy(map->key_ops, map->val_ops, kv);
        }
    }

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

    if (found) {
        KV* kv = GET_KV(map->buckets, slot);

        // Free old value's resources
        if (map->val_ops && map->val_ops->del_fn) {
            map->val_ops->del_fn(kv->val);
        }

        // Update value
        if (map->val_ops && map->val_ops->copy_fn) {
            map->val_ops->copy_fn(kv->val, val);
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

    if (map->key_ops && map->key_ops->copy_fn) {
        map->key_ops->copy_fn(k, key);
    } else {
        memcpy(k, key, map->key_size);
    }

    if (map->val_ops && map->val_ops->copy_fn) {
        map->val_ops->copy_fn(v, val);
    } else {
        memcpy(v, val, map->val_size);
    }

    KV* old_kv   = GET_KV(map->buckets, slot);
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
        KV* kv = GET_KV(map->buckets, slot);

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

    KV* old_kv   = GET_KV(map->buckets, slot);
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
        KV* kv = GET_KV(map->buckets, slot);

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

    KV* old_kv   = GET_KV(map->buckets, slot);
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
        KV* kv = GET_KV(map->buckets, slot);

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

    KV* old_kv   = GET_KV(map->buckets, slot);
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
        const KV* kv = GET_KV(map->buckets, slot);

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
        return (GET_KV(map->buckets, slot))->val;
    }

    return NULL;
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
        KV* kv = GET_KV(map->buckets, slot);

        if (out) {
            if (map->val_ops && map->val_ops->copy_fn) {
                map->val_ops->copy_fn(out, kv->val);
            } else {
                memcpy(out, kv->val, map->val_size);
            }
        }

        kv_destroy(map->key_ops, map->val_ops, kv);

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
        const KV* kv = GET_KV(map->buckets, i);
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
