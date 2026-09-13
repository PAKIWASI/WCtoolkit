#ifndef WC_MATRIX_SINGLE_H
#define WC_MATRIX_SINGLE_H

/*
 * matrix_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "matrix_single.h"
 *
 * All other files just:
 *     #include "matrix_single.h"
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

/* ===== arena.h ===== */
#ifndef WC_ARENA_H
#define WC_ARENA_H

#include <stdlib.h>


typedef struct {
    u8* base;
    u64 idx;
    u64 size;
} Arena;


// Tweakable settings
#ifndef ARENA_DEFAULT_ALIGNMENT
    #define ARENA_DEFAULT_ALIGNMENT (sizeof(void*)) // 8 bytes
#endif
#ifndef ARENA_DEFAULT_SIZE
    #define ARENA_DEFAULT_SIZE      (nKB(4))      // 4 KB
#endif


/*
Allocate and return a pointer to memory to the Arena
with a region with the specified size. Providing a
size = 0 results in size = ARENA_DEFAULT_SIZE (user can modify)

Parameters:
  u64 size    |    The size (in bytes) of the Arena
                      memory region.
Return:
  Pointer to Arena on success, NULL on failure
*/
Arena* Arena_create(u64 capacity) __attribute__((warn_unused_result));

/*
Initialize an Arena object with pointers to the Arena and a
pre-allocated region(base ptr), as well as the size of the provided
region. Good for using the Stack instead of the heap.
The Arena and the data may be Stack initialized, so no Arena_destroy.
Note that ARENA_DEFAULT_SIZE is not used.

Parameters:
  Arena* Arena    |   The Arena object being initialized.
  u8*    data     |   The region to be Arena-fyed.
  u64    size     |   The size of the region in bytes.
*/
void Arena_create_arr_stk(Arena* arena, u64 size, u8* data) __attribute__((nonnull(1, 3)));



void Arena_create_stk(Arena* arena, u64 capacity) __attribute__((nonnull(1)));

/*
Reset the pointer to the Arena region to the beginning
of the allocation. Allows reuse of the memory without
expensive frees.

Parameters:
  Arena *Arena    |    The Arena to be cleared.
*/
static inline __attribute__((nonnull(1))) void Arena_clear(Arena* Arena)
{
    Arena->idx = 0;
}

/*
Free the memory allocated for the entire Arena region.

Parameters:
  Arena *Arena    |    The Arena to be destroyed.
*/
static inline __attribute__((nonnull(1))) void Arena_destroy(Arena* Arena)
{
    free(Arena->base);
    free(Arena);
}

/*
Return a pointer to a portion of specified size of the
specified Arena's region. By default, memory is
aligned by alignof(size_t), but you can change this by
#defining ARENA_DEFAULT_ALIGNMENT before #include'ing
Arena.h. Providing a size of zero results in a failure.

Parameters:
  Arena* Arena    |    The Arena of which the pointer
                       from the region will be
                       distributed
  u64 size        |    The size (in bytes) of
                       allocated memory planned to be
                       used.
Return:
  Pointer to Arena region segment on success, NULL on
  failure.
*/
u8* Arena_alloc(Arena* Arena, u64 size) __attribute__((nonnull(1), alloc_size(2)));

/*
Same as Arena_alloc, except you can specify a memory
alignment for allocations.

Return a pointer to a portion of specified size of the
specified Arena's region. Providing a size of
zero results in a failure.

Parameters:
  Arena* Arena              |    The Arena of which the pointer
                                 from the region will be
                                 distributed
  u64 size                  |    The size (in bytes) of
                                 allocated memory planned to be
                                 used.
  u32 alignment             |    Alignment (in bytes) for each
                                 memory allocation.
Return:
  Pointer to Arena region segment on success, NULL on
  failure.
*/
u8* Arena_alloc_aligned(Arena* Arena, u64 size, u32 alignment) __attribute__((nonnull(1), alloc_size(2)));


// Get used capacity
static inline __attribute__((nonnull(1))) u64 Arena_used(Arena* Arena)
{
    return Arena->idx;
}

// Get remaining capacity
static inline __attribute__((nonnull(1))) u64 Arena_remaining(Arena* Arena)
{
    return Arena->size - Arena->idx;
}



// explicit scratch Arena

typedef struct {
    Arena* Arena;
    u64 mark;
} ArenaScratch;


static inline __attribute__((nonnull(1))) ArenaScratch Arena_scratch_begin(Arena* Arena)
{
    return (ArenaScratch){ .Arena = Arena, .mark = Arena->idx };
}

static inline void Arena_scratch_end(ArenaScratch scratch)
{
    if (scratch.Arena) {
        scratch.Arena->idx = scratch.mark;
        scratch.Arena = NULL;
    }
}

static inline void wc_Arena_scratch_cleanup(ArenaScratch* s)
{
    if (s && s->Arena) {
        s->Arena->idx = s->mark;
        s->Arena = NULL;
    }
}

// macro for automatic cleanup Arena_scratch — safe with return/break/goto
#define ARENA_SCRATCH(Arena_ptr)                                                                             \
    for (int _as_once = 1; _as_once; _as_once = 0)                                                          \
        for (ArenaScratch __attribute__((cleanup(wc_Arena_scratch_cleanup))) _as_s = Arena_scratch_begin(Arena_ptr); \
             _as_once; _as_once = 0)

/* USAGE:
// Manual:
ScratchArena scratch = Arena_scratch_begin(Arena);
char* tmp = ARENA_ALLOC_N(Arena, char, 256);
Arena_scratch_end(scratch);

// Automatic:
ARENA_SCRATCH(Arena) {
    char* tmp = ARENA_ALLOC_N(Arena, char, 256);
} // auto cleanup
*/


// USEFULL MACROS

#define ARENA_CREATE_STK_ARR(Arena, n) (Arena_create_arr_stk((Arena), nKB(n), (u8[nKB(n)]){0}))

// typed allocation
#define ARENA_ALLOC(Arena, T) ((T*)Arena_alloc((Arena), sizeof(T)))

#define ARENA_ALLOC_N(Arena, T, n) ((T*)Arena_alloc((Arena), sizeof(T) * (n)))

// common for structs
#define ARENA_ALLOC_ZERO(Arena, T) ((T*)memset(ARENA_ALLOC(Arena, T), 0, sizeof(T)))

#define ARENA_ALLOC_ZERO_N(Arena, T, n) ((T*)memset(ARENA_ALLOC_N(Arena, T, n), 0, sizeof(T) * (n)))

// Allocate and copy array into Arena
#define ARENA_PUSH_ARRAY(Arena, T, src, count)      \
    ({                                              \
        (T)* _dst = ARENA_ALLOC_N(Arena, T, count); \
        memcpy(_dst, src, sizeof(T) * (count));     \
        _dst;                                       \
    })

#endif /* WC_ARENA_H */

/* ===== matrix.h ===== */
#ifndef WC_MATRIX_H
#define WC_MATRIX_H

#include <string.h>



// ROW MAJOR 2D MATRIX
typedef struct {
    float* data;
    u64    m; // rows
    u64    n; // cols
} Matrixf;


// CREATION AND DESTRUCTION
// ============================================================================

// create heap matrix with m rows and n cols
Matrixf* matrix_create(u64 m, u64 n) __attribute__((warn_unused_result));

// create heap matrix with m rows and n cols and an array of size m x n
Matrixf* matrix_create_arr(u64 m, u64 n, const float* arr) __attribute__((nonnull(3), warn_unused_result));

// create matrix with everything on the Stack
void matrix_create_stk(Matrixf* mat, u64 m, u64 n, float* data) __attribute__((nonnull(1, 4)));

// destroy the matrix created with matrix_create or matrix_create_arr
// DO NOT use on Stack-allocated matrices (created with matrix_create_stk)
void matrix_destroy(Matrixf* mat) __attribute__((nonnull(1)));
// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void matrix_copy(Matrixf* dest, const Matrixf* src) __attribute__((nonnull(1, 2)));
void matrix_move(Matrixf* dest, Matrixf** src) __attribute__((nonnull(1, 2)));


// SETTERS
// ============================================================================

/* Preferred method for setting values from arrays
   For direct arrays (float[len]){...} ROW MAJOR or (float[row][col]){{...},{...}} MORE EXPLICIT
   
   Usage:
       matrix_set_val_arr(mat, 9, (float*)(float[3][3]){
           {1, 2, 3},
           {4, 5, 6},
           {7, 8, 9}
       });
*/
void matrix_set_val_arr(Matrixf* mat, u64 count, const float* arr) __attribute__((nonnull(1, 3)));

// for 2D arrays (array of pointers)
void matrix_set_val_arr2(Matrixf* mat, u64 m, u64 n, const float** arr2) __attribute__((nonnull(1, 4)));

// set the value at position (i, j) where i is row and j is column
void matrix_set_elm(Matrixf* mat, float elm, u64 i, u64 j) __attribute__((nonnull(1)));

float matrix_get_elm(const Matrixf* mat, u64 i, u64 j) __attribute__((nonnull(1)));


// BASIC OPERATIONS
// ============================================================================

// Matrix addition: out = a + b
// out must NOT alias a or b (restrict enables auto-vectorization)
void matrix_add(Matrixf* restrict out, const Matrixf* restrict a, const Matrixf* restrict b) __attribute__((nonnull(1, 2, 3)));

// Matrix subtraction: out = a - b
// out must NOT alias a or b (restrict enables auto-vectorization)
void matrix_sub(Matrixf* restrict out, const Matrixf* restrict a, const Matrixf* restrict b) __attribute__((nonnull(1, 2, 3)));

// Scalar multiplication: mat = mat * val
void matrix_scale(Matrixf* restrict mat, float val) __attribute__((nonnull(1)));

// Element wise division
void matrix_div(Matrixf* restrict mat, float val) __attribute__((nonnull(1)));


// MATRIX MULTIPLICATION
// ============================================================================

// Matrix multiplication: out = a × b
// (m×k) * (k×n) = (m×n)
// out must NOT alias a or b
// Uses blocked ikj multiplication for cache efficiency (good for small-medium matrices)
void matrix_xply(Matrixf* restrict out, const Matrixf* restrict a, const Matrixf* restrict b) __attribute__((nonnull(1, 2, 3)));

// Matrix multiplication variant 2: out = a × b
// Transposes b internally for better cache locality
// Takes more memory but can be faster for large matrices
// out must NOT alias a or b
void matrix_xply_2(Matrixf* restrict out, const Matrixf* restrict a, const Matrixf* restrict b) __attribute__((nonnull(1, 2, 3)));



// ADVANCED OPERATIONS
// ============================================================================

// Transpose: out = mat^T
// out must NOT alias mat
void matrix_T(Matrixf* restrict out, const Matrixf* restrict mat) __attribute__((nonnull(1, 2)));

// LU Decomposition: mat = L × U
// Decomposes square matrix into Lower and Upper triangular matrices
void matrix_LU_Decomp(Matrixf* restrict L, Matrixf* restrict U, const Matrixf* restrict mat) __attribute__((nonnull(1, 2, 3)));

// Calculate determinant using LU decomposition
float matrix_det(const Matrixf* mat) __attribute__((nonnull(1)));

// Calculate adjugate (adjoint) matrix
// TODO: NOT IMPLEMENTED
void matrix_adj(Matrixf* out, const Matrixf* mat) __attribute__((nonnull(1, 2)));

// Calculate matrix inverse: out = mat^(-1)
// TODO: NOT IMPLEMENTED
void matrix_inv(Matrixf* out, const Matrixf* mat) __attribute__((nonnull(1, 2)));


// UTILITIES
// ============================================================================

// print the formatted, aligned matrix to stdout
void matrix_print(const Matrixf* mat) __attribute__((nonnull(1)));


#define MATRIX_TOTAL(mat)    ((u64)((mat)->n * (mat)->m))
#define IDX(mat, i, j)       (((i) * (mat)->n) + (j))
#define MATRIX_AT(mat, i, j) ((mat)->data[((i) * (mat)->n) + (j)])

#define ZEROS_1D(n)    ((float[n]){0})
#define ZEROS_2D(m, n) ((float[m][n]){0})



// ARENA-BASED MATRIX ALLOCATION MACROS
// ============================================================================
/*
Create a matrix allocated from Arena (heap-style)
Matrix struct and data both allocated from Arena
No need to call matrix_destroy - freed when Arena is cleared/released

Usage:
    Matrix* mat = MATRIX_ARENA(Arena, 3, 3);
*/
__attribute__((nonnull(1))) static inline Matrixf* matrix_Arena_alloc(Arena* arena, u64 m, u64 n)
{
    CHECK_FATAL(m == 0 && n == 0, "n == m == 0");

    Matrixf* mat = ARENA_ALLOC(arena, Matrixf);
    CHECK_FATAL(!mat, "matrix Arena allocation failed");

    mat->m = m;
    mat->n = n;

    mat->data = ARENA_ALLOC_N(arena, float, (u64)(m * n));
    CHECK_FATAL(!mat->data, "matrix data Arena allocation failed");

    return mat;
}

/*
Create a matrix allocated from Arena with initial values
Matrix struct and data allocated from Arena

Usage:
    Matrix* mat = MATRIX_ARENA_ARR(Arena, 3, 3, (float[9]){1,2,3,4,5,6,7,8,9});
*/

__attribute__((nonnull(1, 4))) static inline Matrixf* matrix_Arena_arr_alloc(Arena* arena, u64 m, u64 n, const float* arr)
{
    CHECK_FATAL(m == 0 || n == 0, "matrix dims must be > 0");

    Matrixf* mat = matrix_Arena_alloc(arena, m, n);
    memcpy(mat->data, arr, sizeof(float) * m * n);
    return mat;
}

#endif /* WC_MATRIX_H */

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

/* ===== arena.c ===== */
#ifndef WC_ARENA_IMPL
#define WC_ARENA_IMPL

#include <stdlib.h>

/*'''python
align a 4 byte thing to 8 bytes alignment boundry:
>>> 4 + (8 - 1) & ~(8 - 1)
8
>>> 7 + (8 - 1) & ~(8 - 1)
8
>>> 9 + (8 - 1) & ~(8 - 1)
16 <- how much bytes should a 9 byte thing occupy to align to boundry
>>> 15 + (8 - 1) & ~(8 - 1)
16
>>> 18 + (8 - 1) & ~(8 - 1)
24
'''*/
// Align a value to alignment boundary
// Note: align MUST be power of 2 and >= 1
#define ALIGN_UP(val, align) \
    ((align) == 0 ? (val) : (((val) + ((align) - 1)) & ~((align) - 1)))

// align value to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_UP_DEFAULT(val) \
    ALIGN_UP((val), ARENA_DEFAULT_ALIGNMENT)


#define ARENA_PTR(Arena, idx) ((Arena)->base + (idx))





Arena* Arena_create(u64 capacity)
{
    if (capacity == 0) {
        capacity = ARENA_DEFAULT_SIZE;
    }

    Arena* arena = (Arena*)malloc(sizeof(Arena));
    CHECK_FATAL(!arena, "Arena malloc failed");

    arena->base = (u8*)malloc(capacity);
    CHECK_FATAL(!arena->base, "Arena base malloc failed");

    arena->idx = 0;
    arena->size = capacity;

    return arena;
}

void Arena_create_stk(Arena* arena, u64 capacity)
{
    if (capacity == 0) {
        capacity = ARENA_DEFAULT_SIZE;
    }

    arena->base = (u8*)malloc(capacity);
    CHECK_FATAL(!arena->base, "Arena base malloc failed");

    arena->idx  = 0;
    arena->size = capacity;
}

void Arena_create_arr_stk(Arena* arena, u64 size, u8* data)
{
    CHECK_FATAL(size == 0, "size can't be zero");

    arena->base = data;
    arena->idx = 0;
    arena->size = size;
}

u8* Arena_alloc(Arena* arena, u64 size)
{
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");

    // Align the current index first
    u64 aligned_idx = ALIGN_UP_DEFAULT(arena->idx);
    WC_SET_RET(WC_ERR_FULL, arena->size - aligned_idx < size, NULL);

    u8* ptr = ARENA_PTR(arena, aligned_idx);
    arena->idx = aligned_idx + size;

    return ptr;
}

u8* Arena_alloc_aligned(Arena* arena, u64 size, u32 alignment)
{

    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    CHECK_FATAL((alignment & (alignment - 1)) != 0,
                "alignment must be power of two");


    u64 aligned_idx = ALIGN_UP(arena->idx, alignment);

    WC_SET_RET(WC_ERR_FULL, arena->size - aligned_idx < size, NULL);

    u8* ptr = ARENA_PTR(arena, aligned_idx);
    arena->idx = aligned_idx + size;

    return ptr;
}

#endif /* WC_ARENA_IMPL */

/* ===== matrix.c ===== */
#ifndef WC_MATRIX_IMPL
#define WC_MATRIX_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



Matrixf* matrix_create(u64 m, u64 n)
{
    CHECK_FATAL(n == 0 && m == 0, "n == m == 0");

    Matrixf* mat = (Matrixf*)malloc(sizeof(Matrixf));
    CHECK_FATAL(!mat, "matrix malloc failed");

    mat->m    = m;
    mat->n    = n;
    mat->data = (float*)malloc(sizeof(float) * n * m);
    CHECK_FATAL(!mat->data, "matrix data malloc failed");

    return mat;
}

Matrixf* matrix_create_arr(u64 m, u64 n, const float* arr)
{
    Matrixf* mat = matrix_create(m, n);
    memcpy(mat->data, arr, sizeof(float) * m * n);
    return mat;
}

void matrix_create_stk(Matrixf* mat, u64 m, u64 n, float* data)
{
    // we can do this on the Stack
    mat->data = data; // copying stk ptr 
    mat->m    = m;
    mat->n    = n;
}

void matrix_destroy(Matrixf* mat)
{
    free(mat->data);
    free(mat);
}


void matrix_set_val_arr(Matrixf* mat, u64 count, const float* arr)
{
    CHECK_FATAL(count != MATRIX_TOTAL(mat), "count doesn't match matrix size");

    memcpy(mat->data, arr, sizeof(float) * count);
}

void matrix_set_val_arr2(Matrixf* mat, u64 m, u64 n, const float** arr2)
{
    CHECK_FATAL(!*arr2, "*arr is null");
    CHECK_FATAL(m != mat->m || n != mat->n,
                "mat dimentions dont match passed arr2");

    u64 idx = 0;
    for (u64 i = 0; i < m; i++) {
        memcpy(mat->data + idx, arr2[i], sizeof(float) * n);
        idx += n;
    }
}

void matrix_set_elm(Matrixf* mat, float elm, u64 i, u64 j)
{
    CHECK_FATAL(i >= mat->m || j >= mat->n, "index out of bounds");

    mat->data[IDX(mat, i, j)] = elm;
}

float matrix_get_elm(const Matrixf* mat, u64 i, u64 j)
{
    CHECK_FATAL(i >= mat->m || j >= mat->n, "index out of bounds");

    return mat->data[IDX(mat, i, j)];
}

void matrix_add(Matrixf* restrict out, const Matrixf* restrict a, const Matrixf* restrict b)
{
    CHECK_FATAL(a->m != b->m || a->n != b->n || a->m != out->m ||
                    a->n != out->n,
                "a, b, out mat dimentions dont match");

    u64 total = MATRIX_TOTAL(a);

    for (u64 i = 0; i < total; i++) { 
        out->data[i] = a->data[i] + b->data[i]; 
    }
}


void matrix_sub(Matrixf* restrict out, const Matrixf* restrict a, const Matrixf* restrict b)
{
    // FIXED: Added dimension check for 'out' matrix
    CHECK_FATAL(a->m != b->m || a->n != b->n || a->m != out->m ||
                    a->n != out->n,
                "a, b, out mat dimentions dont match");

    u64 total = MATRIX_TOTAL(a);

    for (u64 i = 0; i < total; i++) { 
        out->data[i] = a->data[i] - b->data[i]; 
    }
}

// ikj multiplication. (mxk) * (kxn) = (mxn)
// this is good for small to medium size matrices
void matrix_xply(Matrixf* restrict out, const Matrixf* restrict a, const Matrixf* restrict b)
{
    CHECK_FATAL(a->n != b->m,
                "incompatible matrix dimensions for multiplication");
    CHECK_FATAL(out->m != a->m || out->n != b->n,
                "output matrix has wrong dimensions");

    u64 m = a->m; // rows of A
    u64 k = a->n; // cols of A = rows of B
    u64 n = b->n; // cols of B

    // Initialize output to zero
    memset(out->data, 0, sizeof(float) * m * n);

    // Block size for cache optimization
    const u64 BLOCK_SIZE = 16;

    // Blocked matrix multiplication (ikj order for cache efficiency)
    for (u64 i = 0; i < m; i += BLOCK_SIZE) {
        for (u64 k_outer = 0; k_outer < k; k_outer += BLOCK_SIZE) {
            for (u64 j = 0; j < n; j += BLOCK_SIZE) {
                // Block boundaries
                u64 i_max = (i + BLOCK_SIZE < m) ? i + BLOCK_SIZE : m;
                u64 k_max =
                    (k_outer + BLOCK_SIZE < k) ? k_outer + BLOCK_SIZE : k;
                u64 j_max = (j + BLOCK_SIZE < n) ? j + BLOCK_SIZE : n;
                // Multiply this block
                for (u64 ii = i; ii < i_max; ii++) {
                    for (u64 kk = k_outer; kk < k_max; kk++) {

                        float a_val = a->data[IDX(a, ii, kk)];
                        for (u64 jj = j; jj < j_max; jj++) {
                            out->data[IDX(out, ii, jj)] +=
                                a_val * b->data[IDX(b, kk, jj)];
                        }
                    }
                }
            }
        }
    }

}


// this function transposes b for cache-friendly access
// takes more memory, good for large size matrices
void matrix_xply_2(Matrixf* restrict out, const Matrixf* restrict a, const Matrixf* restrict b)
{
    CHECK_FATAL(a->n != b->m, "incompatible matrix dimensions");
    CHECK_FATAL(out->m != a->m || out->n != b->n,
                "output matrix has wrong dimensions");

    u64 m = a->m;
    u64 k = a->n;
    u64 n = b->n;

    // Transpose B for cache-friendly access
    Matrixf b_T;
    float  data[n * k]; // random vals
    matrix_create_stk(&b_T, n, k, data);
    matrix_T(&b_T, b); // transpose sets all vals

    memset(out->data, 0, sizeof(float) * m * n);

    const u64 BLOCK_SIZE = 16;

    // Now both A and B^T are accessed row-wise
    for (u64 i = 0; i < m; i += BLOCK_SIZE) {
        for (u64 j = 0; j < n; j += BLOCK_SIZE) {
            u64 i_max = (i + BLOCK_SIZE < m) ? i + BLOCK_SIZE : m;
            u64 j_max = (j + BLOCK_SIZE < n) ? j + BLOCK_SIZE : n;

            for (u64 ii = i; ii < i_max; ii++) {
                for (u64 jj = j; jj < j_max; jj++) {

                    float sum = 0;
                    // Dot product of A[ii] and B_T[jj] (both row-wise)
                    for (u64 kk = 0; kk < k; kk++) {
                        sum += a->data[IDX(a, ii, kk)] *
                               // b_T->data[IDX(b_T, jj, kk)];
                               b_T.data[IDX(&b_T, jj, kk)];
                    }
                    out->data[IDX(out, ii, jj)] = sum;
                }
            }
        }
    }

}

/*
Doolittle algorithm computes U's i-th row, then L's i-th column, alternating.
For each element, you subtract the dot product of already-computed L and U values.
*/
void matrix_LU_Decomp(Matrixf* restrict L, Matrixf* restrict U, const Matrixf* restrict mat)
{
    CHECK_FATAL(mat->n != mat->m, "mat is not a square matrix");
    CHECK_FATAL(L->n != mat->n || L->m != mat->m, "L dimensions don't match");
    CHECK_FATAL(U->n != mat->n || U->m != mat->m, "U dimensions don't match");

    const u64 n = mat->n;

    // 0 init matrices
    memset(L->data, 0, sizeof(float) * n * n);
    memset(U->data, 0, sizeof(float) * n * n);
    // L main diagonal is 1
    for (u64 i = 0; i < n; i++) { L->data[IDX(L, i, i)] = 1; }


    // Build U and L row by row
    for (u64 i = 0; i < n; i++) {
        // Upper triangular matrix U (row i, columns from i to n-1)
        for (u64 k = i; k < n; k++) {
            float sum = 0;
            for (u64 j = 0; j < i; j++) {
                sum += L->data[IDX(L, i, j)] * U->data[IDX(U, j, k)];
            }
            U->data[IDX(U, i, k)] = MATRIX_AT(mat, i, k) - sum;
        }

        // Lower triangular matrix L (column i, rows from i+1 to n-1)
        for (u64 k = i + 1; k < n; k++) {
            float sum = 0;
            for (u64 j = 0; j < i; j++) {
                sum += L->data[IDX(L, k, j)] * U->data[IDX(U, j, i)];
            }

            // Check for zero diagonal in U (would cause division by zero)
            if (U->data[IDX(U, i, i)] == 0) {
                CHECK_FATAL(1, "Matrix is singular - LU decomposition failed");
            }

            L->data[IDX(L, k, i)] =
                (MATRIX_AT(mat, k, i) - sum) / U->data[IDX(U, i, i)];
        }
    }
}

/*
    det of triangular mat is product of main diagonal
    so det of a mat that is decomposed with LU method becomes
    product of elements on the diagonal of L and U
    det(A) = det(L) * det(U)
    
    Since L has 1s on diagonal: det(L) = 1
    So: det(A) = det(U) = product of U's diagonal elements
    
    LU Decomposition is when we make 2 triangular matrices from one,
    which when multiplied give original matrix: A = L * U
*/
float matrix_det(const Matrixf* mat)
{
    CHECK_FATAL(mat->m != mat->n, "only square matrices have determinant");


    u64 n = mat->n;

    Matrixf L, U;
    float  Ldata[n * n]; // random vals
    float  Udata[n * n];
    matrix_create_stk(&L, n, n, Ldata);
    matrix_create_stk(&U, n, n, Udata);

    // Perform LU decomposition
    matrix_LU_Decomp(&L, &U, mat); // L and U set to zero

    // Calculate determinant as product of U's diagonal
    float det = 1;
    for (u64 i = 0; i < n; i++) { det *= U.data[IDX(&U, i, i)]; }

    return det;
}


void matrix_T(Matrixf* restrict out, const Matrixf* restrict mat)
{
    CHECK_FATAL(mat->m != out->n || mat->n != out->m,
                "incompatible matrix dimensions");

    // Block size for cache optimization (tune based on cache line size)
    const u64 BLOCK_SIZE = 16; // TODO: user adjustable macro?

    // Blocked transpose: process matrix in BLOCK_SIZE x BLOCK_SIZE tiles
    for (u64 i = 0; i < mat->m; i += BLOCK_SIZE) {
        for (u64 j = 0; j < mat->n; j += BLOCK_SIZE) {

            // Calculate block boundaries
            u64 i_max = (i + BLOCK_SIZE < mat->m) ? i + BLOCK_SIZE : mat->m;
            u64 j_max = (j + BLOCK_SIZE < mat->n) ? j + BLOCK_SIZE : mat->n;

            // Transpose the block
            for (u64 ii = i; ii < i_max; ii++) {
                for (u64 jj = j; jj < j_max; jj++) {
                    // mat[ii][jj] -> out[jj][ii]
                    out->data[IDX(out, jj, ii)] = mat->data[IDX(mat, ii, jj)];
                }
            }
        }
    }
}

void matrix_scale(Matrixf* restrict mat, float val)
{
    u64 total = MATRIX_TOTAL(mat);
    for (u64 i = 0; i < total; i++) { mat->data[i] *= val; }
}


void matrix_div(Matrixf* restrict mat, float val)
{
    CHECK_FATAL(val == 0, "division by zero!");

    u64 total = MATRIX_TOTAL(mat);
    for (u64 i = 0; i < total; i++) { mat->data[i] /= val; }
}

void matrix_copy(Matrixf* dest, const Matrixf* src)
{
    if (dest == src) {
        return;
    }

    u64 count = src->m * src->n;
    dest->data = malloc(count * sizeof(float));
    CHECK_FATAL(!dest->data, "matrix copy malloc failed");
    memcpy(dest->data, src->data, count * sizeof(float));

    dest->m = src->m;
    dest->n = src->n;
}

void matrix_move(Matrixf* dest, Matrixf** src)
{
    if (dest == *src) {
        *src = NULL;
        return;
    }

    memcpy(dest, *src, sizeof(Matrixf));
    free(*src);
    *src = NULL;
}

void matrix_print(const Matrixf* mat)
{
    u64 total = mat->m * mat->n;

    // Single linear loop O(n)
    for (u64 i = 0; i < total; i++) {
        // Print row separator
        if (i % mat->n == 0) {           // divide by columns to detect new row
            if (i > 0) { putchar('|'); } // Close previous row
            putchar('\n');
            putchar('|');
            putchar(' ');
        }

        // Print element
        printf("%f ", mat->data[i]);
    }

    // Close last row
    putchar('|');
    putchar('\n');
}

#endif /* WC_MATRIX_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_MATRIX_SINGLE_H */
