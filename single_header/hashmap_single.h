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

/* ===== gen_vector.h ===== */
#ifndef WC_GEN_VECTOR_H
#define WC_GEN_VECTOR_H

/*          TLDR
 * genVec is a value-based generic vector.
 * Elements are stored inline and managed via user-supplied
 * copy/move/destructor callbacks.
 *
 * This avoids pointer ownership ambiguity and improves cache locality.
 *
 * Callbacks are grouped into a shared genVec_ops struct (vtable).
 * Define one static ops instance per type and share it across all
 * vectors of that type —  improves cache locality when many vectors of the same type exist.
 *
 * Example:
 *   static const genVec_ops string_ops = { str_copy, str_move, str_del };
 *   genVec* vec = genVec_init(8, sizeof(String), &string_ops);
 *
 * For POD types (int, float, flat structs) pass NULL for ops:
 *   genVec* vec = genVec_init(8, sizeof(int), NULL);
 */


// genVec growth/shrink settings (user can change)

#ifndef GENVEC_GROWTH
    #define GENVEC_GROWTH 1.5F      // vec capacity multiplier
#endif
#ifndef GENVEC_SHRINK_AT
    #define GENVEC_SHRINK_AT 0.25F  // % filled to shrink at (25% filled)
#endif
#ifndef GENVEC_SHRINK_BY
    #define GENVEC_SHRINK_BY 0.5F   // capacity divisor (half)
#endif


// // Vtable: one instance shared across all vectors of the same type.
// // Pass NULL for any callback not needed.
// // For POD types, pass NULL for the whole ops pointer.
// typedef struct {
//     copy_fn   copy_fn; // Deep copy function for owned resources (or NULL)
//     move_fn   move_fn; // Transfer ownership and null original (or NULL)
//     delete_fn del_fn;  // Cleanup function for owned resources (or NULL)
// } container_ops;


// generic vector container
typedef struct {
    u8* data; // pointer to generic data

    u64 size;      // Number of elements currently in vector
    u64 capacity;  // Total allocated capacity (in elements)
    u32 data_size; // Size of each element in bytes

    // Pointer to shared type-ops vtable (or NULL for POD types)
    const container_ops* ops;
} genVec;

// sizeof(genVec) == 48


// Convenience: access ops callbacks safely
#define VEC_COPY_FN(vec) ((vec)->ops ? (vec)->ops->copy_fn : NULL)
#define VEC_MOVE_FN(vec) ((vec)->ops ? (vec)->ops->move_fn : NULL)
#define VEC_DEL_FN(vec)  ((vec)->ops ? (vec)->ops->del_fn  : NULL)



// Memory Management
// ===========================

// Initialize vector with capacity n.
// ops: pointer to a shared genVec_ops vtable, or NULL for POD types.
genVec* genVec_init(u64 n, u32 data_size, const container_ops* ops);

// Initialize vector on stack (struct on stack, data on heap).
void genVec_init_stk(u64 n, u32 data_size, const container_ops* ops, genVec* vec);

// Initialize vector of size n with all elements set to val.
genVec* genVec_init_val(u64 n, const u8* val, u32 data_size, const container_ops* ops);

void genVec_init_val_stk(u64 n, const u8* val, u32 data_size, const container_ops* ops, genVec* vec);

// Vector COMPLETELY on stack (can't grow in size).
// You provide a stack-allocated array which becomes the internal array.
// WARNING: crashes when size == capacity and you try to push.
void genVec_init_arr(u64 n, u8* arr, u32 data_size, const container_ops* ops, genVec* vec);

// Destroy heap-allocated vector and clean up all elements.
void genVec_destroy(genVec* vec);

// Destroy stack-allocated vector (cleans up data, but not vec itself).
void genVec_destroy_stk(genVec* vec);

// Remove all elements (calls del_fn on each), keep capacity.
void genVec_clear(genVec* vec);

// Remove all elements and free memory, shrink capacity to 0.
void genVec_reset(genVec* vec);

// Ensure vector has at least new_capacity space (never shrinks).
void genVec_reserve(genVec* vec, u64 new_capacity);

// Grow to new_capacity and fill new slots with val.
void genVec_reserve_val(genVec* vec, u64 new_capacity, const u8* val);

// Shrink vector to its size (reallocates).
void genVec_shrink_to_fit(genVec* vec);



// Operations
// ===========================

// Append element to end (makes deep copy if copy_fn provided).
void genVec_push(genVec* vec, const u8* data);

// Append element to end, transfer ownership (nulls original pointer).
void genVec_push_move(genVec* vec, u8** data);

// Remove element from end. If popped is provided, copies element before deletion.
// Note: del_fn is called regardless to clean up owned resources.
void genVec_pop(genVec* vec, u8* popped);

// Copy element at index i into out buffer.
void genVec_get(const genVec* vec, u64 i, u8* out);

// Get pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
const u8* genVec_get_ptr(const genVec* vec, u64 i);

// Get MUTABLE pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
u8* genVec_get_ptr_mut(const genVec* vec, u64 i);

// Replace element at index i with data (cleans up old element).
void genVec_replace(genVec* vec, u64 i, const u8* data);

// Replace element at index i, transfer ownership (cleans up old element).
void genVec_replace_move(genVec* vec, u64 i, u8** data);

// Insert element at index i, shifting elements right.
void genVec_insert(genVec* vec, u64 i, const u8* data);

// Insert element at index i with ownership transfer, shifting elements right.
void genVec_insert_move(genVec* vec, u64 i, u8** data);

// Insert num_data elements from data array into vec at index i.
void genVec_insert_multi(genVec* vec, u64 i, const u8* data, u64 num_data);

// Insert (move) num_data elements from data starting at index i.
void genVec_insert_multi_move(genVec* vec, u64 i, u8** data, u64 num_data);

// Remove element at index i, optionally copy to out, shift elements left.
void genVec_remove(genVec* vec, u64 i, u8* out);

// Remove elements in range [l, r] inclusive.
void genVec_remove_range(genVec* vec, u64 l, u64 r);

// Get pointer to first element.
const u8* genVec_front(const genVec* vec);

// Get pointer to last element.
const u8* genVec_back(const genVec* vec);


// Utility
// ===========================

// Print all elements using provided print function.
void genVec_print(const genVec* vec, print_fn fn);

// Deep copy src vector into dest.
// Note: cleans up dest (if already inited).
void genVec_copy(genVec* dest, const genVec* src);

// Transfer ownership from src to dest.
// Note: src must be heap-allocated.
void genVec_move(genVec* dest, genVec** src);


// Get number of elements in vector.
static inline u64 genVec_size(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    return vec->size;
}

// Get total capacity of vector.
static inline u64 genVec_capacity(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    return vec->capacity;
}

// Check if vector is empty.
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
// 0 -> equal, negative -> str1 < str2, positive -> str1 > str2
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

/*
 Macro to create a temporary cstr for read ops.
 Note: Must not break or return in the block.
 Usage:

TEMP_CSTR_READ(s) {
    printf("%s\n", string_data_ptr(s));
}
*/
#define TEMP_CSTR_READ(str) \
    for (u8 _once = 0; (_once == 0) && (string_append_char((str), '\0'), 1); _once++, string_pop_char((str)))

// TODO: how to do this??
#define TEMP_CSTR_READ_NAMED(str, name) \
    for (u8 _once = 0; (_once == 0) && (string_append_char((str), '\0'), 1); _once++, string_pop_char((str)))

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

/* ===== hashmap.h ===== */
#ifndef WC_HASHMAP_H
#define WC_HASHMAP_H

typedef struct {
    u8*   key;
    u8*   val;
    STATE state;
} KV;

typedef struct {
    KV*            buckets;
    u64            size;
    u64            capacity;
    u32            key_size;
    u32            val_size;
    custom_hash_fn hash_fn;
    compare_fn     cmp_fn;

    // Shared ops vtables for keys and values.
    // Pass NULL for POD types (int, float, flat structs).
    // For types with heap resources define one static ops per type:
    const container_ops* key_ops;
    const container_ops* val_ops;
} hashmap;


// Safely extract callbacks — always NULL-safe on ops itself.
#define MAP_COPY(ops) ((ops) ? (ops)->copy_fn : NULL)
#define MAP_MOVE(ops) ((ops) ? (ops)->move_fn : NULL)
#define MAP_DEL(ops)  ((ops) ? (ops)->del_fn  : NULL)


// Create a new hashmap.
// hash_fn and cmp_fn default to fnv1a_hash / default_compare if NULL.
// key_ops / val_ops: pass NULL for POD types.
hashmap* hashmap_create(u32 key_size, u32 val_size,
                        custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* key_ops, const container_ops* val_ops);

void hashmap_destroy(hashmap* map);

// Insert or update — COPY semantics.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put(hashmap* map, const u8* key, const u8* val);

// Insert or update — MOVE semantics (key and val are u8**, both nulled).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put_move(hashmap* map, u8** key, u8** val);

// Mixed: key copied, val moved.
b8 hashmap_put_val_move(hashmap* map, const u8* key, u8** val);

// Mixed: key moved, val copied.
b8 hashmap_put_key_move(hashmap* map, u8** key, const u8* val);

// Get value for key — copies into val. Returns 1 if found, 0 if not.
b8 hashmap_get(const hashmap* map, const u8* key, u8* val);

// Get pointer to value — no copy.
// WARNING: invalidated by put/del operations.
u8* hashmap_get_ptr(hashmap* map, const u8* key);

// Get raw bucket pointer at index i.
KV* hashmap_get_bucket(hashmap* map, u64 i);

// Delete key. If out is provided, value is copied to it before deletion.
// Returns 1 if found and deleted, 0 if not found.
b8 hashmap_del(hashmap* map, const u8* key, u8* out);

// Check if key exists.
b8 hashmap_has(const hashmap* map, const u8* key);

// Print all key-value pairs.
void hashmap_print(const hashmap* map, print_fn key_print, print_fn val_print);

// Remove all elements, keep capacity.
void hashmap_clear(hashmap* map);

// Deep copy src into dest (dest must be uninitialised or already destroyed).
void hashmap_copy(hashmap* dest, const hashmap* src);


static inline u64 hashmap_size(const hashmap* map)     { CHECK_FATAL(!map, "map is null"); return map->size;      }
static inline u64 hashmap_capacity(const hashmap* map) { CHECK_FATAL(!map, "map is null"); return map->capacity;  }
static inline b8  hashmap_empty(const hashmap* map)    { CHECK_FATAL(!map, "map is null"); return map->size == 0; }

#endif /* WC_HASHMAP_H */

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
#define GET_PTR(vec, i) ((vec->data) + ((u64)(i) * ((vec)->data_size)))
// get total size in bytes for i elements
#define GET_SCALED(vec, i) ((u64)(i) * ((vec)->data_size))

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


// ops accessors (safe when ops is NULL)
#define COPY_FN(vec) VEC_COPY_FN(vec)
#define MOVE_FN(vec) VEC_MOVE_FN(vec)
#define DEL_FN(vec)  VEC_DEL_FN(vec)


// private functions

static void genVec_grow(genVec* vec);
static void genVec_shrink(genVec* vec);


// API Implementation

genVec* genVec_init(u64 n, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    genVec* vec = malloc(sizeof(genVec));
    CHECK_FATAL(!vec, "vec init failed");

    // Only allocate memory if n > 0, otherwise data can be NULL
    vec->data = (n > 0) ? malloc(data_size * n) : NULL;

    if (n > 0 && !vec->data) {
        free(vec);
        FATAL("data init failed");
    }

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->ops       = ops;

    return vec;
}


void genVec_init_stk(u64 n, u32 data_size, const container_ops* ops, genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    vec->data = (n > 0) ? malloc(data_size * n) : NULL;
    CHECK_FATAL(n > 0 && !vec->data, "data init failed");

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->ops       = ops;
}


genVec* genVec_init_val(u64 n, const u8* val, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(!val, "val can't be null");
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    genVec* vec = genVec_init(n, data_size, ops);

    vec->size = n; // capacity set to n in genVec_init

    copy_fn copy = COPY_FN(vec);
    for (u64 i = 0; i < n; i++) {
        if (copy) {
            copy(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    }

    return vec;
}


void genVec_init_val_stk(u64 n, const u8* val, u32 data_size, const container_ops* ops, genVec* vec)
{
    CHECK_FATAL(!val, "val can't be null");
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    genVec_init_stk(n, data_size, ops, vec);

    vec->size = n;

    copy_fn copy = COPY_FN(vec);
    for (u64 i = 0; i < n; i++) {
        if (copy) {
            copy(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    }
}


void genVec_init_arr(u64 n, u8* arr, u32 data_size, const container_ops* ops, genVec* vec)
{
    CHECK_FATAL(!arr, "arr is null");
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(n == 0, "size of arr can't be 0");
    CHECK_FATAL(data_size == 0, "data_size of arr can't be 0");

    vec->data      = arr;
    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->ops       = ops;
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

    delete_fn del = DEL_FN(vec);
    if (del) {
        for (u64 i = 0; i < vec->size; i++) {
            del(GET_PTR(vec, i));
        }
    }

    free(vec->data);
    vec->data = NULL;
}


void genVec_clear(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    delete_fn del = DEL_FN(vec);
    if (del) {
        for (u64 i = 0; i < vec->size; i++) {
            del(GET_PTR(vec, i));
        }
    }

    vec->size = 0;
}


void genVec_reset(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    delete_fn del = DEL_FN(vec);
    if (del) {
        for (u64 i = 0; i < vec->size; i++) {
            del(GET_PTR(vec, i));
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

    copy_fn copy = COPY_FN(vec);
    for (u64 i = vec->size; i < new_capacity; i++) {
        if (copy) {
            copy(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, vec->data_size);
        }
    }
    vec->size = new_capacity;
}


void genVec_shrink_to_fit(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    u64 min_cap  = vec->size > GENVEC_MIN_CAPACITY ? vec->size : GENVEC_MIN_CAPACITY;
    u64 curr_cap = vec->capacity;

    if (curr_cap <= min_cap) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, min_cap));
    CHECK_FATAL(!new_data, "data realloc failed");

    vec->data     = new_data;
    vec->capacity = min_cap;
}


void genVec_push(genVec* vec, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");

    MAYBE_GROW(vec);

    copy_fn copy = COPY_FN(vec);
    if (copy) {
        copy(GET_PTR(vec, vec->size), data);
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

    MAYBE_GROW(vec);

    move_fn move = MOVE_FN(vec);
    if (move) {
        move(GET_PTR(vec, vec->size), data);
    } else {
        memcpy(GET_PTR(vec, vec->size), *data, vec->data_size);
        *data = NULL;
    }

    vec->size++;
}


void genVec_pop(genVec* vec, u8* popped)
{
    CHECK_FATAL(!vec, "vec is null");

    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, );

    u8* last_elm = GET_PTR(vec, vec->size - 1);

    if (popped) {
        copy_fn copy = COPY_FN(vec);
        if (copy) {
            copy(popped, last_elm);
        } else {
            memcpy(popped, last_elm, vec->data_size);
        }
    }

    delete_fn del = DEL_FN(vec);
    if (del) {
        del(last_elm);
    }

    vec->size--;

    MAYBE_SHRINK(vec);
}


void genVec_get(const genVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!out, "out is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    copy_fn copy = COPY_FN(vec);
    if (copy) {
        copy(out, GET_PTR(vec, i));
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


void genVec_replace(genVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
    CHECK_FATAL(!data, "data is null");

    u8* to_replace = GET_PTR(vec, i);

    delete_fn del = DEL_FN(vec);
    if (del) {
        del(to_replace);
    }

    copy_fn copy = COPY_FN(vec);
    if (copy) {
        copy(to_replace, data);
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

    delete_fn del = DEL_FN(vec);
    if (del) {
        del(to_replace);
    }

    move_fn move = MOVE_FN(vec);
    if (move) {
        move(to_replace, data);
    } else {
        memcpy(to_replace, *data, vec->data_size);
        *data = NULL;
    }
}


void genVec_insert(genVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    u64 elements_to_shift = vec->size - i;

    MAYBE_GROW(vec);

    u8* src  = GET_PTR(vec, i);
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift));

    copy_fn copy = COPY_FN(vec);
    if (copy) {
        copy(src, data);
    } else {
        memcpy(src, data, vec->data_size);
    }

    vec->size++;
}


void genVec_insert_move(genVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data ptr is null");
    CHECK_FATAL(!*data, "*data is null");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    u64 elements_to_shift = vec->size - i;

    MAYBE_GROW(vec);

    u8* src  = GET_PTR(vec, i);
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift));

    move_fn move = MOVE_FN(vec);
    if (move) {
        move(src, data);
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

    u64 elements_to_shift = vec->size - i;

    vec->size += num_data;
    genVec_reserve(vec, vec->size);

    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i + num_data);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
    }

    copy_fn copy = COPY_FN(vec);
    if (copy) {
        for (u64 j = 0; j < num_data; j++) {
            copy(GET_PTR(vec, j + i), data + (size_t)(j * vec->data_size));
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

    u64 elements_to_shift = vec->size - i;

    vec->size += num_data;
    genVec_reserve(vec, vec->size);

    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i + num_data);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
    }

    memcpy(src, *data, GET_SCALED(vec, num_data));
    *data = NULL;
}


void genVec_remove(genVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (out) {
        copy_fn copy = COPY_FN(vec);
        if (copy) {
            copy(out, GET_PTR(vec, i));
        } else {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        }
    }

    delete_fn del = DEL_FN(vec);
    if (del) {
        del(GET_PTR(vec, i));
    }

    u64 elements_to_shift = vec->size - i - 1;
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i);
        u8* src  = GET_PTR(vec, i + 1);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
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

    delete_fn del = DEL_FN(vec);
    if (del) {
        for (u64 i = l; i <= r; i++) {
            del(GET_PTR(vec, i));
        }
    }

    u64 elms_to_shift = vec->size - (r + 1);

    u8* dest = GET_PTR(vec, l);
    u8* src  = GET_PTR(vec, r + 1);
    memmove(dest, src, GET_SCALED(vec, elms_to_shift));

    vec->size -= (r - l + 1);

    MAYBE_SHRINK(vec);
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

    genVec_destroy_stk(dest);

    // Copy all fields (including ops pointer)
    memcpy(dest, src, sizeof(genVec));

    dest->data = malloc(GET_SCALED(src, src->capacity));
    CHECK_FATAL(!dest->data, "dest data malloc failed");

    copy_fn copy = COPY_FN(src);
    if (copy) {
        for (u64 i = 0; i < src->size; i++) {
            copy(GET_PTR(dest, i), GET_PTR(src, i));
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

    if (dest == *src) {
        *src = NULL;
        return;
    }

    memcpy(dest, *src, sizeof(genVec));

    (*src)->data = NULL;
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
        if (new_cap <= vec->capacity) {
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
        return;
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
    // chars are POD — no ops needed
    return (String*)genVec_init(0, sizeof(char), NULL);
}


void string_create_stk(String* str, const char* cstr)
{
    CHECK_FATAL(!str, "str is null");

    u64 len = 0;
    if (cstr) {
        len = cstr_len(cstr);
    }

    genVec_init_stk(len, sizeof(char), NULL, str);

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

    genVec_init_stk(other->size, sizeof(char), NULL, str);

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

    string_destroy_stk(dest);

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

    string_destroy_stk(dest);

    memcpy(dest, src, sizeof(String));

    dest->data = malloc(src->capacity);

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

    char* out = malloc(str->size + 1);
    CHECK_FATAL(!out, "out str malloc failed");

    memcpy(out, str->data, str->size);
    out[str->size] = '\0';

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

    genVec_insert_multi(str, str->size, other->data, other->size);
}


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

    int cmp = memcmp(str1->data, str2->data, min_len);

    if (cmp != 0) {
        return cmp;
    }

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

    if (str->size != len) {
        return false;
    }
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

    return (u64)-1;
}


u64 string_find_cstr(const String* str, const char* substr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!substr, "substr is null");

    u64 len = cstr_len(substr);

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

    if (actual_len > 0) {
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

#include <string.h>


#define GET_KV(map, i) ((map)->buckets + (i))


/*
====================PRIVATE FUNCTIONS====================
*/

static void memset_buckets(KV* buckets, u64 size);
static void kv_destroy(KV* kv, const container_ops* key_ops, const container_ops* val_ops);

static u64  find_slot(const hashmap* map, const u8* key, b8* found, int* tombstone);

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
    u64 slot      = find_slot(map, key, &found, &tombstone);

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
    u64 slot      = find_slot(map, *key, &found, &tombstone);

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
    u64 slot      = find_slot(map, key, &found, &tombstone);

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
    u64 slot      = find_slot(map, *key, &found, &tombstone);

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
    u64 slot      = find_slot(map, key, &found, &tombstone);

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
    u64 slot      = find_slot(map, key, &found, &tombstone);

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
    u64 slot      = find_slot(map, key, &found, &tombstone);

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
    find_slot(map, key, &found, &tombstone);

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

    // Copy all scalar fields (sizes, fn ptrs, ops), then give dest its own bucket array.
    // Can't memcpy the whole struct — that would alias dest->buckets to src->buckets.
    dest->capacity = src->capacity;
    dest->size     = 0;
    dest->key_size = src->key_size;
    dest->val_size = src->val_size;
    dest->hash_fn  = src->hash_fn;
    dest->cmp_fn   = src->cmp_fn;
    dest->key_ops  = src->key_ops;
    dest->val_ops  = src->val_ops;

    dest->buckets = malloc(src->capacity * sizeof(KV));
    CHECK_FATAL(!dest->buckets, "dest bucket malloc failed");
    memset_buckets(dest->buckets, src->capacity);

    MAP_FOREACH_BUCKET(src, kv) {
        b8  found     = 0;
        int tombstone = -1;
        u64 slot      = find_slot(dest, kv->key, &found, &tombstone);

        u8* k = malloc(src->key_size);
        CHECK_FATAL(!k, "key malloc failed");
        u8* v = malloc(src->val_size);
        CHECK_FATAL(!v, "val malloc failed");

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
    memset_buckets(map->buckets, new_capacity);

    map->capacity = new_capacity;
    map->size     = 0;

    // Rehash — pointers are moved as-is, no copy/del needed
    for (u64 i = 0; i < old_cap; i++) {
        const KV* old_kv = old_buckets + i;

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

#endif /* WC_HASHMAP_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_HASHMAP_SINGLE_H */
