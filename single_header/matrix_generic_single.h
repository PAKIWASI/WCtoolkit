#ifndef WC_MATRIX_GENERIC_SINGLE_H
#define WC_MATRIX_GENERIC_SINGLE_H

/*
 * matrix_generic_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "matrix_generic_single.h"
 *
 * All other files just:
 *     #include "matrix_generic_single.h"
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

/* ===== arena.h ===== */
#ifndef WC_ARENA_H
#define WC_ARENA_H

typedef struct {
    u8* base;
    u64 idx;
    u64 size;
} Arena;




// Tweakable settings
#define ARENA_DEFAULT_ALIGNMENT (sizeof(u64)) // 8 byte
#define ARENA_DEFAULT_SIZE      (nKB(4))      // 4 KB


/*
Allocate and return a pointer to memory to the arena
with a region with the specified size. Providing a
size = 0 results in size = ARENA_DEFAULT_SIZE (user can modify)

Parameters:
  u64 size    |    The size (in bytes) of the arena
                      memory region.
Return:
  Pointer to arena on success, NULL on failure
*/
Arena* arena_create(u64 capacity);

/*
Initialize an arena object with pointers to the arena and a
pre-allocated region(base ptr), as well as the size of the provided
region. Good for using the stack instead of the heap.
The arena and the data may be stack initialized, so no arena_release.
Note that ARENA_DEFAULT_SIZE is not used.

Parameters:
  Arena* arena    |   The arena object being initialized.
  u8*    data     |   The region to be arena-fyed.
  u64    size     |   The size of the region in bytes.
*/
void arena_create_arr_stk(Arena* arena, u8* data, u64 size);

/*
Reset the pointer to the arena region to the beginning
of the allocation. Allows reuse of the memory without
expensive frees.

Parameters:
  Arena *arena    |    The arena to be cleared.
*/
void arena_clear(Arena* arena);

/*
Free the memory allocated for the entire arena region.

Parameters:
  Arena *arena    |    The arena to be destroyed.
*/
void arena_release(Arena* arena);

/*
Return a pointer to a portion of specified size of the
specified arena's region. Nothing will restrict you
from allocating more memory than you specified, so be
mindful of your memory (as you should anyways) or you
will get some hard-to-track bugs. By default, memory is
aligned by alignof(size_t), but you can change this by
#defining ARENA_DEFAULT_ALIGNMENT before #include'ing
arena.h. Providing a size of zero results in a failure.

Parameters:
  Arena* arena    |    The arena of which the pointer
                       from the region will be
                       distributed
  u64 size        |    The size (in bytes) of
                       allocated memory planned to be
                       used.
Return:
  Pointer to arena region segment on success, NULL on
  failure.
*/
u8* arena_alloc(Arena* arena, u64 size);

/*
Same as arena_alloc, except you can specify a memory
alignment for allocations.

Return a pointer to a portion of specified size of the
specified arena's region. Nothing will restrict you
from allocating more memory than you specified, so be
mindful of your memory (as you should anyways) or you
will get some hard-to-track bugs. Providing a size of
zero results in a failure.

Parameters:
  Arena* arena              |    The arena of which the pointer
                                 from the region will be
                                 distributed
  u64 size                  |    The size (in bytes) of
                                 allocated memory planned to be
                                 used.
  u32 alignment             |    Alignment (in bytes) for each
                                 memory allocation.
Return:
  Pointer to arena region segment on success, NULL on
  failure.
*/
u8* arena_alloc_aligned(Arena* arena, u64 size, u32 alignment);


/*
Get the value of index at the current state of arena
This can be used to later clear upto that point using arena_clear_mark

Parameters:
  Arena* arena          |   The arena whose idx will be returned

Return:
  The current value of idx variable
*/
u64 arena_get_mark(Arena* arena);

/*
Clear the arena from current index back to mark

Parameters:
  Arena* arena          |   The arena you want to clear using it's mark
  u64    mark           |   The mark previosly obtained by arena_get_mark 
*/
void arena_clear_mark(Arena* arena, u64 mark);


// Get used capacity
static inline u64 arena_used(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    return arena->idx;
}

// Get remaining capacity
static inline u64 arena_remaining(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    return arena->size - arena->idx;
}



// explicit scratch arena

typedef struct {
    Arena* arena;
    u64 saved_idx;
} arena_scratch;


static inline arena_scratch arena_scratch_begin(Arena* arena) {
    CHECK_FATAL(!arena, "arena is null");
    return (arena_scratch){ .arena = arena, .saved_idx = arena->idx };
}

static inline void arena_scratch_end(arena_scratch* scratch) {
    if (scratch && scratch->arena) {
        scratch->arena->idx = scratch->saved_idx;
    }
}

// macro for automatic cleanup arena_scratch
#define ARENA_SCRATCH(name, arena_ptr) \
    for (arena_scratch name = arena_scratch_begin(arena_ptr); \
         (name).arena != NULL; \
         arena_scratch_end(&(name)), (name).arena = NULL)

/* USAGE:
// Manual:
ScratchArena scratch = arena_scratch_begin(arena);
char* tmp = ARENA_ALLOC_N(arena, char, 256);
arena_scratch_end(&scratch);

// Automatic:
ARENA_SCRATCH(scratch, arena) {
    char* tmp = ARENA_ALLOC_N(arena, char, 256);
} // auto cleanup
*/


// USEFULL MACROS

#define ARENA_CREATE_STK_ARR(arena, n) (arena_create_arr_stk((arena), (u8[nKB(n)]){0}, nKB(n)))

// typed allocation
#define ARENA_ALLOC(arena, T) ((T*)arena_alloc((arena), sizeof(T)))

#define ARENA_ALLOC_N(arena, T, n) ((T*)arena_alloc((arena), sizeof(T) * (n)))

// common for structs
#define ARENA_ALLOC_ZERO(arena, T) ((T*)memset(ARENA_ALLOC(arena, T), 0, sizeof(T)))

#define ARENA_ALLOC_ZERO_N(arena, T, n) ((T*)memset(ARENA_ALLOC_N(arena, T, n), 0, sizeof(T) * (n)))

// Allocate and copy array into arena
#define ARENA_PUSH_ARRAY(arena, T, src, count)      \
    ({                                              \
        (T)* _dst = ARENA_ALLOC_N(arena, T, count); \
        memcpy(_dst, src, sizeof(T) * (count));     \
        _dst;                                       \
    })

#endif /* WC_ARENA_H */

/* ===== matrix_generic.h ===== */
#ifndef WC_MATRIX_GENERIC_H
#define WC_MATRIX_GENERIC_H

#include <string.h>


// ============================================================================
// GENERIC MATRIX MACRO DEFINITIONS
// ============================================================================

// Define a matrix type for a specific data type
#define MATRIX_TYPE(T)    \
    typedef struct {      \
        T*  data;         \
        u64 m; /* rows */ \
        u64 n; /* cols */ \
    } Matrix_##T


// Helper macros (type-agnostic)
#define MATRIX_TOTAL(mat)    ((u64)((mat)->n * (mat)->m))
#define IDX(mat, i, j)       (((i) * (mat)->n) + (j))
#define MATRIX_AT(mat, i, j) ((mat)->data[((i) * (mat)->n) + (j)])

// Zero initialization helpers
#define ZEROS_1D(T, n)    (((T)[n]){0})
#define ZEROS_2D(T, m, n) (((T)[m][n]){0})

// ============================================================================
// MATRIX CREATION/DESTRUCTION
// ============================================================================

#define MATRIX_CREATE(T)                                           \
    Matrix_##T* matrix_create_##T(u64 m, u64 n)                    \
    {                                                              \
        CHECK_FATAL(n == 0 && m == 0, "n == m == 0");              \
        Matrix_##T* mat = (Matrix_##T*)malloc(sizeof(Matrix_##T)); \
        CHECK_FATAL(!mat, "matrix malloc failed");                 \
        mat->m    = m;                                             \
        mat->n    = n;                                             \
        mat->data = (T*)malloc(sizeof(T) * n * m);                 \
        CHECK_FATAL(!mat->data, "matrix data malloc failed");      \
        return mat;                                                \
    }

#define MATRIX_CREATE_ARR(T)                                      \
    Matrix_##T* matrix_create_arr_##T(u64 m, u64 n, const T* arr) \
    {                                                             \
        CHECK_FATAL(!arr, "input arr is null");                   \
        Matrix_##T* mat = matrix_create_##T(m, n);                \
        memcpy(mat->data, arr, sizeof(T) * m * n);                \
        return mat;                                               \
    }

#define MATRIX_CREATE_STK(T)                                           \
    void matrix_create_stk_##T(Matrix_##T* mat, u64 m, u64 n, T* data) \
    {                                                                  \
        CHECK_FATAL(!mat, "matrix is null");                           \
        CHECK_FATAL(!data, "data is null");                            \
        mat->data = data;                                              \
        mat->m    = m;                                                 \
        mat->n    = n;                                                 \
    }

#define MATRIX_DESTROY(T)                    \
    void matrix_destroy_##T(Matrix_##T* mat) \
    {                                        \
        CHECK_FATAL(!mat, "matrix is null"); \
        free(mat->data);                     \
        free(mat);                           \
    }

// ============================================================================
// MATRIX SETTERS
// ============================================================================

/* Preferred method for setting values from arrays
   For direct arrays (T[len]){...} ROW MAJOR or (T[row][col]){{...},{...}} MORE EXPLICIT
   
   Usage:
       matrix_set_val_arr_T(mat, 9, (T*)(T[3][3]){
           {1, 2, 3},
           {4, 5, 6},
           {7, 8, 9}
       });
*/
#define MATRIX_SET_VAL_ARR(T)                                                       \
    void matrix_set_val_arr_##T(Matrix_##T* mat, u64 count, const T* arr)           \
    {                                                                               \
        CHECK_FATAL(!mat, "matrix is null");                                        \
        CHECK_FATAL(!arr, "arr is null");                                           \
        CHECK_FATAL(count != MATRIX_TOTAL(mat), "count doesn't match matrix size"); \
        memcpy(mat->data, arr, sizeof(T) * count);                                  \
    }

// For 2D arrays (array of pointers)
#define MATRIX_SET_VAL_ARR2(T)                                                            \
    void matrix_set_val_arr2_##T(Matrix_##T* mat, u64 m, u64 n, const T** arr2)           \
    {                                                                                     \
        CHECK_FATAL(!mat, "matrix is null");                                              \
        CHECK_FATAL(!arr2, "arr is null");                                                \
        CHECK_FATAL(!*arr2, "*arr is null");                                              \
        CHECK_FATAL(m != mat->m || n != mat->n, "mat dimensions dont match passed arr2"); \
                                                                                          \
        u64 idx = 0;                                                                      \
        for (u64 i = 0; i < m; i++) {                                                     \
            memcpy(mat->data + idx, arr2[i], sizeof(T) * n);                              \
            idx += n;                                                                     \
        }                                                                                 \
    }

#define MATRIX_SET_ELM(T)                                               \
    void matrix_set_elm_##T(Matrix_##T* mat, T elm, u64 i, u64 j)       \
    {                                                                   \
        CHECK_FATAL(!mat, "matrix is null");                            \
        CHECK_FATAL(i >= mat->m || j >= mat->n, "index out of bounds"); \
        mat->data[IDX(mat, i, j)] = elm;                                \
    }

// ============================================================================
// MATRIX OPERATIONS
// ============================================================================

#define MATRIX_ADD(T)                                                                 \
    void matrix_add_##T(Matrix_##T* out, const Matrix_##T* a, const Matrix_##T* b)    \
    {                                                                                 \
        CHECK_FATAL(!out, "out matrix is null");                                      \
        CHECK_FATAL(!a, "a matrix is null");                                          \
        CHECK_FATAL(!b, "b matrix is null");                                          \
        CHECK_FATAL(a->m != b->m || a->n != b->n || a->m != out->m || a->n != out->n, \
                    "a, b, out mat dimensions don't match");                          \
        u64 total = MATRIX_TOTAL(a);                                                  \
        for (u64 i = 0; i < total; i++) {                                             \
            out->data[i] = a->data[i] + b->data[i];                                   \
        }                                                                             \
    }

#define MATRIX_SUB(T)                                                                 \
    void matrix_sub_##T(Matrix_##T* out, const Matrix_##T* a, const Matrix_##T* b)    \
    {                                                                                 \
        CHECK_FATAL(!out, "out matrix is null");                                      \
        CHECK_FATAL(!a, "a matrix is null");                                          \
        CHECK_FATAL(!b, "b matrix is null");                                          \
        CHECK_FATAL(a->m != b->m || a->n != b->n || a->m != out->m || a->n != out->n, \
                    "a, b, out mat dimensions don't match");                          \
        u64 total = MATRIX_TOTAL(a);                                                  \
        for (u64 i = 0; i < total; i++) {                                             \
            out->data[i] = a->data[i] - b->data[i];                                   \
        }                                                                             \
    }

#define MATRIX_SCALE(T)                           \
    void matrix_scale_##T(Matrix_##T* mat, T val) \
    {                                             \
        CHECK_FATAL(!mat, "matrix is null");      \
        u64 total = MATRIX_TOTAL(mat);            \
        for (u64 i = 0; i < total; i++) {         \
            mat->data[i] *= val;                  \
        }                                         \
    }

#define MATRIX_DIV(T)                               \
    void matrix_div_##T(Matrix_##T* mat, T val)     \
    {                                               \
        CHECK_FATAL(!mat, "mat is null");           \
        CHECK_FATAL(val == 0, "division by zero!"); \
        u64 total = MATRIX_TOTAL(mat);              \
        for (u64 i = 0; i < total; i++) {           \
            mat->data[i] /= val;                    \
        }                                           \
    }

// ============================================================================
// MATRIX MULTIPLICATION (Blocked ikj)
// ============================================================================

#define MATRIX_XPLY(T)                                                                          \
    void matrix_xply_##T(Matrix_##T* out, const Matrix_##T* a, const Matrix_##T* b)             \
    {                                                                                           \
        CHECK_FATAL(!out, "out matrix is null");                                                \
        CHECK_FATAL(!a, "a matrix is null");                                                    \
        CHECK_FATAL(!b, "b matrix is null");                                                    \
        CHECK_FATAL(a->n != b->m, "incompatible matrix dimensions for multiplication");         \
        CHECK_FATAL(out->m != a->m || out->n != b->n, "output matrix has wrong dimensions");    \
                                                                                                \
        u64 m = a->m;                                                                           \
        u64 k = a->n;                                                                           \
        u64 n = b->n;                                                                           \
                                                                                                \
        memset(out->data, 0, sizeof(T) * m * n);                                                \
                                                                                                \
        const u64 BLOCK_SIZE = 16;                                                              \
                                                                                                \
        for (u64 i = 0; i < m; i += BLOCK_SIZE) {                                               \
            for (u64 k_outer = 0; k_outer < k; k_outer += BLOCK_SIZE) {                         \
                for (u64 j = 0; j < n; j += BLOCK_SIZE) {                                       \
                    u64 i_max = (i + BLOCK_SIZE < m) ? i + BLOCK_SIZE : m;                      \
                    u64 k_max = (k_outer + BLOCK_SIZE < k) ? k_outer + BLOCK_SIZE : k;          \
                    u64 j_max = (j + BLOCK_SIZE < n) ? j + BLOCK_SIZE : n;                      \
                                                                                                \
                    for (u64 ii = i; ii < i_max; ii++) {                                        \
                        for (u64 kk = k_outer; kk < k_max; kk++) {                              \
                            T a_val = a->data[IDX(a, ii, kk)];                                  \
                            for (u64 jj = j; jj < j_max; jj++) {                                \
                                out->data[IDX(out, ii, jj)] += a_val * b->data[IDX(b, kk, jj)]; \
                            }                                                                   \
                        }                                                                       \
                    }                                                                           \
                }                                                                               \
            }                                                                                   \
        }                                                                                       \
    }

// ============================================================================
// MATRIX MULTIPLICATION VARIANT 2 (Transpose-based)
// ============================================================================

// This function transposes b for cache-friendly access
// Takes more memory, good for large size matrices
#define MATRIX_XPLY_2(T)                                                                     \
    void matrix_xply_2_##T(Matrix_##T* out, const Matrix_##T* a, const Matrix_##T* b)        \
    {                                                                                        \
        CHECK_FATAL(!out, "out matrix is null");                                             \
        CHECK_FATAL(!a, "a matrix is null");                                                 \
        CHECK_FATAL(!b, "b matrix is null");                                                 \
        CHECK_FATAL(a->n != b->m, "incompatible matrix dimensions");                         \
        CHECK_FATAL(out->m != a->m || out->n != b->n, "output matrix has wrong dimensions"); \
                                                                                             \
        u64 m = a->m;                                                                        \
        u64 k = a->n;                                                                        \
        u64 n = b->n;                                                                        \
                                                                                             \
        Matrix_##T* b_T = matrix_create_##T(n, k);                                           \
        matrix_T_##T(b_T, b);                                                                \
                                                                                             \
        memset(out->data, 0, sizeof(T) * m * n);                                             \
                                                                                             \
        const u64 BLOCK_SIZE = 16;                                                           \
                                                                                             \
        for (u64 i = 0; i < m; i += BLOCK_SIZE) {                                            \
            for (u64 j = 0; j < n; j += BLOCK_SIZE) {                                        \
                u64 i_max = (i + BLOCK_SIZE < m) ? i + BLOCK_SIZE : m;                       \
                u64 j_max = (j + BLOCK_SIZE < n) ? j + BLOCK_SIZE : n;                       \
                                                                                             \
                for (u64 ii = i; ii < i_max; ii++) {                                         \
                    for (u64 jj = j; jj < j_max; jj++) {                                     \
                        T sum = 0;                                                           \
                        for (u64 kk = 0; kk < k; kk++) {                                     \
                            sum += a->data[IDX(a, ii, kk)] * b_T->data[IDX(b_T, jj, kk)];    \
                        }                                                                    \
                        out->data[IDX(out, ii, jj)] = sum;                                   \
                    }                                                                        \
                }                                                                            \
            }                                                                                \
        }                                                                                    \
        matrix_destroy_##T(b_T);                                                             \
    }

// ============================================================================
// MATRIX TRANSPOSE
// ============================================================================

#define MATRIX_T(T)                                                                          \
    void matrix_T_##T(Matrix_##T* out, const Matrix_##T* mat)                                \
    {                                                                                        \
        CHECK_FATAL(!mat, "mat matrix is null");                                             \
        CHECK_FATAL(!out, "out matrix is null");                                             \
        CHECK_FATAL(mat->m != out->n || mat->n != out->m, "incompatible matrix dimensions"); \
                                                                                             \
        const u64 BLOCK_SIZE = 16;                                                           \
                                                                                             \
        for (u64 i = 0; i < mat->m; i += BLOCK_SIZE) {                                       \
            for (u64 j = 0; j < mat->n; j += BLOCK_SIZE) {                                   \
                u64 i_max = (i + BLOCK_SIZE < mat->m) ? i + BLOCK_SIZE : mat->m;             \
                u64 j_max = (j + BLOCK_SIZE < mat->n) ? j + BLOCK_SIZE : mat->n;             \
                                                                                             \
                for (u64 ii = i; ii < i_max; ii++) {                                         \
                    for (u64 jj = j; jj < j_max; jj++) {                                     \
                        out->data[IDX(out, jj, ii)] = mat->data[IDX(mat, ii, jj)];           \
                    }                                                                        \
                }                                                                            \
            }                                                                                \
        }                                                                                    \
    }

// ============================================================================
// MATRIX COPY
// ============================================================================

#define MATRIX_COPY(T)                                                                        \
    void matrix_copy_##T(Matrix_##T* dest, const Matrix_##T* src)                             \
    {                                                                                         \
        CHECK_FATAL(!dest, "dest matrix is null");                                            \
        CHECK_FATAL(!src, "src matrix is null");                                              \
        CHECK_FATAL(dest->m != src->m || dest->n != src->n, "matrix dimensions don't match"); \
        memcpy(dest->data, src->data, sizeof(T) * MATRIX_TOTAL(src));                         \
    }

// ============================================================================
// LU DECOMPOSITION (works for all types via double arithmetic)
// ============================================================================

#define MATRIX_LU_DECOMP(T)                                                                 \
    void matrix_LU_Decomp_##T(Matrix_##T* L, Matrix_##T* U, const Matrix_##T* mat)          \
    {                                                                                       \
        CHECK_FATAL(!L, "L mat is null");                                                   \
        CHECK_FATAL(!U, "U mat is null");                                                   \
        CHECK_FATAL(!mat, "mat is null");                                                   \
        CHECK_FATAL(mat->n != mat->m, "mat is not a square matrix");                        \
        CHECK_FATAL(L->n != mat->n || L->m != mat->m, "L dimensions don't match");          \
        CHECK_FATAL(U->n != mat->n || U->m != mat->m, "U dimensions don't match");          \
                                                                                            \
        const u64 n = mat->n;                                                               \
                                                                                            \
        memset(L->data, 0, sizeof(T) * n * n);                                              \
        memset(U->data, 0, sizeof(T) * n * n);                                              \
                                                                                            \
        for (u64 i = 0; i < n; i++) {                                                       \
            L->data[IDX(L, i, i)] = (T)1;                                                   \
        }                                                                                   \
                                                                                            \
        for (u64 i = 0; i < n; i++) {                                                       \
            for (u64 k = i; k < n; k++) {                                                   \
                double sum = 0;                                                             \
                for (u64 j = 0; j < i; j++) {                                               \
                    sum += (double)L->data[IDX(L, i, j)] * (double)U->data[IDX(U, j, k)];   \
                }                                                                           \
                U->data[IDX(U, i, k)] = (T)((double)MATRIX_AT(mat, i, k) - sum);            \
            }                                                                               \
                                                                                            \
            for (u64 k = i + 1; k < n; k++) {                                               \
                double sum = 0;                                                             \
                for (u64 j = 0; j < i; j++) {                                               \
                    sum += (double)L->data[IDX(L, k, j)] * (double)U->data[IDX(U, j, i)];   \
                }                                                                           \
                                                                                            \
                double u_diag = (double)U->data[IDX(U, i, i)];                              \
                CHECK_FATAL(u_diag == 0, "Matrix is singular - LU decomposition failed");   \
                                                                                            \
                L->data[IDX(L, k, i)] = (T)(((double)MATRIX_AT(mat, k, i) - sum) / u_diag); \
            }                                                                               \
        }                                                                                   \
    }

// ============================================================================
// DETERMINANT (via LU decomposition, works for all types)
// ============================================================================

#define MATRIX_DET(T)                                                           \
    double matrix_det_##T(const Matrix_##T* mat)                                \
    {                                                                           \
        CHECK_FATAL(!mat, "mat matrix is null");                                \
        CHECK_FATAL(mat->m != mat->n, "only square matrices have determinant"); \
                                                                                \
        u64         n = mat->n;                                                 \
        Matrix_##T* L = matrix_create_##T(n, n);                                \
        Matrix_##T* U = matrix_create_##T(n, n);                                \
                                                                                \
        matrix_LU_Decomp_##T(L, U, mat);                                        \
                                                                                \
        double det = 1.0;                                                       \
        for (u64 i = 0; i < n; i++) {                                           \
            det *= (double)U->data[IDX(U, i, i)];                               \
        }                                                                       \
                                                                                \
        matrix_destroy_##T(L);                                                  \
        matrix_destroy_##T(U);                                                  \
                                                                                \
        return det;                                                             \
    }

// ============================================================================
// UNIFIED MATRIX PRINT
// ============================================================================

#define MATRIX_PRINT(T, fmt)                     \
    void matrix_print_##T(const Matrix_##T* mat) \
    {                                            \
        CHECK_FATAL(!mat, "matrix is null");     \
        u64 total = mat->m * mat->n;             \
                                                 \
        for (u64 i = 0; i < total; i++) {        \
            if (i % mat->n == 0) {               \
                if (i > 0) {                     \
                    putchar('|');                \
                }                                \
                putchar('\n');                   \
                putchar('|');                    \
                putchar(' ');                    \
            }                                    \
            printf(fmt, mat->data[i]);           \
            putchar(' ');                        \
        }                                        \
        putchar('|');                            \
        putchar('\n');                           \
    }



// ARENA-BASED MATRIX ALLOCATION MACROS
// ============================================================================

/*
Create a matrix allocated from arena (heap-style)
Matrix struct and data both allocated from arena
No need to call matrix_destroy - freed when arena is cleared/released

Usage:
    Matrix* mat = MATRIX_ARENA(arena, 3, 3);
*/
#define MATRIX_ARENA_ALLOC(T)                                           \
    Matrix_##T* matrix_arena_alloc_##T(Arena* arena, u64 m, u64 n)      \
    {                                                                   \
        CHECK_FATAL(m == 0 && n == 0, "n == m == 0");                   \
        Matrix_##T* mat = ARENA_ALLOC(arena, Matrix_##T);               \
                                                                        \
        CHECK_FATAL(!mat, "matrix arena allocation failed");            \
                                                                        \
        mat->m = m;                                                     \
        mat->n = n;                                                     \
                                                                        \
        mat->data = ARENA_ALLOC_N(arena, T, (u64)(m * n));              \
        CHECK_FATAL(!mat->data, "matrix data arena allocation failed"); \
                                                                        \
        return mat;                                                     \
    }

// ============================================================================
// MACRO TO INSTANTIATE ALL FUNCTIONS FOR A TYPE
// ============================================================================

// Unified instantiation for all types
// Order matters: functions must be defined before they're called
#define INSTANTIATE_MATRIX(T, fmt) \
    MATRIX_TYPE(T);                \
    MATRIX_CREATE(T)               \
    MATRIX_CREATE_ARR(T)           \
    MATRIX_CREATE_STK(T)           \
    MATRIX_DESTROY(T)              \
    MATRIX_SET_VAL_ARR(T)          \
    MATRIX_SET_VAL_ARR2(T)         \
    MATRIX_SET_ELM(T)              \
    MATRIX_ADD(T)                  \
    MATRIX_SUB(T)                  \
    MATRIX_SCALE(T)                \
    MATRIX_DIV(T)                  \
    MATRIX_COPY(T)                 \
    MATRIX_T(T)                    \
    MATRIX_XPLY(T)                 \
    MATRIX_XPLY_2(T)               \
    MATRIX_LU_DECOMP(T)            \
    MATRIX_DET(T)                  \
    MATRIX_PRINT(T, fmt)           \
    MATRIX_ARENA_ALLOC(T)

#endif /* WC_MATRIX_GENERIC_H */

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

/* python
align to 8 bytes
>>> 4 + 7 & ~(7)
8
align to 4 bytes
>>> 1 + 4 & ~(4)
1
*/
// Align a value to alignment boundary
#define ALIGN_UP(val, align) \
    (((val) + ((align) - 1)) & ~((align) - 1))

// align value to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_UP_DEFAULT(val) \
    ALIGN_UP((val), ARENA_DEFAULT_ALIGNMENT)

// Align a pointer to alignment boundary  
// turn ptr to a u64 val to align, then turn to ptr again
#define ALIGN_PTR(ptr, align) \
    ((u8*)ALIGN_UP((ptr), (align)))

// align a pointer to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_PTR_DEFAULT(ptr) \
    ALIGN_PTR((ptr), ARENA_DEFAULT_ALIGNMENT)


#define ARENA_CURR_IDX_PTR(arena) ((arena)->base + (arena)->idx)
#define ARENA_PTR(arena, idx) ((arena)->base + (idx))





Arena* arena_create(u64 capacity)
{
    if (capacity == 0) {
        capacity = ARENA_DEFAULT_SIZE;
    }

    Arena* arena = (Arena*)malloc(sizeof(Arena));
    CHECK_FATAL(!arena, "arena malloc failed");

    arena->base = (u8*)malloc(capacity);
    CHECK_FATAL(!arena->base, "arena base malloc failed");

    arena->idx = 0;
    arena->size = capacity;

    return arena;
}

void arena_create_arr_stk(Arena* arena, u8* data, u64 size)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(size == 0, "size can't be zero");

    arena->base = data;
    arena->idx = 0;
    arena->size = size;
}

void arena_clear(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    arena->idx = 0;
}

void arena_release(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    
    free(arena->base);
    free(arena);
}

u8* arena_alloc(Arena* arena, u64 size)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    
    // Align the current index first
    u64 aligned_idx = ALIGN_UP_DEFAULT(arena->idx);
    WC_SET_RET(WC_ERR_FULL, arena->size - aligned_idx < size, NULL);
    
    u8* ptr = ARENA_PTR(arena, aligned_idx);
    arena->idx = aligned_idx + size;
    
    return ptr;
}

u8* arena_alloc_aligned(Arena* arena, u64 size, u32 alignment)
{

    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    CHECK_FATAL((alignment & (alignment - 1)) != 0,
                "alignment must be power of two");


    u64 aligned_idx = ALIGN_UP(arena->idx, alignment);

    WC_SET_RET(WC_ERR_FULL, arena->size - aligned_idx < size, NULL);

    u8* ptr = ARENA_PTR(arena, aligned_idx);
    arena->idx = aligned_idx + size;

    return ptr;
}

u64 arena_get_mark(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    return arena->idx;
}

void arena_clear_mark(Arena* arena, u64 mark)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(mark > arena->idx, "mark is out of bounds");

    if (mark == arena->idx) { return; }

    arena->idx = mark;
}

#endif /* WC_ARENA_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_MATRIX_GENERIC_SINGLE_H */
