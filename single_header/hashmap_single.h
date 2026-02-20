#ifndef WC_HASHMAP_SINGLE_H
#define WC_HASHMAP_SINGLE_H

/*
 * hashmap_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "hashmap_single.h"
 *
 * All other files just:
 *     #include "hashmap_single.h"
 */

/* ===== common.h ===== */
#ifndef WC_COMMON_H
#define WC_COMMON_H

/*
 * C Data Structures Library
 * Copyright (c) 2026 Wasi Ullah (PAKIWASI)
 * Licensed under the MIT License. See LICENSE file for details.
 */



// LOGGING/ERRORS

#include <stdio.h>
#include <stdlib.h>

// ANSI Color Codes
#define COLOR_RESET  "\033[0m"
#define COLOR_RED    "\033[1;31m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_GREEN  "\033[1;32m"
#define COLOR_BLUE   "\033[1;34m"
#define COLOR_CYAN   "\033[1;36m"


#define WARN(fmt, ...)                                            \
    do {                                                          \
        printf(COLOR_YELLOW "[WARN]"                              \
                            " %s:%d:%s(): " fmt "\n" COLOR_RESET, \
               __FILE__, __LINE__, __func__, ##__VA_ARGS__);      \
    } while (0)

#define FATAL(fmt, ...)                                         \
    do {                                                        \
        fprintf(stderr,                                         \
                COLOR_RED "[FATAL]"                             \
                          " %s:%d:%s(): " fmt "\n" COLOR_RESET, \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__);   \
        exit(EXIT_FAILURE);                                     \
    } while (0)

#define CHECK_WARN(cond, fmt, ...)                           \
    do {                                                     \
        if (__builtin_expect(!!(cond), 0)) {                                        \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                    \
    } while (0)

#define CHECK_WARN_RET(cond, ret, fmt, ...)                  \
    do {                                                     \
        if (__builtin_expect(!!(cond), 0)) {                                        \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                                      \
        }                                                    \
    } while (0)

#define CHECK_FATAL(cond, fmt, ...)                           \
    do {                                                      \
        if (__builtin_expect(!!(cond), 0)) {                                           \
            FATAL("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                     \
    } while (0)

#define LOG(fmt, ...)                                       \
    do {                                                    \
        printf(COLOR_CYAN "[LOG]"                           \
                          " : %s(): " fmt "\n" COLOR_RESET, \
               __func__, ##__VA_ARGS__);                    \
    } while (0)


// TYPES

#include <stdint.h>

typedef uint8_t  u8;
typedef uint8_t  b8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define false ((b8)0)
#define true  ((b8)1)


// GENERIC FUNCTIONS
typedef void (*copy_fn)(u8* dest, const u8* src);
typedef void (*move_fn)(u8* dest, u8** src);
typedef void (*delete_fn)(u8* key);
typedef void (*print_fn)(const u8* elm);
typedef int (*compare_fn)(const u8* a, const u8* b, u64 size);


// CASTING

#define cast(x)    ((u8*)(&(x)))
#define castptr(x) ((u8*)(x))


// COMMON SIZES

#define KB (1 << 10)
#define MB (1 << 20)

#define nKB(n) ((u64)((n) * KB))
#define nMB(n) ((u64)((n) * MB))


// RAW BYTES TO HEX

static inline void print_hex(const u8* ptr, u64 size, u32 bytes_per_line) 
{
    if (ptr == NULL || size == 0 || bytes_per_line == 0) { return; }

    // hex rep 0-15
    const char* hex = "0123456789ABCDEF";
    
    for (u64 i = 0; i < size; i++) 
    {
        u8 val1 = ptr[i] >> 4;      // get upper 4 bits as num b/w 0-15
        u8 val2 = ptr[i] & 0x0F;    // get lower 4 bits as num b/w 0-15
        
        printf("%c%c", hex[val1], hex[val2]);
        
        // Add space or newline appropriately
        if ((i + 1) % bytes_per_line == 0) {
            printf("\n");
        } else if (i < size - 1) {
            printf(" ");
        }
    }

    // Add final newline if we didn't just print one
    if (size % bytes_per_line != 0) {
        printf("\n");
    }
}


// TEST HELPERS

// Generic print functions for primitive types
static inline void wc_print_int(const u8* elm)
{
    printf("%d ", *(int*)elm);
}
static inline void wc_print_u32(const u8* elm)
{
    printf("%u ", *(u32*)elm);
}
static inline void wc_print_u64(const u8* elm)
{
    printf("%llu ", (unsigned long long)*(u64*)elm);
}
static inline void wc_print_float(const u8* elm)
{
    printf("%.2f ", *(float*)elm);
}
static inline void wc_print_char(const u8* elm)
{
    printf("%c ", *(char*)elm);
}
static inline void wc_print_cstr(const u8* elm)
{
    printf("%s ", (const char*)elm);
}

#endif /* WC_COMMON_H */

/* ===== map_setup.h ===== */
#ifndef WC_MAP_SETUP_H
#define WC_MAP_SETUP_H

#ifndef MAP_SETUP
#define MAP_SETUP

#include <string.h>


typedef enum { 
    EMPTY     = 0,
    FILLED    = 1,
    TOMBSTONE = 2
} STATE;


typedef u64 (*custom_hash_fn)(const u8* key, u64 size);



#define LOAD_FACTOR_GROW      0.70
#define LOAD_FACTOR_SHRINK    0.20
#define HASHMAP_INIT_CAPACITY 17 //prime no (index = hash % capacity)


/*
====================DEFAULT FUNCTIONS====================
*/
// 32-bit FNV-1a (default hash)
static u64 fnv1a_hash(const u8* bytes, u64 size)
{
    u32 hash = 2166136261U; // FNV offset basis

    for (u64 i = 0; i < size; i++) {
        hash ^= bytes[i];  // XOR with current byte
        hash *= 16777619U; // Multiply by FNV prime
    }

    return hash;
}


// Default compare function
static int default_compare(const u8* a, const u8* b, u64 size)
{
    return memcmp(a, b, size);
}

static const u32 PRIMES[] = {
    17,      37,      79,      163,      331,      673,      1361,    2729,
    5471,    10949,   21911,   43853,    87719,    175447,   350899,  701819,
    1403641, 2807303, 5614657, 11229331, 22458671, 44917381, 89834777
};

static const u32 PRIMES_COUNT = sizeof(PRIMES) / sizeof(PRIMES[0]);

// Find the next prime number larger than current
static u64 next_prime(u64 current)
{
    for (u64 i = 0; i < PRIMES_COUNT; i++) {
        if (PRIMES[i] > current) {
            return PRIMES[i];
        }
    }

    // If we've exceeded our prime table, fall back to approximate prime
    // Using formula: next ≈ current * 2 + 1 (often prime or close to it)
    LOG("Warning: exceeded prime table, using approximation\n");
    return (current * 2) + 1;
}

// Find the previous prime number smaller than current
static u64 prev_prime(u64 current)
{
    // Search backwards through prime table
    for (u64 i = PRIMES_COUNT - 1; i >= 0; i--) {
        if (PRIMES[i] < current) {
            return PRIMES[i];
        }
    }

    // Should never happen if HASHMAP_INIT_CAPACITY is in our table
    LOG("Warning: no smaller prime found");
    return HASHMAP_INIT_CAPACITY;
}


// custom hash function for "String"

static u64 murmurhash3_str(const u8* key, u64 size) 
{
    (void)size;
    
    String* str= (String*)key;
    const char* data = string_data_ptr(str);
    u64 len = string_len(str);
    
    const u32 c1 = 0xcc9e2d51;
    const u32 c2 = 0x1b873593;
    const u32 seed = 0x9747b28c;
    
    u32 h1 = seed;
    
    // Body - process 4-byte chunks
    const u32* blocks = (const u32*)data;
    const u64 nblocks = len / 4;
    
    for (u64 i = 0; i < nblocks; i++) {
        u32 k1 = blocks[i];
        
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        
        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = (h1 * 5) + 0xe6546b64;
    }
    
    // Tail - handle remaining bytes
    const u8* tail = (const uint8_t*)(data + ((size_t)nblocks * 4));
    u32 k1 = 0;
    
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1;
                k1 = (k1 << 15) | (k1 >> 17);
                k1 *= c2;
                h1 ^= k1;
        default:
           break; 
    }
    
    // Finalization
    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    
    return h1;
}

#endif /* WC_MAP_SETUP_H */

/* ===== hashmap.h ===== */
#ifndef WC_HASHMAP_H
#define WC_HASHMAP_H

typedef struct {
    u8*             buckets;
    u64             size;
    u64             capacity;
    u32             key_size;
    u32             val_size;
    custom_hash_fn  hash_fn;
    compare_fn      cmp_fn;
    copy_fn         key_copy_fn;
    move_fn         key_move_fn;
    delete_fn       key_del_fn;
    copy_fn         val_copy_fn;
    move_fn         val_move_fn;
    delete_fn       val_del_fn;
} hashmap;

/**
 * Create a new hashmap
 */
hashmap* hashmap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn,
                        compare_fn cmp_fn, copy_fn key_copy, copy_fn val_copy,
                        move_fn key_move, move_fn val_move,
                        delete_fn key_del, delete_fn val_del);

void hashmap_destroy(hashmap* map);

/**
 * Insert or update key-value pair (COPY semantics)
 * Both key and val are passed as const u8*
 * 
 * @return 1 if key existed (updated), 0 if new key inserted
 */
b8 hashmap_put(hashmap* map, const u8* key, const u8* val);

/**
 * Insert or update key-value pair (MOVE semantics)
 * Both key and val are passed as u8** and will be nulled
 * 
 * @return 1 if key existed (updated), 0 if new key inserted
 */
b8 hashmap_put_move(hashmap* map, u8** key, u8** val);

/**
 * Insert with mixed semantics
 */
b8 hashmap_put_val_move(hashmap* map, const u8* key, u8** val);
b8 hashmap_put_key_move(hashmap* map, u8** key, const u8* val);

/**
 * Get value for key (copy semantics)
 */
b8 hashmap_get(const hashmap* map, const u8* key, u8* val);

/**
 * Get pointer to value (no copy)
 * WARNING: Pointer invalidated by put/del operations
 */
u8* hashmap_get_ptr(hashmap* map, const u8* key);

/**
 * Delete key-value pair
 * If out is provided, value is copied to it before deletion
 */
b8 hashmap_del(hashmap* map, const u8* key, u8* out);



/**
 * Check if key exists
 */
b8 hashmap_has(const hashmap* map, const u8* key);

/**
 * Print all key-value pairs
 */
void hashmap_print(const hashmap* map, print_fn key_print, print_fn val_print);



static inline u64 hashmap_size(const hashmap* map)
{
    CHECK_FATAL(!map, "map is null");
    return map->size;
}

static inline u64 hashmap_capacity(const hashmap* map)
{
    CHECK_FATAL(!map, "map is null");
    return map->capacity;
}

static inline b8 hashmap_empty(const hashmap* map)
{
    CHECK_FATAL(!map, "map is null");
    return map->size == 0;
}

#endif /* WC_HASHMAP_H */

#ifdef WC_IMPLEMENTATION

/* ===== hashmap.c ===== */
#ifndef WC_HASHMAP_IMPL
#define WC_HASHMAP_IMPL

#define GET_KV(data, i) ((KV*)(data) + i)


typedef struct {
    u8* key;
    u8* val;
    STATE state;
} KV;

/*
====================KV HANDLERS====================
*/

static void kv_destroy(delete_fn key_del, delete_fn val_del, const KV* kv)
{
    CHECK_FATAL(!kv, "kv is null");

    if (kv->key) {
        if (key_del) {
            key_del(kv->key); 
        }
        free(kv->key);
    }

    if (kv->val) {
        if (val_del) {
            val_del(kv->val);
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
        .key = NULL, 
        .val = NULL, 
        .state = EMPTY 
    };

    for (u64 i = 0; i < size; i++) {
        memcpy(GET_KV(buckets, i), &kv, sizeof(KV));
    }
}



static u64 find_slot(const hashmap* map, const u8* key,
                        b8* found, int* tombstone)
{
    u64 index = map->hash_fn(key, map->key_size) % map->capacity;

    *found = 0;
    *tombstone = -1;

    for (u64 x = 0; x < map->capacity; x++) 
    {
        u64 i = (index + x) % map->capacity;
        const KV* kv = GET_KV(map->buckets, i);

        switch (kv->state) {
            case EMPTY:
                return i;
            case FILLED:
                if (map->cmp_fn(kv->key, key, map->key_size) == 0) 
                {
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
    map->size = 0;


    for (u64 i = 0; i < old_cap; i++) 
    {
        const KV* old_kv = GET_KV(old_vec, i);
        
        if (old_kv->state == FILLED) {
            b8 found = 0;
            int tombstone = -1;
            u64 slot = find_slot(map, old_kv->key, &found, &tombstone);

            KV* new_kv = GET_KV(map->buckets, slot);
            new_kv->key = old_kv->key;
            new_kv->val = old_kv->val;
            new_kv->state = FILLED;

            map->size++;
        }
    }

     // free the container, 
     free(old_vec);  // the key, vals of each KV are transferred    
}

static void hashmap_maybe_resize(hashmap* map) 
{
    CHECK_FATAL(!map, "map is null");
    
    double load_factor = (double)map->size / (double)map->capacity;
    
    if (load_factor > LOAD_FACTOR_GROW) {
        u64 new_cap = next_prime(map->capacity);
        hashmap_resize(map, new_cap);
    }
    else if (load_factor < LOAD_FACTOR_SHRINK && map->capacity > HASHMAP_INIT_CAPACITY) 
    {
        u64 new_cap = prev_prime(map->capacity);
        if (new_cap >= HASHMAP_INIT_CAPACITY) {
            hashmap_resize(map, new_cap);
        }
    }
}

/*
====================PUBLIC FUNCTIONS====================
*/

hashmap* hashmap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn,
                        compare_fn cmp_fn, copy_fn key_copy, copy_fn val_copy,
                        move_fn key_move, move_fn val_move,
                        delete_fn key_del, delete_fn val_del)
{
    CHECK_FATAL(key_size == 0, "key size can't be zero");
    CHECK_FATAL(val_size == 0, "val size can't be zero");

    hashmap* map = malloc(sizeof(hashmap));
    CHECK_FATAL(!map, "map malloc failed");

    map->buckets = malloc(HASHMAP_INIT_CAPACITY * sizeof(KV));
    CHECK_FATAL(!map->buckets, "map bucket init failed");

    reset_buckets(map->buckets, HASHMAP_INIT_CAPACITY);

    
    map->capacity = HASHMAP_INIT_CAPACITY;
    map->size = 0;
    map->key_size = key_size;
    map->val_size = val_size;

    map->hash_fn = hash_fn ? hash_fn : fnv1a_hash;
    map->cmp_fn = cmp_fn ? cmp_fn : default_compare;
    
    map->key_copy_fn = key_copy;
    map->key_move_fn = key_move;
    map->key_del_fn = key_del;
    
    map->val_copy_fn = val_copy;
    map->val_move_fn = val_move;
    map->val_del_fn = val_del;

    return map;
}

void hashmap_destroy(hashmap* map)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!map->buckets, "map bucket is null");

    // if KV own memory, free it
    for (u64 i = 0; i < map->capacity; i++) {
        const KV* kv = GET_KV(map->buckets, i);
        if (kv->state == FILLED) {
            kv_destroy(map->key_del_fn, map->val_del_fn, kv);
        }
    }

    free(map->buckets); // free KV container
    free(map);          // free struct
}




// COPY semantics - key and val are const u8*
b8 hashmap_put(hashmap* map, const u8* key, const u8* val)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");
    CHECK_FATAL(!val, "val is null");

    hashmap_maybe_resize(map);
    
    b8 found = 0;
    int tombstone = -1;
    u64 slot = find_slot(map, key, &found, &tombstone);
    
    // found the key - update val
    if (found) {
        KV* kv = GET_KV(map->buckets, slot);
        
        // Free old value's resources
        if (map->val_del_fn) {
            map->val_del_fn(kv->val);
        }
        
        // Update value
        if (map->val_copy_fn) {
            map->val_copy_fn(kv->val, val);
        } else {
            memcpy(kv->val, val, map->val_size);
        }
        
        return 1; // found - updated
    } 
    
    // New key - insert

    u8* k = malloc(map->key_size);
    CHECK_FATAL(!k, "key malloc failed");
    u8* v = malloc(map->val_size);
    CHECK_FATAL(!v, "val malloc failed");

    // this done so we can don't have garbage value when passed to copy/move fns
    // memset(k, 0, map->key_size);     // user my want to read the casted struct
    // memset(v, 0, map->val_size);
    
    // Copy key
    if (map->key_copy_fn) {
        map->key_copy_fn(k, key);
    } else {
        memcpy(k, key, map->key_size);
    }
    
    // Copy value
    if (map->val_copy_fn) {
        map->val_copy_fn(v, val);
    } else {
        memcpy(v, val, map->val_size);
    }
    
    KV* old_kv = GET_KV(map->buckets, slot); 
    old_kv->key = k;
    old_kv->val = v;
    old_kv->state = FILLED;

    map->size++;
    
    return 0;
}

// MOVE semantics - key and val are u8**
b8 hashmap_put_move(hashmap* map, u8** key, u8** val)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");
    CHECK_FATAL(!*key, "*key is null");
    CHECK_FATAL(!val, "val is null");
    CHECK_FATAL(!*val, "*val is null");
    
    hashmap_maybe_resize(map);
    
    b8 found = 0;
    int tombstone = -1;
    // IMPORTANT: Dereference *key to pass u8* to find_slot
    u64 slot = find_slot(map, *key, &found, &tombstone);
    
    if (found) {
        KV* kv = GET_KV(map->buckets, slot);
        
        // Free old value's resources
        if (map->val_del_fn) {
            map->val_del_fn(kv->val);
        }
        
        // Move value
        if (map->val_move_fn) {
            map->val_move_fn(kv->val, val);
        } else {
            memcpy(kv->val, *val, map->val_size);
            *val = NULL;
        }
        
        // Key already exists, clean up the passed key
        if (map->key_del_fn) {
            map->key_del_fn(*key);
        }
        free(*key);
        *key = NULL;
        
        return 1;
    }
    
    // New key - insert with move semantics
    u8* k = malloc(map->key_size);
    CHECK_FATAL(!k, "key malloc failed");
    u8* v = malloc(map->val_size);
    CHECK_FATAL(!v, "val malloc failed");
    
    
    // Move key
    if (map->key_move_fn) {
        map->key_move_fn(k, key);
    } else {
        memcpy(k, *key, map->key_size);
        *key = NULL;
    }
    
    // Move value
    if (map->val_move_fn) {
        map->val_move_fn(v, val);
    } else {
        memcpy(v, *val, map->val_size);
        *val = NULL;
    }

    KV* old_kv = GET_KV(map->buckets, slot);
    old_kv->key = k;
    old_kv->val = v;
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
    
    b8 found = 0;
    int tombstone = -1;
    u64 slot = find_slot(map, key, &found, &tombstone);
    
    if (found) {
        KV* kv = GET_KV(map->buckets, slot);
        
        if (map->val_del_fn) {
            map->val_del_fn(kv->val);
        }
        
        if (map->val_move_fn) {
            map->val_move_fn(kv->val, val);
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
    
    
    if (map->key_copy_fn) {
        map->key_copy_fn(k, key);
    } else {
        memcpy(k, key, map->key_size);
    }
    
    if (map->val_move_fn) {
        map->val_move_fn(v, val);
    } else {
        memcpy(v, *val, map->val_size);
        *val = NULL;
    }

    KV* old_kv = GET_KV(map->buckets, slot);
    
    old_kv->key = k;
    old_kv->val = v;
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
    
    b8 found = 0;
    int tombstone = -1;
    u64 slot = find_slot(map, *key, &found, &tombstone);
    
    if (found) {
        KV* kv = GET_KV(map->buckets, slot);
        
        if (map->val_del_fn) {
            map->val_del_fn(kv->val);
        }
        
        if (map->val_copy_fn) {
            map->val_copy_fn(kv->val, val);
        } else {
            memcpy(kv->val, val, map->val_size);
        }
        
        if (map->key_del_fn) {
            map->key_del_fn(*key);
        }
        free(*key);
        *key = NULL;
        
        return 1;
    }
    
    u8* k = malloc(map->key_size);
    CHECK_FATAL(!k, "key malloc failed");
    u8* v = malloc(map->val_size);
    CHECK_FATAL(!v, "val malloc failed");
    
    
    if (map->key_move_fn) {
        map->key_move_fn(k, key);
    } else {
        memcpy(k, *key, map->key_size);
        *key = NULL;
    }
    
    if (map->val_copy_fn) {
        map->val_copy_fn(v, val);
    } else {
        memcpy(v, val, map->val_size);
    }

    KV* old_kv = GET_KV(map->buckets, slot);

    old_kv->key = k;
    old_kv->val = v;
    old_kv->state = FILLED;
    
    map->size++;
    
    return 0;
}

b8 hashmap_get(const hashmap* map, const u8* key, u8* val)
{
    CHECK_FATAL(!map, "map is null");
    CHECK_FATAL(!key, "key is null");
    CHECK_FATAL(!val, "val is null");
    
    b8 found = 0;
    int tombstone = -1;
    u64 slot = find_slot(map, key, &found, &tombstone);

    if (found) {
        const KV* kv = GET_KV(map->buckets, slot);
        
        if (map->val_copy_fn) {
            map->val_copy_fn(val, kv->val);
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

    b8 found = 0;
    int tombstone = -1;
    u64 slot = find_slot(map, key, &found, &tombstone);

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

    b8 found = 0;
    int tombstone = -1;
    u64 slot = find_slot(map, key, &found, &tombstone);

    if (found) {
        KV* kv = GET_KV(map->buckets, slot);

        if (out) {
            if (map->val_copy_fn) {
                map->val_copy_fn(out, kv->val);
            } else {
                memcpy(out, kv->val, map->val_size);
            }
        }
        
        kv_destroy(map->key_del_fn, map->val_del_fn, kv);

        kv->key = NULL;
        kv->val = NULL;
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
    
    b8 found = 0;
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

#endif /* WC_HASHMAP_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_HASHMAP_SINGLE_H */
