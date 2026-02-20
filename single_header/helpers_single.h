#ifndef WC_HELPERS_SINGLE_H
#define WC_HELPERS_SINGLE_H

/*
 * helpers_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "helpers_single.h"
 *
 * All other files just:
 *     #include "helpers_single.h"
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

/* ===== gen_vector.h ===== */
#ifndef WC_GEN_VECTOR_H
#define WC_GEN_VECTOR_H

/*          TLDR
 * genVec is a value-based generic vector.
 * Elements are stored inline and managed via user-supplied
 * copy/move/destructor callbacks.
 *
 * This avoids pointer ownership ambiguity and improves cache locality.
 */


// User-provided callback functions
// moved to common.h


// genVec growth/shrink settings (user can change)
#ifndef GENVEC_GROWTH
#define GENVEC_GROWTH 1.5F // vec capacity multiplier
#endif
#ifndef GENVEC_SHRINK_AT
#define GENVEC_SHRINK_AT 0.25F // % filled to shrink at (25% filled)
#endif
#ifndef GENVEC_SHRINK_BY
#define GENVEC_SHRINK_BY 0.5F // capacity dividor (half)
#endif


// generic vector container
typedef struct {
    u8* data; // pointer to generic data

    u64 size;      // Number of elements currently in vector
    u64 capacity;  // Total allocated capacity
    u32 data_size; // Size of each element in bytes

    // Function Pointers (Type based Memory Management)
    copy_fn   copy_fn; // Deep copy function for owned resources (or NULL)
    move_fn   move_fn; // Get a double pointer, transfer ownership and null original (or NULL)
    delete_fn del_fn;  // Cleanup function for owned resources (or NULL)
} genVec;

// 8 8 8 4  '4'  8 8 8  = 56



// Memory Management
// ===========================

// Initialize vector with capacity n. If elements own heap resources,
// provide copy_fn (deep copy) and del_fn (cleanup). Otherwise pass NULL.
genVec* genVec_init(u64 n, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn);

// Initialize vector on stack with data on heap
// SVO works best here as it is on the stack***
void genVec_init_stk(u64 n, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn, genVec* vec);

// Initialize vector of size n, all elements set to val
genVec* genVec_init_val(u64 n, const u8* val, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn);

void genVec_init_val_stk(u64 n, const u8* val, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn,
                         genVec* vec);

// vector COMPLETELY on stack (can't grow in size)
// you provide a stack inited array which becomes internal array of vector
// WARNING - This crashes when size = capacity and you try to push
void genVec_init_arr(u64 n, u8* arr, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn, genVec* vec);

// Destroy heap-allocated vector and clean up all elements
void genVec_destroy(genVec* vec);

// Destroy stack-allocated vector (cleans up data, but not vec itself)
void genVec_destroy_stk(genVec* vec);

// Remove all elements (calls del_fn on each), keep capacity
void genVec_clear(genVec* vec);

// Remove all elements and free memory, shrink capacity to 0
void genVec_reset(genVec* vec);

// Ensure vector has at least new_capacity space (never shrinks)
void genVec_reserve(genVec* vec, u64 new_capacity);

// Grow to new_capacity and fill new slots with val
void genVec_reserve_val(genVec* vec, u64 new_capacity, const u8* val);

// Shrink vector to it's size (reallocates)
void genVec_shrink_to_fit(genVec* vec);



// Operations
// ===========================

// Append element to end (makes deep copy if copy_fn provided)
void genVec_push(genVec* vec, const u8* data);

// Append element to end, transfer ownership (nulls original pointer)
void genVec_push_move(genVec* vec, u8** data);

// Remove element from end. If popped provided, copies element before deletion.
// Note: del_fn is called regardless to clean up owned resources.
void genVec_pop(genVec* vec, u8* popped);

// Copy element at index i into out buffer
void genVec_get(const genVec* vec, u64 i, u8* out);

// Get pointer to element at index i
// Note: Pointer invalidated by push/insert/remove operations
const u8* genVec_get_ptr(const genVec* vec, u64 i);

// Get MUTABLE pointer to element at index i
// Note: Pointer invalidated by push/insert/remove operations
u8* genVec_get_ptr_mut(const genVec* vec, u64 i);

// Replace element at index i with data (cleans up old element)
void genVec_replace(genVec* vec, u64 i, const u8* data);

// Replace element at index i, transfer ownership (cleans up old element)
void genVec_replace_move(genVec* vec, u64 i, u8** data);

// Insert element at index i, shifting elements right
void genVec_insert(genVec* vec, u64 i, const u8* data);

// Insert element at index i with ownership transfer, shifting elements right
void genVec_insert_move(genVec* vec, u64 i, u8** data);

// Insert num_data elements from data arr to vec. data should have same size data as vec
void genVec_insert_multi(genVec* vec, u64 i, const u8* data, u64 num_data);

// Insert (move) num_data  elements from data starting at index i. Transfers ownership
void genVec_insert_multi_move(genVec* vec, u64 i, u8** data, u64 num_data);

// Remove element at index i, optionally copy to out, shift elements left
void genVec_remove(genVec* vec, u64 i, u8* out);

// Remove elements in range [l, r] inclusive.
void genVec_remove_range(genVec* vec, u64 l, u64 r);

// Get pointer to first element
const u8* genVec_front(const genVec* vec);

// Get pointer to last element
const u8* genVec_back(const genVec* vec);


// Utility
// ===========================

// Print all elements using provided print function
void genVec_print(const genVec* vec, print_fn fn);

// Deep copy src vector into dest
// Note: cleans up dest (if already inited)
void genVec_copy(genVec* dest, const genVec* src);

// transfers ownership from src to dest
// Note: src must be heap-allocated
void genVec_move(genVec* dest, genVec** src);


// Get number of elements in vector
static inline u64 genVec_size(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    return vec->size;
}

// Get total capacity of vector
static inline u64 genVec_capacity(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    return vec->capacity;
}

// Check if vector is empty
static inline b8 genVec_empty(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    return vec->size == 0;
}

#endif /* WC_GEN_VECTOR_H */

/* ===== String.h ===== */
#ifndef WC_STRING_H
#define WC_STRING_H

// ===== STRING =====
// the string is just a genVec of char type (length based string - not cstr)
typedef genVec String;
// ==================


// Construction/Destruction

// create string on the heap
String* string_create(void);

// create string with struct on the stack and data on heap
void string_create_stk(String* str, const char* cstr);

// create string on heap from a cstr
String* string_from_cstr(const char* cstr);

// get copy of a string (heap allocated)
String* string_from_string(const String* other);

// reserve a capacity for a string (must be greater than current cap)
void string_reserve(String* str, u64 capacity);

// reserve a capacity with a char
void string_reserve_char(String* str, u64 capacity, char c);

// destroy the heap allocated string
void string_destroy(String* str);

// destroy only the data ptr of string struct (for stk created str)
void string_destroy_stk(String* str);

// move string contents (nulls source)
// Note: src must be heap allocated
void string_move(String* dest, String** src);

// make deep copy
void string_copy(String* dest, const String* src);

// get cstr as COPY ('\0' present)
// cstr is MALLOCED and must be freed by user
char* string_to_cstr(const String* str);
// get ptr to the cstr buffer
// Note: NO NULL TERMINATOR
char* string_data_ptr(const String* str);


// TODO:
// void string_to_cstr_buf(const String* str, char* buf, u32 buf_size);
// void string_to_cstr_buf_move(const String* str, char* buf, u32 buf_size);


// Modification


// append a cstr to the end of a string
void string_append_cstr(String* str, const char* cstr);

// append a string "other" to the end of a string "str"
void string_append_string(String* str, const String* other);

// Concatenate string "other" and destroy source "str"
void string_append_string_move(String* str, String** other);

// append a char to the end of a string
void string_append_char(String* str, char c);

// insert a char at index i of string
void string_insert_char(String* str, u64 i, char c);

// insert a cstr at index i
void string_insert_cstr(String* str, u64 i, const char* cstr);

// insert a string "str" at index i
void string_insert_string(String* str, u64 i, const String* other);

// remove char from end of a string
char string_pop_char(String* str);

// remove a char from index i of string
void string_remove_char(String* str, u64 i);

// remove elements from l to r (inclusive)
void string_remove_range(String* str, u64 l, u64 r);

// remove all chars (keep memory)
void string_clear(String* str);


// Access

// return char at index i
char string_char_at(const String* str, u64 i);

// set the value of char at index i
void string_set_char(String* str, u64 i, char c);

// Comparison


// compare (c-style) two strings
// 0 -> equal, 1 -> not equal
int string_compare(const String* str1, const String* str2);

// return true if string's data matches
b8 string_equals(const String* str1, const String* str2);

// return true if a string's data matches a cstr
b8 string_equals_cstr(const String* str, const char* cstr);

// Search


// return index of char c (UINT_MAX otherwise)
u64 string_find_char(const String* str, char c);

// return index of cstr "substr" (UINT_MAX otherwise)
u64 string_find_cstr(const String* str, const char* substr);

// Set a heap allocated string of a substring starting at index "start", upto length
String* string_substr(const String* str, u64 start, u64 length);

// TODO: pass a buffer version of substr??

// I/O

// print the content of str
void string_print(const String* str);

// Basic properties

// get the current length of the string
static inline u64 string_len(const String* str)
{
    CHECK_FATAL(!str, "str is null");

    return str->size;
}

// get the capacity of the genVec container of string
static inline u64 string_capacity(const String* str)
{
    return str->capacity;
}

// return true if str is empty
static inline b8 string_empty(const String* str)
{
    return string_len(str) == 0;
}

// TODO: test
/*
 macro to create a temporary cstr for read ops
 Note: Must not break or return in the block
 Usage:

TEMP_CSTR_READ(s)
{
    printf("%s\n", string_data_ptr(s));
}
*/
#define TEMP_CSTR_READ(str) \
    for (u8 _once = 0; (_once == 0) && (string_append_char((str), '\0'), 1); _once++, string_pop_char((str)))

#endif /* WC_STRING_H */

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

/* ===== hashmap.h ===== */
#ifndef WC_HASHMAP_H
#define WC_HASHMAP_H

// TODO: make ergonomic macros


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

/* ===== helpers.h ===== */
#ifndef WC_HELPERS_H
#define WC_HELPERS_H

/*
 * helpers.h — Generic container callbacks and typed macros for WCtoolkit
 * =======================================================================
 *
 * ALL functions are static inline to avoid multiple-definition linker
 * errors when this header is included in more than one translation unit.
 *
 * SECTIONS
 * --------
 *   1.  String by value  (sizeof(String) per slot)
 *   2.  String by pointer (sizeof(String*) per slot)
 *   3.  genVec by value  (sizeof(genVec) per slot)  — vec of vecs
 *   4.  genVec by pointer (sizeof(genVec*) per slot)
 *   5.  Typed convenience macros
 *
 * RULES FOR WRITING copy/move/del CALLBACKS
 * ------------------------------------------
 * These rules apply universally, whatever T you are storing.
 *
 * BY VALUE  (slot holds the full struct, sizeof(T) bytes)
 *   copy_fn(u8* dest, const u8* src)
 *     dest  — raw bytes of the slot (uninitialised / garbage — treat as blank)
 *     src   — raw bytes of the source element (T*)src is valid
 *     job   — deep-copy all owned resources into dest; DO NOT free dest first
 *
 *   move_fn(u8* dest, u8** src)
 *     dest  — raw bytes of the slot (uninitialised)
 *     *src  — heap pointer to the source T
 *     job   — memcpy the struct fields, free(*src), *src = NULL
 *             The data ptr moves; only the container struct is freed.
 *
 *   del_fn(u8* elm)
 *     elm   — raw bytes of the slot (T*)elm is valid
 *     job   — free owned resources (e.g. data buffer) but NOT elm itself
 *             (the slot memory is owned by the vector, not you)
 *             → call string_destroy_stk / genVec_destroy_stk etc.
 *
 * BY POINTER  (slot holds T*, sizeof(T*) = 8 bytes)
 *   copy_fn(u8* dest, const u8* src)
 *     *(T**)src  is the source pointer
 *     *(T**)dest must be set to a newly heap-allocated deep copy
 *
 *   move_fn(u8* dest, u8** src)
 *     *(T**)dest = *(T**)src;  *src = NULL;
 *     (transfer the pointer, null the source — no struct free needed)
 *
 *   del_fn(u8* elm)
 *     *(T**)elm  is the pointer stored in the slot
 *     job — fully destroy the heap object (free data + free struct)
 *           → call string_destroy / genVec_destroy etc.
 */

#include <string.h>


/* ══════════════════════════════════════════════════════════════════════════
 * 1.  STRING BY VALUE
 *     Slot stores the full String struct (sizeof(String) bytes).
 *     copy_fn deep-copies the data buffer.
 *     move_fn transfers the heap struct, nulls source.
 *     del_fn  frees only the data buffer (not the slot).
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void str_copy(u8* dest, const u8* src)
{
    const String* s = (const String*)src;
    String*       d = (String*)dest;

    memcpy(d, s, sizeof(String));       /* copy all fields (data ptr too)  */

    u64 n   = s->size * s->data_size;
    d->data = malloc(n);                /* independent data buffer          */
    memcpy(d->data, s->data, n);
}

static inline void str_move(u8* dest, u8** src)
{
    memcpy(dest, *src, sizeof(String)); /* copy all fields into slot        */
    free(*src);                         /* free heap container, not data    */
    *src = NULL;
}

static inline void str_del(u8* elm)
{
    string_destroy_stk((String*)elm);   /* free data buffer, NOT the slot   */
}

static inline void str_print(const u8* elm)
{
    string_print((const String*)elm);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 2.  STRING BY POINTER
 *     Slot stores String* (sizeof(String*) = 8 bytes).
 *     Each slot is a pointer to a heap-allocated String.
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void str_copy_ptr(u8* dest, const u8* src)
{
    const String* s = *(const String**)src;

    String* d = malloc(sizeof(String));
    memcpy(d, s, sizeof(String));

    u64 n   = s->size * s->data_size;
    d->data = malloc(n);
    memcpy(d->data, s->data, n);

    *(String**)dest = d;
}

static inline void str_move_ptr(u8* dest, u8** src)
{
    *(String**)dest = *(String**)src;   /* transfer pointer into slot       */
    *src            = NULL;
}

static inline void str_del_ptr(u8* elm)
{
    string_destroy(*(String**)elm);     /* free data buffer + struct        */
}

static inline void str_print_ptr(const u8* elm)
{
    string_print(*(const String**)elm);
}

static inline int str_cmp(const u8* a, const u8* b, u64 size)
{
    (void)size;
    return string_compare((const String*)a, (const String*)b);
}

static inline int str_cmp_ptr(const u8* a, const u8* b, u64 size)
{
    (void)size;
    return string_compare(*(const String**)a, *(const String**)b);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 3.  GENVEC BY VALUE  (vec of vecs)
 *     Slot stores the full genVec struct (sizeof(genVec) bytes).
 *
 *     WHY vec_move works the way it does:
 *       *src is a heap-allocated genVec*.
 *       We memcpy all fields (including the data ptr) into the slot.
 *       Then we free the heap container (*src) — NOT the data buffer.
 *       The data buffer now lives inside the slot's genVec.
 *       We null *src so the caller can't use the dangling pointer.
 *
 *     WHY vec_copy cannot call genVec_copy:
 *       genVec_copy first calls genVec_destroy_stk on dest, which would
 *       try to free garbage memory (dest is uninitialised). We do it
 *       manually: memcpy fields, then malloc + copy the data buffer.
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy(u8* dest, const u8* src)
{
    const genVec* s = (const genVec*)src;
    genVec*       d = (genVec*)dest;

    memcpy(d, s, sizeof(genVec));                           /* copy all fields  */
    d->data = malloc(s->capacity * (u64)s->data_size);      /* new data buffer  */

    if (s->copy_fn) {
        for (u64 i = 0; i < s->size; i++) {
            s->copy_fn(d->data + (i * d->data_size),
                       genVec_get_ptr(s, i));
        }
    } else {
        memcpy(d->data, s->data, s->capacity * (u64)s->data_size);
    }
}

static inline void vec_move(u8* dest, u8** src)
{
    genVec* s = *(genVec**)src;
    genVec* d = (genVec*)dest;

    memcpy(d, s, sizeof(genVec));   /* transfer all fields (incl. data ptr) */
    free(*src);                     /* free container struct only           */
    *src = NULL;
}

static inline void vec_del(u8* elm)
{
    genVec_destroy_stk((genVec*)elm); /* free data buffer, NOT the slot     */
}

static inline void vec_print_int(const u8* elm)
{
    const genVec* v = (const genVec*)elm;
    printf("[");
    for (u64 i = 0; i < v->size; i++) {
        printf("%d", *(int*)genVec_get_ptr(v, i));
        if (i + 1 < v->size) { printf(", "); }
    }
    printf("]");
}


/* ══════════════════════════════════════════════════════════════════════════
 * 4.  GENVEC BY POINTER  (slot holds genVec*)
 *     Slot stores genVec* (sizeof(genVec*) = 8 bytes).
 *     The pointed-to genVec is fully heap-allocated.
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy_ptr(u8* dest, const u8* src)
{
    const genVec* s = *(const genVec**)src;

    genVec* d = malloc(sizeof(genVec));
    memcpy(d, s, sizeof(genVec));
    d->data   = malloc(s->capacity * (u64)s->data_size);

    if (s->copy_fn) {
        for (u64 i = 0; i < s->size; i++) {
            s->copy_fn(d->data + (i * d->data_size),
                       genVec_get_ptr(s, i));
        }
    } else {
        memcpy(d->data, s->data, s->capacity * (u64)s->data_size);
    }

    *(genVec**)dest = d;
}

static inline void vec_move_ptr(u8* dest, u8** src)
{
    *(genVec**)dest = *(genVec**)src;  /* transfer pointer into slot        */
    *src            = NULL;
}

static inline void vec_del_ptr(u8* elm)
{
    genVec_destroy(*(genVec**)elm);    /* free data buffer + struct         */
}

static inline void vec_print_int_ptr(const u8* elm)
{
    vec_print_int((const u8*)*(const genVec**)elm);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 5.  TYPED CONVENIENCE MACROS
 *
 * These fill gaps in wc_macros.h for common patterns:
 *   VEC_OF_INT      — quick int vector creation
 *   VEC_OF_STR      — String-by-value vector
 *   VEC_OF_STR_PTR  — String-by-pointer vector
 *   VEC_OF_VEC      — genVec-by-value vector (vec of vecs)
 *   VEC_OF_VEC_PTR  — genVec-by-pointer vector
 *
 *   VEC_PUSH_VEC      — push a heap genVec* by move into a vec-of-vecs (by val)
 *   VEC_PUSH_VEC_PTR  — push a heap genVec* by move into a vec-of-vecs (by ptr)
 *
 *   MAP_PUT_INT_STR  — one-liner: int key, cstr value (moves String in)
 *   MAP_PUT_STR_STR  — one-liner: cstr key, cstr value (both moved in)
 * ══════════════════════════════════════════════════════════════════════════ */

#include "wc_macros.h"
/* ── Vector creation shorthands ─────────────────────────────────────────── */

#define VEC_OF_INT(cap) \
    genVec_init((cap), sizeof(int), NULL, NULL, NULL)

#define VEC_OF_STR(cap) \
    VEC_CX(String, (cap), str_copy, str_move, str_del)

#define VEC_OF_STR_PTR(cap) \
    VEC_CX(String*, (cap), str_copy_ptr, str_move_ptr, str_del_ptr)

#define VEC_OF_VEC(cap) \
    VEC_CX(genVec, (cap), vec_copy, vec_move, vec_del)

#define VEC_OF_VEC_PTR(cap) \
    VEC_CX(genVec*, (cap), vec_copy_ptr, vec_move_ptr, vec_del_ptr)


/* ── Push shorthands ────────────────────────────────────────────────────── */

/*
 * VEC_PUSH_VEC(outer, inner_ptr)
 * Push a heap-allocated genVec* by move into a by-value vec-of-vecs.
 * inner_ptr is nulled after the call.
 * The inner vec's struct is freed; its data buffer lives in the outer slot.
 */
#define VEC_PUSH_VEC(outer, inner_ptr) \
    VEC_PUSH_MOVE((outer), (inner_ptr))

/*
 * VEC_PUSH_VEC_PTR(outer, inner_ptr)
 * Push a heap-allocated genVec* by move into a by-pointer vec-of-vecs.
 * inner_ptr is nulled after the call.
 */
#define VEC_PUSH_VEC_PTR(outer, inner_ptr) \
    VEC_PUSH_MOVE((outer), (inner_ptr))


/* ── Hashmap shorthands ─────────────────────────────────────────────────── */

/*
 * MAP_PUT_INT_STR(map, int_key, cstr_literal)
 * Inserts key=int_key, val=string_from_cstr(cstr_literal) with move semantics.
 * Map must have been created with int key, String val, str_copy/str_move/str_del.
 */
#define MAP_PUT_INT_STR(map, k, cstr_val)                           \
    do {                                                            \
        String* _v = string_from_cstr(cstr_val);                   \
        hashmap_put_val_move((map), (u8*)&(int){(k)}, (u8**)&_v);  \
    } while (0)

/*
 * MAP_PUT_STR_STR(map, cstr_key, cstr_val)
 * Inserts key=string_from_cstr(cstr_key), val=string_from_cstr(cstr_val).
 * Both moved in. Map must use str_copy/str_move/str_del for both key and val.
 */
#define MAP_PUT_STR_STR(map, cstr_key, cstr_val)                    \
    do {                                                            \
        String* _k = string_from_cstr(cstr_key);                   \
        String* _v = string_from_cstr(cstr_val);                   \
        hashmap_put_move((map), (u8**)&_k, (u8**)&_v);             \
    } while (0)


#endif /* HELPERS_H */

#endif /* WC_HELPERS_H */

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

/* ===== gen_vector.c ===== */
#ifndef WC_GEN_VECTOR_IMPL
#define WC_GEN_VECTOR_IMPL

#include <string.h>



#define GENVEC_MIN_CAPACITY 4


// MACROS

// get ptr to elm at index i
#define GET_PTR(vec, i) \
    ((vec->data) + ((u64)(i) * ((vec)->data_size)))
// get total_size in bytes for i elements
#define GET_SCALED(vec, i) \
    ((u64)(i) * ((vec)->data_size))

#define MAYBE_GROW(vec)                                 \
    do {                                                \
        if (!vec->data || vec->size >= vec->capacity) { \
            genVec_grow(vec);                           \
        }                                               \
    } while (0)

#define MAYBE_SHRINK(vec)                                                  \
    do {                                                                   \
        if (vec->size <= (u64)((float)vec->capacity * GENVEC_SHRINK_AT)) { \
            genVec_shrink(vec);                                            \
        }                                                                  \
    } while (0)


//private functions

static void genVec_grow(genVec* vec);
static void genVec_shrink(genVec* vec);


// API Implementation

genVec* genVec_init(u64 n, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn)
{
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    genVec* vec = malloc(sizeof(genVec));
    CHECK_FATAL(!vec, "vec init failed");

    // Only allocate memory if n > 0, otherwise data can be NULL
    vec->data = (n > 0) ? malloc(data_size * n) : NULL;

    // Only check for allocation failure if we actually tried to allocate
    if (n > 0 && !vec->data) {
        free(vec);
        FATAL("data init failed");
    }

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->copy_fn   = copy_fn;
    vec->move_fn   = move_fn;
    vec->del_fn    = del_fn;

    return vec;
}


void genVec_init_stk(u64 n, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn, genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    // Only allocate memory if n > 0, otherwise data can be NULL
    vec->data = (n > 0) ? malloc(data_size * n) : NULL;
    CHECK_FATAL(n > 0 && !vec->data, "data init failed");

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->copy_fn   = copy_fn;
    vec->move_fn   = move_fn;
    vec->del_fn    = del_fn;
}

genVec* genVec_init_val(u64 n, const u8* val, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn)
{
    CHECK_FATAL(!val, "val can't be null");
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    genVec* vec = genVec_init(n, data_size, copy_fn, move_fn, del_fn);

    vec->size = n; //capacity set to n in upper func

    for (u64 i = 0; i < n; i++) {
        if (copy_fn) {
            copy_fn(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    }

    return vec;
}

void genVec_init_val_stk(u64 n, const u8* val, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn,
                         genVec* vec)
{
    CHECK_FATAL(!val, "val can't be null");
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    genVec_init_stk(n, data_size, copy_fn, move_fn, del_fn, vec);

    vec->size = n;

    for (u64 i = 0; i < n; i++) {
        if (copy_fn) {
            copy_fn(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    }
}

void genVec_init_arr(u64 n, u8* arr, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn, genVec* vec)
{
    CHECK_FATAL(!arr, "arr is null");
    CHECK_FATAL(!vec, "vec is null");

    CHECK_FATAL(n == 0, "size of arr can't be 0");
    CHECK_FATAL(data_size == 0, "data_size of arr can't be 0");

    vec->data = arr;

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;

    vec->copy_fn = copy_fn;
    vec->move_fn = move_fn;
    vec->del_fn  = del_fn;
}

void genVec_destroy(genVec* vec)
{
    genVec_destroy_stk(vec);

    free(vec);
}


void genVec_destroy_stk(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    if (!vec->data) {
        return;
    }

    if (vec->del_fn) {
        // Custom cleanup for each element
        for (u64 i = 0; i < vec->size; i++) {
            vec->del_fn(GET_PTR(vec, i));
        }
    }

    free(vec->data);
    vec->data = NULL;
    // dont free vec as on stk (don't own memory)
}

void genVec_clear(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    if (vec->del_fn) { // owns resources
        for (u64 i = 0; i < vec->size; i++) {
            vec->del_fn(GET_PTR(vec, i));
        }
    }
    // doesn't free container
    vec->size = 0;
}

void genVec_reset(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    if (vec->del_fn) {
        for (u64 i = 0; i < vec->size; i++) {
            vec->del_fn(GET_PTR(vec, i));
        }
    }

    free(vec->data);
    vec->data     = NULL;
    vec->size     = 0;
    vec->capacity = 0;
}


void genVec_reserve(genVec* vec, u64 new_capacity)
{
    CHECK_FATAL(!vec, "vec is null");

    // Only grow, never shrink with reserve
    if (new_capacity <= vec->capacity) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, new_capacity));
    CHECK_FATAL(!new_data, "realloc failed");

    vec->data     = new_data;
    vec->capacity = new_capacity;
}

void genVec_reserve_val(genVec* vec, u64 new_capacity, const u8* val)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!val, "val is null");
    CHECK_FATAL(new_capacity < vec->size, "new_capacity must be >= current size");

    genVec_reserve(vec, new_capacity);

    for (u64 i = vec->size; i < new_capacity; i++) {
        if (vec->copy_fn) {
            vec->copy_fn(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, vec->data_size);
        }
    }
    vec->size = new_capacity;
}

void genVec_shrink_to_fit(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    // min allowd cap or size
    u64 min_cap  = vec->size > GENVEC_MIN_CAPACITY ? vec->size : GENVEC_MIN_CAPACITY;
    u64 curr_cap = vec->capacity;

    // if curr cap is already equal (or less??) than min allowed cap
    if (curr_cap <= min_cap) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, min_cap));
    CHECK_FATAL(!new_data, "data realloc failed");

    // update data ptr
    vec->data     = new_data;
    vec->size     = min_cap;
    vec->capacity = min_cap;
}


void genVec_push(genVec* vec, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");

    // Check if we need to allocate or grow
    MAYBE_GROW(vec);

    if (vec->copy_fn) {
        vec->copy_fn(GET_PTR(vec, vec->size), data);
    } else {
        memcpy(GET_PTR(vec, vec->size), data, vec->data_size);
    }

    vec->size++;
}


void genVec_push_move(genVec* vec, u8** data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(!*data, "*data is null");

    // Check if we need to allocate or grow
    MAYBE_GROW(vec);

    if (vec->move_fn) {
        vec->move_fn(GET_PTR(vec, vec->size), data);
    } else {
        // copy the pointer to resource
        memcpy(GET_PTR(vec, vec->size), *data, vec->data_size);
        *data = NULL; // now arr owns the resource
    }

    vec->size++;
}

// pop can't be a move operation (array is contiguos)
void genVec_pop(genVec* vec, u8* popped)
{
    CHECK_FATAL(!vec, "vec is null");

    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, );

    u8* last_elm = GET_PTR(vec, vec->size - 1);

    if (popped) {
        if (vec->copy_fn) {
            vec->copy_fn(popped, last_elm);
        } else {
            memcpy(popped, last_elm, vec->data_size);
        }
    }

    if (vec->del_fn) { // del func frees the resources owned by last_elm, but not ptr
        vec->del_fn(last_elm);
    }

    vec->size--; // set for re-write

    MAYBE_SHRINK(vec);
}

void genVec_get(const genVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
    CHECK_FATAL(!out, "need a valid out variable to get the element");

    if (vec->copy_fn) {
        vec->copy_fn(out, GET_PTR(vec, i));
    } else {
        memcpy(out, GET_PTR(vec, i), vec->data_size);
    }
}

const u8* genVec_get_ptr(const genVec* vec, u64 i)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    return GET_PTR(vec, i);
}

u8* genVec_get_ptr_mut(const genVec* vec, u64 i)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    return GET_PTR(vec, i);
}

void genVec_insert(genVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    if (i == vec->size) {
        genVec_push(vec, data);
        return;
    }

    MAYBE_GROW(vec);

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;
    // the place where we want to insert
    u8* src = GET_PTR(vec, i);

    // Shift elements right by one unit
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // Use memmove for overlapping regions

    //src pos is now free to insert (it's data copied to next location)
    if (vec->copy_fn) {
        vec->copy_fn(src, data);
    } else {
        memcpy(src, data, vec->data_size);
    }

    vec->size++;
}

void genVec_insert_move(genVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(!*data, "*data is null");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    if (i == vec->size) {
        genVec_push_move(vec, data);
        return;
    }

    MAYBE_GROW(vec);

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;
    // the place where we want to insert
    u8* src = GET_PTR(vec, i);

    // Shift elements right by one unit
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // Use memmove for overlapping regions


    if (vec->move_fn) {
        vec->move_fn(src, data);
    } else {
        memcpy(src, *data, vec->data_size);
        *data = NULL;
    }

    vec->size++;
}


void genVec_insert_multi(genVec* vec, u64 i, const u8* data, u64 num_data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(num_data == 0, "num_data can't be 0");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;

    vec->size += num_data; // no of new elements in chunk

    genVec_reserve(vec, vec->size); // reserve with new size

    // the place where we want to insert
    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        // Shift elements right by num_data units to right
        u8* dest = GET_PTR(vec, i + num_data);

        memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // using memmove for overlapping regions
    }

    //src pos is now free to insert (it's data copied to next location)
    if (vec->copy_fn) {
        for (u64 j = 0; j < num_data; j++) {
            vec->copy_fn(GET_PTR(vec, j + i), (data + (size_t)(j * vec->data_size)));
        }
    } else {
        memcpy(src, data, GET_SCALED(vec, num_data));
    }
}

void genVec_insert_multi_move(genVec* vec, u64 i, u8** data, u64 num_data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(!*data, "*data is null");
    CHECK_FATAL(num_data == 0, "num_data can't be 0");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;

    vec->size += num_data; // no of new elements in chunk

    genVec_reserve(vec, vec->size); // reserve with new size

    // the place where we want to insert
    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        // Shift elements right by num_data units to right
        u8* dest = GET_PTR(vec, i + num_data);

        memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // using memmove for overlapping regions
    }

    //src pos is now free to insert (it's data copied to next location)
    // Move entire contiguous block at once
    memcpy(src, *data, GET_SCALED(vec, num_data));
    *data = NULL; // Transfer ownership
}

void genVec_remove(genVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (out) {
        if (vec->copy_fn) {
            vec->copy_fn(out, GET_PTR(vec, i));
        } else {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        }
    }

    if (vec->del_fn) {
        vec->del_fn(GET_PTR(vec, i));
    }
    // Calculate the number of elements to shift
    u64 elements_to_shift = vec->size - i - 1;

    if (elements_to_shift > 0) {
        // Shift elements left to overwrite the deleted element
        u8* dest = GET_PTR(vec, i);
        u8* src  = GET_PTR(vec, i + 1);

        memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // Use memmove for overlapping regions
    }

    vec->size--;

    MAYBE_SHRINK(vec);
}


void genVec_remove_range(genVec* vec, u64 l, u64 r)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(l >= vec->size, "index out of range");
    CHECK_FATAL(l > r, "invalid range");

    if (r >= vec->size) {
        r = vec->size - 1;
    }

    if (vec->del_fn) {
        for (u64 i = l; i <= r; i++) {
            u8* elm = GET_PTR(vec, i);
            vec->del_fn(elm);
        }
    }

    u64 elms_to_shift = vec->size - (r + 1);

    // move from r + 1 to l
    u8* dest = GET_PTR(vec, l);
    u8* src  = GET_PTR(vec, r + 1);
    memmove(dest, src, GET_SCALED(vec, elms_to_shift)); // Use memmove for overlapping regions

    vec->size -= (r - l + 1);

    MAYBE_SHRINK(vec);
}


void genVec_replace(genVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
    CHECK_FATAL(!data, "data is null");

    u8* to_replace = GET_PTR(vec, i);

    if (vec->del_fn) {
        vec->del_fn(to_replace);
    }

    if (vec->copy_fn) {
        vec->copy_fn(to_replace, data);
    } else {
        memcpy(to_replace, data, vec->data_size);
    }
}


void genVec_replace_move(genVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
    CHECK_FATAL(!data, "need a valid data variable");
    CHECK_FATAL(!*data, "need a valid *data variable");

    u8* to_replace = GET_PTR(vec, i);

    if (vec->del_fn) {
        vec->del_fn(to_replace);
    }

    if (vec->move_fn) {
        vec->move_fn(to_replace, data);
    } else {
        memcpy(to_replace, *data, vec->data_size);
        *data = NULL;
    }
}


const u8* genVec_front(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, NULL);
    return GET_PTR(vec, 0);
}

const u8* genVec_back(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, NULL);
    return GET_PTR(vec, vec->size - 1);
}


void genVec_print(const genVec* vec, print_fn print_fn)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!print_fn, "print func is null");

    printf("[ ");
    for (u64 i = 0; i < vec->size; i++) {
        print_fn(GET_PTR(vec, i));
        putchar(' ');
    }
    putchar(']');
}


void genVec_copy(genVec* dest, const genVec* src)
{
    CHECK_FATAL(!dest, "dest is null");
    CHECK_FATAL(!src, "src is null");

    if (dest == src) {
        return;
    }

    // if data ptr is null, no op
    genVec_destroy_stk(dest);

    // copy all fields
    memcpy(dest, src, sizeof(genVec));

    // malloc data ptr
    dest->data = malloc(GET_SCALED(src, src->capacity));
    CHECK_FATAL(!dest->data, "dest data malloc failed");

    // Copy elements
    if (src->copy_fn) {
        for (u64 i = 0; i < src->size; i++) {
            src->copy_fn(GET_PTR(dest, i), GET_PTR(src, i));
        }
    } else {
        memcpy(dest->data, src->data, GET_SCALED(src, src->size));
    }
}


void genVec_move(genVec* dest, genVec** src)
{
    CHECK_FATAL(!src, "src is null");
    CHECK_FATAL(!*src, "*src is null");
    CHECK_FATAL(!dest, "dest is null");
    // CHECK_FATAL((*src)->svo, "can't move a SVO (stack) vec");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    // Transfer all fields from src to dest
    memcpy(dest, *src, sizeof(genVec));

    // Null out src's data pointer so it doesn't get freed
    (*src)->data = NULL;

    // Free src if it was-allocated
    // This only frees the genVec struct itself, not the data
    // (which was transferred to dest)
    free(*src);
    *src = NULL;
}


static void genVec_grow(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    u64 new_cap;
    if (vec->capacity < GENVEC_MIN_CAPACITY) {
        new_cap = vec->capacity + 1;
    } else {
        new_cap = (u64)((float)vec->capacity * GENVEC_GROWTH);
        if (new_cap <= vec->capacity) { // Ensure at least +1 growth
            new_cap = vec->capacity + 1;
        }
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, new_cap));
    CHECK_FATAL(!new_data, "data realloc failed");

    vec->data     = new_data;
    vec->capacity = new_cap;
}


static void genVec_shrink(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    u64 reduced_cap = (u64)((float)vec->capacity * GENVEC_SHRINK_BY);
    if (reduced_cap < vec->size || reduced_cap == 0) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, reduced_cap));
    if (!new_data) {
        WARN("shrink realloc failed");
        return; // keep original
    }

    vec->data     = new_data;
    vec->capacity = reduced_cap;
}

#endif /* WC_GEN_VECTOR_IMPL */

/* ===== String.c ===== */
#ifndef WC_STRING_IMPL
#define WC_STRING_IMPL

#include <string.h>




// private func
u64 cstr_len(const char* cstr);



String* string_create(void)
{
    return (String*)genVec_init(0, sizeof(char), NULL, NULL, NULL);
}


void string_create_stk(String* str, const char* cstr)
{
    // the difference is that we dont use string_create(), so str is not initilised
    CHECK_FATAL(!str, "str is null");

    u64 len = 0;
    if (cstr) {
        len = cstr_len(cstr);
    }

    genVec_init_stk(len, sizeof(char), NULL, NULL, NULL, str);
    
    if (len != 0) {
        genVec_insert_multi(str, 0, (const u8*)cstr, len);
    }
}


String* string_from_cstr(const char* cstr)
{
    String* str = malloc(sizeof(String));
    CHECK_FATAL(!str, "str malloc failed");

    string_create_stk(str, cstr);
    return str;
}


String* string_from_string(const String* other)
{
    CHECK_FATAL(!other, "other str is null");

    String* str = malloc(sizeof(String));
    CHECK_FATAL(!str, "str malloc failed");

    genVec_init_stk(other->size, sizeof(char), NULL, NULL, NULL, str);

    if (other->size != 0) {
        genVec_insert_multi(str, 0, other->data, other->size);
    }

    return str;
}


void string_reserve(String* str, u64 capacity)
{
    genVec_reserve(str, capacity);
}


void string_reserve_char(String* str, u64 capacity, char c)
{
    genVec_reserve_val(str, capacity, cast(c));
}


void string_destroy(String* str)
{
    string_destroy_stk(str);
    free(str);
}

void string_destroy_stk(String* str)
{
    genVec_destroy_stk(str);
}


void string_move(String* dest, String** src)
{
    CHECK_FATAL(!src, "src is null");
    CHECK_FATAL(!*src, "src is null");
    CHECK_FATAL(!dest, "dest is null");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    // no op if dest's data ptr is null (like stack inited)
    string_destroy_stk(dest);

    // copy fields (including data ptr)
    memcpy(dest, *src, sizeof(String));

    (*src)->data = NULL;
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

    // no op if data ptr is null
    string_destroy_stk(dest);

    // copy all fields (data ptr too)
    memcpy(dest, src, sizeof(String));

    // malloc new data ptr
    dest->data = malloc(src->capacity);

    // copy all data (arr of chars)
    memcpy(dest->data, src->data, src->size);
}


char* string_to_cstr(const String* str)
{
    CHECK_FATAL(!str, "str is null");

    if (str->size == 0) {
        char* empty = malloc(1);
        CHECK_FATAL(!empty, "malloc failed");
        empty[0] = '\0';
        return empty;
    }

    char* out = malloc(str->size + 1); // + 1 for null term
    CHECK_FATAL(!out, "out str malloc failed");

    memcpy(out, str->data, str->size);

    out[str->size] = '\0'; // add null term

    return out;
}


char* string_data_ptr(const String* str)
{
    CHECK_FATAL(!str, "str is null");

    if (str->size == 0) {
        return NULL;
    }

    return (char*)str->data;
}


void string_append_cstr(String* str, const char* cstr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    genVec_insert_multi(str, str->size, (const u8*)cstr, len);
}


void string_append_string(String* str, const String* other)
{
    CHECK_FATAL(!str, "str is empty");
    CHECK_FATAL(!other, "other is empty");

    if (other->size == 0) {
        return;
    }

    // direct insertion from other's buffer
    genVec_insert_multi(str, str->size, other->data, other->size);
}

// append and consume source string
void string_append_string_move(String* str, String** other)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!other, "other ptr is null");
    CHECK_FATAL(!*other, "*other is null");

    if ((*other)->size > 0) {
        genVec_insert_multi(str, str->size, (*other)->data, (*other)->size);
    }

    string_destroy(*other);
    *other = NULL;
}


void string_append_char(String* str, char c)
{
    CHECK_FATAL(!str, "str is null");
    genVec_push(str, cast(c));
}


char string_pop_char(String* str)
{
    CHECK_FATAL(!str, "str is null");

    char c;
    genVec_pop(str, cast(c));

    return c;
}


void string_insert_char(String* str, u64 i, char c)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(i > str->size, "index out of bounds");

    genVec_insert(str, i, cast(c));
}


void string_insert_cstr(String* str, u64 i, const char* cstr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");
    CHECK_FATAL(i > str->size, "index out of bounds");

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    genVec_insert_multi(str, i, castptr(cstr), len);
}


void string_insert_string(String* str, u64 i, const String* other)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!other, "other is null");
    CHECK_FATAL(i > str->size, "index out of bounds");

    if (other->size == 0) {
        return;
    }

    // direct insertion
    genVec_insert_multi(str, i, other->data, other->size);
}


void string_remove_char(String* str, u64 i)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(i >= str->size, "index out of bounds");

    genVec_remove(str, i, NULL);
}


void string_remove_range(String* str, u64 l, u64 r)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(l >= str->size, "index out of bounds");
    CHECK_FATAL(l > r, "invalid range");

    genVec_remove_range(str, l, r);
}


void string_clear(String* str)
{
    CHECK_FATAL(!str, "str is null");
    genVec_clear(str);
}


char string_char_at(const String* str, u64 i)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(i >= str->size, "index out of bounds");

    return ((char*)str->data)[i];
}


void string_set_char(String* str, u64 i, char c)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(i >= str->size, "index out of bounds");

    ((char*)str->data)[i] = c;
}


int string_compare(const String* str1, const String* str2)
{
    CHECK_FATAL(!str1, "str1 is null");
    CHECK_FATAL(!str2, "str2 is null");

    u64 min_len = str1->size < str2->size ? str1->size : str2->size;

    // Compare byte by byte
    int cmp = memcmp(str1->data, str2->data, min_len);

    if (cmp != 0) {
        return cmp;
    }

    // If equal so far, shorter string is "less"
    if (str1->size < str2->size) {
        return -1;
    }
    if (str1->size > str2->size) {
        return 1;
    }

    return 0;
}


b8 string_equals(const String* str1, const String* str2)
{
    return string_compare(str1, str2) == 0;
}


b8 string_equals_cstr(const String* str, const char* cstr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

    u64 len = cstr_len(cstr);

    // Different lengths = not equal
    if (str->size != len) {
        return false;
    }
    // Both empty
    if (len == 0) {
        return true;
    }

    return memcmp(str->data, cstr, len) == 0;
}


u64 string_find_char(const String* str, char c)
{
    CHECK_FATAL(!str, "str is null");

    for (u64 i = 0; i < str->size; i++) {
        if (((char*)str->data)[i] == c) {
            return i;
        }
    }

    return (u64)-1; // Not found
}


u64 string_find_cstr(const String* str, const char* substr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!substr, "substr is null");

    u64 len = cstr_len(substr);

    // Empty substring is found at index 0
    if (len == 0) {
        return 0;
    }

    if (len > str->size) {
        return (u64)-1;
    }

    for (u64 i = 0; i <= str->size - len; i++) {
        if (memcmp(str->data + i, substr, len) == 0) {
            return i;
        }
    }

    return (u64)-1;
}


String* string_substr(const String* str, u64 start, u64 length)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(start >= str->size, "index out of bounds");

    String* result = string_create();

    u64 end     = start + length;
    u64 str_len = string_len(str);
    if (end > str_len) {
        end = str_len;
    }

    u64 actual_len = end - start;

    if (actual_len > 0) { // Insert substring all at once
        const char* csrc = string_data_ptr(str) + start;
        genVec_insert_multi(result, 0, (const u8*)csrc, actual_len);
    }

    return result;
}


void string_print(const String* str)
{
    CHECK_FATAL(!str, "str is null");

    putchar('\"');
    for (u64 i = 0; i < str->size; i++) {
        putchar(((char*)str->data)[i]);
    }
    putchar('\"');
}


u64 cstr_len(const char* cstr)
{
    u64 len = 0;
    u64 i   = 0;

    while (cstr[i++] != '\0') {
        len++;
    }

    return len;
}

#endif /* WC_STRING_IMPL */

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

#endif /* WC_HELPERS_SINGLE_H */
