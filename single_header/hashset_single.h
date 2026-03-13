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
 * WCtoolkit
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


// TODO: warm paths ?

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
        if (__builtin_expect(!!(cond), 0)) {                 \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                    \
    } while (0)

#define CHECK_WARN_RET(cond, ret, fmt, ...)                  \
    do {                                                     \
        if (__builtin_expect(!!(cond), 0)) {                 \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                                      \
        }                                                    \
    } while (0)

#define CHECK_FATAL(cond, fmt, ...)                           \
    do {                                                      \
        if (__builtin_expect(!!(cond), 0)) {                  \
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
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint8_t  b8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// #define false ((b8)0)
// #define true  ((b8)1)


// GENERIC FUNCTIONS
typedef void (*copy_fn)(u8* dest, const u8* src);
typedef void (*move_fn)(u8* dest, u8** src);
typedef void (*delete_fn)(u8* key);
typedef void (*print_fn)(const u8* elm);
typedef int (*compare_fn)(const u8* a, const u8* b, u64 size);


// Vtable: one instance shared across all vectors of the same type.
// Pass NULL for any callback not needed.
// For POD types, pass NULL for the whole ops pointer.
typedef struct {
    copy_fn   copy_fn; // Deep copy function for owned resources (or NULL)
    move_fn   move_fn; // Transfer ownership and null original (or NULL)
    delete_fn del_fn;  // Cleanup function for owned resources (or NULL)
} container_ops;


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
    if (ptr == NULL || size == 0 || bytes_per_line == 0) {
        return;
    }

    // hex rep 0-15
    const char* hex = "0123456789ABCDEF";

    for (u64 i = 0; i < size; i++) {
        u8 val1 = ptr[i] >> 4;   // get upper 4 bits as num b/w 0-15
        u8 val2 = ptr[i] & 0x0F; // get lower 4 bits as num b/w 0-15

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

/* ===== wc_errno.h ===== */
#ifndef WC_WC_ERRNO_H
#define WC_WC_ERRNO_H

#include <stdio.h>


/* wc_errno.h — Error reporting for WCtoolkit
 * ============================================
 *
 * Two tiers:
 *
 *   CHECK_FATAL  Programmer errors: null pointer, out of bounds, OOM.
 *                Crashes with a message. These are bugs, not conditions.
 *
 *   wc_errno     Expected conditions: pop on empty, arena full.
 *                Function returns NULL / 0 / void. wc_errno says why.
 *                Ignore it if you don't care. Check it if you do.
 *
 *
 * USAGE
 * -----
 *   // Check a single call:
 *   wc_errno = WC_OK;
 *   u8* p = arena_alloc(arena, size);
 *   if (!p && wc_errno == WC_ERR_FULL) { ... }
 *
 *   // Check a batch — wc_errno stays set if any call failed:
 *   wc_errno = WC_OK;
 *   float* a = (float*)arena_alloc(arena, 256);
 *   float* b = (float*)arena_alloc(arena, 256);
 *   if (wc_errno) { wc_perror("alloc"); }
 *
 *
 * RULES
 * -----
 *   1. Successful calls do NOT clear wc_errno — clear it yourself.
 *   2. Check the return value first. wc_errno tells you WHY, not WHETHER.
 *   3. wc_errno is thread-local. Each thread has its own copy.
 *
 *
 * WHAT SETS wc_errno
 * ------------------
 *   arena_alloc, arena_alloc_aligned      WC_ERR_FULL    arena exhausted
 *   genVec_pop, genVec_front, genVec_back WC_ERR_EMPTY   vec is empty
 *   dequeue, queue_peek, queue_peek_ptr   WC_ERR_EMPTY   queue is empty
 *   stack_pop, stack_peek                 WC_ERR_EMPTY   stack is empty
 */


typedef enum {
    WC_OK        = 0,
    WC_ERR_FULL,       // arena exhausted / container at capacity
    WC_ERR_EMPTY,      // pop or peek on empty container
} wc_err;

static inline const char* wc_strerror(wc_err e)
{
    switch (e) {
        case WC_OK:        return "ok";
        case WC_ERR_FULL:  return "full";
        case WC_ERR_EMPTY: return "empty";
        default:           return "unknown";
    }
}

/* Defined in wc_errno.c:
 *   _Thread_local wc_err wc_errno = WC_OK;
 */
extern _Thread_local wc_err wc_errno;

/* Print last error — same pattern as perror(3).
 *   wc_perror("arena_alloc");  ->  "arena_alloc: full"
 */
static inline void wc_perror(const char* prefix)
{
    if (prefix && prefix[0]) {
        fprintf(stderr, "%s: %s\n", prefix, wc_strerror(wc_errno));
    } else {
        fprintf(stderr, "%s\n", wc_strerror(wc_errno));
    }
}


/* Internal macros (library use only)
 * ------------------------------------
 * WC_SET_RET — replaces CHECK_WARN_RET at expected-condition sites.
 * Sets wc_errno silently and returns. No print.
 *
 *   WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, );     void return
 *   WC_SET_RET(WC_ERR_FULL,  cond,           NULL); pointer return
 */
#define WC_SET_RET(err_code, cond, ret) \
    do {                                \
        if (cond) {                     \
            wc_errno = (err_code);      \
            return ret;                 \
        }                               \
    } while (0)

/* WC_PROPAGATE_RET — exit immediately if a callee already set wc_errno.
 *
 *   some_internal_fn(vec);
 *   WC_PROPAGATE_RET( );   // exits if some_internal_fn set wc_errno
 */
#define WC_PROPAGATE_RET(ret)    \
    do {                         \
        if (wc_errno != WC_OK) { \
            return ret;          \
        }                        \
    } while (0)

#endif /* WC_WC_ERRNO_H */

/* ===== String.h ===== */
#ifndef WC_STRING_H
#define WC_STRING_H

#define STR_SSO_SIZE 24

#ifndef STRING_GROWTH
    #define STRING_GROWTH    1.5F    // capacity multiplier on grow
#endif
#ifndef STRING_SHRINK_AT
    #define STRING_SHRINK_AT 0.25F   // shrink when size/cap falls below this
#endif
#ifndef STRING_SHRINK_BY
    #define STRING_SHRINK_BY 0.5F    // multiply capacity by this on shrink
#endif


typedef struct {
    union {
        char* heap;
        char  stk[STR_SSO_SIZE];
    };
    // b8  sso;     // HACK: if cap is = STR_SSO_SIZE then we are in sso mode, if greater then heap mode
    u64 size;
    u64 capacity;
} String;

// 24 8 8 = 40 bytes


//  Construction / Destruction 

// Create an empty string on the heap.
String* string_create(void);

// Create a string on the heap from a cstr.
String* string_from_cstr(const char* cstr);

// Create a copy of another heap-allocated string.
String* string_from_string(const String* other);

// Initialise a String whose struct lives on the stack (data may be on heap).
void string_create_stk(String* str, const char* cstr);

// Destroy a heap-allocated String (frees struct + data).
void string_destroy(String* str);

// Destroy only the internal data of a stack-allocated String.
void string_destroy_stk(String* str);

// Move: transfer ownership from *src to dest, nulling *src.
// *src must be heap-allocated.
void string_move(String* dest, String** src);

// Deep copy src into dest (dest is re-initialised).
void string_copy(String* dest, const String* src);


//  Capacity 

// Ensure capacity >= new_cap (never shrinks).
void string_reserve(String* str, u64 new_cap);

// Reserve capacity and fill new slots with c.
void string_reserve_char(String* str, u64 new_cap, char c);

// Shrink allocation to exactly fit current size.
void string_shrink_to_fit(String* str);


//  Conversion 

// Return a malloc'd NUL-terminated copy — caller must free().
char* string_to_cstr(const String* str);

// Return a raw pointer into the internal buffer (no NUL terminator).
char* string_data_ptr(const String* str);


//  Modification 

void string_append_char(String* str, char c);
void string_append_cstr(String* str, const char* cstr);
void string_append_string(String* str, const String* other);
// Append other then destroy it (nulls *other).
void string_append_string_move(String* str, String** other);

char string_pop_char(String* str);

void string_insert_char(String* str, u64 i, char c);
void string_insert_cstr(String* str, u64 i, const char* cstr);
void string_insert_string(String* str, u64 i, const String* other);

void string_remove_char(String* str, u64 i);
// Remove chars in range [l, r] inclusive.
void string_remove_range(String* str, u64 l, u64 r);

// Remove all chars (keep allocation).
void string_clear(String* str);


//  Access 

char string_char_at(const String* str, u64 i);
void string_set_char(String* str, u64 i, char c);


//  Comparison 

// 0 == equal, <0 == str1 < str2, >0 == str1 > str2
int string_compare(const String* s1, const String* s2);
b8  string_equals(const String* s1, const String* s2);
b8  string_equals_cstr(const String* str, const char* cstr);


//  Search 

// Returns index, or (u64)-1 if not found.
u64 string_find_char(const String* str, char c);
u64 string_find_cstr(const String* str, const char* substr);

// Return a heap-allocated substring starting at `start` of `length` chars.
String* string_substr(const String* str, u64 start, u64 length);


//  I/O 

void string_print(const String* str);


//  Inline helpers 

static inline u64 string_len(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->size;
}

static inline u64 string_capacity(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->capacity;
}

static inline b8 string_empty(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->size == 0;
}


/*
 Macro to temporarily NUL-terminate a String for read-only C APIs.
 Do NOT break/return/goto inside the block.

 Usage:
   TEMP_CSTR_READ(s) {
       printf("%s\n", string_data_ptr(s));
   }
*/
#define TEMP_CSTR_READ(str) \
    for (u8 _once = 0; \
         (_once == 0) && (string_append_char((str), '\0'), 1); \
         _once++, string_pop_char((str)))

#endif /* WC_STRING_H */

/* ===== map_setup.h ===== */
#ifndef WC_MAP_SETUP_H
#define WC_MAP_SETUP_H

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

// custom hash for String*

static u64 murmurhash3_str_ptr(const u8* key, u64 size) 
{
    (void)size;
    
    String* str= *(String**)key;
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
    u8*   elm;
    STATE state;
} ELM;

typedef struct {
    ELM*           buckets;
    u64            size;
    u64            capacity;
    u32            elm_size;
    custom_hash_fn hash_fn;
    compare_fn     cmp_fn;

    // Shared ops vtable for elements.
    // Pass NULL for POD types (int, float, flat structs).
    const container_ops* ops;
} hashset;


// Create a new hashset.
// hash_fn and cmp_fn default to fnv1a_hash / default_compare if NULL.
// ops: pass NULL for POD types.
hashset* hashset_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* ops);

void hashset_destroy(hashset* set);

// Insert element — COPY semantics.
// Returns 1 if already existed (no-op), 0 if newly inserted.
b8 hashset_insert(hashset* set, const u8* elm);

// Insert element — MOVE semantics (elm is nulled on insert, or freed if duplicate).
// Returns 1 if already existed (elm freed), 0 if newly inserted.
b8 hashset_insert_move(hashset* set, u8** elm);

// Returns 1 if found, 0 if not.
b8 hashset_has(const hashset* set, const u8* elm);

// Returns 1 if found and removed, 0 if not found.
b8 hashset_remove(hashset* set, const u8* elm);

// Get raw bucket pointer at index i.
ELM* hashset_get_bucket(hashset* set, u64 i);

// Remove all elements, keep capacity.
void hashset_clear(hashset* set);

// Remove all elements and reset to initial capacity.
void hashset_reset(hashset* set);

void hashset_print(const hashset* set, print_fn print_fn);

// Deep copy src into dest (dest must be uninitialised or already destroyed).
void hashset_copy(hashset* dest, const hashset* src);

static inline u64 hashset_size(const hashset* set)     { CHECK_FATAL(!set, "set is null"); return set->size;      }
static inline u64 hashset_capacity(const hashset* set) { CHECK_FATAL(!set, "set is null"); return set->capacity;  }
static inline b8  hashset_empty(const hashset* set)    { CHECK_FATAL(!set, "set is null"); return set->size == 0; }

#endif /* WC_HASHSET_H */

#ifdef WC_IMPLEMENTATION

/* ===== wc_errno.c ===== */
#ifndef WC_WC_ERRNO_IMPL
#define WC_WC_ERRNO_IMPL

/* One definition of the thread-local error variable.
 * Every translation unit that includes wc_error.h sees the extern declaration.
 * This file provides the actual storage.
 */
_Thread_local wc_err wc_errno = WC_OK;

#endif /* WC_WC_ERRNO_IMPL */

/* ===== String.c ===== */
#ifndef WC_STRING_IMPL
#define WC_STRING_IMPL

#include <string.h>


//  Internal macros

#define IS_SSO(s)        ((s)->capacity == STR_SSO_SIZE)
#define GET_STR(s)       (IS_SSO(s) ? (s)->stk : (s)->heap)
#define GET_STR_AT(s, i) (GET_STR(s)[i])
#define STR_REMAINING(s) ((s)->capacity - (s)->size)

// Grow if full.
#define MAYBE_GROW(s)                     \
    do {                                  \
        if ((s)->size >= (s)->capacity) { \
            if (IS_SSO(s)) {              \
                stk_to_heap(s);           \
            } else {                      \
                string_grow(s);           \
            }                             \
        }                                 \
    } while (0)

// Shrink heap string if very sparse.
#define MAYBE_SHRINK(s)                                                                  \
    do {                                                                                 \
        if (!IS_SSO(s) && (s)->size <= (u64)((float)(s)->capacity * STRING_SHRINK_AT)) { \
            string_shrink(s);                                                            \
        }                                                                                \
    } while (0)


//  Private helpers

static u64  cstr_len(const char* cstr);
static void str_copy_n(char* dest, const char* src, u64 n);
static void stk_to_heap(String* s);
static void heap_to_stk(String* s);
static void string_grow(String* s);
static void string_shrink(String* s);
// Ensure capacity >= needed (handles SSO → heap transition).
static void ensure_capacity(String* s, u64 needed);



//  Construction / Destruction

String* string_create(void)
{
    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    s->size     = 0;
    s->capacity = STR_SSO_SIZE;

    return s;
}

String* string_from_cstr(const char* cstr)
{
    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    string_create_stk(s, cstr);
    return s;
}

String* string_from_string(const String* other)
{
    CHECK_FATAL(!other, "other is null");

    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    s->size     = 0;
    s->capacity = STR_SSO_SIZE;

    if (other->size > 0) {
        ensure_capacity(s, other->size);
        str_copy_n(GET_STR(s), GET_STR(other), other->size);
        s->size = other->size;
    }

    return s;
}

void string_create_stk(String* s, const char* cstr)
{
    CHECK_FATAL(!s, "str is null");

    s->size     = 0;
    s->capacity = STR_SSO_SIZE;

    if (!cstr) {
        return;
    }

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, len);
    str_copy_n(GET_STR(s), cstr, len);
    s->size = len;
}

void string_destroy(String* s)
{
    CHECK_FATAL(!s, "str is null");
    string_destroy_stk(s);
    free(s);
}

void string_destroy_stk(String* s)
{
    CHECK_FATAL(!s, "str is null");

    if (!IS_SSO(s)) {
        free(s->heap);
        s->heap = NULL;
    }

    s->size     = 0;
    s->capacity = STR_SSO_SIZE; // leave in valid SSO state, not capacity=0
}

void string_move(String* dest, String** src)
{
    CHECK_FATAL(!src, "src ptr is null");
    CHECK_FATAL(!*src, "*src is null");
    CHECK_FATAL(!dest, "dest is null");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    string_destroy_stk(dest);
    memcpy(dest, *src, sizeof(String));

    // Zero out src so its destructor is harmless, then free the struct
    (*src)->size     = 0;
    (*src)->capacity = STR_SSO_SIZE;
    free(*src);
    *src = NULL;
}

void string_copy(String* dest, const String* src)
{
    CHECK_FATAL(!src, "src is null");
    CHECK_FATAL(!dest, "dest is null");

    if (src == dest) {
        return;
    }

    string_destroy_stk(dest);

    dest->size     = 0;
    dest->capacity = STR_SSO_SIZE;

    if (src->size > 0) {
        ensure_capacity(dest, src->size);
        str_copy_n(GET_STR(dest), GET_STR(src), src->size);
        dest->size = src->size;
    }
}


//  Capacity

void string_reserve(String* s, u64 new_cap)
{
    CHECK_FATAL(!s, "str is null");

    if (new_cap <= s->capacity) {
        return;
    }
    ensure_capacity(s, new_cap);
}

void string_reserve_char(String* s, u64 new_cap, char c)
{
    CHECK_FATAL(!s, "str is null");
    if (new_cap <= s->capacity) {
        // Fill from current size up to new_cap within existing allocation.
        char* buf = GET_STR(s);
        for (u64 i = s->size; i < new_cap; i++) {
            buf[i] = c;
        }
        s->size = new_cap;
        return;
    }

    u64 old_size = s->size;
    ensure_capacity(s, new_cap);

    char* buf = GET_STR(s);
    for (u64 i = old_size; i < new_cap; i++) {
        buf[i] = c;
    }
    s->size = new_cap;
}

void string_shrink_to_fit(String* s)
{
    CHECK_FATAL(!s, "str is null");

    if (IS_SSO(s)) {
        return;
    } // already optimal

    if (s->size == 0) {
        free(s->heap);
        s->heap     = NULL;
        s->capacity = STR_SSO_SIZE;
        return;
    }

    if (s->size <= STR_SSO_SIZE) {
        // Bring back to SSO.
        heap_to_stk(s);
        return;
    }

    char* new_data = realloc(s->heap, s->size);
    if (!new_data) {
        WARN("shrink_to_fit realloc failed — keeping current allocation");
        return;
    }
    s->heap     = new_data;
    s->capacity = s->size;
}


//  Conversion

char* string_to_cstr(const String* s)
{
    CHECK_FATAL(!s, "str is null");

    char* out = malloc(s->size + 1);
    CHECK_FATAL(!out, "malloc failed");

    if (s->size > 0) {
        str_copy_n(out, GET_STR(s), s->size);
    }
    out[s->size] = '\0';

    return out;
}

char* string_data_ptr(const String* s)
{
    CHECK_FATAL(!s, "str is null");
    if (s->size == 0) {
        return NULL;
    }
    // Cast away const intentionally: caller may mutate via this pointer.
    return (char*)(IS_SSO(s) ? s->stk : s->heap);
}


//  Modification

void string_append_char(String* s, char c)
{
    CHECK_FATAL(!s, "str is null");
    MAYBE_GROW(s);
    GET_STR_AT(s, s->size++) = c;
}

// TODO: this always sets size = cap if size was not enough, no extra
void string_append_cstr(String* s, const char* cstr)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, s->size + len);
    str_copy_n(GET_STR(s) + s->size, cstr, len);
    s->size += len;
}

void string_append_string(String* s, const String* other)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!other, "other is null");

    if (other->size == 0) {
        return;
    }

    ensure_capacity(s, s->size + other->size);
    str_copy_n(GET_STR(s) + s->size, GET_STR(other), other->size);
    s->size += other->size;
}

void string_append_string_move(String* s, String** other)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!other, "other ptr is null");
    CHECK_FATAL(!*other, "*other is null");

    if ((*other)->size > 0) {
        string_append_string(s, *other);
    }

    string_destroy(*other);
    *other = NULL;
}

char string_pop_char(String* s)
{
    CHECK_FATAL(!s, "str is null");
    WC_SET_RET(WC_ERR_EMPTY, s->size == 0, '\0');

    char c = GET_STR_AT(s, --s->size);
    MAYBE_SHRINK(s);
    return c;
}

void string_insert_char(String* s, u64 i, char c)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(i > s->size, "index out of bounds");

    MAYBE_GROW(s);

    char* buf = GET_STR(s);
    // Shift right.
    for (u64 j = s->size; j > i; j--) {
        buf[j] = buf[j - 1];
    }
    buf[i] = c;
    s->size++;
}

void string_insert_cstr(String* s, u64 i, const char* cstr)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");
    CHECK_FATAL(i > s->size, "index out of bounds");

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, s->size + len);

    char* buf = GET_STR(s);
    // Shift existing chars right by len positions.
    for (u64 j = s->size; j > i; j--) {
        buf[j + len - 1] = buf[j - 1];
    }
    str_copy_n(buf + i, cstr, len);
    s->size += len;
}

void string_insert_string(String* s, u64 i, const String* other)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!other, "other is null");
    CHECK_FATAL(i > s->size, "index out of bounds");

    if (other->size == 0) {
        return;
    }

    // If src == dest we need a snapshot to avoid aliasing after realloc.
    if (s == other) {
        char* snap = malloc(other->size);
        CHECK_FATAL(!snap, "malloc failed");
        str_copy_n(snap, GET_STR(other), other->size);

        ensure_capacity(s, s->size + other->size);
        char* buf = GET_STR(s);
        for (u64 j = s->size; j > i; j--) {
            buf[j + other->size - 1] = buf[j - 1];
        }
        str_copy_n(buf + i, snap, other->size);
        s->size += other->size;
        free(snap);
        return;
    }

    u64 len = other->size;
    ensure_capacity(s, s->size + len);

    char* buf = GET_STR(s);
    for (u64 j = s->size; j > i; j--) {
        buf[j + len - 1] = buf[j - 1];
    }
    str_copy_n(buf + i, GET_STR(other), len);
    s->size += len;
}

void string_remove_char(String* s, u64 i)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(i >= s->size, "index out of bounds");

    char* buf = GET_STR(s);
    for (u64 j = i; j < s->size - 1; j++) {
        buf[j] = buf[j + 1];
    }
    s->size--;
    MAYBE_SHRINK(s);
}

void string_remove_range(String* s, u64 l, u64 r)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(l >= s->size, "l out of bounds");
    CHECK_FATAL(l > r, "invalid range: l > r");

    if (r >= s->size) {
        r = s->size - 1;
    }

    u64   count = r - l + 1;
    char* buf   = GET_STR(s);

    for (u64 j = l; j + count < s->size; j++) {
        buf[j] = buf[j + count];
    }
    s->size -= count;
    MAYBE_SHRINK(s);
}

void string_clear(String* s)
{
    CHECK_FATAL(!s, "str is null");
    s->size = 0;
}


//  Access

char string_char_at(const String* s, u64 i)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(i >= s->size, "index out of bounds");
    return GET_STR_AT(s, i);
}

void string_set_char(String* s, u64 i, char c)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(i >= s->size, "index out of bounds");
    GET_STR_AT(s, i) = c;
}


//  Comparison

int string_compare(const String* s1, const String* s2)
{
    CHECK_FATAL(!s1, "str1 is null");
    CHECK_FATAL(!s2, "str2 is null");

    u64 min_len = s1->size < s2->size ? s1->size : s2->size;

    if (min_len > 0) {
        int cmp = memcmp(GET_STR(s1), GET_STR(s2), min_len);
        if (cmp != 0) {
            return cmp;
        }
    }

    if (s1->size < s2->size) {
        return -1;
    }
    if (s1->size > s2->size) {
        return 1;
    }
    return 0;
}

b8 string_equals(const String* s1, const String* s2)
{
    return string_compare(s1, s2) == 0;
}

b8 string_equals_cstr(const String* s, const char* cstr)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

    u64 len = cstr_len(cstr);

    if (s->size != len) {
        return false;
    }
    if (len == 0) {
        return true;
    }

    return memcmp(GET_STR(s), cstr, len) == 0;
}


//  Search

u64 string_find_char(const String* s, char c)
{
    CHECK_FATAL(!s, "str is null");

    const char* buf = GET_STR(s);
    for (u64 i = 0; i < s->size; i++) {
        if (buf[i] == c) {
            return i;
        }
    }
    return (u64)-1;
}

u64 string_find_cstr(const String* s, const char* substr)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!substr, "substr is null");

    u64 len = cstr_len(substr);
    if (len == 0) {
        return 0;
    }
    if (len > s->size) {
        return (u64)-1;
    }

    const char* buf = GET_STR(s);
    for (u64 i = 0; i <= s->size - len; i++) {
        if (memcmp(buf + i, substr, len) == 0) {
            return i;
        }
    }
    return (u64)-1;
}

String* string_substr(const String* s, u64 start, u64 length)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(start >= s->size, "start out of bounds");

    u64 end = start + length;
    if (end > s->size) {
        end = s->size;
    }

    u64 actual_len = end - start;

    String* result = string_create();
    if (actual_len > 0) {
        ensure_capacity(result, actual_len);
        str_copy_n(GET_STR(result), GET_STR(s) + start, actual_len);
        result->size = actual_len;
    }

    return result;
}


//  I/O

void string_print(const String* s)
{
    CHECK_FATAL(!s, "str is null");

    putchar('"');
    const char* buf = GET_STR(s);
    for (u64 i = 0; i < s->size; i++) {
        putchar(buf[i]);
    }
    putchar('"');
}



static u64 cstr_len(const char* cstr)
{
    u64 i = 0;
    while (cstr[i] != '\0') {
        i++;
    }
    return i;
}

static void str_copy_n(char* dest, const char* src, u64 n)
{
    for (u64 i = 0; i < n; i++) {
        dest[i] = src[i];
    }
}

// Promote SSO buffer to heap allocation.
static void stk_to_heap(String* s)
{
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);

    char* new_data = malloc(new_cap);
    CHECK_FATAL(!new_data, "malloc failed");

    str_copy_n(new_data, s->stk, s->size);

    s->heap     = new_data;
    s->capacity = new_cap;
}

static void heap_to_stk(String* s)
{
    char tmp[STR_SSO_SIZE];
    str_copy_n(tmp, s->heap, s->size);
    free(s->heap);
    str_copy_n(s->stk, tmp, s->size);
    s->capacity = STR_SSO_SIZE;
}

static void string_grow(String* s)
{
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);

    char* new_data = realloc(s->heap, new_cap);
    CHECK_FATAL(!new_data, "realloc failed");

    s->heap     = new_data;
    s->capacity = new_cap;
}

// TODO: move from heap to stk if cap too low?
static void string_shrink(String* s)
{
    u64 new_cap = (u64)((float)s->capacity * STRING_SHRINK_BY);

    if (new_cap <= STR_SSO_SIZE) {
        heap_to_stk(s);
        return;
    }

    char* new_data = realloc(s->heap, new_cap);
    if (!new_data) {
        WARN("shrink realloc failed — keeping current allocation");
        return;
    }

    s->heap     = new_data;
    s->capacity = new_cap;
}

// Grow (possibly multiple times) until capacity >= needed.
static void ensure_capacity(String* s, u64 needed)
{
    if (needed <= s->capacity) {
        return;
    }

    // Grow by at least STRING_GROWTH factor so we don't alloc on every push.
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);
    if (new_cap < needed) {
        new_cap = needed;
    }

    if (IS_SSO(s)) {
        char* new_data = malloc(new_cap);
        CHECK_FATAL(!new_data, "malloc failed");
        str_copy_n(new_data, s->stk, s->size);
        s->heap     = new_data;
        s->capacity = new_cap;
    } else {
        char* new_data = realloc(s->heap, new_cap);
        CHECK_FATAL(!new_data, "realloc failed");
        s->heap     = new_data;
        s->capacity = new_cap;
    }
}

#endif /* WC_STRING_IMPL */

/* ===== hashset.c ===== */
#ifndef WC_HASHSET_IMPL
#define WC_HASHSET_IMPL

#include <string.h>




#define GET_ELM(set, i) ((set)->buckets + (i))


/*
====================PRIVATE FUNCTIONS====================
*/

static void elm_destroy(const container_ops* ops, const ELM* elm);
static void hashset_memset_buckets(ELM* buckets, u64 size);

static u64 hashset_find_slot(const hashset* set, const u8* element, b8* found, int* tombstone);

static void hashset_resize(hashset* set, u64 new_capacity);
static void hashset_maybe_resize(hashset* set);


/*
====================PUBLIC FUNCTIONS====================
*/

hashset* hashset_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* ops)
{
    CHECK_FATAL(elm_size == 0, "elm size can't be 0");

    hashset* set = malloc(sizeof(hashset));
    CHECK_FATAL(!set, "set malloc failed");

    set->buckets = malloc(HASHMAP_INIT_CAPACITY * sizeof(ELM));
    CHECK_FATAL(!set->buckets, "set bucket init failed");

    hashset_memset_buckets(set->buckets, HASHMAP_INIT_CAPACITY);

    set->capacity = HASHMAP_INIT_CAPACITY;
    set->size     = 0;
    set->elm_size = elm_size;

    set->hash_fn = hash_fn ? hash_fn : fnv1a_hash;
    set->cmp_fn  = cmp_fn  ? cmp_fn  : default_compare;

    set->ops = ops;

    return set;
}

void hashset_destroy(hashset* set)
{
    CHECK_FATAL(!set,          "set is null");
    CHECK_FATAL(!set->buckets, "set buckets is null");

    SET_FOREACH_BUCKET(set, elm) {
        elm_destroy(set->ops, elm);
    }

    free(set->buckets);
    free(set);
}

void hashset_clear(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    for (u64 i = 0; i < set->capacity; i++) {
        ELM* elm = GET_ELM(set, i);
        if (elm->state == FILLED) {
            elm_destroy(set->ops, elm);
        }
        elm->elm   = NULL;
        elm->state = EMPTY;
    }

    set->size = 0;
}

void hashset_reset(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    hashset_clear(set);

    if (set->capacity > HASHMAP_INIT_CAPACITY) {
        free(set->buckets);
        set->buckets = malloc(HASHMAP_INIT_CAPACITY * sizeof(ELM));
        CHECK_FATAL(!set->buckets, "reset malloc failed");
        hashset_memset_buckets(set->buckets, HASHMAP_INIT_CAPACITY);
        set->capacity = HASHMAP_INIT_CAPACITY;
    }
}

// COPY semantics
b8 hashset_insert(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!elm, "elm is null");

    hashset_maybe_resize(set);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = hashset_find_slot(set, elm, &found, &tombstone);

    if (found) {
        return 1; // already exists
    }

    u8* new_elm = malloc(set->elm_size);
    CHECK_FATAL(!new_elm, "elm malloc failed");

    if (set->ops && set->ops->copy_fn) {
        set->ops->copy_fn(new_elm, elm);
    } else {
        memcpy(new_elm, elm, set->elm_size);
    }

    ELM* elem   = GET_ELM(set, slot);
    elem->elm   = new_elm;
    elem->state = FILLED;

    set->size++;

    return 0;
}

// MOVE semantics
b8 hashset_insert_move(hashset* set, u8** elm)
{
    CHECK_FATAL(!set,  "set is null");
    CHECK_FATAL(!elm,  "elm is null");
    CHECK_FATAL(!*elm, "*elm is null");

    hashset_maybe_resize(set);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = hashset_find_slot(set, *elm, &found, &tombstone);

    if (found) {
        // Already exists — clean up the passed element
        if (set->ops && set->ops->del_fn) {
            set->ops->del_fn(*elm);
        }
        free(*elm);
        *elm = NULL;
        return 1;
    }

    u8* new_elm = malloc(set->elm_size);
    CHECK_FATAL(!new_elm, "elm malloc failed");

    if (set->ops && set->ops->move_fn) {
        set->ops->move_fn(new_elm, elm);
    } else {
        memcpy(new_elm, *elm, set->elm_size);
        *elm = NULL;
    }

    ELM* elem   = GET_ELM(set, slot);
    elem->elm   = new_elm;
    elem->state = FILLED;

    set->size++;

    return 0;
}

b8 hashset_remove(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!elm, "elm is null");

    if (set->size == 0) {
        return 0;
    }

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = hashset_find_slot(set, elm, &found, &tombstone);

    if (found) {
        ELM* elem = GET_ELM(set, slot);
        elm_destroy(set->ops, elem);

        elem->elm   = NULL;
        elem->state = TOMBSTONE;

        set->size--;

        hashset_maybe_resize(set);
        return 1;
    }

    return 0;
}

ELM* hashset_get_bucket(hashset* set, u64 i)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(i >= set->capacity, "index out of bounds");

    return (set->buckets + i);
}

b8 hashset_has(const hashset* set, const u8* elm)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!elm, "elm is null");

    b8  found     = 0;
    int tombstone = -1;
    hashset_find_slot(set, elm, &found, &tombstone);

    return found;
}

void hashset_print(const hashset* set, print_fn print_fn)
{
    CHECK_FATAL(!set,      "set is null");
    CHECK_FATAL(!print_fn, "print_fn is null");

    printf("\t=========\n");
    printf("\tSize: %lu / Capacity: %lu\n", set->size, set->capacity);
    printf("\t=========\n");

    SET_FOREACH_BUCKET(set, elm) {
        printf("\t   ");
        print_fn(elm->elm);
        printf("\n");
    }

    printf("\t=========\n");
}

// Deep copy src into dest (dest must be uninitialised or already destroyed).
void hashset_copy(hashset* dest, const hashset* src)
{
    CHECK_FATAL(!dest, "dest is null");
    CHECK_FATAL(!src,  "src is null");

    copy_fn e_copy = src->ops ? src->ops->copy_fn : NULL;

    // Clear dest KVs (runs del callbacks, resets to EMPTY), keeps the bucket array.
    hashset_clear(dest);

    // Copy all scalar fields and fn/ops pointers from src, but preserve dest->buckets.
    ELM* old_elm  = dest->buckets;
    u64 old_capacity = dest->capacity;
    memcpy(dest, src, sizeof(hashset));
    dest->buckets = old_elm;
    dest->size = 0;

    // If src is larger than dest's existing bucket array, grow it.
    if (src->capacity > old_capacity) {
        ELM* grown = realloc(dest->buckets, src->capacity * sizeof(ELM));
        CHECK_FATAL(!grown, "bucket realloc failed");
        hashset_memset_buckets(grown + old_capacity, src->capacity - old_capacity);
        dest->buckets = grown;
    }

    SET_FOREACH_BUCKET(src, kv) {
        b8  found     = 0;
        int tombstone = -1;
        u64 slot      = hashset_find_slot(dest, kv->elm, &found, &tombstone);

        u8* e = malloc(src->elm_size);
        CHECK_FATAL(!e, "elm malloc failed");

        if (e_copy) { e_copy(e, kv->elm); }
        else        { memcpy(e, kv->elm, src->elm_size); }

        ELM* new_elm   = GET_ELM(dest, slot);
        new_elm->elm   = e;
        new_elm->state = FILLED;

        dest->size++;
    }
}



/*
====================PRIVATE FUNCTION IMPLEMENTATIONS====================
*/


static void elm_destroy(const container_ops* ops, const ELM* elm)
{
    CHECK_FATAL(!elm, "ELM is null");

    if (elm->elm) {
        if (ops && ops->del_fn) {
            ops->del_fn(elm->elm);
        }
        free(elm->elm);
    }
}

// memset gives: elm = NULL, state = EMPTY (= 0)
static void hashset_memset_buckets(ELM* buckets, u64 size)
{
    memset(buckets, 0, sizeof(ELM) * size);
}

static u64 hashset_find_slot(const hashset* set, const u8* element, b8* found, int* tombstone)
{
    u64 index = set->hash_fn(element, set->elm_size) % set->capacity;

    *found     = 0;
    *tombstone = -1;

    for (u64 x = 0; x < set->capacity; x++) {
        u64        i   = (index + x) % set->capacity;
        const ELM* elm = GET_ELM(set, i);

        switch (elm->state) {
            case EMPTY:
                // Return tombstone slot if we passed one — reuse it
                return (*tombstone != -1) ? (u64)*tombstone : i;
            case FILLED:
                if (set->cmp_fn(elm->elm, element, set->elm_size) == 0) {
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
    if (new_capacity < HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    ELM* old_buckets = set->buckets;
    u64 old_cap     = set->capacity;

    set->buckets = malloc(new_capacity * sizeof(ELM));
    CHECK_FATAL(!set->buckets, "resize malloc failed");
    hashset_memset_buckets(set->buckets, new_capacity);

    set->capacity = new_capacity;
    set->size     = 0;

    // Rehash — pointers are moved as-is, no copy/del needed
    for (u64 i = 0; i < old_cap; i++) {
        const ELM* old_elm = old_buckets + i;

        if (old_elm->state == FILLED) {
            b8  found     = 0;
            int tombstone = -1;
            u64 slot      = hashset_find_slot(set, old_elm->elm, &found, &tombstone);

            ELM* new_elm   = GET_ELM(set, slot);
            new_elm->elm   = old_elm->elm;
            new_elm->state = FILLED;

            set->size++;
        }
    }

    free(old_buckets);
}

static void hashset_maybe_resize(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    double load = (double)set->size / (double)set->capacity;

    if (load > LOAD_FACTOR_GROW) {
        hashset_resize(set, next_prime(set->capacity));
    } else if (load < LOAD_FACTOR_SHRINK && set->capacity > HASHMAP_INIT_CAPACITY) {
        u64 new_cap = prev_prime(set->capacity);
        if (new_cap >= HASHMAP_INIT_CAPACITY) {
            hashset_resize(set, new_cap);
        }
    }
}

#endif /* WC_HASHSET_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_HASHSET_SINGLE_H */
