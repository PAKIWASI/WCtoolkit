#ifndef WC_CHAIN_ARENA_SINGLE_H
#define WC_CHAIN_ARENA_SINGLE_H

/*
 * chain_arena_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "chain_arena_single.h"
 *
 * All other files just:
 *     #include "chain_arena_single.h"
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

/* ===== chain_arena.h ===== */
#ifndef WC_CHAIN_ARENA_H
#define WC_CHAIN_ARENA_H

#ifndef ARENA_DEFAULT_ALIGNMENT
#define ARENA_DEFAULT_ALIGNMENT (sizeof(void*)) // 8 bytes
#endif

#ifndef ARENA_NODE_INLINE_SIZE
#define ARENA_NODE_INLINE_SIZE (nKB(4) - 8) // 4096 bytes
#endif

typedef struct ArenaNode {
    u64 used; // bytes consumed inside this node
    u8  base[ARENA_NODE_INLINE_SIZE];
} ArenaNode;

// NOT COPYABLE: backs a bump allocator; external pointers into it would be invalidated.
typedef struct {
    GenVec nodes; // vector of ArenaNode*
    u64    used;  // total bytes allocated (sum of all node->used).
                  // Used for scratch save/restore.
} ChainArena;

typedef struct {
    ChainArena* Arena;
    u64         node_idx;        // index of the last node at scratch start
    u64         node_used_mark;  // that node's `used` value at scratch start
    u64         Arena_used_mark; // Arena->used at scratch start
} ChainArenaScratch;

// TODO: any node release strategies


ChainArena* chain_Arena_create(void) __attribute__((warn_unused_result));

void chain_Arena_destroy(ChainArena* Arena) __attribute__((nonnull(1)));


u8* chain_Arena_alloc_aligned(ChainArena* Arena, u64 size, u32 align) __attribute__((nonnull(1), alloc_size(2)));

static inline __attribute__((nonnull(1), alloc_size(2))) u8* chain_Arena_alloc(ChainArena* Arena, u64 size)
{
    return chain_Arena_alloc_aligned(Arena, size, ARENA_DEFAULT_ALIGNMENT);
}


// reset back to initial state with a single free node
void chain_Arena_reset(ChainArena* Arena) __attribute__((nonnull(1)));

// clear all space but dont free any nodes
void chain_Arena_clear(ChainArena* Arena) __attribute__((nonnull(1)));


ChainArenaScratch chain_Arena_scratch_begin(ChainArena* Arena) __attribute__((nonnull(1)));

void chain_Arena_scratch_end(ChainArenaScratch scratch);

#define CHAIN_ARENA_SCRATCH(c_Arena_ptr)                                                              \
    for (ChainArenaScratch __nme__ = chain_Arena_scratch_begin(c_Arena_ptr); (__nme__).Arena != NULL; \
         chain_Arena_scratch_end((__nme__)), (__nme__).Arena = NULL)


// Typed allocation macros
#define CHAIN_ARENA_ALLOC(Arena, T)           ((T*)chain_Arena_alloc((Arena), sizeof(T)))
#define CHAIN_ARENA_ALLOC_N(Arena, T, n)      ((T*)chain_Arena_alloc((Arena), sizeof(T) * (n)))
#define CHAIN_ARENA_ALLOC_ZERO(Arena, T)      ((T*)memset(CHAIN_ARENA_ALLOC(Arena, T), 0, sizeof(T)))
#define CHAIN_ARENA_ALLOC_ZERO_N(Arena, T, n) ((T*)memset(CHAIN_ARENA_ALLOC_N(Arena, T, n), 0, sizeof(T) * (n)))

#endif /* WC_CHAIN_ARENA_H */

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

/* ===== chain_arena.c ===== */
#ifndef WC_CHAIN_ARENA_IMPL
#define WC_CHAIN_ARENA_IMPL

#include <stdlib.h>


// Align `val` up to the next multiple of `align` (power of 2).
#define ALIGN_UP(val, align) \
    (((align) == 0) ? (val) : (((val) + ((align) - 1)) & ~((align) - 1)))

// Access the last node (assumes at least one node exists).
#define LAST_NODE(Arena) (*(ArenaNode**)GenVec_back(&(Arena)->nodes))
#define REMAINING(node)  (ARENA_NODE_INLINE_SIZE - (node)->used)
#define NODE_PTR(node)   ((node)->base + (node)->used)

#define NODES_INIT_SIZE 10


static inline ArenaNode* append_node(ChainArena* Arena);


// GenVec operations for ArenaNode*
static void chain_move(u8* dest, u8** src)
{
    *(ArenaNode**)dest = *(ArenaNode**)src;
    *(ArenaNode**)src  = NULL;
}

static void chain_del(u8* key)
{
    free(*(ArenaNode**)key);
}

static container_ops chain_ops_ptr = {
    .copy_fn = NULL,
    .move_fn = chain_move,
    .del_fn  = chain_del
};


// Public API

ChainArena* chain_Arena_create(void)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    CHECK_FATAL(!n, "node malloc failed");
    n->used = 0;

    ChainArena* Arena = malloc(sizeof(ChainArena));
    CHECK_FATAL(!Arena, "Arena malloc failed");

    GenVec_create_stk(&Arena->nodes, NODES_INIT_SIZE, sizeof(ArenaNode*), &chain_ops_ptr);
    GenVec_push_move(&Arena->nodes, (u8**)&n);   // initial node

    Arena->used = 0;
    return Arena;
}

void chain_Arena_destroy(ChainArena* Arena)
{
    GenVec_destroy_stk(&Arena->nodes);   // frees all nodes via chain_del
    free(Arena);
}

u8* chain_Arena_alloc_aligned(ChainArena* Arena, u64 size, u32 align)
{
    CHECK_FATAL(size == 0, "allocation size must be > 0");
    CHECK_FATAL((align & (align - 1)) != 0, "alignment must be power of two");

    ArenaNode* last = LAST_NODE(Arena);

    u64 aligned_idx = ALIGN_UP(last->used, align);
    u64 required = (aligned_idx - last->used) + size;

    if (required > REMAINING(last)) {
        last = append_node(Arena);       // idx automatically bumps inside append_node
        aligned_idx = ALIGN_UP(0, align);
        required = (aligned_idx - 0) + size;
        CHECK_FATAL(required > ARENA_NODE_INLINE_SIZE,
                    "allocation size exceeds node capacity");
    }

    // Update the actual usage counters
    Arena->used += required;
    last->used = aligned_idx;
    u8* ret = NODE_PTR(last);
    last->used += size;

    return ret;
}

// Reset back to initial state: only the first node remains, empty.
void chain_Arena_reset(ChainArena* Arena)
{
    // Remove all nodes except the first one
    u64 total = GenVec_size(&Arena->nodes);
    if (total > 1) {
        GenVec_remove_range(&Arena->nodes, 1, total - 1);
    }

    // Reset the first node
    (*(ArenaNode**)GenVec_get_ptr_mut(&Arena->nodes, 0))->used = 0;

    Arena->used = 0;
}

// clear all space but dont free any nodes
void chain_Arena_clear(ChainArena* Arena)
{
    u64 node_count = GenVec_size(&Arena->nodes);
    for (u64 i = 0; i < node_count; i++) {
        (*(ArenaNode**)GenVec_get_ptr_mut(&Arena->nodes, i))->used = 0;
    }
    Arena->used = 0;
}


// Scratch API

ChainArenaScratch chain_Arena_scratch_begin(ChainArena* Arena)
{
    u64 node_idx = GenVec_size(&Arena->nodes) - 1;
    ArenaNode* last = LAST_NODE(Arena);

    return (ChainArenaScratch){
        .Arena           = Arena,
        .node_idx        = node_idx,
        .node_used_mark  = last->used,
        .Arena_used_mark = Arena->used
    };
}

void chain_Arena_scratch_end(ChainArenaScratch scratch)
{
    if (!scratch.Arena) { return; }
    ChainArena* a = scratch.Arena;

    // 1. Restore the target node’s fill level
    (*(ArenaNode**)GenVec_get_ptr_mut(&a->nodes, scratch.node_idx))
        ->used = scratch.node_used_mark;

    // 2. Remove all nodes that were appended after the saved node
    u64 total = GenVec_size(&a->nodes);
    if (total > scratch.node_idx + 1) {
        GenVec_remove_range(&a->nodes, scratch.node_idx + 1,
                            total - (scratch.node_idx + 1));
    }

    // 3. Restore the global counters
    a->used = scratch.Arena_used_mark;

    scratch.Arena = NULL;   // mark as consumed
}


// Private helpers

static inline ArenaNode* append_node(ChainArena* Arena)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    CHECK_FATAL(!n, "node malloc failed");
    n->used = 0;

    ArenaNode* ret = n;
    GenVec_push_move(&Arena->nodes, (u8**)&n);  // push move consumes n

    return ret;
}

#endif /* WC_CHAIN_ARENA_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_CHAIN_ARENA_SINGLE_H */
