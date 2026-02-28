#include "hashmap.h"
#include "common.h"
#include "map_setup.h"
#include "wc_macros.h"
#include <string.h>


#define GET_KV(map, i) ((map)->buckets + (i))


/*
====================PRIVATE FUNCTIONS====================
*/

static void hashmap_memset_buckets(KV* buckets, u64 size);
static void kv_destroy(KV* kv, const container_ops* key_ops, const container_ops* val_ops);

static u64  hashmap_find_slot(const hashmap* map, const u8* key, b8* found, int* tombstone);

static void hashmap_resize(hashmap* map, u64 new_capacity);
static void hashmap_maybe_resize(hashmap* map);



/*
====================PUBLIC FUNCTIONS====================
*/

hashmap* hashmap_create(u32 key_size, u32 val_size,
                        custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* key_ops, const container_ops* val_ops)
{
    CHECK_FATAL(key_size == 0, "key size can't be zero");
    CHECK_FATAL(val_size == 0, "val size can't be zero");

    hashmap* map = malloc(sizeof(hashmap));
    CHECK_FATAL(!map, "map malloc failed");

    map->buckets = malloc(HASHMAP_INIT_CAPACITY * sizeof(KV));
    CHECK_FATAL(!map->buckets, "map bucket init failed");

    hashmap_memset_buckets(map->buckets, HASHMAP_INIT_CAPACITY);

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
    CHECK_FATAL(!map,          "map is null");
    CHECK_FATAL(!map->buckets, "map buckets is null");

    MAP_FOREACH_BUCKET(map, kv) {
        kv_destroy(kv, map->key_ops, map->val_ops);
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

    copy_fn   k_copy = MAP_COPY(map->key_ops);
    copy_fn   v_copy = MAP_COPY(map->val_ops);
    delete_fn v_del  = MAP_DEL(map->val_ops);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = hashmap_find_slot(map, key, &found, &tombstone);

    if (found) {
        KV* kv = GET_KV(map, slot);

        if (v_del)  { v_del(kv->val); }
        if (v_copy) { v_copy(kv->val, val); }
        else        { memcpy(kv->val, val, map->val_size); }

        return 1;
    }

    u8* k = malloc(map->key_size);
    CHECK_FATAL(!k, "key malloc failed");
    u8* v = malloc(map->val_size);
    CHECK_FATAL(!v, "val malloc failed");

    if (k_copy) { k_copy(k, key); }
    else        { memcpy(k, key, map->key_size); }

    if (v_copy) { v_copy(v, val); }
    else        { memcpy(v, val, map->val_size); }

    KV* kv   = GET_KV(map, slot);
    kv->key   = k;
    kv->val   = v;
    kv->state = FILLED;

    map->size++;
    return 0;
}

// MOVE semantics
b8 hashmap_put_move(hashmap* map, u8** key, u8** val)
{
    CHECK_FATAL(!map,  "map is null");
    CHECK_FATAL(!key,  "key is null");
    CHECK_FATAL(!*key, "*key is null");
    CHECK_FATAL(!val,  "val is null");
    CHECK_FATAL(!*val, "*val is null");

    hashmap_maybe_resize(map);

    move_fn   k_move = MAP_MOVE(map->key_ops);
    move_fn   v_move = MAP_MOVE(map->val_ops);
    delete_fn k_del  = MAP_DEL(map->key_ops);
    delete_fn v_del  = MAP_DEL(map->val_ops);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = hashmap_find_slot(map, *key, &found, &tombstone);

    if (found) {
        KV* kv = GET_KV(map, slot);

        if (v_del)  { v_del(kv->val); }
        if (v_move) { v_move(kv->val, val); }
        else        { memcpy(kv->val, *val, map->val_size); *val = NULL; }

        // Key already exists — discard the incoming key
        if (k_del) { k_del(*key); }
        free(*key);
        *key = NULL;

        return 1;
    }

    u8* k = malloc(map->key_size);
    CHECK_FATAL(!k, "key malloc failed");
    u8* v = malloc(map->val_size);
    CHECK_FATAL(!v, "val malloc failed");

    if (k_move) { k_move(k, key); }
    else        { memcpy(k, *key, map->key_size); *key = NULL; }

    if (v_move) { v_move(v, val); }
    else        { memcpy(v, *val, map->val_size); *val = NULL; }

    KV* kv   = GET_KV(map, slot);
    kv->key   = k;
    kv->val   = v;
    kv->state = FILLED;

    map->size++;
    return 0;
}

// Mixed: key copy, val move
b8 hashmap_put_val_move(hashmap* map, const u8* key, u8** val)
{
    CHECK_FATAL(!map,  "map is null");
    CHECK_FATAL(!key,  "key is null");
    CHECK_FATAL(!val,  "val is null");
    CHECK_FATAL(!*val, "*val is null");

    hashmap_maybe_resize(map);

    copy_fn   k_copy = MAP_COPY(map->key_ops);
    move_fn   v_move = MAP_MOVE(map->val_ops);
    delete_fn v_del  = MAP_DEL(map->val_ops);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = hashmap_find_slot(map, key, &found, &tombstone);

    if (found) {
        KV* kv = GET_KV(map, slot);

        if (v_del)  { v_del(kv->val); }
        if (v_move) { v_move(kv->val, val); }
        else        { memcpy(kv->val, *val, map->val_size); *val = NULL; }

        return 1;
    }

    u8* k = malloc(map->key_size);
    CHECK_FATAL(!k, "key malloc failed");
    u8* v = malloc(map->val_size);
    CHECK_FATAL(!v, "val malloc failed");

    if (k_copy) { k_copy(k, key); }
    else        { memcpy(k, key, map->key_size); }

    if (v_move) { v_move(v, val); }
    else        { memcpy(v, *val, map->val_size); *val = NULL; }

    KV* kv   = GET_KV(map, slot);
    kv->key   = k;
    kv->val   = v;
    kv->state = FILLED;

    map->size++;
    return 0;
}

// Mixed: key move, val copy
b8 hashmap_put_key_move(hashmap* map, u8** key, const u8* val)
{
    CHECK_FATAL(!map,  "map is null");
    CHECK_FATAL(!key,  "key is null");
    CHECK_FATAL(!*key, "*key is null");
    CHECK_FATAL(!val,  "val is null");

    hashmap_maybe_resize(map);

    move_fn   k_move = MAP_MOVE(map->key_ops);
    copy_fn   v_copy = MAP_COPY(map->val_ops);
    delete_fn k_del  = MAP_DEL(map->key_ops);
    delete_fn v_del  = MAP_DEL(map->val_ops);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = hashmap_find_slot(map, *key, &found, &tombstone);

    if (found) {
        KV* kv = GET_KV(map, slot);

        if (v_del)  { v_del(kv->val); }
        if (v_copy) { v_copy(kv->val, val); }
        else        { memcpy(kv->val, val, map->val_size); }

        // Discard the incoming key
        if (k_del) { k_del(*key); }
        free(*key);
        *key = NULL;

        return 1;
    }

    u8* k = malloc(map->key_size);
    CHECK_FATAL(!k, "key malloc failed");
    u8* v = malloc(map->val_size);
    CHECK_FATAL(!v, "val malloc failed");

    if (k_move) { k_move(k, key); }
    else        { memcpy(k, *key, map->key_size); *key = NULL; }

    if (v_copy) { v_copy(v, val); }
    else        { memcpy(v, val, map->val_size); }

    KV* kv   = GET_KV(map, slot);
    kv->key   = k;
    kv->val   = v;
    kv->state = FILLED;

    map->size++;
    return 0;
}

b8 hashmap_get(const hashmap* map, const u8* key, u8* val)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");
    CHECK_FATAL(!val, "val is null");

    copy_fn v_copy = MAP_COPY(map->val_ops);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = hashmap_find_slot(map, key, &found, &tombstone);

    if (found) {
        const KV* kv = GET_KV(map, slot);
        if (v_copy) { v_copy(val, kv->val); }
        else        { memcpy(val, kv->val, map->val_size); }
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
    u64 slot      = hashmap_find_slot(map, key, &found, &tombstone);

    return found ? GET_KV(map, slot)->val : NULL;
}

KV* hashmap_get_bucket(hashmap* map, u64 i)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(i >= map->capacity, "index out of bounds");
    return GET_KV(map, i);
}

b8 hashmap_del(hashmap* map, const u8* key, u8* out)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");

    if (map->size == 0) { return 0; }

    copy_fn v_copy = MAP_COPY(map->val_ops);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = hashmap_find_slot(map, key, &found, &tombstone);

    if (found) {
        KV* kv = GET_KV(map, slot);

        if (out) {
            if (v_copy) { v_copy(out, kv->val); }
            else        { memcpy(out, kv->val, map->val_size); }
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
    hashmap_find_slot(map, key, &found, &tombstone);

    return found;
}

void hashmap_print(const hashmap* map, print_fn key_print, print_fn val_print)
{
    CHECK_FATAL(!map,       "map is null");
    CHECK_FATAL(!key_print, "key_print is null");
    CHECK_FATAL(!val_print, "val_print is null");

    printf("\t=========\n");
    printf("\tSize: %lu / Capacity: %lu\n", map->size, map->capacity);
    printf("\t=========\n");

    MAP_FOREACH_BUCKET(map, kv) {
        putchar('\t');
        key_print(kv->key);
        printf(" => ");
        val_print(kv->val);
        putchar('\n');
    }

    printf("\t=========\n");
}

void hashmap_clear(hashmap* map)
{
    CHECK_FATAL(!map, "map is null");

    MAP_FOREACH_BUCKET(map, kv) {
        kv_destroy(kv, map->key_ops, map->val_ops);
        kv->key   = NULL;
        kv->val   = NULL;
        kv->state = EMPTY;
    }

    map->size = 0;
}

void hashmap_copy(hashmap* dest, const hashmap* src)
{
    CHECK_FATAL(!dest, "dest is null");
    CHECK_FATAL(!src,  "src is null");

    copy_fn k_copy = MAP_COPY(src->key_ops);
    copy_fn v_copy = MAP_COPY(src->val_ops);

    // Clear dest KVs (runs del callbacks, resets to EMPTY), keeps the bucket array.
    hashmap_clear(dest);

    // Copy all scalar fields and fn/ops pointers from src, but preserve dest->buckets.
    KV* old_buckets  = dest->buckets;
    u64 old_capacity = dest->capacity;
    memcpy(dest, src, sizeof(hashmap));
    dest->buckets = old_buckets;
    dest->size = 0;

    // If src is larger than dest's existing bucket array, grow it.
    if (src->capacity > old_capacity) {
        KV* grown = realloc(dest->buckets, src->capacity * sizeof(KV));
        CHECK_FATAL(!grown, "bucket realloc failed");
        hashmap_memset_buckets(grown + old_capacity, src->capacity - old_capacity);
        dest->buckets = grown;
    }

    MAP_FOREACH_BUCKET(src, kv) {
        b8  found     = 0;
        int tombstone = -1;
        u64 slot      = hashmap_find_slot(dest, kv->key, &found, &tombstone);

        // calloc (not malloc) so that copy_fn sees zero-initialised memory.
        // copy_fns like string_copy call destroy_stk on dest before writing,
        // and destroy_stk on a zeroed struct is a safe no-op.
        u8* k = calloc(1, src->key_size);
        CHECK_FATAL(!k, "key calloc failed");
        u8* v = calloc(1, src->val_size);
        CHECK_FATAL(!v, "val calloc failed");

        if (k_copy) { k_copy(k, kv->key); }
        else        { memcpy(k, kv->key, src->key_size); }

        if (v_copy) { v_copy(v, kv->val); }
        else        { memcpy(v, kv->val, src->val_size); }

        KV* new_kv   = GET_KV(dest, slot);
        new_kv->key   = k;
        new_kv->val   = v;
        new_kv->state = FILLED;

        dest->size++;
    }
}


/*
====================PRIVATE FUNCTION IMPLEMENTATIONS====================
*/

static void kv_destroy(KV* kv, const container_ops* key_ops, const container_ops* val_ops)
{
    CHECK_FATAL(!kv, "kv is null");

    delete_fn k_del = MAP_DEL(key_ops);
    delete_fn v_del = MAP_DEL(val_ops);

    if (kv->key) {
        if (k_del) { k_del(kv->key); }
        free(kv->key);
    }

    if (kv->val) {
        if (v_del) { v_del(kv->val); }
        free(kv->val);
    }
}

// memset gives: key = NULL, val = NULL, state = EMPTY (= 0)
static void hashmap_memset_buckets(KV* buckets, u64 size)
{
    memset(buckets, 0, sizeof(KV) * size);
}


static u64 hashmap_find_slot(const hashmap* map, const u8* key, b8* found, int* tombstone)
{
    u64 index = map->hash_fn(key, map->key_size) % map->capacity;

    *found     = 0;
    *tombstone = -1;

    for (u64 x = 0; x < map->capacity; x++) {
        u64       i  = (index + x) % map->capacity;
        const KV* kv = GET_KV(map, i);

        switch (kv->state) {
            case EMPTY:
                // Return tombstone slot if we passed one — reuse it
                return (*tombstone != -1) ? (u64)*tombstone : i;
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
    if (new_capacity < HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    KV* old_buckets = map->buckets;
    u64 old_cap     = map->capacity;

    map->buckets = malloc(new_capacity * sizeof(KV));
    CHECK_FATAL(!map->buckets, "resize malloc failed");
    hashmap_memset_buckets(map->buckets, new_capacity);

    map->capacity = new_capacity;
    map->size     = 0;

    // Rehash — pointers are moved as-is, no copy/del needed
    for (u64 i = 0; i < old_cap; i++) {
        const KV* old_kv = old_buckets + i;

        if (old_kv->state == FILLED) {
            b8  found     = 0;
            int tombstone = -1;
            u64 slot      = hashmap_find_slot(map, old_kv->key, &found, &tombstone);

            KV* new_kv   = GET_KV(map, slot);
            new_kv->key   = old_kv->key;
            new_kv->val   = old_kv->val;
            new_kv->state = FILLED;

            map->size++;
        }
    }

    free(old_buckets);
}

static void hashmap_maybe_resize(hashmap* map)
{
    CHECK_FATAL(!map, "map is null");

    double load = (double)map->size / (double)map->capacity;

    if (load > LOAD_FACTOR_GROW) {
        hashmap_resize(map, next_prime(map->capacity));
    } else if (load < LOAD_FACTOR_SHRINK && map->capacity > HASHMAP_INIT_CAPACITY) {
        u64 new_cap = prev_prime(map->capacity);
        if (new_cap >= HASHMAP_INIT_CAPACITY) {
            hashmap_resize(map, new_cap);
        }
    }
}
