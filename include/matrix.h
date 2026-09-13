#ifndef MATRIX_H
#define MATRIX_H

#include "common.h"
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
#include "arena.h"

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




#endif // MATRIX_H
