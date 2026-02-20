#ifndef WC_HASHSET_SINGLE_H
#define WC_HASHSET_SINGLE_H

/*
 * hashset_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "hashset_single.h"
 *
 * All other files just:
 *     #include "hashset_single.h"
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

/* ===== hashset.h ===== */
#ifndef WC_HASHSET_H
#define WC_HASHSET_H

typedef struct {
    u8*             buckets;
    u64             size;
    u64             capacity;
    u32             elm_size;
    custom_hash_fn  hash_fn;
    compare_fn      cmp_fn;
    copy_fn         copy_fn;
    move_fn         move_fn;
    delete_fn       del_fn;
} hashset;



/**
 * Create a new hashset
 * 
 * @param elm_size - Size in bytes of each element
 * @param hash_fn - Custom hash function (or NULL for default FNV-1a)
 * @param cmp_fn - Custom comparison function (or NULL for memcmp)
 * @param copy_fn - Deep copy function for elms (or NULL for memcpy)
 * @param move_fn - Move function for elms (or NULL for default move)
 * @param del_fn - Cleanup function for elms (or NULL if elms don't own resources)
 */
hashset* hashset_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn, 
                         copy_fn copy_fn, move_fn move_fn, delete_fn del_fn);

/**
 * Destroy hashset and clean up all resources
 */
void hashset_destroy(hashset* set);

/**
 * Insert new element in hashset (if not already present) with COPY semantics
 * 
 * @return 1 if element existed (do nothing), 0 if new element inserted
 */
b8 hashset_insert(hashset* set, const u8* elm);

/**
 * Insert new element in hashset (if not already present) with MOVE semantics
 * 
 * @return 1 if element existed (do nothing), 0 if new element inserted
 */
b8 hashset_insert_move(hashset* set, u8** elm);

/**
 * Check if elm is present in hashset
 * 
 * @return 1 if found, 0 if not found
 */
b8 hashset_has(const hashset* set, const u8* elm);

/**
 * Delete elm from hashset
 * 
 * @return 1 if found and deleted, 0 if not found
 */
b8 hashset_remove(hashset* set, const u8* elm);

/**
 * Print all elements using print_fn
 */
void hashset_print(const hashset* set, print_fn print_fn);

/**
 * Clear all elements from set but keep capacity
 */
void hashset_clear(hashset* set);

/**
 * Reset set to initial capacity and remove all elements
 */
void hashset_reset(hashset* set);

// Inline utility functions
static inline u64 hashset_size(const hashset* set)
{
    CHECK_FATAL(!set, "set is null");
    return set->size;
}

static inline u64 hashset_capacity(const hashset* set)
{
    CHECK_FATAL(!set, "set is null");
    return set->capacity;
}

static inline b8 hashset_empty(const hashset* set)
{
    CHECK_FATAL(!set, "set is null");
    return set->size == 0;
}

#endif /* WC_HASHSET_H */

#ifdef WC_IMPLEMENTATION

/* ===== hashset.c ===== */
#ifndef WC_HASHSET_IMPL
#define WC_HASHSET_IMPL

#include <stdlib.h>
#include <string.h>


typedef struct {
    u8*    elm;
    STATE  state;
} ELM;

#define GET_ELM(data, i) ((ELM*)(data) + (i))

/*
====================ELM HANDLERS====================
*/

static void elm_destroy(delete_fn del_fn, const ELM* elm)
{
    CHECK_FATAL(!elm, "ELM is null");

    if (elm->elm) {
        if (del_fn) {
            del_fn(elm->elm);
        }
        free(elm->elm);
    }
}

/*
====================PRIVATE FUNCTIONS====================
*/

static void reset_buckets(u8* buckets, u64 size)
{
    ELM elm = { 
        .elm = NULL, 
        .state = EMPTY 
    };

    for (u64 i = 0; i < size; i++) {
        memcpy(GET_ELM(buckets, i), &elm, sizeof(ELM));
    }
}


static u64 find_slot(const hashset* set, const u8* element,
                        b8* found, int* tombstone)
{
    u64 index = set->hash_fn(element, set->elm_size) % set->capacity;

    *found = 0;
    *tombstone = -1;

    for (u64 x = 0; x < set->capacity; x++) 
    {
        u64 i = (index + x) % set->capacity;
        const ELM* elm = GET_ELM(set->buckets, i);

        switch (elm->state) {
            case EMPTY:
                return i;
            case FILLED:
                if (set->cmp_fn(elm->elm, element, set->elm_size) == 0) 
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

static void hashset_resize(hashset* set, u64 new_capacity) 
{
    if (new_capacity <= HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    u8* old_buckets = set->buckets;
    u64 old_cap = set->capacity;

    set->buckets = malloc(new_capacity * sizeof(ELM));
    CHECK_FATAL(!set->buckets, "resize malloc failed");
    reset_buckets(set->buckets, new_capacity);

    set->capacity = new_capacity;
    set->size = 0;

    for (u64 i = 0; i < old_cap; i++) 
    {
        const ELM* old_elm = GET_ELM(old_buckets, i);
        
        if (old_elm->state == FILLED) {
            b8 found = 0;
            int tombstone = -1;
            u64 slot = find_slot(set, old_elm->elm, &found, &tombstone);

            ELM* new_elm = GET_ELM(set->buckets, slot);
            new_elm->elm = old_elm->elm;
            new_elm->state = FILLED;

            set->size++;
        }
    }

    free(old_buckets);
}

static void hashset_maybe_resize(hashset* set) 
{
    CHECK_FATAL(!set, "set is null");
    
    double load_factor = (double)set->size / (double)set->capacity;
    
    if (load_factor > LOAD_FACTOR_GROW) {
        u64 new_cap = next_prime(set->capacity);
        hashset_resize(set, new_cap);
    }
    else if (load_factor < LOAD_FACTOR_SHRINK && set->capacity > HASHMAP_INIT_CAPACITY) 
    {
        u64 new_cap = prev_prime(set->capacity);
        if (new_cap >= HASHMAP_INIT_CAPACITY) {
            hashset_resize(set, new_cap);
        }
    }
}

/*
====================PUBLIC FUNCTIONS====================
*/

hashset* hashset_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn, 
                         copy_fn copy_fn, move_fn move_fn, delete_fn del_fn)
{
    CHECK_FATAL(elm_size == 0, "elm size can't be 0");

    hashset* set = malloc(sizeof(hashset));
    CHECK_FATAL(!set, "set malloc failed");

    set->buckets = malloc(HASHMAP_INIT_CAPACITY * sizeof(ELM));
    CHECK_FATAL(!set->buckets, "set bucket init failed");

    reset_buckets(set->buckets, HASHMAP_INIT_CAPACITY);

    set->capacity = HASHMAP_INIT_CAPACITY;
    set->size = 0;
    set->elm_size = elm_size;

    set->hash_fn = hash_fn ? hash_fn : fnv1a_hash;
    set->cmp_fn = cmp_fn ? cmp_fn : default_compare;

    set->copy_fn = copy_fn;
    set->move_fn = move_fn;
    set->del_fn  = del_fn;

    return set;
}

void hashset_destroy(hashset* set)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!set->buckets, "set bucket is null");

    for (u64 i = 0; i < set->capacity; i++) {
        const ELM* elm = GET_ELM(set->buckets, i);
        if (elm->state == FILLED) {
            elm_destroy(set->del_fn, elm);
        }
    }

    free(set->buckets);
    free(set);
}

void hashset_clear(hashset* set)
{
    CHECK_FATAL(!set, "set is null");
    
    for (u64 i = 0; i < set->capacity; i++) {
        ELM* elm = GET_ELM(set->buckets, i);
        if (elm->state == FILLED) {
            elm_destroy(set->del_fn, elm);
            elm->elm = NULL;
            elm->state = EMPTY;
        } else if (elm->state == TOMBSTONE) {
            elm->state = EMPTY;
        }
    }
    
    set->size = 0;
}

void hashset_reset(hashset* set)
{
    CHECK_FATAL(!set, "set is null");
    
    // Clear all elements
    hashset_clear(set);
    
    // Reset to initial capacity
    if (set->capacity > HASHMAP_INIT_CAPACITY) {
        free(set->buckets);
        set->buckets = malloc(HASHMAP_INIT_CAPACITY * sizeof(ELM));
        CHECK_FATAL(!set->buckets, "reset malloc failed");
        reset_buckets(set->buckets, HASHMAP_INIT_CAPACITY);
        set->capacity = HASHMAP_INIT_CAPACITY;
    }
}

// COPY semantics
b8 hashset_insert(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!elm, "elm is null");
    
    hashset_maybe_resize(set);

    b8 found = 0;
    int tombstone = -1;
    u64 slot = find_slot(set, elm, &found, &tombstone);

    if (found) {
        return 1; // already exists
    }
    
    // New element - insert
    u8* new_elm = malloc(set->elm_size);
    CHECK_FATAL(!new_elm, "elm malloc failed");

    if (set->copy_fn) {
        set->copy_fn(new_elm, elm);
    } else {
        memcpy(new_elm, elm, set->elm_size);
    }
    
    ELM* elem = GET_ELM(set->buckets, slot);
    elem->elm = new_elm;
    elem->state = FILLED;
    
    set->size++;

    return 0; // inserted
}

// MOVE semantics
b8 hashset_insert_move(hashset* set, u8** elm)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!elm, "elm is null");
    CHECK_FATAL(!*elm, "*elm is null");
    
    hashset_maybe_resize(set);

    b8 found = 0;
    int tombstone = -1;
    u64 slot = find_slot(set, *elm, &found, &tombstone);

    if (found) {
        // Element already exists - clean up the passed element
        if (set->del_fn) {
            set->del_fn(*elm);
        }
        free(*elm);
        *elm = NULL;
        return 1; // already exists
    }

    // New element - insert
    u8* new_elm = malloc(set->elm_size);
    CHECK_FATAL(!new_elm, "elm malloc failed");

    if (set->move_fn) {
        set->move_fn(new_elm, elm);
    } else {
        memcpy(new_elm, *elm, set->elm_size);
        *elm = NULL;
    }
    
    ELM* elem = GET_ELM(set->buckets, slot);
    elem->elm = new_elm;
    elem->state = FILLED;
    
    set->size++;

    return 0; // inserted
}

b8 hashset_remove(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!elm, "elm is null");

    if (set->size == 0) {
        return 0;
    }

    b8 found = 0;
    int tombstone = -1;
    u64 slot = find_slot(set, elm, &found, &tombstone);

    if (found) {
        ELM* elem = GET_ELM(set->buckets, slot);
        elm_destroy(set->del_fn, elem);

        elem->elm = NULL;
        elem->state = TOMBSTONE;
        
        set->size--;

        hashset_maybe_resize(set);
        return 1; // found
    }

    return 0; // not found
}

b8 hashset_has(const hashset* set, const u8* elm)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!elm, "elm is null");
    
    b8 found = 0;
    int tombstone = -1;
    find_slot(set, elm, &found, &tombstone);
    
    return found;
}

void hashset_print(const hashset* set, print_fn print_fn)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!print_fn, "print_fn is null");

    printf("\t=========\n");
    printf("\tSize: %lu / Capacity: %lu\n", set->size, set->capacity);
    printf("\t=========\n");

    for (u64 i = 0; i < set->capacity; i++) {
        const ELM* elm = GET_ELM(set->buckets, i);
        if (elm->state == FILLED) {
            printf("\t   ");
            print_fn(elm->elm);
            printf("\n");
        }
    }

    printf("\t=========\n");
}

#endif /* WC_HASHSET_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_HASHSET_SINGLE_H */
