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
    WC_ERR_INVALID_OP, // call to a function with preconditions not met
} wc_err;

static inline const char* wc_strerror(wc_err e)
{
    switch (e) {
        case WC_OK:             return "ok";
        case WC_ERR_FULL:       return "full";
        case WC_ERR_EMPTY:      return "empty";
        case WC_ERR_INVALID_OP: return "invalid op";
        default:                return "unknown";
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

#ifndef STRING_GROWTH
    #define STRING_GROWTH    1.5F    // capacity multiplier on grow
#endif

#define STR_SSO_SIZE 24


typedef struct {
    union {
        char* heap;
        char  stk[STR_SSO_SIZE];
    };
    // b8  sso;     // if cap is = STR_SSO_SIZE then we are in sso mode, if greater then heap mode
    u64 size;
    u64 capacity;
} String;

// 24 8 8 = 40 bytes (same as genVec)


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

// TODO: test
// Remove chars in range [start, start + len)
void string_remove_range(String* str, u64 start, u64 len);

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

static inline b8 string_sso(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->capacity == STR_SSO_SIZE;
}


/*
 Macro to temporarily NUL-terminate a String for read-only C APIs.
Note: Do NOT break/return/goto inside the block.

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


typedef u64 (*custom_hash_fn)(const u8* key, u64 size);

#define LOAD_FACTOR_GROW      0.75 // Robin Hood sweet spot
#define HASHMAP_INIT_CAPACITY 16   // power-of-2 to avoid modulo

typedef enum {
    NOT_FOUND = 0,
    FOUND,
    ROBINHOOD_EXIT,
} LOOKUP_RES;

/*
====================WYHASH====================
*/
// wyhash v4 — public domain, Wang Yi
// Best default for hashmaps: fast, excellent avalanche, low collision rate.
// Beats FNV1a and MurmurHash3 on all key sizes.

static inline u64 wyr8(const u8* p)
{
    u64 v;
    memcpy(&v, p, 8);
    return v;
}
static inline u64 wyr4(const u8* p)
{
    u32 v;
    memcpy(&v, p, 4);
    return v;
}

static inline u64 wymix(u64 a, u64 b)
{
    __uint128_t r = (__uint128_t)a * b;
    return (u64)(r) ^ (u64)(r >> 64);
}

static u64 wyhash(const u8* key, u64 len)
{
    const u64 seed = 0x517cc1b727220a95ULL;
    const u64 s0   = 0x2d358dccaa6c78a5ULL;
    const u64 s1   = 0x8bb84b93962eacc9ULL;
    const u64 s2   = 0x4b33a62ed433d4a3ULL;

    u64 a, b;
    u64 h = seed ^ wymix(seed ^ s0, s1) ^ len;

    const u8* p = key;
    u64       i = len;

    // bulk: 16 bytes at a time
    for (; i >= 16; i -= 16, p += 16) {
        h = wymix(wyr8(p) ^ s1, wyr8(p + 8) ^ h);
    }

    // tail
    if (i >= 8) {
        a = wyr8(p);
        b = wyr8(p + i - 8);
    } else if (i >= 4) {
        a = wyr4(p);
        b = wyr4(p + i - 4);
    } else if (i > 0) {
        a = ((u64)p[0] << 16) | ((u64)p[i >> 1] << 8) | p[i - 1];
        b = 0;
    } else {
        a = 0;
        b = 0;
    }

    return wymix(a ^ s2 ^ h, b ^ s2);
}

/*
====================DEFAULT FUNCTIONS====================
*/

static inline u64 fnv1a_hash(const u8* bytes, u64 size)
{
    u64 hash = 14695981039346656037ULL; // 64-bit FNV offset basis
    for (u64 i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL; // 64-bit FNV prime
    }
    return hash;
}

static inline int default_compare(const u8* a, const u8* b, u64 size)
{
    return memcmp(a, b, size);
}


/*
====================STRING HASHING====================
*/

// wyhash variants for String

static u64 wyhash_str(const u8* key, u64 size)
{
    (void)size;
    String* str = (String*)key;
    return wyhash((const u8*)string_data_ptr(str), string_len(str));
}

static u64 wyhash_str_ptr(const u8* key, u64 size)
{
    (void)size;
    String* str = *(String**)key;
    return wyhash((const u8*)string_data_ptr(str), string_len(str));
}

#define ALIGN8(size) (((u64)(size) + 7u) & ~7u)

#endif /* WC_MAP_SETUP_H */

/* ===== hashset.h ===== */
#ifndef WC_HASHSET_H
#define WC_HASHSET_H

/* Generic Hashset with Ownership Semantics
  - Robin Hood Hashing
  - we have 2 arrays: elms, psls
  - PSL: probe sequence length - the distance from hashing location
  - we actually store psl + 1 as psl = 0 means empty bucket
  - Robin Hood Invariant: all elms that hash to i come before elms that hash to i + 1
  - elms stored inline
*/


typedef struct {
    u8*            elms;
    u8*            psls;
    u64            size;
    u64            capacity;
    u32            elm_size;
    u8*            scratch;  // 2 * elm_size bytes — stage (first half) + RH swap (second half)
    custom_hash_fn hash_fn;
    compare_fn     cmp_fn;

    // Shared ops vtable for elements.
    // Pass NULL for POD types (int, float, flat structs).
    const container_ops* ops;
} hashset;


// Safely extract callbacks — always NULL-safe on ops itself.
#define SET_COPY(ops) ((ops) ? (ops)->copy_fn : NULL)
#define SET_MOVE(ops) ((ops) ? (ops)->move_fn : NULL)
#define SET_DEL(ops)  ((ops) ? (ops)->del_fn  : NULL)


// Create a new hashset.
// hash_fn and cmp_fn default to wyhash / default_compare if NULL.
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

// Print all elements.
void hashset_print(const hashset* set, print_fn print);

// Remove all elements, keep capacity.
void hashset_clear(hashset* set);

// Deep copy src into dest
// dest should be pre-inited
void hashset_copy(hashset* dest, const hashset* src);


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

#define IS_SSO(s)          ((s)->capacity == STR_SSO_SIZE)
#define GET_STR(s)         (IS_SSO(s) ? (s)->stk : (s)->heap)
#define GET_STR_PTR(s, i)  (GET_STR(s) + i)
#define GET_STR_CHAR(s, i) (GET_STR(s)[i])
#define STR_REMAINING(s)   ((s)->capacity - (s)->size)

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



//  Private helpers

static u64  cstr_len(const char* cstr);
static void str_copy_n(char* dest, const char* src, u64 n);
static void stk_to_heap(String* s);
static void heap_to_stk(String* s);
static void string_grow(String* s);
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
        WARN("shrink_to_fit realloc failed");
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
    GET_STR_CHAR(s, s->size++) = c;
}

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

    char c = GET_STR_CHAR(s, --s->size);
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

    CHECK_WARN_RET(s == other, , "can't insert aliasing(same) strings");

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
}

/*
    0 1 2 3 4 5  (1, 2)
      ^ ^ 
    start = 1
    len = 2
    end = 2

*/

void string_remove_range(String* s, u64 start, u64 len)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(start >= s->size, "start out of bounds");

    if (len == 0) { return; }

    if (start + len >= s->size) {
        len = s->size - start;
    }

    memmove(GET_STR_PTR(s, start), GET_STR_PTR(s, start + len), s->size - len);

    s->size -= len;
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
    return GET_STR_CHAR(s, i);
}

void string_set_char(String* s, u64 i, char c)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(i >= s->size, "index out of bounds");
    *GET_STR_PTR(s, i) = c; // TODO: test this
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

    if (start + length > s->size) {
        length = s->size - start;
    }

    String* result = string_create();

    if (length > 0) {
        ensure_capacity(result, length);
        str_copy_n(GET_STR(result), GET_STR(s) + start, length);
        result->size = length;
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

    // currently in sso but sso_cap is not enough
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


#define GET_ELM(set, i) ((set)->elms + ((u64)(set)->elm_size * (i)))
#define GET_PSL(set, i) ((set)->psls + (i))

// capacity is always power-of-2 — use bitmask instead of %
#define SET_MASK(set)     ((set)->capacity - 1)
#define SET_IDX(set, elm) ((set)->hash_fn((elm), (set)->elm_size) & SET_MASK(set))
#define SET_NEXT(set, i)  (((i) + 1) & SET_MASK(set))

// PSL 0 == empty bucket; stored PSL is (real_psl + 1), starting at 1
#define BUCKET_EMPTY 0

// scratch layout: [0 .. elm_size) = stage,  [elm_size .. 2*elm_size) = swap
// stage: where hashset_insert copies the incoming elm before calling set_insert
// swap:  where set_insert saves a displaced resident during Robin Hood eviction
// The two halves are alternated each eviction to avoid aliasing (elm pointer
// is always in the half that set_insert is NOT currently writing into).
#define STAGE_ELM(set) ((set)->scratch)
#define SWAP_ELM(set)  ((set)->scratch + (set)->elm_size)


/*
====================PRIVATE DECLARATIONS====================
*/

static u64         set_lookup(const hashset* set, const u8* elm, LOOKUP_RES* res, u8* out_psl);
static void        set_insert(hashset* set, u8* elm, u8 psl, u64 idx);
static void        set_resize(hashset* set, u64 new_capacity);
static inline void set_maybe_resize(hashset* set);


/*
====================PUBLIC FUNCTIONS====================
*/

hashset* hashset_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* ops)
{
    CHECK_FATAL(elm_size == 0, "elm_size can't be 0");

    hashset* set = malloc(sizeof(hashset));
    CHECK_FATAL(!set, "set malloc failed");

    set->elms = malloc((u64)HASHMAP_INIT_CAPACITY * elm_size);
    CHECK_FATAL(!set->elms, "elms calloc failed");
    set->psls = calloc(HASHMAP_INIT_CAPACITY, sizeof(u8));
    CHECK_FATAL(!set->psls, "psls calloc failed");

    // 2 * elm_size: first half = staging, second half = RH swap buffer
    set->scratch = malloc(2 * (u64)elm_size);
    CHECK_FATAL(!set->scratch, "scratch malloc failed");

    set->size     = 0;
    set->capacity = HASHMAP_INIT_CAPACITY;
    set->elm_size = elm_size;

    set->hash_fn = hash_fn ? hash_fn : wyhash;
    set->cmp_fn  = cmp_fn  ? cmp_fn  : default_compare;

    set->ops = ops;

    return set;
}


void hashset_destroy(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    delete_fn e_del = SET_DEL(set->ops);

    if (e_del) {
        for (u64 i = 0; i < set->capacity; i++) {
            if (*GET_PSL(set, i) == BUCKET_EMPTY) {
                continue;
            }
            e_del(GET_ELM(set, i));
        }
    }

    free(set->elms);
    free(set->psls);
    free(set->scratch);
    free(set);
}

void hashset_destroy_stk(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    delete_fn e_del = SET_DEL(set->ops);

    if (e_del) {
        for (u64 i = 0; i < set->capacity; i++) {
            if (*GET_PSL(set, i) == BUCKET_EMPTY) {
                continue;
            }
            e_del(GET_ELM(set, i));
        }
    }

    free(set->elms);
    free(set->psls);
    free(set->scratch);
}


// Insert element — COPY semantics.
// Returns 1 if already existed (no-op), 0 if newly inserted.
b8 hashset_insert(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set || !elm, "args null");

    copy_fn e_cp = SET_COPY(set->ops);

    LOOKUP_RES res;
    u8             out_psl;
    u64            slot = set_lookup(set, elm, &res, &out_psl);

    if (res == FOUND) {
        return 1;
    }

    // Stage a deep copy into scratch before calling set_insert.
    // set_insert only does raw memcpy moves between slots — it never calls copy/del.
    if (e_cp) {
        e_cp(STAGE_ELM(set), elm);
    } else {
        memcpy(STAGE_ELM(set), elm, set->elm_size);
    }

    set_insert(set, STAGE_ELM(set), out_psl, slot);
    set_maybe_resize(set);
    return 0;
}


// Insert element — MOVE semantics (elm is nulled on insert, or freed if duplicate).
// Returns 1 if already existed (elm freed), 0 if newly inserted.
b8 hashset_insert_move(hashset* set, u8** elm)
{
    CHECK_FATAL(!set || !elm || !*elm, "args null");

    move_fn   e_mv  = SET_MOVE(set->ops);
    delete_fn e_del = SET_DEL(set->ops);

    CHECK_FATAL(!e_mv, "elm move func required");

    LOOKUP_RES res;
    u8             out_psl;
    u64            slot = set_lookup(set, *elm, &res, &out_psl);

    if (res == FOUND) {
        // Already exists — consume (destroy) the incoming duplicate.
        if (e_del) {
            e_del(*elm);
        }
        free(*elm);
        *elm = NULL;
        return 1;
    }

    // Stage: move elm into STAGE_ELM — transfers heap resource, nulls *elm.
    e_mv(STAGE_ELM(set), elm);

    set_insert(set, STAGE_ELM(set), out_psl, slot);
    set_maybe_resize(set);
    return 0;
}


// Returns 1 if found, 0 if not.
b8 hashset_has(const hashset* set, const u8* elm)
{
    CHECK_FATAL(!set || !elm, "null arg");

    LOOKUP_RES res;
    u8             out_psl;
    set_lookup(set, elm, &res, &out_psl);
    return res == FOUND;
}


// Returns 1 if found and removed, 0 if not found.
// Uses Robin Hood backward-shift deletion to maintain the probe-sequence invariant
// without tombstones: after removing a slot, shift subsequent entries back one
// position as long as they have PSL > 1 (i.e. they are not at their home slot).
b8 hashset_remove(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set || !elm, "null arg");

    LOOKUP_RES res;
    u8             out_psl;
    u64            slot = set_lookup(set, elm, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    delete_fn e_del = SET_DEL(set->ops);

    if (e_del) {
        e_del(GET_ELM(set, slot));
    }

    // Backward-shift: pull subsequent entries one slot back as long as
    // they have PSL > 1. Entries at their home slot (PSL == 1) must not move.
    u64 cur = slot;
    for (;;) {
        u64 next     = SET_NEXT(set, cur);
        u8  next_psl = *GET_PSL(set, next);

        if (next_psl <= 1) {
            *GET_PSL(set, cur) = BUCKET_EMPTY;
            break;
        }

        *GET_PSL(set, cur) = next_psl - 1;
        memcpy(GET_ELM(set, cur), GET_ELM(set, next), set->elm_size);

        cur = next;
    }

    set->size--;
    return 1;
}


// Print all elements.
void hashset_print(const hashset* set, print_fn print)
{
    CHECK_FATAL(!set || !print, "null arg");

    printf("\t=========\n");
    printf("\tSize: %lu / Capacity: %lu\n", set->size, set->capacity);
    printf("\t=========\n");

    for (u64 i = 0; i < set->capacity; i++) {
        if (*GET_PSL(set, i) == BUCKET_EMPTY) {
            continue;
        }
        putchar('\t');
        print(GET_ELM(set, i));
        putchar('\n');
    }

    printf("\t=========\n");
}


// Remove all elements, keep capacity.
void hashset_clear(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    delete_fn e_del = SET_DEL(set->ops);

    for (u64 i = 0; i < set->capacity; i++) {
        if (*GET_PSL(set, i) == BUCKET_EMPTY) {
            continue;
        }
        if (e_del) {
            e_del(GET_ELM(set, i));
        }
    }

    memset(set->psls, 0, set->capacity * sizeof(u8));
    set->size = 0;
}


// Deep copy src into dest
// Ownership: dest gets independently owned copies of all elements.
void hashset_copy(hashset* dest, const hashset* src)
{
    CHECK_FATAL(!dest || !src, "null arg");

    hashset_destroy_stk(dest);

    dest->elms = calloc(src->capacity, src->elm_size);
    CHECK_FATAL(!dest->elms, "copy elms calloc failed");
    dest->psls = calloc(src->capacity, sizeof(u8));
    CHECK_FATAL(!dest->psls, "copy psls calloc failed");
    dest->scratch = malloc(2 * (u64)src->elm_size);
    CHECK_FATAL(!dest->scratch, "copy scratch malloc failed");

    dest->size     = src->size;
    dest->capacity = src->capacity;
    dest->elm_size = src->elm_size;
    dest->hash_fn  = src->hash_fn;
    dest->cmp_fn   = src->cmp_fn;
    dest->ops      = src->ops;

    copy_fn e_cp = SET_COPY(src->ops);

    for (u64 i = 0; i < src->capacity; i++) {
        u8 psl = *GET_PSL(src, i);
        if (psl == BUCKET_EMPTY) {
            continue;
        }

        *GET_PSL(dest, i) = psl;

        if (e_cp) {
            e_cp(GET_ELM(dest, i), GET_ELM(src, i));
        } else {
            memcpy(GET_ELM(dest, i), GET_ELM(src, i), src->elm_size);
        }
    }
}


/*
====================PRIVATE FUNCTIONS====================
*/

static inline void set_maybe_resize(hashset* set)
{
    // integer multiply avoids float — equivalent to load > 0.75
    if (set->size * 4 >= set->capacity * 3) {
        set_resize(set, set->capacity * 2);
    }
}


static u64 set_lookup(const hashset* set, const u8* elm, LOOKUP_RES* res, u8* out_psl)
{
    u64 idx = SET_IDX(set, elm);
    u8  psl = 1; // stored PSL=1 means real probe distance 0 (home slot)

    for (u64 i = idx;; i = SET_NEXT(set, i))
    {
        u8 slot_psl = *GET_PSL(set, i);
        *out_psl    = psl;

        if (slot_psl == BUCKET_EMPTY) {
            *res = NOT_FOUND;
            return i;
        }

        if (slot_psl < psl) {
            // The resident was inserted closer to home than we are —
            // our elm can't be further ahead (Robin Hood invariant).
            *res = ROBINHOOD_EXIT;
            return i;
        }

        if (set->cmp_fn(GET_ELM(set, i), elm, set->elm_size) == 0) {
            *res = FOUND;
            return i;
        }

        psl++;
    }
}


static void set_insert(hashset* set, u8* elm, u8 psl, u64 idx)
{
    // elm is already owned (either staged copy or moved pointer).
    // Alternates between the two scratch halves on each Robin Hood eviction
    // so that elm never aliases the buffer being written into.
    u8* cur = STAGE_ELM(set);
    u8* swp = SWAP_ELM(set);

    // elm may already be STAGE_ELM (called from hashset_insert/insert_move);
    // only copy if it isn't already there.
    if (elm != cur) {
        memcpy(cur, elm, set->elm_size);
    }

    for (u64 i = idx;; i = SET_NEXT(set, i))
    {
        u8 slot_psl = *GET_PSL(set, i);

        if (slot_psl == BUCKET_EMPTY) {
            *GET_PSL(set, i) = psl;
            memcpy(GET_ELM(set, i), cur, set->elm_size);
            set->size++;
            return;
        }

        // Robin Hood: evict the "rich" resident (lower PSL = closer to home).
        if (slot_psl < psl) {
            u8 tmp_psl = slot_psl;

            // Save displaced resident into swp (disjoint from cur).
            memcpy(swp, GET_ELM(set, i), set->elm_size);

            // Place incoming element into slot.
            *GET_PSL(set, i) = psl;
            memcpy(GET_ELM(set, i), cur, set->elm_size);

            // The evicted entry is now in swp; swap roles so cur always
            // points to the element being placed and swp is the free buffer.
            u8* tmp = cur; cur = swp; swp = tmp;
            psl = tmp_psl + 1; // +1: evicted entry moves one slot further from home
            continue;          // skip the unconditional psl++ below
        }

        psl++;
    }
}


static void set_resize(hashset* set, u64 new_capacity)
{
    if (new_capacity < HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    u8* old_elms = set->elms;
    u8* old_psls = set->psls;
    u64 old_cap  = set->capacity;

    set->elms = calloc(new_capacity, set->elm_size);
    CHECK_FATAL(!set->elms, "resize elms calloc failed");
    set->psls = calloc(new_capacity, sizeof(u8));
    CHECK_FATAL(!set->psls, "resize psls calloc failed");

    set->capacity = new_capacity;
    set->size     = 0;

    for (u64 i = 0; i < old_cap; i++) {
        if (old_psls[i] == BUCKET_EMPTY) {
            continue;
        }

        u8* old_elm = old_elms + ((u64)set->elm_size * i);

        // Stage each entry before inserting — set_insert uses SWAP_ELM (second
        // half of scratch) as its eviction buffer, so old_elm must not alias it.
        memcpy(STAGE_ELM(set), old_elm, set->elm_size);

        LOOKUP_RES res;
        u8             out_psl;
        u64            slot = set_lookup(set, STAGE_ELM(set), &res, &out_psl);
        set_insert(set, STAGE_ELM(set), out_psl, slot);
    }

    free(old_elms);
    free(old_psls);
}

#endif /* WC_HASHSET_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_HASHSET_SINGLE_H */
