#ifndef WC_WC_MACROS_SINGLE_H
#define WC_WC_MACROS_SINGLE_H

/*
 * wc_macros_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "wc_macros_single.h"
 *
 * All other files just:
 *     #include "wc_macros_single.h"
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
// Best default for HashMaps: fast, excellent avalanche, low collision rate.
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

__attribute__((unused)) static u64 wyhash_str(const u8* key, u64 size)
{
    (void)size;
    String* str = (String*)key;
    return wyhash((const u8*)String_data_ptr(str), String_len(str));
}

__attribute__((unused)) static u64 wyhash_str_ptr(const u8* key, u64 size)
{
    (void)size;
    String* str = *(String**)key;
    return wyhash((const u8*)String_data_ptr(str), String_len(str));
}

#define ALIGN8(size) (((u64)(size) + 7u) & ~7u)

#endif /* WC_MAP_SETUP_H */

/* ===== hashmap.h ===== */
#ifndef WC_HASHMAP_H
#define WC_HASHMAP_H

/* Generic Hashmap with Ownership Semantics
  - Robin Hood Hashing
  - we have 3 arrays: keys, psls, and vals
  - PSL: probe sequence length - the distance from hashing location
  - we actuall store psl + 1 as psl = 0 means empty bucket
  - Robin Hood Invarient: all keys that hash to i come before keys that hash to i + 1
  - vals store [val] inline
*/


typedef struct {
    u8*            keys;
    u8*            psls;
    u8*            vals;
    u64            size;
    u64            capacity;
    u32            key_size;
    u32            val_size;
    u8*            scratch; // key_size + val_size bytes + alignment - temp buffer for robin hood swaps
    custom_hash_fn hash_fn;
    compare_fn     cmp_fn;

    // Shared ops vtables for keys and values.
    // Pass NULL for POD types (int, float, flat structs).
    // For types with heap resources define one static ops per type:
    const container_ops* key_ops;
    const container_ops* val_ops;
} HashMap;


// Safely extract callbacks — always NULL-safe on ops itself.
#define MAP_COPY(ops) ((ops) ? (ops)->copy_fn : NULL)
#define MAP_MOVE(ops) ((ops) ? (ops)->move_fn : NULL)
#define MAP_DEL(ops)  ((ops) ? (ops)->del_fn : NULL)

/* TODO:
    reserve one extra slot at the end of the key/val arrays that never holds a real entry.
    During insert, you keep the “current” key/value in registers or local variables
    and only write them into the array when the final empty slot is found.
    This requires a small rewrite of map_insert, but it saves two memcpy calls per eviction
    and eliminates scratch
*/

// Create a new HashMap.
// hash_fn and cmp_fn default to fnv1a_hash / default_compare if NULL.
// key_ops / val_ops: pass NULL for POD types.
HashMap* HashMap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* key_ops, const container_ops* val_ops) __attribute__((warn_unused_result));
void     HashMap_create_stk(HashMap* map, u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                            const container_ops* key_ops, const container_ops* val_ops)
    __attribute__((nonnull(1)));

void HashMap_destroy(HashMap* map) __attribute__((nonnull(1)));
void HashMap_destroy_stk(HashMap* map) __attribute__((nonnull(1)));

// Insert or update — COPY semantics.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put(HashMap* map, const u8* key, const u8* val) __attribute__((nonnull(1, 2, 3)));

// Insert or update — MOVE semantics (key and val are u8**, both nulled).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put_move(HashMap* map, u8** key, u8** val) __attribute__((nonnull(1, 2, 3)));

// Mixed: key copied, val moved.
b8 HashMap_put_val_move(HashMap* map, const u8* key, u8** val) __attribute__((nonnull(1, 2, 3)));

// Mixed: key moved, val copied.
b8 HashMap_put_key_move(HashMap* map, u8** key, const u8* val) __attribute__((nonnull(1, 2, 3)));

// Get value for key — copies into val. Returns 1 if found, 0 if not.
b8 HashMap_get(const HashMap* map, const u8* key, u8* val) __attribute__((nonnull(1, 2, 3)));

// Get pointer to value
const u8* HashMap_get_ptr(const HashMap* map, const u8* key) __attribute__((nonnull(1, 2)));

__attribute__((nonnull(1, 2))) static inline u8* HashMap_get_ptr_mut(HashMap* map, const u8* key)
{
    return (u8*)HashMap_get_ptr(map, key);
}

// Bucket iteration accessors
__attribute__((nonnull(1))) static inline u64 HashMap_bucket_count(const HashMap* map)
{
    return map->capacity;
}

b8        HashMap_bucket_occupied(const HashMap* map, u64 i) __attribute__((nonnull(1)));
const u8* HashMap_bucket_key_ptr(const HashMap* map, u64 i) __attribute__((nonnull(1)));
u8*       HashMap_bucket_val_ptr(HashMap* map, u64 i) __attribute__((nonnull(1)));

// Delete key. If out is provided, value is copied to it before deletion.
// Returns 1 if found and deleted, 0 if not found.
b8 HashMap_del(HashMap* map, const u8* key, u8* out) __attribute__((nonnull(1, 2)));

// Check if key exists.
b8 HashMap_has(const HashMap* map, const u8* key) __attribute__((nonnull(1, 2)));

// Print all key-value pairs.
void HashMap_print(const HashMap* map, print_fn key_print, print_fn val_print) __attribute__((nonnull(1, 2, 3)));

// Remove all elements, keep capacity.
void HashMap_clear(HashMap* map) __attribute__((nonnull(1)));

// Deep copy src into dest
// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void HashMap_copy(HashMap* dest, const HashMap* src) __attribute__((nonnull(1, 2)));


static inline __attribute__((nonnull(1))) u64 HashMap_size(const HashMap* map)
{
    return map->size;
}
static inline __attribute__((nonnull(1))) u64 HashMap_capacity(const HashMap* map)
{
    return map->capacity;
}
static inline __attribute__((nonnull(1))) b8 HashMap_empty(const HashMap* map)
{
    return map->size == 0;
}

#endif /* WC_HASHMAP_H */

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
    u8*            scratch; // 2 * elm_size bytes — stage (first half) + RH swap (second half)
    custom_hash_fn hash_fn;
    compare_fn     cmp_fn;

    // Shared ops vtable for elements.
    // Pass NULL for POD types (int, float, flat structs).
    const container_ops* ops;
} HashSet;


// Safely extract callbacks — always NULL-safe on ops itself.
#define SET_COPY(ops) ((ops) ? (ops)->copy_fn : NULL)
#define SET_MOVE(ops) ((ops) ? (ops)->move_fn : NULL)
#define SET_DEL(ops)  ((ops) ? (ops)->del_fn : NULL)


// Create a new HashSet.
// hash_fn and cmp_fn default to wyhash / default_compare if NULL.
// ops: pass NULL for POD types.
HashSet* HashSet_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn, const container_ops* ops)
    __attribute__((warn_unused_result));
void HashSet_create_stk(HashSet* set, u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn, const container_ops* ops)
    __attribute__((nonnull(1)));

void HashSet_destroy(HashSet* set) __attribute__((nonnull(1)));
void HashSet_destroy_stk(HashSet* set) __attribute__((nonnull(1)));

// Insert element — COPY semantics.
// Returns 1 if already existed (no-op), 0 if newly inserted.
b8 HashSet_insert(HashSet* set, const u8* elm) __attribute__((nonnull(1, 2)));

// Insert element — MOVE semantics (elm is nulled on insert, or freed if duplicate).
// Returns 1 if already existed (elm freed), 0 if newly inserted.
b8 HashSet_insert_move(HashSet* set, u8** elm) __attribute__((nonnull(1, 2)));

// Returns 1 if found, 0 if not.
b8 HashSet_has(const HashSet* set, const u8* elm) __attribute__((nonnull(1, 2)));

// Get pointer to element in-place. Returns NULL if not found.
const u8* HashSet_get_ptr(const HashSet* set, const u8* elm) __attribute__((nonnull(1, 2)));

__attribute__((nonnull(1, 2))) static inline u8* HashSet_get_ptr_mut(HashSet* set, const u8* elm)
{
    return (u8*)HashSet_get_ptr(set, elm);
}

// Bucket iteration accessors
__attribute__((nonnull(1))) static inline u64 HashSet_bucket_count(const HashSet* set)
{
    return set->capacity;
}

b8        HashSet_bucket_occupied(const HashSet* set, u64 i) __attribute__((nonnull(1)));
const u8* HashSet_bucket_elm_ptr(const HashSet* set, u64 i) __attribute__((nonnull(1)));

// Returns 1 if found and removed, 0 if not found.
b8 HashSet_remove(HashSet* set, const u8* elm) __attribute__((nonnull(1, 2)));

// Print all elements.
void HashSet_print(const HashSet* set, print_fn print) __attribute__((nonnull(1, 2)));

// Remove all elements, keep capacity.
void HashSet_clear(HashSet* set) __attribute__((nonnull(1)));

// Deep copy src into dest
// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void HashSet_copy(HashSet* dest, const HashSet* src) __attribute__((nonnull(1, 2)));


static inline __attribute__((nonnull(1))) u64 HashSet_size(const HashSet* set)
{
    return set->size;
}

static inline __attribute__((nonnull(1))) u64 HashSet_capacity(const HashSet* set)
{
    return set->capacity;
}

static inline __attribute__((nonnull(1))) b8 HashSet_empty(const HashSet* set)
{
    return set->size == 0;
}

#endif /* WC_HASHSET_H */

/* ===== queue.h ===== */
#ifndef WC_QUEUE_H
#define WC_QUEUE_H

typedef struct { // Circular Queue
    GenVec* arr;
    u64 head;   // pop from (head + 1) % capacity
    u64 tail;   // push at  (head + size) % capacity
    u64 size;
} Queue;


Queue*    Queue_create(u64 n, u32 data_size, const container_ops* ops) __attribute__((warn_unused_result));
Queue*    Queue_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops) __attribute__((nonnull(2), warn_unused_result));
void      Queue_create_stk(Queue* q, u64 n, u32 data_size, const container_ops* ops) __attribute__((nonnull(1)));

void      Queue_destroy(Queue* q) __attribute__((nonnull(1)));
void      Queue_destroy_stk(Queue* q) __attribute__((nonnull(1)));
void      Queue_clear(Queue* q) __attribute__((nonnull(1)));
void      Queue_reset(Queue* q) __attribute__((nonnull(1)));
void      Queue_shrink_to_fit(Queue* q) __attribute__((nonnull(1)));

// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void      Queue_copy(Queue* dest, const Queue* src) __attribute__((nonnull(1, 2)));
void      Queue_move(Queue* dest, Queue** src) __attribute__((nonnull(1, 2)));

void      Queue_push(Queue* q, const u8* x) __attribute__((nonnull(1, 2)));
void      Queue_push_move(Queue* q, u8** x) __attribute__((nonnull(1, 2)));
void      Queue_pop(Queue* q, u8* out) __attribute__((nonnull(1)));
void      Queue_peek(Queue* q, u8* peek) __attribute__((nonnull(1, 2)));
const u8* Queue_peek_ptr(const Queue* q) __attribute__((nonnull(1)));

void      Queue_print(Queue* q, print_fn print_fn) __attribute__((nonnull(1, 2)));

// 6-J: nonnull-validated — no CHECK_FATAL(!q) re-checks (mirrors gen_vector.c)
static inline __attribute__((nonnull(1))) u64 Queue_size(const Queue* q) { return q->size;                  }
static inline __attribute__((nonnull(1))) u8 Queue_empty(const Queue* q) { return q->size == 0;             }
static inline __attribute__((nonnull(1))) u64 Queue_capacity(const Queue* q) { return GenVec_capacity(q->arr); }

#endif /* WC_QUEUE_H */

/* ===== wc_macros.h ===== */
#ifndef WC_WC_MACROS_H
#define WC_WC_MACROS_H

// Required by WC_OPS (6-I): _Generic association expressions must name declared
// symbols in every TU that sees this header, even when the macro is never used.
/* C11 + GNU extensions used here:
 *   typeof  (__typeof__)  — GNU ext, available with Clang/GCC + -std=c11
 *   ({ })   statement expressions — GNU ext, Clang/GCC only
 */

#define typeof __typeof__

/* WC_ASSERT_ELEM_SIZE — developer guard for typed macro layers (6-H).
 * Fires when a type-asserting macro (VEC_AT, VEC_POP, ...) is used on a vec
 * whose element size doesn't match sizeof(T) — i.e. the wrong T was passed.
 * The container never knows T, so this lives in the macro layer.
 */
#define WC_ASSERT_ELEM_SIZE(vec, T)                                                                     \
    do {                                                                                                \
        CHECK_FATAL((vec)->data_size != sizeof(T),                                                      \
                    "element size mismatch: " #vec " (data_size=%u), macro given " #T " (sizeof=%llu)", \
                    (unsigned)(vec)->data_size, (unsigned long long)sizeof(T));                         \
    } while (0)


/* WC_OPS — pick the right container_ops for T at compile time (6-I).
 * Requires wc_helpers.h (the ops instances it names live there).
 *   VEC_CREATE_OF(int, 8)                  -> POD, NULL ops
 *   VEC_CREATE_OF(String, 8)               -> &wc_str_ops (by value)
 *   VEC_CREATE_OF(String*, 8)              -> &wc_str_ptr_ops
 *   VEC_CREATE_OF(GenVec, 8)               -> &wc_vec_ops
 *   VEC_CREATE_OF(GenVec*, 8)              -> &wc_vec_ptr_ops
 * Unknown types fall back to POD (NULL ops).
 */
#define WC_OPS(T)                                          \
    _Generic((T*)0,                                        \
        String*: (const container_ops*)&wc_str_ops,        \
        String * *: (const container_ops*)&wc_str_ptr_ops, \
        GenVec*: (const container_ops*)&wc_vec_ops,        \
        GenVec * *: (const container_ops*)&wc_vec_ptr_ops, \
        default: (const container_ops*)NULL)



/* STORAGE STRATEGY
 * ================
 *
 * Strategy A — by value: slot holds the full struct (sizeof(String) bytes)
 *   GenVec* v = VEC_CX(String, 10, &wc_str_ops);
 *   Better cache locality. Addresses change on realloc.
 *
 * Strategy B — by pointer: slot holds a pointer (sizeof(String*) = 8 bytes)
 *   GenVec* v = VEC_CX(String*, 10, &wc_str_ptr_ops);
 *   One extra dereference. Addresses stable across growth.
 *
 * ops structs (wc_str_ops, wc_str_ptr_ops, etc.) are defined in wc_helpers.h.
 */


// Creation

// POD types (int, float, flat structs) — pass NULL for ops
#define VEC(T, cap)  GenVec_create((cap), sizeof(T), NULL)
#define VEC_EMPTY(T) GenVec_create(0, sizeof(T), NULL)

// Types with owned heap resources — pass a GenVec_ops pointer
#define VEC_CX(T, cap, ops)  GenVec_create((cap), sizeof(T), (ops))
#define VEC_EMPTY_CX(T, ops) GenVec_create(0, sizeof(T), (ops))

// Ops picked automatically from T (see WC_OPS).
#define VEC_CREATE_OF(T, cap) GenVec_create((cap), sizeof(T), WC_OPS(T))
#define MAP_CREATE_OF(K, V)   HashMap_create(sizeof(K), sizeof(V), NULL, NULL, WC_OPS(K), WC_OPS(V))

#define VEC_MAKE_OPS(copy, move, del) \
    (container_ops)                   \
    {                                 \
        (copy), (move), (del)         \
    }

// Stack variants
#define VEC_STK(T, cap, vec)         GenVec_create_stk((vec), (cap), sizeof(T), NULL)
#define VEC_CX_STK(T, cap, ops, vec) GenVec_create_stk((vec), (cap), sizeof(T), (ops))

/* Create vector from an initializer list
Usage:
    GenVec* v = VEC_FROM_ARR(int, 4, ((int[4]){1,2,3,4}));
*/
#define VEC_FROM_ARR(T, n, arr)                           \
    ({                                                    \
        GenVec* _v = GenVec_create((n), sizeof(T), NULL); \
        for (u64 _i = 0; _i < (n); _i++) {                \
            GenVec_push(_v, (u8*)&(arr)[_i]);             \
        }                                                 \
        _v;                                               \
    })


// Push

// VEC_PUSH — push any POD value
#define VEC_PUSH(vec, val)                 \
    ({                                     \
        typeof(val) wvp_tmp = (val);       \
        GenVec_push((vec), (u8*)&wvp_tmp); \
    })

// VEC_PUSH_MOVE — transfer ownership. Source becomes NULL.
#define VEC_PUSH_MOVE(vec, ptr)                \
    ({                                         \
        typeof(ptr) wvm_p = (ptr);             \
        GenVec_push_move((vec), (u8**)&wvm_p); \
        (ptr) = wvm_p;                         \
    })

// VEC_PUSH_CSTR — allocate a heap String and move it in.
#define VEC_PUSH_CSTR(vec, cstr)                \
    ({                                          \
        String* wpc_s = String_from_cstr(cstr); \
        GenVec_push_move((vec), (u8**)&wpc_s);  \
    })


// Access
// All type-asserting macros guard with WC_ASSERT_ELEM_SIZE (6-H) — pass the right T.

#define VEC_AT(vec, T, i)                \
    ({                                   \
        WC_ASSERT_ELEM_SIZE((vec), T);   \
        *(T*)GenVec_get_ptr((vec), (i)); \
    })

#define VEC_AT_MUT(vec, T, i)               \
    ({                                      \
        WC_ASSERT_ELEM_SIZE((vec), T);      \
        (T*)GenVec_get_ptr_mut((vec), (i)); \
    })

#define VEC_FRONT(vec, T)              \
    ({                                 \
        WC_ASSERT_ELEM_SIZE((vec), T); \
        *(T*)GenVec_front((vec));      \
    })

#define VEC_BACK(vec, T)               \
    ({                                 \
        WC_ASSERT_ELEM_SIZE((vec), T); \
        *(T*)GenVec_back((vec));       \
    })


// Mutate

#define VEC_SET(vec, i, val)                       \
    ({                                             \
        typeof(val) wvs_tmp = (val);               \
        GenVec_replace((vec), (i), (u8*)&wvs_tmp); \
    })


// Pop

#define VEC_POP(vec, T)                 \
    ({                                  \
        WC_ASSERT_ELEM_SIZE((vec), T);  \
        T wvpop;                        \
        GenVec_pop((vec), (u8*)&wvpop); \
        wvpop;                          \
    })


// Iterate
/* bounds hoisted into the outer loop (_wvf_n) and element fetch elided via
 * GenVec_get_ptr_mut_unsafe — the index is provably < size. NOTE: no size assert
 * here (a leading statement would break `if (x) VEC_FOREACH(...) ...` usage);
 * the T* assignment still gives compile-time type checking.
 */
#define VEC_FOREACH(vec, T, name)                                         \
    for (u64 _wvf_n = (vec)->size, _wvf_i = 0; _wvf_i < _wvf_n; _wvf_i++) \
        for (T* name = (T*)GenVec_get_ptr_mut_unsafe((vec), _wvf_i); name; name = NULL)



/* ══════════════════════════════════════════════════════════════════════════
 * TYPED CONVENIENCE MACROS
 * (require wc_helpers.h to be included for the ops structs)
 * ══════════════════════════════════════════════════════════════════════════ */

// Vector creation shorthands

#define VEC_OF_INT(cap)     GenVec_create((cap), sizeof(int), NULL)
#define VEC_OF_STR(cap)     VEC_CX(String, (cap), &wc_str_ops)
#define VEC_OF_STR_PTR(cap) VEC_CX(String*, (cap), &wc_str_ptr_ops)
#define VEC_OF_VEC(cap)     VEC_CX(GenVec, (cap), &wc_vec_ops)
#define VEC_OF_VEC_PTR(cap) VEC_CX(GenVec*, (cap), &wc_vec_ptr_ops)


// Push shorthands

#define VEC_PUSH_VEC(outer, inner_ptr)     VEC_PUSH_MOVE((outer), (inner_ptr))
#define VEC_PUSH_VEC_PTR(outer, inner_ptr) VEC_PUSH_MOVE((outer), (inner_ptr))


// Hashmap shorthands

/*
 * MAP_PUT_INT_STR(map, int_key, cstr_literal)
 * Map must have int key, String val, created with &wc_str_ops for val.
 */
#define MAP_PUT_INT_STR(map, k, cstr_val)                         \
    ({                                                            \
        String* _v = String_from_cstr(cstr_val);                  \
        HashMap_put_val_move((map), (u8*)&(int){(k)}, (u8**)&_v); \
    })

/*
 * MAP_PUT_STR_STR(map, cstr_key, cstr_val)
 * Map must use &wc_str_ops for both key and val.
 */
#define MAP_PUT_STR_STR(map, cstr_key, cstr_val)       \
    ({                                                 \
        String* _k = String_from_cstr(cstr_key);       \
        String* _v = String_from_cstr(cstr_val);       \
        HashMap_put_move((map), (u8**)&_k, (u8**)&_v); \
    })


// Put (COPY semantics)

#define MAP_PUT(map, key, val)                                \
    ({                                                        \
        typeof(key) _mk = (key);                              \
        typeof(val) _mv = (val);                              \
        HashMap_put((map), (const u8*)&_mk, (const u8*)&_mv); \
    })


// Put (MOVE semantics)

#define MAP_PUT_MOVE(map, kptr, vptr)                      \
    ({                                                     \
        typeof(kptr) _mkp = (kptr);                        \
        typeof(vptr) _mvp = (vptr);                        \
        HashMap_put_move((map), (u8**)&_mkp, (u8**)&_mvp); \
        (kptr) = _mkp;                                     \
        (vptr) = _mvp;                                     \
    })

#define MAP_PUT_KEY_MOVE(map, kptr, val)                             \
    ({                                                               \
        typeof(val) _mv = (val);                                     \
        HashMap_put_key_move((map), (u8**)&(kptr), (const u8*)&_mv); \
    })

#define MAP_PUT_VAL_MOVE(map, key, vptr)                             \
    ({                                                               \
        typeof(key) _mk = (key);                                     \
        HashMap_put_val_move((map), (const u8*)&_mk, (u8**)&(vptr)); \
    })


// Get

// V - Type of the value. Asserts key is present.
#define MAP_GET(map, V, key)                                                                     \
    ({                                                                                           \
        V           _out;                                                                        \
        typeof(key) _mk = (key);                                                                 \
        CHECK_FATAL(!HashMap_get((map), (const u8*)&_mk, (u8*)&_out), "MAP_GET: key not found"); \
        _out;                                                                                    \
    })

// Returns b8 (1 if found, 0 if not). Writes *out_ptr on hit.
#define MAP_TRY_GET(map, V, key, out_ptr)                    \
    ({                                                       \
        typeof(key) _mk = (key);                             \
        HashMap_get((map), (const u8*)&_mk, (u8*)(out_ptr)); \
    })



// Iterate

#define MAP_FOREACH_KEY(map, T, name)                                                                                 \
    for (u64 _i = 0, _n = HashMap_bucket_count(map); _i < _n; _i++)                                                   \
        for (const T* name = HashMap_bucket_occupied((map), _i) ? (const T*)HashMap_bucket_key_ptr((map), _i) : NULL; \
             name; name    = NULL)

#define MAP_FOREACH_VAL(map, T, name)                                                                           \
    for (u64 _i = 0, _n = HashMap_bucket_count(map); _i < _n; _i++)                                             \
        for (T* name = HashMap_bucket_occupied((map), _i) ? (T*)HashMap_bucket_val_ptr((map), _i) : NULL; name; \
             name    = NULL)


// Hashset shorthands

#define SET_FROM_VEC(vec, hash_fn, cmp_fn)                                             \
    ({                                                                                 \
        HashSet* _set = HashSet_create((vec)->data_size, hash_fn, cmp_fn, (vec)->ops); \
        for (u64 i = 0, _n = GenVec_size(vec); i < _n; i++) {                          \
            HashSet_insert(_set, GenVec_get_ptr((vec), i));                            \
        }                                                                              \
        _set;                                                                          \
    })

#define SET_INSERT(set, elm)                  \
    ({                                        \
        typeof(elm) _temp = (elm);            \
        HashSet_insert((set), (u8*)&(_temp)); \
    })

#define SET_INSERT_MOVE(set, ptr)                  \
    ({                                             \
        typeof(ptr) _ptr = (ptr);                  \
        HashSet_insert_move((set), (u8**)&(_ptr)); \
        (ptr) = _ptr;                              \
    })

#define SET_INSERT_CSTR(set, cstr)               \
    ({                                           \
        String* _s = String_from_cstr(cstr);     \
        HashSet_insert_move((set), (u8**)&(_s)); \
    })


#define SET_FOREACH(set, T, name)                                                                                     \
    for (u64 _i = 0, _n = HashSet_bucket_count(set); _i < _n; _i++)                                                   \
        for (const T* name = HashSet_bucket_occupied((set), _i) ? (const T*)HashSet_bucket_elm_ptr((set), _i) : NULL; \
             name; name    = NULL)


// Stack macros

#define STACK_CREATE(T, cap)         VEC(T, cap)
#define STACK_CREATE_CX(T, cap, ops) VEC_CX(T, cap, ops)
#define STACK_STK(T, cap, vec)       VEC_STK(T, cap, vec)
#define STACK_PUSH(stk, val)         VEC_PUSH((stk), (val))
#define STACK_PUSH_MOVE(stk, ptr)    VEC_PUSH_MOVE((stk), (ptr))
#define STACK_POP(stk, T)            VEC_POP((stk), T)
#define STACK_AT(stk, T, i)          VEC_AT((stk), T, (i))
#define STACK_FOREACH(stk, T, name)  VEC_FOREACH((stk), T, name)


// Queue macros

#define QUEUE_CREATE(T, cap)         Queue_create((cap), sizeof(T), NULL)
#define QUEUE_CREATE_CX(T, cap, ops) Queue_create((cap), sizeof(T), (ops))
#define QUEUE_PUSH(q, val)              \
    ({                                  \
        typeof(val) _qp_tmp = (val);    \
        Queue_push((q), (u8*)&_qp_tmp); \
    })

#define QUEUE_PUSH_MOVE(q, ptr)             \
    ({                                      \
        typeof(ptr) _qp_p = (ptr);          \
        Queue_push_move((q), (u8**)&_qp_p); \
        (ptr) = _qp_p;                      \
    })

#define QUEUE_PUSH_CSTR(q, cstr)                \
    ({                                          \
        String* _qp_s = String_from_cstr(cstr); \
        Queue_push_move((q), (u8**)&_qp_s);     \
    })

#define QUEUE_POP(q, T)                \
    ({                                 \
        T _qp_out;                     \
        Queue_pop((q), (u8*)&_qp_out); \
        _qp_out;                       \
    })

#define QUEUE_PEEK(q, T) (*(T*)Queue_peek_ptr(q))

#endif /* WC_WC_MACROS_H */

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

/* ===== hashmap.c ===== */
#ifndef WC_HASHMAP_IMPL
#define WC_HASHMAP_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define GET_KEY(map, i) ((map)->keys + ((u64)(map)->key_size * (i)))
#define GET_PSL(map, i) ((map)->psls + (i))
#define GET_VAL(map, i) ((map)->vals + ((u64)(map)->val_size * (i)))

// capacity is always power-of-2 — use bitmask instead of %
#define MAP_MASK(map)     ((map)->capacity - 1)
#define MAP_IDX(map, key) ((map)->hash_fn((key), (map)->key_size) & MAP_MASK(map))
#define MAP_NEXT(map, i)  (((i) + 1) & MAP_MASK(map))

// PSL 0 == empty bucket; stored PSL is (real_psl + 1), starting at 1
#define BUCKET_EMPTY 0

// scratch layout:
//   STAGE region (first half)  — used by HashMap_put to copy incoming key/val
//     [0                         ..  ALIGN8(key_size))             = STAGE_KEY
//     [ALIGN8(key_size)          ..  ALIGN8(key_size) + val_size)  = STAGE_VAL
//   SWAP region (second half)  — used by map_insert for Robin Hood evictions
//     [ALIGN8(key_size)+val_size ..  +ALIGN8(key_size))            = SWAP_KEY
//     [+ALIGN8(key_size)         ..  +val_size)                    = SWAP_VAL
//
// The two regions must NOT overlap: map_insert is called with pointers INTO
// the STAGE region, and it writes displaced residents into the SWAP region.
// Total: 2 * (ALIGN8(key_size) + val_size) bytes.
#define STAGE_KEY(map) ((map)->scratch)
#define STAGE_VAL(map) ((map)->scratch + ALIGN8((map)->key_size))
#define SWAP_KEY(map)  ((map)->scratch + ALIGN8((map)->key_size) + (map)->val_size)
#define SWAP_VAL(map)  ((map)->scratch + ALIGN8((map)->key_size) + (map)->val_size + ALIGN8((map)->key_size))

#define IS_POD_K(map) (map->key_ops == NULL)
#define IS_POD_V(map) (map->val_ops == NULL)


/*
====================PRIVATE DECLARATIONS====================
*/

static u64         map_lookup(const HashMap* map, const u8* key, LOOKUP_RES* res, u8* out_psl);
static void        map_insert(HashMap* map, u8* key, u8* val, u8 psl, u64 idx);
static inline void map_maybe_resize(HashMap* map);
static void        map_resize(HashMap* map, u64 new_capacity);


/*
====================PUBLIC FUNCTIONS====================
*/

void HashMap_create_stk(HashMap* map, u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn, const container_ops* key_ops, const container_ops* val_ops)
{
    CHECK_FATAL(key_size == 0 || val_size == 0, "key/val size can't be 0");

    map->keys = malloc((u64)HASHMAP_INIT_CAPACITY * key_size);
    CHECK_FATAL(!map->keys, "keys malloc failed");
    map->psls = calloc(HASHMAP_INIT_CAPACITY, sizeof(u8));
    CHECK_FATAL(!map->psls, "psls calloc failed");
    map->vals = malloc((u64)HASHMAP_INIT_CAPACITY * val_size);
    CHECK_FATAL(!map->vals, "vals malloc failed");

    map->scratch = malloc(2 * (ALIGN8(key_size) + val_size));
    CHECK_FATAL(!map->scratch, "scratch malloc failed");

    map->size     = 0;
    map->capacity = HASHMAP_INIT_CAPACITY;
    map->key_size = key_size;
    map->val_size = val_size;

    map->hash_fn = hash_fn ? hash_fn : wyhash;
    map->cmp_fn  = cmp_fn ? cmp_fn : default_compare;

    map->key_ops = key_ops;
    map->val_ops = val_ops;
}

HashMap* HashMap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* key_ops, const container_ops* val_ops)
{
    HashMap* map = malloc(sizeof(HashMap));
    CHECK_FATAL(!map, "map malloc failed");

    HashMap_create_stk(map, key_size, val_size, hash_fn, cmp_fn, key_ops, val_ops);

    return map;
}


void HashMap_destroy(HashMap* map)
{
    HashMap_destroy_stk(map);
    free(map);
}

void HashMap_destroy_stk(HashMap* map)
{
    if (!IS_POD_K(map) || !IS_POD_V(map)) {
        delete_fn k_del = IS_POD_K(map) ? NULL : map->key_ops->del_fn;
        delete_fn v_del = IS_POD_V(map) ? NULL : map->val_ops->del_fn;
        if (k_del || v_del) {
            for (u64 i = 0; i < map->capacity; i++) {
                if (*GET_PSL(map, i) == BUCKET_EMPTY) {
                    continue;
                }
                if (k_del) {
                    k_del(GET_KEY(map, i));
                }
                if (v_del) {
                    v_del(GET_VAL(map, i));
                }
            }
        }
    }

    free(map->keys);
    free(map->psls);
    free(map->vals);
    free(map->scratch);
}


// Insert or update — COPY semantics.
// Ownership: map takes a deep copy of key and val via ops->copy_fn (or memcpy for POD).
// The caller retains ownership of its key/val and is responsible for freeing them.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put(HashMap* map, const u8* key, const u8* val)
{
    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res == FOUND) {
        if (IS_POD_V(map)) {
            memcpy(GET_VAL(map, slot), val, map->val_size);
        } else {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
            copy_fn v_cp = map->val_ops->copy_fn;
            if (v_cp) {
                v_cp(GET_VAL(map, slot), val);
            } else {
                memcpy(GET_VAL(map, slot), val, map->val_size);
            }
        }
        return 1;
    }

    if (IS_POD_K(map)) {
        memcpy(STAGE_KEY(map), key, map->key_size);
    } else {
        copy_fn k_cp = map->key_ops->copy_fn;
        if (k_cp) {
            k_cp(STAGE_KEY(map), key);
        } else {
            memcpy(STAGE_KEY(map), key, map->key_size);
        }
    }
    if (IS_POD_V(map)) {
        memcpy(STAGE_VAL(map), val, map->val_size);
    } else {
        copy_fn v_cp = map->val_ops->copy_fn;
        if (v_cp) {
            v_cp(STAGE_VAL(map), val);
        } else {
            memcpy(STAGE_VAL(map), val, map->val_size);
        }
    }

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Insert or update — MOVE semantics.
// Ownership: the map takes ownership of *key and *val directly (no copy made).
// On success both pointers are nulled. Requires move_fn for both key and val.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put_move(HashMap* map, u8** key, u8** val)
{
    CHECK_FATAL(!*key || !*val, "*key/*val null");

    move_fn k_mv = MAP_MOVE(map->key_ops);
    move_fn v_mv = MAP_MOVE(map->val_ops);

    // move_fn is mandatory: it must transfer the heap resource and null the source.
    // For by-value types with no heap resources, use HashMap_put (copy semantics) instead.
    CHECK_FATAL(!k_mv || !v_mv, "key/val move funcs required");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, *key, &res, &out_psl);

    if (res == FOUND) {
        if (!IS_POD_V(map)) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
        }
        v_mv(GET_VAL(map, slot), val);
        if (!IS_POD_K(map)) {
            delete_fn k_del = map->key_ops->del_fn;
            if (k_del) {
                k_del(*key);
            }
        }
        free(*key);
        *key = NULL;
        return 1;
    }

    // Stage: move key into STAGE_KEY, move val into STAGE_VAL.
    // move_fn transfers the heap resource pointer into the dest slot and nulls src.
    k_mv(STAGE_KEY(map), key); // nulls *key
    v_mv(STAGE_VAL(map), val); // nulls *val

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Insert or update — mixed: key is COPIED, val is MOVED.
// Ownership: map deep-copies the key (caller retains it); map takes ownership of *val (*val nulled).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put_val_move(HashMap* map, const u8* key, u8** val)
{
    CHECK_FATAL(!*val, "*val null");

    move_fn v_mv = MAP_MOVE(map->val_ops);

    CHECK_FATAL(!v_mv, "val move func required");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res == FOUND) {
        if (!IS_POD_V(map)) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
        }
        v_mv(GET_VAL(map, slot), val);
        return 1;
    }

    if (IS_POD_K(map)) {
        memcpy(STAGE_KEY(map), key, map->key_size);
    } else {
        copy_fn k_cp = map->key_ops->copy_fn;
        if (k_cp) {
            k_cp(STAGE_KEY(map), key);
        } else {
            memcpy(STAGE_KEY(map), key, map->key_size);
        }
    }
    v_mv(STAGE_VAL(map), val);

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Insert or update — mixed: key is MOVED, val is COPIED.
// Ownership: map takes ownership of *key (*key nulled); map deep-copies val (caller retains it).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put_key_move(HashMap* map, u8** key, const u8* val)
{
    CHECK_FATAL(!*key, "*key null");

    move_fn k_mv = MAP_MOVE(map->key_ops);

    CHECK_FATAL(!k_mv, "key move func required for HashMap_put_key_move");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, *key, &res, &out_psl);

    if (res == FOUND) {
        if (!IS_POD_V(map)) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
            copy_fn v_cp = map->val_ops->copy_fn;
            if (v_cp) {
                v_cp(GET_VAL(map, slot), val);
            } else {
                memcpy(GET_VAL(map, slot), val, map->val_size);
            }
        } else {
            memcpy(GET_VAL(map, slot), val, map->val_size);
        }
        // Key already in map — consume (and discard) the incoming duplicate.
        if (!IS_POD_K(map)) {
            delete_fn k_del = map->key_ops->del_fn;
            if (k_del) {
                k_del(*key);
            }
        }
        free(*key);
        *key = NULL;
        return 1;
    }

    // Stage key (move) and val (copy).
    k_mv(STAGE_KEY(map), key);
    if (IS_POD_V(map)) {
        memcpy(STAGE_VAL(map), val, map->val_size);
    } else {
        copy_fn v_cp = map->val_ops->copy_fn;
        if (v_cp) {
            v_cp(STAGE_VAL(map), val);
        } else {
            memcpy(STAGE_VAL(map), val, map->val_size);
        }
    }

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Get value for key — COPIES into val. Returns 1 if found, 0 if not.
// Caller owns the copy returned in val and must free it when done.
b8 HashMap_get(const HashMap* map, const u8* key, u8* val)
{
    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    if (IS_POD_V(map)) {
        memcpy(val, GET_VAL(map, slot), map->val_size);
    } else {
        copy_fn v_copy = map->val_ops->copy_fn;
        if (v_copy) {
            v_copy(val, GET_VAL(map, slot));
        } else {
            memcpy(val, GET_VAL(map, slot), map->val_size);
        }
    }
    return 1;
}


// Get pointer to value in-place (read-only). Returns NULL if not found.
// The pointer is valid until the next mutation (put/del/resize).
// Do NOT free the returned pointer — the map owns it.
const u8* HashMap_get_ptr(const HashMap* map, const u8* key)
{
    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    return (res == FOUND) ? GET_VAL(map, slot) : NULL;
}

b8 HashMap_bucket_occupied(const HashMap* map, u64 i)
{
    CHECK_FATAL(i >= map->capacity, "index out of bounds");
    return *GET_PSL(map, i) != BUCKET_EMPTY;
}

const u8* HashMap_bucket_key_ptr(const HashMap* map, u64 i)
{
    CHECK_FATAL(i >= map->capacity, "index out of bounds");
    return GET_KEY(map, i);
}

u8* HashMap_bucket_val_ptr(HashMap* map, u64 i)
{
    CHECK_FATAL(i >= map->capacity, "index out of bounds");
    return GET_VAL(map, i);
}


// Delete key.
// If out != NULL, the value is MOVED into it before deletion (caller takes ownership).
// If out == NULL, the value is destroyed via del_fn (or simply discarded for POD).
// Returns 1 if found and deleted, 0 if not found.
//
// Uses Robin Hood backward-shift deletion to maintain the probe-sequence invariant
// without tombstones: after removing a slot, we shift subsequent entries back one
// position as long as they have PSL > 1 (i.e. they are not sitting at their home slot).
b8 HashMap_del(HashMap* map, const u8* key, u8* out)
{
    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    if (out) {
        memcpy(out, GET_VAL(map, slot), map->val_size);
    } else {
        if (!IS_POD_V(map)) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
        }
    }

    if (!IS_POD_K(map)) {
        delete_fn k_del = map->key_ops->del_fn;
        if (k_del) {
            k_del(GET_KEY(map, slot));
        }
    }

    // Backward-shift deletion: pull subsequent entries one slot back as long as
    // they have PSL > 1.  Entries at their home slot (PSL == 1) must not move.
    // This restores the Robin Hood invariant without tombstones.
    u64 cur = slot;
    for (;;) {
        u64 next     = MAP_NEXT(map, cur);
        u8  next_psl = *GET_PSL(map, next);

        // Stop if next slot is empty or the next entry is already at its home slot.
        if (next_psl <= 1) {
            *GET_PSL(map, cur) = BUCKET_EMPTY;
            break;
        }

        // Shift next entry one slot back; its PSL decreases by 1.
        *GET_PSL(map, cur) = next_psl - 1;
        memcpy(GET_KEY(map, cur), GET_KEY(map, next), map->key_size);
        memcpy(GET_VAL(map, cur), GET_VAL(map, next), map->val_size);

        cur = next;
    }

    map->size--;
    return 1;
}


// Check if key exists.
b8 HashMap_has(const HashMap* map, const u8* key)
{
    LOOKUP_RES res;
    u8         out_psl;
    map_lookup(map, key, &res, &out_psl);
    return res == FOUND;
}


// Print all key-value pairs.
void HashMap_print(const HashMap* map, print_fn key_print, print_fn val_print)
{
    printf("\t=========\n");
    printf("\tSize: %lu / Capacity: %lu\n", map->size, map->capacity);
    printf("\t=========\n");

    for (u64 i = 0; i < map->capacity; i++) {
        if (*GET_PSL(map, i) == BUCKET_EMPTY) {
            continue;
        }
        putchar('\t');
        key_print(GET_KEY(map, i));
        printf(" => ");
        val_print(GET_VAL(map, i));
        putchar('\n');
    }

    printf("\t=========\n");
}


// Remove all elements, keep capacity.
// Destroys all keys and values via their del_fn callbacks, then zeroes the arrays.
void HashMap_clear(HashMap* map)
{
    if (!IS_POD_K(map) || !IS_POD_V(map)) {
        delete_fn k_del = IS_POD_K(map) ? NULL : map->key_ops->del_fn;
        delete_fn v_del = IS_POD_V(map) ? NULL : map->val_ops->del_fn;
        for (u64 i = 0; i < map->capacity; i++) {
            if (*GET_PSL(map, i) == BUCKET_EMPTY) {
                continue;
            }
            if (k_del) {
                k_del(GET_KEY(map, i));
            }
            if (v_del) {
                v_del(GET_VAL(map, i));
            }
        }
    }

    memset(map->psls, 0, map->capacity * sizeof(u8));
    map->size = 0;
}


// TODO: test
// Deep copy src into dest.
// Ownership: dest gets independently owned copies of all keys and values.
void HashMap_copy(HashMap* dest, const HashMap* src)
{
    if (dest == src) {
        return;
    }

    dest->keys = malloc(src->capacity * src->key_size);
    CHECK_FATAL(!dest->keys, "copy keys calloc failed");
    dest->psls = calloc(src->capacity, sizeof(u8));
    CHECK_FATAL(!dest->psls, "copy psls calloc failed");
    dest->vals = malloc(src->capacity * src->val_size);
    CHECK_FATAL(!dest->vals, "copy vals calloc failed");
    dest->scratch = malloc(2 * (ALIGN8(src->key_size) + src->val_size));
    CHECK_FATAL(!dest->scratch, "copy scratch malloc failed");

    dest->size     = src->size;
    dest->capacity = src->capacity;
    dest->key_size = src->key_size;
    dest->val_size = src->val_size;
    dest->hash_fn  = src->hash_fn;
    dest->cmp_fn   = src->cmp_fn;
    dest->key_ops  = src->key_ops;
    dest->val_ops  = src->val_ops;

    copy_fn k_cp = IS_POD_K(src) ? NULL : src->key_ops->copy_fn;
    copy_fn v_cp = IS_POD_V(src) ? NULL : src->val_ops->copy_fn;

    for (u64 i = 0; i < src->capacity; i++) {
        u8 psl = *GET_PSL(src, i);
        if (psl == BUCKET_EMPTY) {
            continue;
        }

        *GET_PSL(dest, i) = psl;

        if (k_cp) {
            k_cp(GET_KEY(dest, i), GET_KEY(src, i));
        } else {
            memcpy(GET_KEY(dest, i), GET_KEY(src, i), src->key_size);
        }

        if (v_cp) {
            v_cp(GET_VAL(dest, i), GET_VAL(src, i));
        } else {
            memcpy(GET_VAL(dest, i), GET_VAL(src, i), src->val_size);
        }
    }
}


/*
====================PRIVATE FUNCTIONS====================
*/

static inline void map_maybe_resize(HashMap* map)
{
    // integer multiply avoids float — equivalent to load > 0.75
    if (map->size * 4 >= map->capacity * 3) {
        map_resize(map, map->capacity * 2);
    }
}

static u64 map_lookup(const HashMap* map, const u8* key, LOOKUP_RES* res, u8* out_psl)
{
    u64        idx = MAP_IDX(map, key);
    u8         psl = 1; // stored PSL=1 means real probe distance 0 (home slot)
    compare_fn cmp = map->cmp_fn;

    for (u64 i = idx;; i = MAP_NEXT(map, i)) {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == BUCKET_EMPTY) {
            *res     = NOT_FOUND;
            *out_psl = psl;
            return i;
        }

        if (slot_psl < psl) {
            // The resident has a lower PSL — it's closer to its home than we are.
            // Under Robin Hood, our key would have displaced this resident on insert,
            // so our key cannot exist at or beyond this slot.
            *res     = ROBINHOOD_EXIT;
            *out_psl = psl;
            return i;
        }

        if (cmp(GET_KEY(map, i), key, map->key_size) == 0) {
            *res     = FOUND;
            *out_psl = psl;
            return i;
        }

        psl++;
    }
}



// Insert key/val with the given starting psl at slot idx.
// key and val are already OWNED by the caller (staged copy or moved pointer).
// This function never calls copy/del — it only shuffles raw bytes between slots.
// Displaced residents are temporarily buffered in SWAP_KEY/SWAP_VAL (second half
// of scratch), which is disjoint from the STAGE region where key/val came from.
static void map_insert(HashMap* map, u8* key, u8* val, u8 psl, u64 idx)
{
    // Use two alternating scratch halves to avoid aliasing
    u8* cur_key = STAGE_KEY(map);
    u8* cur_val = STAGE_VAL(map);
    u8* swp_key = SWAP_KEY(map);
    u8* swp_val = SWAP_VAL(map);

    // key/val may already be STAGE — only copy if not already there
    if (key != cur_key) {
        memcpy(cur_key, key, map->key_size);
    }
    if (val != cur_val) {
        memcpy(cur_val, val, map->val_size);
    }
    key = cur_key;
    val = cur_val;

    for (u64 i = idx;; i = MAP_NEXT(map, i)) {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == BUCKET_EMPTY) {
            *GET_PSL(map, i) = psl;
            memcpy(GET_KEY(map, i), key, map->key_size);
            memcpy(GET_VAL(map, i), val, map->val_size);
            map->size++;
            return;
        }

        if (slot_psl < psl) {
            u8 tmp_psl = slot_psl;
            // Evict into swp (disjoint from key which is in cur)
            memcpy(swp_key, GET_KEY(map, i), map->key_size);
            memcpy(swp_val, GET_VAL(map, i), map->val_size);

            *GET_PSL(map, i) = psl;
            memcpy(GET_KEY(map, i), key, map->key_size);
            memcpy(GET_VAL(map, i), val, map->val_size);

            // Swap roles: evicted becomes current, current becomes swap buffer
            u8* tmp = cur_key;
            cur_key = swp_key;
            swp_key = tmp;
            tmp     = cur_val;
            cur_val = swp_val;
            swp_val = tmp;
            key     = cur_key;
            val     = cur_val;
            psl     = tmp_psl + 1;
            continue;
        }

        psl++;
    }
}


// Rehash into a new array of new_capacity (must be power-of-2).
// Ownership transfers as raw bytes — no copy/del callbacks are invoked.
// This is safe because the data itself doesn't move, only the slot positions.
static void map_resize(HashMap* map, u64 new_capacity)
{
    if (new_capacity < HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    u8* old_keys = map->keys;
    u8* old_psls = map->psls;
    u8* old_vals = map->vals;
    u64 old_cap  = map->capacity;

    // map->keys = calloc(new_capacity, map->key_size);
    map->keys = malloc(new_capacity * map->key_size);
    CHECK_FATAL(!map->keys, "resize keys calloc failed");
    map->psls = calloc(new_capacity, sizeof(u8));
    CHECK_FATAL(!map->psls, "resize psls calloc failed");
    // map->vals = calloc(new_capacity, map->val_size);
    map->vals = malloc(new_capacity * map->val_size);
    CHECK_FATAL(!map->vals, "resize vals calloc failed");

    map->capacity = new_capacity;
    map->size     = 0;


    for (u64 i = 0; i < old_cap; i++) {

        if (old_psls[i] == BUCKET_EMPTY) {
            continue;
        }

        u8* old_key = old_keys + ((u64)map->key_size * i);
        u8* old_val = old_vals + ((u64)map->val_size * i);

        // Stage into scratch first — map_insert uses SWAP region of scratch
        // and would clobber old_key/old_val if they happened to alias it
        memcpy(STAGE_KEY(map), old_key, map->key_size);
        memcpy(STAGE_VAL(map), old_val, map->val_size);

        LOOKUP_RES res;
        u8         out_psl;
        u64        slot = map_lookup(map, STAGE_KEY(map), &res, &out_psl);

        map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    }

    free(old_keys);
    free(old_psls);
    free(old_vals);
}

#endif /* WC_HASHMAP_IMPL */

/* ===== hashset.c ===== */
#ifndef WC_HASHSET_IMPL
#define WC_HASHSET_IMPL

#include <stdio.h>
#include <stdlib.h>
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
// stage: where HashSet_insert copies the incoming elm before calling set_insert
// swap:  where set_insert saves a displaced resident during Robin Hood eviction
// The two halves are alternated each eviction to avoid aliasing (elm pointer
// is always in the half that set_insert is NOT currently writing into).
#define STAGE_ELM(set) ((set)->scratch)
#define SWAP_ELM(set)  ((set)->scratch + (set)->elm_size)


/*
====================PRIVATE DECLARATIONS====================
*/

static u64         set_lookup(const HashSet* set, const u8* elm, LOOKUP_RES* res, u8* out_psl);
static void        set_insert(HashSet* set, u8* elm, u8 psl, u64 idx);
static void        set_resize(HashSet* set, u64 new_capacity);
static inline void set_maybe_resize(HashSet* set);


/*
====================PUBLIC FUNCTIONS====================
*/

void HashSet_create_stk(HashSet* set, u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn, const container_ops* ops)
{
    CHECK_FATAL(elm_size == 0, "elm_size can't be 0");

    set->elms = malloc((u64)HASHMAP_INIT_CAPACITY * elm_size);
    CHECK_FATAL(!set->elms, "elms malloc failed");
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
}

HashSet* HashSet_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* ops)
{
    HashSet* set = malloc(sizeof(HashSet));
    CHECK_FATAL(!set, "set malloc failed");

    HashSet_create_stk(set, elm_size, hash_fn, cmp_fn, ops);

    return set;
}


void HashSet_destroy(HashSet* set)
{
    HashSet_destroy_stk(set);
    free(set);
}

void HashSet_destroy_stk(HashSet* set)
{
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
b8 HashSet_insert(HashSet* set, const u8* elm)
{
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
b8 HashSet_insert_move(HashSet* set, u8** elm)
{
    CHECK_FATAL(!*elm, "*elm null");

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
b8 HashSet_has(const HashSet* set, const u8* elm)
{
    LOOKUP_RES res;
    u8         out_psl;
    set_lookup(set, elm, &res, &out_psl);
    return res == FOUND;
}

const u8* HashSet_get_ptr(const HashSet* set, const u8* elm)
{
    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = set_lookup(set, elm, &res, &out_psl);
    return (res == FOUND) ? GET_ELM(set, slot) : NULL;
}

b8 HashSet_bucket_occupied(const HashSet* set, u64 i)
{
    CHECK_FATAL(i >= set->capacity, "index out of bounds");
    return *GET_PSL(set, i) != BUCKET_EMPTY;
}

const u8* HashSet_bucket_elm_ptr(const HashSet* set, u64 i)
{
    CHECK_FATAL(i >= set->capacity, "index out of bounds");
    return GET_ELM(set, i);
}


// Returns 1 if found and removed, 0 if not found.
// Uses Robin Hood backward-shift deletion to maintain the probe-sequence invariant
// without tombstones: after removing a slot, shift subsequent entries back one
// position as long as they have PSL > 1 (i.e. they are not at their home slot).
b8 HashSet_remove(HashSet* set, const u8* elm)
{
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
void HashSet_print(const HashSet* set, print_fn print)
{
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
void HashSet_clear(HashSet* set)
{
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
void HashSet_copy(HashSet* dest, const HashSet* src)
{
    if (dest == src) {
        return;
    }

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

static inline void set_maybe_resize(HashSet* set)
{
    // integer multiply avoids float — equivalent to load > 0.75
    if (set->size * 4 >= set->capacity * 3) {
        set_resize(set, set->capacity * 2);
    }
}


static u64 set_lookup(const HashSet* set, const u8* elm, LOOKUP_RES* res, u8* out_psl)
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


static void set_insert(HashSet* set, u8* elm, u8 psl, u64 idx)
{
    // elm is already owned (either staged copy or moved pointer).
    // Alternates between the two scratch halves on each Robin Hood eviction
    // so that elm never aliases the buffer being written into.
    u8* cur = STAGE_ELM(set);
    u8* swp = SWAP_ELM(set);

    // elm may already be STAGE_ELM (called from HashSet_insert/insert_move);
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


static void set_resize(HashSet* set, u64 new_capacity)
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

/* ===== queue.c ===== */
#ifndef WC_QUEUE_IMPL
#define WC_QUEUE_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define QUEUE_MIN_CAP   4
#define QUEUE_GROWTH    1.5f
#define QUEUE_SHRINK_AT 0.25f
#define QUEUE_SHRINK_BY 0.5f


#define HEAD_UPDATE(q)                                    \
    {                                                     \
        (q)->head = ((q)->head + 1) % (q)->arr->capacity; \
    }

#define TAIL_UPDATE(q)                                              \
    {                                                               \
        (q)->tail = (((q)->head + (q)->size) % (q)->arr->capacity); \
    }

#define Q_MAYBE_GROW(q)                        \
    do {                                       \
        if ((q)->size == (q)->arr->capacity) { \
            Queue_grow((q));                   \
        }                                      \
    } while (0)

#define Q_MAYBE_SHRINK(q)                                       \
    do {                                                        \
        u64 capacity = (q)->arr->capacity;                      \
        if (capacity <= 4) {                                    \
            return;                                             \
        }                                                       \
        float load_factor = (float)(q)->size / (float)capacity; \
        if (load_factor < QUEUE_SHRINK_AT) {                    \
            Queue_shrink((q));                                  \
        }                                                       \
    } while (0)


static void Queue_grow(Queue* q);
static void Queue_shrink(Queue* q);
static void Queue_compact(Queue* q, u64 new_capacity);


Queue* Queue_create(u64 n, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(n == 0 || data_size == 0, "n/data_size can't be 0");

    Queue* q = malloc(sizeof(Queue));
    CHECK_FATAL(!q, "Queue malloc failed");

    q->arr = GenVec_create(n, data_size, ops);

    q->head = 0;
    q->tail = 0;
    q->size = 0;

    return q;
}

Queue* Queue_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(n == 0 || data_size == 0, "n/data_size can't be 0");

    Queue* q = malloc(sizeof(Queue));
    CHECK_FATAL(!q, "Queue malloc failed");

    q->arr = GenVec_create_val(n, val, data_size, ops);

    q->head = 0;
    q->tail = n % GenVec_capacity(q->arr);
    q->size = n;

    return q;
}


void Queue_create_stk(Queue* q, u64 n, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(n == 0 || data_size == 0, "n/data_size can't be 0");

    q->arr = GenVec_create(n, data_size, ops);

    q->head = 0;
    q->tail = 0;
    q->size = 0;
}

void Queue_destroy(Queue* q)
{
    GenVec_destroy(q->arr);
    free(q);
}

void Queue_destroy_stk(Queue* q)
{
    GenVec_destroy(q->arr);
}

void Queue_clear(Queue* q)
{
    GenVec_clear(q->arr);
    q->size = 0;
    q->head = 0;
    q->tail = 0;
}

void Queue_reset(Queue* q)
{
    GenVec_reset(q->arr);
    q->size = 0;
    q->head = 0;
    q->tail = 0;
}

void Queue_shrink_to_fit(Queue* q)
{
    if (q->size == 0) {
        Queue_reset(q);
        return;
    }

    u64 min_capacity     = q->size > QUEUE_MIN_CAP ? q->size : QUEUE_MIN_CAP;
    u64 current_capacity = GenVec_capacity(q->arr);

    if (current_capacity > min_capacity) {
        Queue_compact(q, min_capacity);
    }
}

void Queue_push(Queue* q, const u8* x)
{
    Q_MAYBE_GROW(q);

    if (q->tail >= GenVec_size(q->arr)) {
        GenVec_push(q->arr, x);
    } else {
        GenVec_replace(q->arr, q->tail, x);
    }

    q->size++;
    TAIL_UPDATE(q);
}

void Queue_push_move(Queue* q, u8** x)
{
    CHECK_FATAL(!*x, "*x is null");

    Q_MAYBE_GROW(q);

    if (q->tail >= GenVec_size(q->arr)) {
        GenVec_push_move(q->arr, x);
    } else {
        GenVec_replace_move(q->arr, q->tail, x);
    }

    q->size++;
    TAIL_UPDATE(q);
}

void Queue_pop(Queue* q, u8* out)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );

    if (out) {
        GenVec_get(q->arr, q->head, out);
    }

    // Clean up the element if del_fn exists
    delete_fn del = VEC_DEL_FN(q->arr);
    if (del) {
        u8* elem = (u8*)GenVec_get_ptr(q->arr, q->head);
        del(elem);
        memset(elem, 0, q->arr->data_size);
    }

    HEAD_UPDATE(q);
    q->size--;
    Q_MAYBE_SHRINK(q);
}

void Queue_peek(Queue* q, u8* peek)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );

    GenVec_get(q->arr, q->head, peek);
}

const u8* Queue_peek_ptr(const Queue* q)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, NULL);

    return GenVec_get_ptr(q->arr, q->head);
}

void Queue_print(Queue* q, print_fn print)
{
    u64 h   = q->head;
    u64 cap = GenVec_capacity(q->arr);

    printf("[ ");
    if (q->size != 0) {
        for (u64 i = 0; i < q->size; i++) {
            const u8* out = GenVec_get_ptr(q->arr, h);
            print(out);
            putchar(' ');
            h = (h + 1) % cap;
        }
    }
    putchar(']');
}


void Queue_copy(Queue* dest, const Queue* src)
{
    GenVec_copy(dest->arr, src->arr);
    dest->head = src->head;
    dest->tail = src->tail;
    dest->size = src->size;
}

void Queue_move(Queue* dest, Queue** src)
{
    if (dest == *src) {
        *src = NULL;
        return;
    }

    memcpy(dest, *src, sizeof(Queue));
    free(*src);
    *src = NULL;
}
static void Queue_grow(Queue* q)
{
    u64 old_cap = GenVec_capacity(q->arr);
    u64 new_cap = (u64)((float)old_cap * QUEUE_GROWTH);
    if (new_cap <= old_cap) {
        new_cap = old_cap + 1;
    }

    Queue_compact(q, new_cap);
}

static void Queue_shrink(Queue* q)
{
    u64 current_cap = GenVec_capacity(q->arr);
    u64 new_cap     = (u64)((float)current_cap * QUEUE_SHRINK_BY);

    u64 min_capacity = q->size > QUEUE_MIN_CAP ? q->size : QUEUE_MIN_CAP;
    if (new_cap < min_capacity) {
        new_cap = min_capacity;
    }

    if (new_cap < current_cap) {
        Queue_compact(q, new_cap);
    }
}

static void Queue_compact(Queue* q, u64 new_capacity)
{
    CHECK_FATAL(new_capacity < q->size, "new_capacity must be >= current size");

    // Share the same ops pointer
    GenVec* new_arr = GenVec_create(new_capacity, q->arr->data_size, q->arr->ops);

    u64 h       = q->head;
    u64 old_cap = GenVec_capacity(q->arr);

    for (u64 i = 0; i < q->size; i++) {
        const u8* elem = GenVec_get_ptr(q->arr, h);
        GenVec_push(new_arr, elem);
        h = (h + 1) % old_cap;
    }

    GenVec_destroy(q->arr);
    q->arr = new_arr;

    q->head = 0;
    q->tail = q->size % new_capacity;
}

#endif /* WC_QUEUE_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_WC_MACROS_SINGLE_H */
