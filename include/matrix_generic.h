#ifndef MATRIX_GENERIC_H
#define MATRIX_GENERIC_H

#include "common.h"
#include "arena.h"
// #include <string.h>


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


#endif // MATRIX_GENERIC_H
