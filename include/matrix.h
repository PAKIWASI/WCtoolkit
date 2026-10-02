#ifndef MATRIX_H
#define MATRIX_H

#include "common.h"
#include "wc_allocator.h"
#include <string.h>



// ROW MAJOR 2D MATRIX
//
// Value type. Storage comes from `alloc`, which the matrix stores (40 bytes).
// Zero state (moved-from / destroyed) is dead: only destroy or re-create it.
// Every mutating op on a zeroed matrix (m == 0) is an unconditional FATAL.
typedef struct {
    float*       data;
    u64          m; // rows
    u64          n; // cols
    wc_allocator alloc;
} Matrixf;

_Static_assert(sizeof(Matrixf) == 40, "Matrixf layout: data + m + n + 16-byte allocator");


// CREATION AND DESTRUCTION
// ============================================================================

// m x n matrix, storage from `a`. Contents are uninitialised.
Matrixf matrix_create(wc_allocator a, u64 m, u64 n) __attribute__((warn_unused_result));

// m x n matrix from `a`, filled from a row-major array of m * n floats.
Matrixf matrix_create_arr(wc_allocator a, u64 m, u64 n, const float* arr) __attribute__((nonnull(4), warn_unused_result));

// Wrap caller-owned memory (stack array, static buffer). Uses wc_borrowed:
// destroy frees nothing. `data` must outlive the matrix.
Matrixf matrix_create_buf(u64 m, u64 n, float* data) __attribute__((nonnull(3), warn_unused_result));

// Free storage through mat->alloc and zero the struct. Safe on zeroed matrices.
void matrix_destroy(Matrixf* mat) __attribute__((nonnull(1)));

// Deep copy of `src` into a new matrix allocated from `a` (never inherits src->alloc).
Matrixf matrix_copy(wc_allocator a, const Matrixf* src) __attribute__((nonnull(2), warn_unused_result));

// Transfer: dest takes src's storage and allocator, src is left zeroed.
// dest must be raw or already destroyed (it is overwritten, not freed).
void matrix_move(Matrixf* dest, Matrixf* src) __attribute__((nonnull(1, 2)));


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



// CONSTRUCTION MACROS
// ============================================================================
// MATRIX(m, n)         libc-backed
// MATRIX_IN(A, m, n)   any allocator, e.g. MATRIX_IN(Arena_allocator(&arena), 3, 3)
#define MATRIX_IN(A, m, n) matrix_create((A), (m), (n))
#define MATRIX(m, n)       MATRIX_IN(WC_LIBC, (m), (n))


#endif // MATRIX_H
