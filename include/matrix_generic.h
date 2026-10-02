#ifndef MATRIX_GENERIC_H
#define MATRIX_GENERIC_H

#include "common.h"
#include "wc_allocator.h"
// #include <String.h>


// ============================================================================
// GENERIC MATRIX MACRO DEFINITIONS
// ============================================================================

// Define a matrix type for a specific data type
// Same contract as Matrixf (matrix.h): value type, stores its allocator,
// zeroed == dead (mutation on m == 0 is an unconditional FATAL).
#define MATRIX_TYPE(T)             \
    typedef struct {               \
        T*           data;         \
        u64          m; /* rows */ \
        u64          n; /* cols */ \
        wc_allocator alloc;        \
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

#define MATRIX_BYTES(T, m, n) wc_mul(wc_mul((m), (n)), sizeof(T))

#define MATRIX_CREATE(T)                                                          \
    Matrix_##T matrix_create_##T(wc_allocator a, u64 m, u64 n)                    \
    {                                                                             \
        FATAL_IF(m == 0 || n == 0, "matrix_create_" #T ": dims must be > 0");     \
        T* data = wc_alloc(a, MATRIX_BYTES(T, m, n), alignof(T));             \
        FATAL_IF(!data, "matrix_create_" #T ": data allocation failed");          \
        return (Matrix_##T){.data = data, .m = m, .n = n, .alloc = a};            \
    }

#define MATRIX_CREATE_ARR(T)                                                        \
    Matrix_##T matrix_create_arr_##T(wc_allocator a, u64 m, u64 n, const T* arr)    \
    {                                                                               \
        WC_ASSERT(arr, "input arr is null");                                        \
        Matrix_##T mat = matrix_create_##T(a, m, n);                                \
        memcpy(mat.data, arr, sizeof(T) * m * n);                                   \
        return mat;                                                                 \
    }

/* Wrap caller-owned memory via wc_borrowed; destroy frees nothing. */
#define MATRIX_CREATE_BUF(T)                                                      \
    Matrix_##T matrix_create_buf_##T(u64 m, u64 n, T* data)                       \
    {                                                                             \
        FATAL_IF(m == 0 || n == 0, "matrix_create_buf_" #T ": dims must be > 0"); \
        WC_ASSERT(data, "data is null");                                          \
        return (Matrix_##T){.data = data, .m = m, .n = n, .alloc = wc_borrowed};  \
    }

/* Safe on zeroed matrices; leaves the struct zeroed. */
#define MATRIX_DESTROY(T)                                                       \
    void matrix_destroy_##T(Matrix_##T* mat)                                    \
    {                                                                           \
        WC_ASSERT(mat, "matrix is null");                                       \
        if (mat->data) {                                                        \
            wc_allocator a = mat->alloc;                                        \
            wc_free(a, mat->data, MATRIX_BYTES(T, mat->m, mat->n), alignof(T)); \
        }                                                                       \
        memset(mat, 0, sizeof(*mat));                                           \
    }

/* Transfer: dest takes src's storage and allocator, src is left zeroed. */
#define MATRIX_MOVE(T)                                         \
    void matrix_move_##T(Matrix_##T* dest, Matrix_##T* src)    \
    {                                                          \
        WC_ASSERT(dest && src, "matrix is null");              \
        if (dest == src) {                                     \
            return;                                            \
        }                                                      \
        memcpy(dest, src, sizeof(*src));                       \
        memset(src, 0, sizeof(*src));                          \
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
        FATAL_IF((mat)->m == 0, "matrix_set_val_arr_" #T " on zeroed/moved-from matrix"); \
        WC_ASSERT(mat, "matrix is null");                                                 \
        WC_ASSERT(arr, "arr is null");                                                    \
        WC_ASSERT(count == MATRIX_TOTAL(mat), "count doesn't match matrix size");   \
        memcpy(mat->data, arr, sizeof(T) * count);                                  \
    }

// For 2D arrays (array of pointers)
#define MATRIX_SET_VAL_ARR2(T)                                                            \
    void matrix_set_val_arr2_##T(Matrix_##T* mat, u64 m, u64 n, const T** arr2)           \
    {                                                                                     \
        FATAL_IF((mat)->m == 0, "matrix_set_val_arr2_" #T " on zeroed/moved-from matrix"); \
        WC_ASSERT(mat, "matrix is null");                                                  \
        WC_ASSERT(arr2, "arr is null");                                                    \
        WC_ASSERT(*arr2, "*arr is null");                                                  \
        WC_ASSERT(m == mat->m && n == mat->n, "mat dimensions dont match passed arr2");   \
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
        FATAL_IF((mat)->m == 0, "matrix_set_elm_" #T " on zeroed/moved-from matrix"); \
        WC_ASSERT(mat, "matrix is null");                                             \
        WC_ASSERT(i < mat->m && j < mat->n, "index out of bounds");     \
        mat->data[IDX(mat, i, j)] = elm;                                \
    }

// ============================================================================
// MATRIX OPERATIONS
// ============================================================================

#define MATRIX_ADD(T)                                                                 \
    void matrix_add_##T(Matrix_##T* out, const Matrix_##T* a, const Matrix_##T* b)    \
    {                                                                                 \
        FATAL_IF((out)->m == 0, "matrix_add_" #T " on zeroed/moved-from matrix");     \
        WC_ASSERT(out, "out matrix is null");                                         \
        WC_ASSERT(a, "a matrix is null");                                             \
        WC_ASSERT(b, "b matrix is null");                                             \
        WC_ASSERT(a->m == b->m && a->n == b->n && a->m == out->m && a->n == out->n,   \
                    "a, b, out mat dimensions don't match");                          \
        u64 total = MATRIX_TOTAL(a);                                                  \
        for (u64 i = 0; i < total; i++) {                                             \
            out->data[i] = a->data[i] + b->data[i];                                   \
        }                                                                             \
    }

#define MATRIX_SUB(T)                                                                 \
    void matrix_sub_##T(Matrix_##T* out, const Matrix_##T* a, const Matrix_##T* b)    \
    {                                                                                 \
        FATAL_IF((out)->m == 0, "matrix_sub_" #T " on zeroed/moved-from matrix");     \
        WC_ASSERT(out, "out matrix is null");                                         \
        WC_ASSERT(a, "a matrix is null");                                             \
        WC_ASSERT(b, "b matrix is null");                                             \
        WC_ASSERT(a->m == b->m && a->n == b->n && a->m == out->m && a->n == out->n,   \
                    "a, b, out mat dimensions don't match");                          \
        u64 total = MATRIX_TOTAL(a);                                                  \
        for (u64 i = 0; i < total; i++) {                                             \
            out->data[i] = a->data[i] - b->data[i];                                   \
        }                                                                             \
    }

#define MATRIX_SCALE(T)                           \
    void matrix_scale_##T(Matrix_##T* mat, T val) \
    {                                             \
        FATAL_IF((mat)->m == 0, "matrix_scale_" #T " on zeroed/moved-from matrix"); \
        WC_ASSERT(mat, "matrix is null");                                           \
        u64 total = MATRIX_TOTAL(mat);            \
        for (u64 i = 0; i < total; i++) {         \
            mat->data[i] *= val;                  \
        }                                         \
    }

#define MATRIX_DIV(T)                               \
    void matrix_div_##T(Matrix_##T* mat, T val)     \
    {                                               \
        FATAL_IF((mat)->m == 0, "matrix_div_" #T " on zeroed/moved-from matrix"); \
        WC_ASSERT(mat, "mat is null");                                            \
        WC_ASSERT(val != 0, "division by zero!");   \
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
        FATAL_IF((out)->m == 0, "matrix_xply_" #T " on zeroed/moved-from matrix");              \
        WC_ASSERT(out, "out matrix is null");                                                   \
        WC_ASSERT(a, "a matrix is null");                                                       \
        WC_ASSERT(b, "b matrix is null");                                                       \
        WC_ASSERT(a->n == b->m, "incompatible matrix dimensions for multiplication");           \
        WC_ASSERT(out->m == a->m && out->n == b->n, "output matrix has wrong dimensions");      \
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
        FATAL_IF((out)->m == 0, "matrix_xply_2_" #T " on zeroed/moved-from matrix");         \
        WC_ASSERT(out, "out matrix is null");                                                \
        WC_ASSERT(a, "a matrix is null");                                                    \
        WC_ASSERT(b, "b matrix is null");                                                    \
        WC_ASSERT(a->n == b->m, "incompatible matrix dimensions");                           \
        WC_ASSERT(out->m == a->m && out->n == b->n, "output matrix has wrong dimensions");   \
                                                                                             \
        u64 m = a->m;                                                                        \
        u64 k = a->n;                                                                        \
        u64 n = b->n;                                                                        \
                                                                                             \
        Matrix_##T  b_T_s = matrix_create_##T(WC_LIBC, n, k); /* scratch */                  \
        Matrix_##T* b_T   = &b_T_s;                                                          \
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
        FATAL_IF((out)->m == 0, "matrix_T_" #T " on zeroed/moved-from matrix");              \
        WC_ASSERT(mat, "mat matrix is null");                                                \
        WC_ASSERT(out, "out matrix is null");                                                \
        WC_ASSERT(mat->m == out->n && mat->n == out->m, "incompatible matrix dimensions");   \
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

/* Deep copy into a new matrix allocated from `a` (never inherits src->alloc). */
#define MATRIX_COPY(T)                                                         \
    Matrix_##T matrix_copy_##T(wc_allocator a, const Matrix_##T* src)          \
    {                                                                          \
        WC_ASSERT(src, "src matrix is null");                                  \
        FATAL_IF(src->m == 0, "matrix_copy_" #T " on zeroed/moved-from matrix"); \
        return matrix_create_arr_##T(a, src->m, src->n, src->data);            \
    }

// ============================================================================
// LU DECOMPOSITION (works for all types via double arithmetic)
// ============================================================================

#define MATRIX_LU_DECOMP(T)                                                                 \
    void matrix_LU_Decomp_##T(Matrix_##T* L, Matrix_##T* U, const Matrix_##T* mat)          \
    {                                                                                       \
        FATAL_IF((L)->m == 0, "matrix_LU_Decomp_" #T " on zeroed/moved-from matrix");       \
        FATAL_IF((U)->m == 0, "matrix_LU_Decomp_" #T " on zeroed/moved-from matrix");       \
        WC_ASSERT(L, "L mat is null");                                                      \
        WC_ASSERT(U, "U mat is null");                                                      \
        WC_ASSERT(mat, "mat is null");                                                      \
        WC_ASSERT(mat->n == mat->m, "mat is not a square matrix");                          \
        WC_ASSERT(L->n == mat->n && L->m == mat->m, "L dimensions don't match");            \
        WC_ASSERT(U->n == mat->n && U->m == mat->m, "U dimensions don't match");            \
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
                WC_ASSERT(u_diag != 0, "Matrix is singular - LU decomposition failed");     \
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
        WC_ASSERT(mat, "mat matrix is null");                                   \
        WC_ASSERT(mat->m == mat->n, "only square matrices have determinant");   \
                                                                                \
        u64         n = mat->n;                                                 \
        Matrix_##T  L_s = matrix_create_##T(WC_LIBC, n, n); /* scratch */       \
        Matrix_##T  U_s = matrix_create_##T(WC_LIBC, n, n);                     \
        Matrix_##T* L   = &L_s;                                                 \
        Matrix_##T* U   = &U_s;                                                 \
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
        WC_ASSERT(mat, "matrix is null");        \
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



// Arena-backed matrices: matrix_create_T(Arena_allocator(&arena), m, n).

// ============================================================================
// MACRO TO INSTANTIATE ALL FUNCTIONS FOR A TYPE
// ============================================================================

// Unified instantiation for all types
// Order matters: functions must be defined before they're called
#define INSTANTIATE_MATRIX(T, fmt) \
    MATRIX_TYPE(T);                \
    MATRIX_CREATE(T)               \
    MATRIX_CREATE_ARR(T)           \
    MATRIX_CREATE_BUF(T)           \
    MATRIX_DESTROY(T)              \
    MATRIX_MOVE(T)                 \
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
    MATRIX_PRINT(T, fmt)


#endif // MATRIX_GENERIC_H
