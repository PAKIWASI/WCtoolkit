#ifndef WC_WC_HELPERS_SINGLE_H
#define WC_WC_HELPERS_SINGLE_H

/*
 * wc_helpers_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "wc_helpers_single.h"
 *
 * All other files just:
 *     #include "wc_helpers_single.h"
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// ANSI Color Codes
#define WC_COLOR_RESET  "\033[0m"
#define WC_COLOR_RED    "\033[1;31m"
#define WC_COLOR_YELLOW "\033[1;33m"
#define WC_COLOR_GREEN  "\033[1;32m"
#define WC_COLOR_BLUE   "\033[1;34m"
#define WC_COLOR_CYAN   "\033[1;36m"



// TODO: warm paths ?

#define WARN(fmt, ...)                                                  \
    do {                                                                \
        printf(WC_COLOR_YELLOW "[WARN]"                                 \
                               " %s:%d:%s(): " fmt "\n" WC_COLOR_RESET, \
               __FILE__, __LINE__, __func__, ##__VA_ARGS__);            \
    } while (0)

__attribute__((noreturn, format(printf, 4, 5))) static inline void
wc_fatal_report(const char* file, int line, const char* func, const char* fmt, ...)
{
    fprintf(stderr, WC_COLOR_RED "[FATAL] %s:%d:%s(): ", file, line, func);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n" WC_COLOR_RESET);
    exit(EXIT_FAILURE);
}

#define FATAL(fmt, ...) wc_fatal_report(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

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

#ifdef NDEBUG
#define CHECK_FATAL(cond, fmt, ...) ((void)0)
#else
#define CHECK_FATAL(cond, fmt, ...)                           \
    do {                                                      \
        if (__builtin_expect(!!(cond), 0)) {                  \
            FATAL("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                     \
    } while (0)
#endif

#define LOG(fmt, ...)                                             \
    do {                                                          \
        printf(WC_COLOR_CYAN "[LOG]"                              \
                             " : %s(): " fmt "\n" WC_COLOR_RESET, \
               __func__, ##__VA_ARGS__);                          \
    } while (0)


#define MALLOC(size, cap, name)                \
    ({                                         \
        void* _mlcd = malloc(size * cap);      \
        CHECK_FATAL(!_mlcd, "\"" #name "\""    \
                            " malloc failed"); \
        _mlcd;                                 \
    })


// TYPES

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t  u8;
typedef uint8_t  b8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define WC_NOT_FOUND ((u64) - 1)

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
    printf("%.2f ", (double)*(float*)elm);
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

/* ===== wc_string.h ===== */
#ifndef WC_WC_STRING_H
#define WC_WC_STRING_H

#ifndef STRING_GROWTH
#define STRING_GROWTH 1.5F // capacity multiplier on grow
#endif

#define STR_SSO_SIZE 24


typedef struct {
    union {
        char* heap;
        char  stk[STR_SSO_SIZE];
    };
    u64 size;
    u64 capacity;
} String;

_Static_assert(sizeof(String) == 40, "String must be 40 bytes");



//  Construction / Destruction

// Create an empty String on the heap.
String* String_create(void) __attribute__((warn_unused_result));

// Create a String on the heap from a cstr.
String* String_from_cstr(const char* cstr) __attribute__((warn_unused_result));

// Create a copy of another heap-allocated String.
String* String_from_String(const String* other) __attribute__((nonnull(1), warn_unused_result));

// Initialise a String whose struct lives on the Stack (data may be on heap).
void String_create_stk(String* str, const char* cstr) __attribute__((nonnull(1)));

// Destroy a heap-allocated String (frees struct + data).
void String_destroy(String* str) __attribute__((nonnull(1)));

// Destroy only the internal data of a Stack-allocated String.
void String_destroy_stk(String* str) __attribute__((nonnull(1)));

// Move: transfer ownership from *src to dest, nulling *src.
// *src must be heap-allocated.
void String_move(String* dest, String** src) __attribute__((nonnull(1, 2)));

// Deep copy src into dest (dest is re-initialised).
// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void String_copy(String* dest, const String* src) __attribute__((nonnull(1, 2)));


//  Capacity

// Ensure capacity >= new_cap (never shrinks).
void String_reserve(String* str, u64 new_cap) __attribute__((nonnull(1)));

// Reserve capacity and fill new slots with c.
void String_reserve_char(String* str, u64 new_cap, char c) __attribute__((nonnull(1)));

// Shrink allocation to exactly fit current size.
void String_shrink_to_fit(String* str) __attribute__((nonnull(1)));


//  Conversion

// Return a malloc'd NUL-terminated copy — caller must free().
char* String_to_cstr(const String* str) __attribute__((nonnull(1), warn_unused_result));

void String_to_cstr_buf(const String* str, char* buff, u64 n) __attribute__((nonnull(1, 2)));

// Return a raw pointer into the internal buffer (no NUL terminator).
char* String_data_ptr(const String* str) __attribute__((nonnull(1)));

// Guarantee a '\0' sits one byte past the last real character, WITHOUT
// touching str->size (str->size is not a "logical length excluding the
// NUL" convention anywhere else in this API, and this function keeps it
// that way). Grows exactly like String_append_char would if the String
// is already full (SSO->heap conversion, or a heap realloc) so the NUL
// always lands in real, owned memory rather than the SSO mode-flag byte.
// See TEMP_CSTR_READ below for the typical use case.
void String_ensure_null_term(String* str) __attribute__((nonnull(1)));


//  Modification

void String_append_char(String* str, char c) __attribute__((nonnull(1)));
void String_append_cstr(String* str, const char* cstr) __attribute__((nonnull(1, 2)));
void String_append_String(String* str, const String* other) __attribute__((nonnull(1, 2)));
// Append other then destroy it (nulls *other).
void String_append_String_move(String* str, String** other) __attribute__((nonnull(1, 2)));

char String_pop_char(String* str) __attribute__((nonnull(1)));

void String_insert_char(String* str, u64 i, char c) __attribute__((nonnull(1)));
void String_insert_cstr(String* str, u64 i, const char* cstr) __attribute__((nonnull(1, 3)));
void String_insert_String(String* str, u64 i, const String* other) __attribute__((nonnull(1, 3)));

void String_remove_char(String* str, u64 i) __attribute__((nonnull(1)));

// Remove chars in range [start, start + len)
void String_remove_range(String* str, u64 start, u64 len) __attribute__((nonnull(1)));

// Remove all chars (keep allocation).
__attribute__((nonnull(1))) static inline void String_clear(String* str)
{
    str->size = 0;
}


//  Access

__attribute__((nonnull(1))) static inline char String_char_at(const String* str, u64 i)
{
    CHECK_FATAL(i >= str->size, "index out of bounds");
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? (str)->stk : (str)->heap)[i];
}

__attribute__((nonnull(1))) static inline char String_char_at_unsafe(const String* str, u64 i)
{
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? (str)->stk : (str)->heap)[i];
}

__attribute__((nonnull(1))) static inline void String_set_char(String* str, u64 i, char c)
{
    CHECK_FATAL(i >= str->size, "index out of bounds");
    ((str->stk[STR_SSO_SIZE - 1] != '\0') ? str->stk : str->heap)[i] = c;
}


//  Comparison

// 0 == equal, <0 == str1 < str2, >0 == str1 > str2
int String_compare(const String* s1, const String* s2) __attribute__((nonnull(1, 2)));
__attribute__((nonnull(1, 2))) static inline b8 String_equals(const String* s1, const String* s2)
{
    return String_compare(s1, s2) == 0;
}
b8 String_equals_cstr(const String* str, const char* cstr) __attribute__((nonnull(1, 2)));


//  Search

// Returns index, or WC_NOT_FOUND if not found.
u64 String_find_char(const String* str, char c) __attribute__((nonnull(1)));
u64 String_find_cstr(const String* str, const char* substr) __attribute__((nonnull(1, 2)));

// Return a heap-allocated subString starting at `start` of `length` chars.
String* String_substr(const String* str, u64 start, u64 length) __attribute__((nonnull(1), warn_unused_result));


//  I/O

void String_print(const String* str) __attribute__((nonnull(1)));


//  Inline helpers

__attribute__((nonnull(1))) static inline u64 String_len(const String* str)
{
    return str->size;
}

__attribute__((nonnull(1))) static inline u64 String_capacity(const String* str)
{
    return str->capacity;
}

__attribute__((nonnull(1))) static inline b8 String_empty(const String* str)
{
    return str->size == 0;
}

__attribute__((nonnull(1))) static inline b8 String_is_sso(const String* str)
{
    return str->stk[STR_SSO_SIZE - 1] != '\0';
}

// Read-only pointer into the buffer. Unlike String_data_ptr, this never
// returns NULL for an empty String. It's meant to be used AFTER
// String_ensure_null_term, where index 0 is guaranteed to hold at least a '\0',
// even when size == 0. Calling this without a prior String_ensure_null_term on 
// a fresh/empty String reads uninitialised memory.
__attribute__((nonnull(1))) static inline const char* String_cstr_view(const String* str)
{
    return String_is_sso(str) ? str->stk : str->heap;
}

#endif /* WC_WC_STRING_H */

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
 *   wc_errno     Expected conditions: pop on empty, Arena full.
 *                Function returns NULL / 0 / void. wc_errno says why.
 *                Ignore it if you don't care. Check it if you do.
 *
 *
 * USAGE
 * -----
 *   // Check a single call:
 *   wc_errno = WC_OK;
 *   u8* p = Arena_alloc(Arena, size);
 *   if (!p && wc_errno == WC_ERR_FULL) { ... }
 *
 *   // Check a batch — wc_errno stays set if any call failed:
 *   wc_errno = WC_OK;
 *   float* a = (float*)Arena_alloc(Arena, 256);
 *   float* b = (float*)Arena_alloc(Arena, 256);
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
 *   Arena_alloc, Arena_alloc_aligned      WC_ERR_FULL    Arena exhausted
 *   GenVec_pop, GenVec_front, GenVec_back WC_ERR_EMPTY   vec is empty
 *   Queue_pop, Queue_peek, Queue_peek_ptr WC_ERR_EMPTY   Queue is empty
 *   Stack_pop, Stack_peek                 WC_ERR_EMPTY   Stack is empty
 */


typedef enum {
    WC_OK        = 0,
    WC_ERR_FULL,       // Arena exhausted / container at capacity
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
 *   wc_perror("Arena_alloc");  ->  "Arena_alloc: full"
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
 * GenVec is a value-based generic vector.
 * Elements are stored inline and managed via user-supplied
 * copy/move/destructor callbacks.
 *
 * This avoids pointer ownership ambiguity and improves cache locality.
 *
 * Callbacks are grouped into a shared GenVec_ops struct (vtable).
 * Define one static ops instance per type and share it across all
 * vectors of that type —  improves cache locality when many vectors of the same type exist.
 *
 * Example:
 *   static const GenVec_ops String_ops = { str_copy, str_move, str_del };
 *   GenVec* vec = GenVec_create(8, sizeof(String), &String_ops);
 *
 * For POD types (int, float, flat structs) pass NULL for ops:
 *   GenVec* vec = GenVec_create(8, sizeof(int), NULL);
 */


// GenVec growth settings

#ifndef GENVEC_GROWTH
#define GENVEC_GROWTH 1.5F // vec capacity multiplier
#endif


// generic vector container
typedef struct {
    u8* data; // pointer to generic data

    // Pointer to shared type-ops vtable (or NULL for POD types)
    const container_ops* ops;

    u64 size;      // Number of elements currently in vector
    u64 capacity;  // Total allocated capacity (in elements)
    u32 data_size; // Size of each element in bytes

    // Cache: 1 if ops==NULL (POD fast path)
    b8 is_pod;
} GenVec;

// 8 8 8 8 4 1 '3'  = 40 bytes
_Static_assert(sizeof(GenVec) == 40, "GenVec layout drifted from expected 40 bytes");


// Convenience: access ops callbacks safely
#define VEC_COPY_FN(vec) ((vec)->ops ? (vec)->ops->copy_fn : NULL)
#define VEC_MOVE_FN(vec) ((vec)->ops ? (vec)->ops->move_fn : NULL)
#define VEC_DEL_FN(vec)  ((vec)->ops ? (vec)->ops->del_fn : NULL)



// Memory Management
// ===========================

// Initialize vector with capacity n.
// ops: pointer to a shared GenVec_ops vtable, or NULL for POD types.
GenVec* GenVec_create(u64 n, u32 data_size, const container_ops* ops) __attribute__((warn_unused_result));

// Initialize vector on Stack (struct on Stack, data on heap).
void GenVec_create_stk(GenVec* vec, u64 n, u32 data_size, const container_ops* ops) __attribute__((nonnull(1)));

// Initialize vector of size n with all elements set to val.
GenVec* GenVec_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops) __attribute__((nonnull(2), warn_unused_result));

void GenVec_create_val_stk(GenVec* vec, u64 n, const u8* val, u32 data_size, const container_ops* ops)
    __attribute__((nonnull(1, 3)));

GenVec* GenVec_create_arr(u64 n, u32 data_size, const container_ops* ops, u8* arr) __attribute__((nonnull(4), warn_unused_result));

// Vector COMPLETELY on Stack (can't grow in size).
// You provide a Stack-allocated array which becomes the internal array.
// should only use if you need GenVec operations on C array
// WARNING: crashes when size == capacity and you try to push.
void GenVec_create_stk_arr(GenVec* vec, u64 n, u8* arr, u32 data_size, const container_ops* ops) __attribute__((nonnull(1, 3)));

// Destroy heap-allocated vector and clean up all elements.
void GenVec_destroy(GenVec* vec) __attribute__((nonnull(1)));

// Destroy Stack-allocated vector (cleans up data, but not vec itself).
void GenVec_destroy_stk(GenVec* vec) __attribute__((nonnull(1)));

// Remove all elements (calls del_fn on each), keep capacity.
void GenVec_clear(GenVec* vec) __attribute__((nonnull(1)));

// Remove all elements and free memory, shrink capacity to 0.
void GenVec_reset(GenVec* vec) __attribute__((nonnull(1)));

// Ensure vector has at least new_capacity space (never shrinks).
void GenVec_reserve(GenVec* vec, u64 new_capacity) __attribute__((nonnull(1)));

// Grow to new_capacity and fill new slots with val.
void GenVec_reserve_val(GenVec* vec, u64 new_capacity, const u8* val) __attribute__((nonnull(1, 3)));

// Shrink vector to its size (reallocates).
void GenVec_shrink_to_fit(GenVec* vec) __attribute__((nonnull(1)));



// Operations
// ===========================

// Append element to end (makes deep copy if copy_fn provided).
void GenVec_push(GenVec* vec, const u8* data) __attribute__((nonnull(1, 2)));

// Append element to end, transfer ownership (nulls original pointer).
void GenVec_push_move(GenVec* vec, u8** data) __attribute__((nonnull(1, 2)));

// Remove element from end. If popped is provided, copies element before deletion.
// Note: del_fn is called regardless to clean up owned resources.
void GenVec_pop(GenVec* vec, u8* popped) __attribute__((nonnull(1)));

// If order doesn't matter, O(1) deletion from middle
void GenVec_swap_pop(GenVec* vec, u64 i, u8* out) __attribute__((nonnull(1)));

// swap element at i with element at j
void GenVec_swap(GenVec* vec, u64 i, u64 j) __attribute__((nonnull(1)));

// Copy element at index i into out buffer.
void GenVec_get(const GenVec* vec, u64 i, u8* out) __attribute__((nonnull(1, 3)));

// Get pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
const u8* GenVec_get_ptr(const GenVec* vec, u64 i) __attribute__((nonnull(1)));

// Get MUTABLE pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
u8* GenVec_get_ptr_mut(GenVec* vec, u64 i) __attribute__((nonnull(1)));

// UNCHECKED variants — same as above but with the bounds CHECK_FATAL elided.
// Preconditions are NOT validated: caller must guarantee i < vec->size.
// Use on hot paths where the check is provably redundant (macros, internal loops).
const u8* GenVec_get_ptr_unsafe(const GenVec* vec, u64 i) __attribute__((nonnull(1)));

u8* GenVec_get_ptr_mut_unsafe(GenVec* vec, u64 i) __attribute__((nonnull(1)));

// Replace element at index i with data (cleans up old element).
void GenVec_replace(GenVec* vec, u64 i, const u8* data) __attribute__((nonnull(1, 3)));

// Replace element at index i, transfer ownership (cleans up old element).
void GenVec_replace_move(GenVec* vec, u64 i, u8** data) __attribute__((nonnull(1, 3)));

// Insert element at index i, shifting elements right.
void GenVec_insert(GenVec* vec, u64 i, const u8* data) __attribute__((nonnull(1, 3)));

// Insert element at index i with ownership transfer, shifting elements right.
void GenVec_insert_move(GenVec* vec, u64 i, u8** data) __attribute__((nonnull(1, 3)));

// Insert num_data elements from data array into vec at index i.
void GenVec_insert_multi(GenVec* vec, u64 i, const u8* data, u64 num_data) __attribute__((nonnull(1, 3)));

// Insert (move) num_data elements from data starting at index i.
void GenVec_insert_multi_move(GenVec* vec, u64 i, u8** data, u64 num_data) __attribute__((nonnull(1, 3)));

// Remove element at index i, optionally copy to out, shift elements left.
void GenVec_remove(GenVec* vec, u64 i, u8* out) __attribute__((nonnull(1)));

// Remove elements in range [start, start + len)
void GenVec_remove_range(GenVec* vec, u64 start, u64 len) __attribute__((nonnull(1)));

// Get pointer to first element.
const u8* GenVec_front(const GenVec* vec) __attribute__((nonnull(1)));

// Get pointer to last element.
const u8* GenVec_back(const GenVec* vec) __attribute__((nonnull(1)));

// Search
// ===========================

// if cmp_fn = NULL, then use memcmp
u64 GenVec_find(const GenVec* vec, u8* elm, compare_fn cmp_fn) __attribute__((nonnull(1, 2)));

GenVec* GenVec_subarr(const GenVec* vec, u64 start, u64 len) __attribute__((nonnull(1), warn_unused_result));


// Utility
// ===========================

// Print all elements using provided print function.
void GenVec_print(const GenVec* vec, print_fn fn) __attribute__((nonnull(1, 2)));

// Deep copy src vector into dest.
// REQUIRES: dest must be uninitialized (or already destroyed/reset) before calling.
// This does NOT clean up any existing dest->data / elements, it overwrites
// dest's fields directly. Calling this on an already-populated dest leaks
// its old buffer and skips del_fn on its old elements.
void GenVec_copy(GenVec* dest, const GenVec* src) __attribute__((nonnull(1, 2)));

// Transfer ownership from src to dest.
// Note: src must be heap-allocated.
void GenVec_move(GenVec* dest, GenVec** src) __attribute__((nonnull(1, 2)));


// Get number of elements in vector.
static inline __attribute__((nonnull(1))) u64 GenVec_size(const GenVec* vec)
{
    return vec->size;
}

// Get total capacity of vector.
static inline __attribute__((nonnull(1))) u64 GenVec_capacity(const GenVec* vec)
{
    return vec->capacity;
}

// Check if vector is empty
static inline __attribute__((nonnull(1))) b8 GenVec_empty(const GenVec* vec)
{
    return vec->size == 0;
}

#endif /* WC_GEN_VECTOR_H */

/* ===== wc_helpers.h ===== */
#ifndef WC_WC_HELPERS_H
#define WC_WC_HELPERS_H

/*
 * wc_helpers.h — Generic container callbacks and typed macros for WCtoolkit
 * =======================================================================
 *
 * ALL functions are static inline to avoid multiple-definition linker
 * errors when this header is included in more than one translation unit.
 *
 * SECTIONS
 * --------
 *   1.  String by value  (sizeof(String) per slot)
 *   2.  String by pointer (sizeof(String*) per slot)
 *   3.  GenVec by value  (sizeof(GenVec) per slot)  — vec of vecs
 *   4.  GenVec by pointer (sizeof(GenVec*) per slot)
 *   5.  Shared ops instances (define once, reference everywhere)
 *
 * RULES FOR WRITING copy/move/del CALLBACKS
 * ------------------------------------------
 *
 * BY VALUE  (slot holds the full struct, sizeof(T) bytes)
 *   copy_fn(u8* dest, const u8* src)
 *     dest  — raw bytes of the slot (uninitialised — treat as blank)
 *     src   — raw bytes of the source element
 *     job   — deep-copy all owned resources into dest; DO NOT free dest first.
 *             If delegating to a function that calls destroy internally (e.g.
 *             String_copy), you MUST first init dest to a valid empty state.
 *
 *   move_fn(u8* dest, u8** src)
 *     dest  — raw bytes of the slot (uninitialised)
 *     *src  — heap pointer to the source T
 *     job   — memcpy the struct fields, free(*src), *src = NULL
 *             The data ptr moves; only the container struct is freed.
 *
 *   del_fn(u8* elm)
 *     elm   — raw bytes of the slot
 *     job   — free owned resources (e.g. data buffer) but NOT elm itself
 *             → call String_destroy_stk / GenVec_destroy_stk etc.
 *
 * BY POINTER  (slot holds T*, sizeof(T*) = 8 bytes)
 *   copy_fn(u8* dest, const u8* src)
 *     *(T**)src  is the source pointer
 *     *(T**)dest must be set to a newly heap-allocated deep copy
 *
 *   move_fn(u8* dest, u8** src)
 *     *(T**)dest = *(T**)src;  *src = NULL;
 *
 *   del_fn(u8* elm)
 *     *(T**)elm  is the pointer stored in the slot
 *     job — fully destroy the heap object
 *           → call String_destroy / GenVec_destroy etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(String) == sizeof(GenVec), "String and GenVec sizes must match for value-storage helpers");


/* ══════════════════════════════════════════════════════════════════════════
 * 1.  STRING BY VALUE
 *
 * String is SSO-based (no data_size / data fields).
 * str_copy  — delegates to String_copy()  (handles SSO vs heap correctly)
 * str_move  — memcpy the struct shell, free the source container
 * str_del   — delegates to String_destroy_stk() (frees heap buf if any)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void str_copy(u8* dest, const u8* src)
{
    String*       d = (String*)dest;
    const String* s = (const String*)src;
    memcpy(d, s, sizeof(String));

    if (String_is_sso(s)) {
        return; // str stored inline, we have everything
    }

    // src owns resources, copy them
    d->heap = malloc(s->capacity);
    CHECK_FATAL(!d->heap, "malloc failed");
    memcpy(d->heap, s->heap, s->capacity);
}

static inline void str_move(u8* dest, u8** src)
{
    // *src is a heap-allocated String* — move its contents into the slot,
    // then free the shell. Works for both SSO (copies stk[]) and heap mode.
    memcpy(dest, *src, sizeof(String));
    free(*src);
    *src = NULL;
}

static inline void str_del(u8* elm)
{
    String_destroy_stk((String*)elm);   // free data buffer, NOT the slot
}

static inline void str_print(const u8* elm)
{
    String_print((const String*)elm);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 2.  STRING BY POINTER
 *
 * str_copy_ptr — delegates to String_from_String() for a full heap copy
 * str_move_ptr — pointer swap, nulls source
 * str_del_ptr  — delegates to String_destroy() (frees buf + struct)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void str_copy_ptr(u8* dest, const u8* src)
{
    *(String**)dest = String_from_String(*(const String**)src);
}

static inline void str_move_ptr(u8* dest, u8** src)
{
    *(String**)dest = *(String**)src;
    *src            = NULL;
}

static inline void str_del_ptr(u8* elm)
{
    String_destroy(*(String**)elm);
}

static inline void str_print_ptr(const u8* elm)
{
    String_print(*(const String**)elm);
}

static inline int str_cmp(const u8* a, const u8* b, u64 size)
{
    (void)size;
    return String_compare((const String*)a, (const String*)b);
}

static inline int str_cmp_ptr(const u8* a, const u8* b, u64 size)
{
    (void)size;
    return String_compare(*(const String**)a, *(const String**)b);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 3.  GENVEC BY VALUE  (vec of vecs)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy(u8* dest, const u8* src)
{
    GenVec_copy((GenVec*)dest, (const GenVec*)src);
}

static inline void vec_move(u8* dest, u8** src)
{
    memcpy(dest, *src, sizeof(GenVec));  // transfer all fields (incl. data ptr and ops ptr)
    free(*src);                          // free container struct only
    *src = NULL;
}

static inline void vec_del(u8* elm)
{
    GenVec_destroy_stk((GenVec*)elm);    // free data buffer, NOT the slot
}

static inline void vec_print_int(const u8* elm)
{
    const GenVec* v = (const GenVec*)elm;
    printf("[");
    for (u64 i = 0; i < v->size; i++) {
        printf("%d", *(int*)GenVec_get_ptr(v, i));
        if (i + 1 < v->size) { printf(", "); }
    }
    printf("]");
}


/* ══════════════════════════════════════════════════════════════════════════
 * 4.  GENVEC BY POINTER  (slot holds GenVec*)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy_ptr(u8* dest, const u8* src)
{
    GenVec* d = malloc(sizeof(GenVec));
    CHECK_FATAL(!d, "malloc failed");
    GenVec_copy(d, *(const GenVec**)src);
    *(GenVec**)dest = d;
}

static inline void vec_move_ptr(u8* dest, u8** src)
{
    *(GenVec**)dest = *(GenVec**)src;
    *src            = NULL;
}

static inline void vec_del_ptr(u8* elm)
{
    GenVec_destroy(*(GenVec**)elm);
}

static inline void vec_print_int_ptr(const u8* elm)
{
    vec_print_int((const u8*)*(const GenVec**)elm);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 5.  SHARED OPS INSTANCES
 *
 * Define once here as static const. Reference by address wherever needed.
 * No per-instance overhead — all vectors of the same type share the pointer.
 *
 * Usage:
 *   GenVec* v = GenVec_create(8, sizeof(String), &wc_str_ops);
 *   HashMap* m = HashMap_create(..., &wc_str_ops, &wc_str_ops);
 * ══════════════════════════════════════════════════════════════════════════ */

static const container_ops wc_str_ops     = { str_copy,     str_move,     str_del     };
static const container_ops wc_str_ptr_ops = { str_copy_ptr, str_move_ptr, str_del_ptr };
static const container_ops wc_vec_ops     = { vec_copy,     vec_move,     vec_del     };
static const container_ops wc_vec_ptr_ops = { vec_copy_ptr, vec_move_ptr, vec_del_ptr };

#endif /* WC_WC_HELPERS_H */

#ifdef WC_IMPLEMENTATION

/* ===== wc_string.c ===== */
#ifndef WC_WC_STRING_IMPL
#define WC_WC_STRING_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//  Internal macros

#define GET_STR_PTR(s, i)  (GET_STR(s) + i)
#define GET_STR_CHAR(s, i) (GET_STR(s)[i])
#define STR_REMAINING(s)   ((s)->capacity - (s)->size)
#define IS_SSO(s)          (s->stk[STR_SSO_SIZE - 1] != '\0')
#define GET_STR(s)         (IS_SSO(s) ? (s)->stk : (s)->heap)

// Grow if full.
#define MAYBE_GROW_STR(s)                        \
    do {                                         \
        if ((s)->size >= (s)->capacity) {        \
            if (IS_SSO(s)) {                     \
                s->stk[STR_SSO_SIZE - 1] = '\0'; \
                stk_to_heap(s);                  \
            } else {                             \
                String_grow(s);                  \
            }                                    \
        }                                        \
    } while (0)



//  Private helpers

static inline u64  cstr_len(const char* cstr);
static inline void stk_to_heap(String* s);
static inline void heap_to_stk(String* s);
static inline void String_grow(String* s);
static inline void ensure_capacity(String* s, u64 needed);



//  Construction / Destruction

String* String_create(void)
{
    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    s->size                  = 0;
    s->capacity              = STR_SSO_SIZE - 1; // 0-22 bit is usable
    s->stk[STR_SSO_SIZE - 1] = 1;                // when last bit (23) is '\0' (NULL) then we have moved to the heap

    return s;
}

String* String_from_cstr(const char* cstr)
{
    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    String_create_stk(s, cstr);
    return s;
}

String* String_from_String(const String* other)
{
    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    s->size                  = 0;
    s->capacity              = STR_SSO_SIZE - 1;
    s->stk[STR_SSO_SIZE - 1] = 1; // mark SSO mode before GET_STR() is used below

    if (other->size > 0) {
        ensure_capacity(s, other->size);
        memcpy(GET_STR(s), GET_STR(other), other->size);
        s->size = other->size;
    }

    return s;
}

void String_create_stk(String* s, const char* cstr)
{
    s->size                  = 0;
    s->stk[STR_SSO_SIZE - 1] = 1;                // mark SSO mode
    s->capacity              = STR_SSO_SIZE - 1; // last byte reserved for the SSO flag

    if (!cstr) {
        return;
    }

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, len);
    memcpy(GET_STR(s), cstr, len);
    s->size = len;
}

void String_destroy(String* s)
{
    String_destroy_stk(s);
    free(s);
}

void String_destroy_stk(String* s)
{
    if (!IS_SSO(s)) {
        free(s->heap);
    }

    s->size                  = 0;
    s->stk[STR_SSO_SIZE - 1] = 1;                // mark SSO mode; NOT preserved from heap mode
    s->capacity              = STR_SSO_SIZE - 1; // leave in valid, reusable SSO state
}

void String_move(String* dest, String** src)
{
    CHECK_FATAL(!*src, "*src is null");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    String_destroy_stk(dest);
    memcpy(dest, *src, sizeof(String));

    // Zero out src so its destructor is harmless, then free the struct
    (*src)->size     = 0;
    (*src)->capacity = STR_SSO_SIZE - 1;
    free(*src);
    *src = NULL;
}

void String_copy(String* dest, const String* src)
{
    if (src == dest) {
        return;
    }

    // dest is documented as "re-initialised": callers may pass raw/uninitialised
    // memory , so we must not read dest's old state before it has ever been initialised.
    dest->size                  = 0;
    dest->capacity              = STR_SSO_SIZE - 1;
    dest->stk[STR_SSO_SIZE - 1] = 1; // mark SSO mode before GET_STR() is used below

    if (src->size > 0) {
        ensure_capacity(dest, src->size);
        memcpy(GET_STR(dest), GET_STR(src), src->size);
        dest->size = src->size;
    }
}


//  Capacity

void String_reserve(String* s, u64 new_cap)
{
    if (new_cap <= s->capacity) {
        return;
    }
    ensure_capacity(s, new_cap);
}

void String_reserve_char(String* s, u64 new_cap, char c)
{
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

void String_shrink_to_fit(String* s)
{
    if (IS_SSO(s)) {
        return;
    } // already optimal

    if (s->size == 0) {
        free(s->heap);
        s->heap     = NULL;
        s->capacity = STR_SSO_SIZE - 1;
        return;
    }

    if (s->size <= STR_SSO_SIZE - 1) {
        // Bring back to SSO (only place this happens)
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

char* String_to_cstr(const String* s)
{
    char* out = malloc(s->size + 1);
    CHECK_FATAL(!out, "malloc failed");

    if (s->size > 0) {
        memcpy(out, GET_STR(s), s->size);
    }
    out[s->size] = '\0';

    return out;
}

void String_to_cstr_buf(const String* str, char* buff, u64 n)
{
    CHECK_FATAL(n < str->size + 1, "buffer not enough");

    if (str->size > 0) {
        memcpy(buff, GET_STR(str), str->size);
    }
    buff[str->size] = '\0';
}

char* String_data_ptr(const String* s)
{
    if (s->size == 0) {
        return NULL;
    }
    // Cast away const intentionally: caller may mutate via this pointer.
    return (char*)(IS_SSO(s) ? s->stk : s->heap);
}

// Same growth path as String_append_char, minus the size++: writes '\0'
// at index s->size and leaves size untouched. Safe against the SSO
// mode-flag byte because MAYBE_GROW_STR converts to heap (or reallocs
// the heap buffer) whenever size == capacity, before we ever write —
// so the write always lands one past the last real char, never on the
// flag byte at stk[STR_SSO_SIZE - 1].
void String_ensure_null_term(String* s)
{
    MAYBE_GROW_STR(s);
    GET_STR_CHAR(s, s->size) = '\0';
}


//  Modification

void String_append_char(String* s, char c)
{
    MAYBE_GROW_STR(s);
    GET_STR_CHAR(s, s->size++) = c;
}

void String_append_cstr(String* s, const char* cstr)
{
    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, s->size + len);
    memcpy(GET_STR(s) + s->size, cstr, len);
    s->size += len;
}

void String_append_String(String* s, const String* other)
{
    if (other->size == 0) {
        return;
    }

    ensure_capacity(s, s->size + other->size);
    memcpy(GET_STR(s) + s->size, GET_STR(other), other->size);
    s->size += other->size;
}

void String_append_String_move(String* s, String** other)
{
    CHECK_FATAL(!*other, "*other is null");

    if ((*other)->size > 0) {
        String_append_String(s, *other);
    }

    String_destroy(*other);
    *other = NULL;
}

char String_pop_char(String* s)
{
    CHECK_FATAL(s->size == 0, "cannot pop from empty String");

    char c = GET_STR_CHAR(s, --s->size);
    return c;
}

void String_insert_char(String* s, u64 i, char c)
{
    CHECK_FATAL(i > s->size, "index out of bounds");

    MAYBE_GROW_STR(s);

    char* buf = GET_STR(s);
    // Shift right.
    for (u64 j = s->size; j > i; j--) {
        buf[j] = buf[j - 1];
    }
    buf[i] = c;
    s->size++;
}

void String_insert_cstr(String* s, u64 i, const char* cstr)
{
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
    memcpy(buf + i, cstr, len);
    s->size += len;
}

void String_insert_String(String* s, u64 i, const String* other)
{
    CHECK_FATAL(i > s->size, "index out of bounds");

    if (other->size == 0) {
        return;
    }

    CHECK_WARN_RET(s == other, , "can't insert aliasing(same) Strings");

    u64 len = other->size;
    ensure_capacity(s, s->size + len);

    char* buf = GET_STR(s);
    for (u64 j = s->size; j > i; j--) {
        buf[j + len - 1] = buf[j - 1];
    }
    memcpy(buf + i, GET_STR(other), len);
    s->size += len;
}

void String_remove_char(String* s, u64 i)
{
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

void String_remove_range(String* s, u64 start, u64 len)
{
    CHECK_FATAL(start >= s->size, "start out of bounds");

    if (len == 0) {
        return;
    }

    if (start + len >= s->size) {
        len = s->size - start;
    }

    memmove(GET_STR_PTR(s, start), GET_STR_PTR(s, start + len), s->size - start - len);

    s->size -= len;
}


//  Access


//  Comparison

int String_compare(const String* s1, const String* s2)
{
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

b8 String_equals_cstr(const String* s, const char* cstr)
{
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

u64 String_find_char(const String* s, char c)
{
    if (s->size == 0) {
        return WC_NOT_FOUND;
    }
    const char* buf = GET_STR(s);
    const char* p   = memchr(buf, (unsigned char)c, s->size);
    return p ? (u64)(p - buf) : WC_NOT_FOUND;
}

u64 String_find_cstr(const String* s, const char* substr)
{
    u64 len = cstr_len(substr);
    if (len == 0) {
        return 0;
    }
    if (len > s->size) {
        return WC_NOT_FOUND;
    }

    const char* buf = GET_STR(s);
    for (u64 i = 0; i <= s->size - len; i++) {
        if (memcmp(buf + i, substr, len) == 0) {
            return i;
        }
    }
    return WC_NOT_FOUND;
}

String* String_substr(const String* s, u64 start, u64 length)
{
    CHECK_FATAL(start >= s->size, "start out of bounds");

    if (start + length > s->size) {
        length = s->size - start;
    }

    String* result = String_create();

    if (length > 0) {
        ensure_capacity(result, length);
        memcpy(GET_STR(result), GET_STR(s) + start, length);
        result->size = length;
    }

    return result;
}


//  I/O

void String_print(const String* s)
{
    putchar('"');
    const char* buf = GET_STR(s);
    for (u64 i = 0; i < s->size; i++) {
        putchar(buf[i]);
    }
    putchar('"');
}


static inline u64 cstr_len(const char* cstr)
{
    return (u64)strlen(cstr);
}


// Promote SSO buffer to heap allocation.
static inline void stk_to_heap(String* s)
{
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);

    char* new_data = malloc(new_cap);
    CHECK_FATAL(!new_data, "malloc failed");

    memcpy(new_data, s->stk, s->size);

    s->heap     = new_data;
    s->capacity = new_cap;
}

static inline void heap_to_stk(String* s)
{
    // save the ptr as memcpy on stk will overwrite
    char* heap = s->heap;
    memcpy(s->stk, heap, s->size);
    free(heap);
    s->stk[STR_SSO_SIZE - 1] = 1; // mark SSO mode; NOT preserved from heap mode
    s->capacity              = STR_SSO_SIZE - 1;
}

static inline void String_grow(String* s)
{
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);

    char* new_data = realloc(s->heap, new_cap);
    CHECK_FATAL(!new_data, "realloc failed");

    s->heap     = new_data;
    s->capacity = new_cap;
}

static inline void ensure_capacity(String* s, u64 needed)
{
    if (needed <= s->capacity) {
        return;
    }

    // Grow by at least STRING_GROWTH factor
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);
    if (new_cap < needed) {
        new_cap = needed;
    }

    // currently in sso but sso_cap is not enough
    if (IS_SSO(s)) {
        s->stk[STR_SSO_SIZE - 1] = '\0';
        char* new_data           = malloc(new_cap);
        CHECK_FATAL(!new_data, "malloc failed");
        memcpy(new_data, s->stk, s->size);
        s->heap     = new_data;
        s->capacity = new_cap;
    } else {
        char* new_data = realloc(s->heap, new_cap);
        CHECK_FATAL(!new_data, "realloc failed");
        s->heap     = new_data;
        s->capacity = new_cap;
    }
}

#endif /* WC_WC_STRING_IMPL */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define GENVEC_MIN_CAPACITY 4


// MACROS

// get ptr to elm at index i
#define GET_PTR(vec, i) ((vec->data) + ((u64)(i) * ((vec)->data_size)))
// get total size in bytes for i elements
#define GET_SCALED(vec, i) ((i) * ((vec)->data_size))

#define MAYBE_GROW(vec)                                 \
    do {                                                \
        if (!vec->data || vec->size >= vec->capacity) { \
            GenVec_grow(vec);                           \
        }                                               \
    } while (0)


// ops accessors (safe when ops is NULL)
#define COPY_FN(vec) VEC_COPY_FN(vec)
#define MOVE_FN(vec) VEC_MOVE_FN(vec)
#define DEL_FN(vec)  VEC_DEL_FN(vec)

#define IS_POD(vec)   (vec->is_pod)    // cached at init (6-M) — consume this everywhere
#define CALC_POD(ops) ((ops) == NULL)  // derive from ops — ONLY valid at init time


// private functions

static void GenVec_grow(GenVec* vec);


// API Implementation

GenVec* GenVec_create(u64 n, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    GenVec* vec = malloc(sizeof(GenVec));
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
    vec->is_pod    = CALC_POD(ops);

    return vec;
}


void GenVec_create_stk(GenVec* vec, u64 n, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    vec->data = (n > 0) ? malloc(data_size * n) : NULL;
    CHECK_FATAL(n > 0 && !vec->data, "data init failed");

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->ops       = ops;
    vec->is_pod    = CALC_POD(ops);
}


GenVec* GenVec_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    GenVec* vec = GenVec_create(n, data_size, ops);

    vec->size = n; // capacity set to n in GenVec_create

    if (vec->is_pod) {
        for (u64 i = 0; i < n; i++) {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            for (u64 i = 0; i < n; i++) {
                copy(GET_PTR(vec, i), val);
            }
        } else {
            for (u64 i = 0; i < n; i++) {
                memcpy(GET_PTR(vec, i), val, data_size);
            }
        }
    }

    return vec;
}


void GenVec_create_val_stk(GenVec* vec, u64 n, const u8* val, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    GenVec_create_stk(vec, n, data_size, ops);

    vec->size = n;

    if (vec->is_pod) {
        for (u64 i = 0; i < n; i++) {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            for (u64 i = 0; i < n; i++) {
                copy(GET_PTR(vec, i), val);
            }
        } else {
            for (u64 i = 0; i < n; i++) {
                memcpy(GET_PTR(vec, i), val, data_size);
            }
        }
    }
}


GenVec* GenVec_create_arr(u64 n, u32 data_size, const container_ops* ops, u8* arr)
{
    GenVec* v = GenVec_create(n, data_size, ops);

    memcpy(v->data, arr, n * data_size);
    v->size = n;

    return v;
}


void GenVec_create_stk_arr(GenVec* vec, u64 n, u8* arr, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(n == 0 || data_size == 0, "size/data_size of arr can't be 0");

    vec->data      = arr;
    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->ops       = ops;
    vec->is_pod    = CALC_POD(ops);
}


void GenVec_destroy(GenVec* vec)
{
    GenVec_destroy_stk(vec);
    free(vec);
}


void GenVec_destroy_stk(GenVec* vec)
{
    if (!vec->data) {
        return;
    }

    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            for (u64 i = 0; i < vec->size; i++) {
                del(GET_PTR(vec, i));
            }
        }
    }

    free(vec->data);
    vec->data = NULL;
}


void GenVec_clear(GenVec* vec)
{
    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            for (u64 i = 0; i < vec->size; i++) {
                del(GET_PTR(vec, i));
            }
        }
    }

    vec->size = 0;
}


void GenVec_reset(GenVec* vec)
{
    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            for (u64 i = 0; i < vec->size; i++) {
                del(GET_PTR(vec, i));
            }
        }
    }

    free(vec->data);
    vec->data     = NULL;
    vec->size     = 0;
    vec->capacity = 0;
}


void GenVec_reserve(GenVec* vec, u64 new_capacity)
{
    if (new_capacity <= vec->capacity) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, new_capacity));
    CHECK_FATAL(!new_data, "realloc failed");

    vec->data     = new_data;
    vec->capacity = new_capacity;
}


void GenVec_reserve_val(GenVec* vec, u64 new_capacity, const u8* val)
{
    CHECK_FATAL(new_capacity < vec->size, "new_capacity must be >= current size");

    GenVec_reserve(vec, new_capacity);

    if (vec->is_pod) {
        for (u64 i = vec->size; i < new_capacity; i++) {
            memcpy(GET_PTR(vec, i), val, vec->data_size);
        }
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            for (u64 i = vec->size; i < new_capacity; i++) {
                copy(GET_PTR(vec, i), val);
            }
        } else {
            for (u64 i = vec->size; i < new_capacity; i++) {
                memcpy(GET_PTR(vec, i), val, vec->data_size);
            }
        }
    }
    vec->size = new_capacity;
}


void GenVec_shrink_to_fit(GenVec* vec)
{
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


void GenVec_push(GenVec* vec, const u8* data)
{
    MAYBE_GROW(vec);

    if (vec->is_pod) {
        memcpy(GET_PTR(vec, vec->size), data, vec->data_size);
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            copy(GET_PTR(vec, vec->size), data);
        } else {
            memcpy(GET_PTR(vec, vec->size), data, vec->data_size);
        }
    }

    vec->size++;
}


void GenVec_push_move(GenVec* vec, u8** data)
{
    CHECK_FATAL(!*data, "*data is null");

    MAYBE_GROW(vec);

    if (vec->is_pod) {
        memcpy(GET_PTR(vec, vec->size), *data, vec->data_size);
        *data = NULL;
    } else {
        move_fn move = vec->ops->move_fn;
        if (move) {
            move(GET_PTR(vec, vec->size), data);
        } else {
            memcpy(GET_PTR(vec, vec->size), *data, vec->data_size);
            *data = NULL;
        }
    }

    vec->size++;
}


void GenVec_pop(GenVec* vec, u8* popped)
{
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, );

    u8* last_elm = GET_PTR(vec, vec->size - 1);

    if (popped) {
        if (vec->is_pod) {
            memcpy(popped, last_elm, vec->data_size);
        } else {
            copy_fn copy = vec->ops->copy_fn;
            if (copy) {
                copy(popped, last_elm);
            } else {
                memcpy(popped, last_elm, vec->data_size);
            }
        }
    }

    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            del(last_elm);
        }
    }

    vec->size--;
}

void GenVec_swap_pop(GenVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (out) {
        if (vec->is_pod) {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        } else {
            copy_fn copy = vec->ops->copy_fn;
            if (copy) {
                copy(out, GET_PTR(vec, i));
            } else {
                memcpy(out, GET_PTR(vec, i), vec->data_size);
            }
        }
    }

    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            del(GET_PTR(vec, i));
        }
    }

    // swap the last container with the removed one
    // if owns memory elsewhere, those are still valid, only container location changes
    memcpy(GET_PTR(vec, i), GET_PTR(vec, vec->size - 1), vec->data_size);
    vec->size--;
}

void GenVec_swap(GenVec* vec, u64 i, u64 j)
{
    CHECK_FATAL(i >= vec->size || j >= vec->size, "index out of bounds");

    if (i == j) {
        return;
    }

    // we need one empty container as temp space for swap
    MAYBE_GROW(vec);

    // shallow copy of the jth container to temp space
    memcpy(GET_PTR(vec, vec->size), GET_PTR(vec, j), vec->data_size);
    // shallow copy into jth container
    memcpy(GET_PTR(vec, j), GET_PTR(vec, i), vec->data_size);
    // shallow copy from temp space into ith container
    memcpy(GET_PTR(vec, i), GET_PTR(vec, vec->size), vec->data_size);

    // temp container will be over written on next push
}


void GenVec_get(const GenVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (vec->is_pod) {
        memcpy(out, GET_PTR(vec, i), vec->data_size);
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            copy(out, GET_PTR(vec, i));
        } else {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        }
    }
}


const u8* GenVec_get_ptr(const GenVec* vec, u64 i)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    return GET_PTR(vec, i);
}


u8* GenVec_get_ptr_mut(GenVec* vec, u64 i)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    return GET_PTR(vec, i);
}


const u8* GenVec_get_ptr_unsafe(const GenVec* vec, u64 i)
{
    // Preconditions NOT validated — caller guarantees i < vec->size.
    return GET_PTR(vec, i);
}


u8* GenVec_get_ptr_mut_unsafe(GenVec* vec, u64 i)
{
    // Preconditions NOT validated — caller guarantees i < vec->size.
    return GET_PTR(vec, i);
}


void GenVec_replace(GenVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    u8* to_replace = GET_PTR(vec, i);

    if (vec->is_pod) {
        memcpy(to_replace, data, vec->data_size);
    } else {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            del(to_replace);
        }
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            copy(to_replace, data);
        } else {
            memcpy(to_replace, data, vec->data_size);
        }
    }
}


void GenVec_replace_move(GenVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(i >= vec->size || !*data, "index out of bounds / need a valid *data variable");

    u8* to_replace = GET_PTR(vec, i);

    if (vec->is_pod) {
        memcpy(to_replace, *data, vec->data_size);
        *data = NULL;
    } else {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            del(to_replace);
        }
        move_fn move = vec->ops->move_fn;
        if (move) {
            move(to_replace, data);
        } else {
            memcpy(to_replace, *data, vec->data_size);
            *data = NULL;
        }
    }
}


void GenVec_insert(GenVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(i > vec->size, "index out of bounds");

    u64 elements_to_shift = vec->size - i;

    MAYBE_GROW(vec);

    u8* src  = GET_PTR(vec, i);
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift));

    if (vec->is_pod) {
        memcpy(src, data, vec->data_size);
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            copy(src, data);
        } else {
            memcpy(src, data, vec->data_size);
        }
    }

    vec->size++;
}


void GenVec_insert_move(GenVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(!*data || i > vec->size, "*data is null / index out of bounds");

    u64 elements_to_shift = vec->size - i;

    MAYBE_GROW(vec);

    u8* src  = GET_PTR(vec, i);
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift));

    if (vec->is_pod) {
        memcpy(src, *data, vec->data_size);
        *data = NULL;
    } else {
        move_fn move = vec->ops->move_fn;
        if (move) {
            move(src, data);
        } else {
            memcpy(src, *data, vec->data_size);
            *data = NULL;
        }
    }

    vec->size++;
}


void GenVec_insert_multi(GenVec* vec, u64 i, const u8* data, u64 num_data)
{
    CHECK_FATAL(num_data == 0 || i > vec->size, "num_data can't be 0 / index out of bounds");

    u64 elements_to_shift = vec->size - i;

    vec->size += num_data;
    GenVec_reserve(vec, vec->size);

    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i + num_data);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
    }

    if (vec->is_pod) {
        memcpy(src, data, GET_SCALED(vec, num_data));
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            for (u64 j = 0; j < num_data; j++) {
                copy(GET_PTR(vec, j + i), data + (size_t)(j * vec->data_size));
            }
        } else {
            memcpy(src, data, GET_SCALED(vec, num_data));
        }
    }
}


void GenVec_insert_multi_move(GenVec* vec, u64 i, u8** data, u64 num_data)
{
    CHECK_FATAL(!*data || num_data == 0 || i > vec->size, "*data is null / num_data can't be 0 / index out of bounds");

    u64 elements_to_shift = vec->size - i;

    vec->size += num_data;
    GenVec_reserve(vec, vec->size);

    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i + num_data);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
    }

    if (vec->is_pod) {
        memcpy(src, *data, GET_SCALED(vec, num_data));
    } else {
        move_fn move = vec->ops->move_fn;
        if (move) {
            for (u64 j = 0; j < num_data; j++) {
                u8* elm_src = *data + (size_t)(j * vec->data_size);
                move(GET_PTR(vec, j + i), &elm_src);
            }
        } else {
            memcpy(src, *data, GET_SCALED(vec, num_data));
        }
    }

    *data = NULL;
}


void GenVec_remove(GenVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (out) {
        if (vec->is_pod) {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        } else {
            copy_fn copy = vec->ops->copy_fn;
            if (copy) {
                copy(out, GET_PTR(vec, i));
            } else {
                memcpy(out, GET_PTR(vec, i), vec->data_size);
            }
        }
    }

    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            del(GET_PTR(vec, i));
        }
    }

    u64 elements_to_shift = vec->size - i - 1;
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i);
        u8* src  = GET_PTR(vec, i + 1);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
    }

    vec->size--;
}


/*
    0 1 2 3 4 5, (1, 3) -> [1, 4)
    start = 1
    len = 3
    end = 1 + 3 - 1 = 3
*/
void GenVec_remove_range(GenVec* vec, u64 start, u64 len)
{
    if (len == 0) {
        return;
    }
    CHECK_FATAL(start >= vec->size, "start out of range");

    if (start + len >= vec->size) {
        len = vec->size - start;
    }

    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            for (u64 i = 0; i < len; i++) {
                del(GET_PTR(vec, start + i));
            }
        }
    }

    u8* dest = GET_PTR(vec, start);
    u8* src  = GET_PTR(vec, start + len);
    memmove(dest, src, GET_SCALED(vec, vec->size - start - len));   // TODO: is this right

    vec->size -= len;
}


const u8* GenVec_front(const GenVec* vec)
{
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, NULL);
    return GET_PTR(vec, 0);
}


const u8* GenVec_back(const GenVec* vec)
{
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, NULL);
    return GET_PTR(vec, vec->size - 1);
}


u64 GenVec_find(const GenVec* vec, u8* elm, compare_fn cmp_fn)
{
    for (u64 i = 0; i < vec->size; i++) {
        if (cmp_fn) {
            if (cmp_fn(GET_PTR(vec, i), elm, vec->data_size) == 0) {
                return i;
            }
        } else {
            if (memcmp(GET_PTR(vec, i), elm, vec->data_size) == 0) {
                return i;
            }
        }
    }

    return WC_NOT_FOUND;
}


GenVec* GenVec_subarr(const GenVec* vec, u64 start, u64 len)
{
    CHECK_FATAL(start >= vec->size, "out of bounds");

    if (start + len >= vec->size) {
        len = vec->size - start;
    }

    GenVec* v = GenVec_create(len, vec->data_size, vec->ops);

    if (len > 0) {
        if (vec->is_pod) {
            memcpy(GET_PTR(v, 0), GET_PTR(vec, start), len * vec->data_size);
        } else {
            copy_fn copy = vec->ops->copy_fn;
            if (copy) {
                for (u64 i = 0; i < len; i++) {
                    copy(GET_PTR(v, i), GET_PTR(vec, i + start));
                }
            } else {
                memcpy(GET_PTR(v, 0), GET_PTR(vec, start), len * vec->data_size);
            }
        }

        v->size = len;
    }

    return v;
}


void GenVec_print(const GenVec* vec, print_fn fn)
{
    printf("[ ");
    for (u64 i = 0; i < vec->size; i++) {
        fn(GET_PTR(vec, i));
        putchar(' ');
    }
    putchar(']');
}


void GenVec_copy(GenVec* dest, const GenVec* src)
{
    if (dest == src) {
        return;
    }

    // Copy all fields (including ops pointer)
    memcpy(dest, src, sizeof(GenVec));

    dest->data = malloc(GET_SCALED(src, src->capacity));
    CHECK_FATAL(!dest->data, "dest data malloc failed");

    if (src->is_pod) {
        memcpy(dest->data, src->data, GET_SCALED(src, src->size));
    } else {
        copy_fn copy = src->ops->copy_fn;
        if (copy) {
            for (u64 i = 0; i < src->size; i++) {
                copy(GET_PTR(dest, i), GET_PTR(src, i));
            }
        } else {
            memcpy(dest->data, src->data, GET_SCALED(src, src->size));
        }
    }
}


void GenVec_move(GenVec* dest, GenVec** src)
{
    CHECK_FATAL(!*src, "*src is null");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    memcpy(dest, *src, sizeof(GenVec));

    (*src)->data = NULL;
    free(*src);
    *src = NULL;
}


static void GenVec_grow(GenVec* vec)
{
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

#endif /* WC_GEN_VECTOR_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_WC_HELPERS_SINGLE_H */
