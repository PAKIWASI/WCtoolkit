#ifndef MATRIX_H
#define MATRIX_H

#include "common.h"
#include <string.h>



// ROW MAJOR 2D MATRIX
typedef struct {
    float* data;
    u64    m; // rows
    u64    n; // cols
} Matrix;


// CREATION AND DESTRUCTION
// ============================================================================

// create heap matrix with m rows and n cols
Matrix* matrix_create(u64 m, u64 n);

// create heap matrix with m rows and n cols and an array of size m x n
Matrix* matrix_create_arr(u64 m, u64 n, const float* arr);

// create matrix with everything on the stack
void matrix_create_stk(Matrix* mat, u64 m, u64 n, float* data);

// destroy the matrix created with matrix_create or matrix_create_arr
// DO NOT use on stack-allocated matrices (created with matrix_create_stk)
void matrix_destroy(Matrix* mat);


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
void matrix_set_val_arr(Matrix* mat, u64 count, const float* arr);

// for 2D arrays (array of pointers)
void matrix_set_val_arr2(Matrix* mat, u64 m, u64 n, const float** arr2);

// set the value at position (i, j) where i is row and j is column
void matrix_set_elm(Matrix* mat, float elm, u64 i, u64 j);


// BASIC OPERATIONS
// ============================================================================

// Matrix addition: out = a + b
// out may alias a and/or b (safe to do: matrix_add(a, a, b))
void matrix_add(Matrix* out, const Matrix* a, const Matrix* b);

// Matrix subtraction: out = a - b
// out may alias a and/or b (safe to do: matrix_sub(a, a, b))
void matrix_sub(Matrix* out, const Matrix* a, const Matrix* b);

// Scalar multiplication: mat = mat * val
void matrix_scale(Matrix* mat, float val);

// Element wise divistion
void matrix_div(Matrix* mat, float val);

// Matrix copy: dest = src
void matrix_copy(Matrix* dest, const Matrix* src);


// MATRIX MULTIPLICATION
// ============================================================================

// Matrix multiplication: out = a × b
// (m×k) * (k×n) = (m×n)
// out may NOT alias a or b
// Uses blocked ikj multiplication for cache efficiency (good for small-medium matrices)
void matrix_xply(Matrix* out, const Matrix* a, const Matrix* b);

// Matrix multiplication variant 2: out = a × b
// Transposes b internally for better cache locality
// Takes more memory but can be faster for large matrices
// out may NOT alias a or b
void matrix_xply_2(Matrix* out, const Matrix* a, const Matrix* b);


/* TODO: 
    vec * mat => (1 x n) * (m * n)T => (1 x n) * (n x m) => (1 x m)
OR  vec * mat => (1 x m) * (m * n) => (1 x n)
OR  vec * mat => (m x 1) * (1 x n) => (m x n) - this doesnot fit here ?
*/
u8* matrix_xply_vec(const Matrix* a, const u8* arr, u32 data_size, u64 size);



// ADVANCED OPERATIONS
// ============================================================================

// Transpose: out = mat^T
// out may NOT alias mat
void matrix_T(Matrix* out, const Matrix* mat);

// LU Decomposition: mat = L × U
// Decomposes square matrix into Lower and Upper triangular matrices
void matrix_LU_Decomp(Matrix* L, Matrix* U, const Matrix* mat);

// Calculate determinant using LU decomposition
float matrix_det(const Matrix* mat);

// Calculate adjugate (adjoint) matrix
// TODO: NOT IMPLEMENTED
void matrix_adj(Matrix* out, const Matrix* mat);

// Calculate matrix inverse: out = mat^(-1)
// TODO: NOT IMPLEMENTED
void matrix_inv(Matrix* out, const Matrix* mat);


// UTILITIES
// ============================================================================

// print the formatted, aligned matrix to stdout
void matrix_print(const Matrix* mat);


#define MATRIX_TOTAL(mat)    ((u64)((mat)->n * (mat)->m))
#define IDX(mat, i, j)       (((i) * (mat)->n) + (j))
#define MATRIX_AT(mat, i, j) ((mat)->data[((i) * (mat)->n) + (j)])

#define ZEROS_1D(n)    ((float[n]){0})
#define ZEROS_2D(m, n) ((float[m][n]){0})



// ARENA-BASED MATRIX ALLOCATION MACROS
// ============================================================================
#include "arena.h"

/*
Create a matrix allocated from arena (heap-style)
Matrix struct and data both allocated from arena
No need to call matrix_destroy - freed when arena is cleared/released

Usage:
    Matrix* mat = MATRIX_ARENA(arena, 3, 3);
*/
static inline Matrix* matrix_arena_alloc(Arena* arena, u64 m, u64 n)
{
    CHECK_FATAL(m == 0 && n == 0, "n == m == 0");

    Matrix* mat = ARENA_ALLOC(arena, Matrix);
    CHECK_FATAL(!mat, "matrix arena allocation failed");

    mat->m = m;
    mat->n = n;

    mat->data = ARENA_ALLOC_N(arena, float, (u64)(m * n));
    CHECK_FATAL(!mat->data, "matrix data arena allocation failed");

    return mat;
}

/*
Create a matrix allocated from arena with initial values
Matrix struct and data allocated from arena

Usage:
    Matrix* mat = MATRIX_ARENA_ARR(arena, 3, 3, (float[9]){1,2,3,4,5,6,7,8,9});
*/

static inline Matrix* matrix_arena_arr_alloc(Arena* arena, u64 m, u64 n, const float* arr)
{
    CHECK_FATAL(m == 0 || n == 0, "matrix dims must be > 0");
    CHECK_FATAL(!arr, "input arr is null");

    Matrix* mat = matrix_arena_alloc(arena, m, n);
    memcpy(mat->data, arr, sizeof(float) * m * n);
    return mat;
}




#endif // MATRIX_H
