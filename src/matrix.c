#include "matrix.h"



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
    CHECK_FATAL(!arr, "input arr is null");
    Matrixf* mat = matrix_create(m, n);
    memcpy(mat->data, arr, sizeof(float) * m * n);
    return mat;
}

void matrix_create_stk(Matrixf* mat, u64 m, u64 n, float* data)
{
    CHECK_FATAL(!mat, "matrix is null");
    CHECK_FATAL(!data, "data is null");

    // we can do this on the stack
    mat->data = data; // copying stk ptr 
    mat->m    = m;
    mat->n    = n;
}

void matrix_destroy(Matrixf* mat)
{
    CHECK_FATAL(!mat, "matrix is null");

    free(mat->data);
    free(mat);
}


void matrix_set_val_arr(Matrixf* mat, u64 count, const float* arr)
{
    CHECK_FATAL(!mat, "matrix is null");
    CHECK_FATAL(!arr, "arr is null");
    CHECK_FATAL(count != MATRIX_TOTAL(mat), "count doesn't match matrix size");

    memcpy(mat->data, arr, sizeof(float) * count);
}

void matrix_set_val_arr2(Matrixf* mat, u64 m, u64 n, const float** arr2)
{
    CHECK_FATAL(!mat, "matrix is null");
    CHECK_FATAL(!arr2, "arr is null");
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
    CHECK_FATAL(!mat, "matrix is null");
    CHECK_FATAL(i >= mat->m || j >= mat->n, "index out of bounds");

    mat->data[IDX(mat, i, j)] = elm;
}

float matrix_get_elm(Matrixf* mat, u64 i, u64 j)
{
    CHECK_FATAL(!mat, "matrix is null");
    CHECK_FATAL(i >= mat->m || j >= mat->n, "index out of bounds");

    return mat->data[IDX(mat, i, j)];
}

void matrix_add(Matrixf* out, const Matrixf* a, const Matrixf* b)
{
    CHECK_FATAL(!out, "out matrix is null");
    CHECK_FATAL(!a, "a matrix is null");
    CHECK_FATAL(!b, "b matrix is null");
    CHECK_FATAL(a->m != b->m || a->n != b->n || a->m != out->m ||
                    a->n != out->n,
                "a, b, out mat dimentions dont match");

    u64 total = MATRIX_TOTAL(a);

    for (u64 i = 0; i < total; i++) { 
        out->data[i] = a->data[i] + b->data[i]; 
    }
}


void matrix_sub(Matrixf* out, const Matrixf* a, const Matrixf* b)
{
    CHECK_FATAL(!out, "out matrix is null");
    CHECK_FATAL(!a, "a matrix is null");
    CHECK_FATAL(!b, "b matrix is null");
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
void matrix_xply(Matrixf* out, const Matrixf* a, const Matrixf* b)
{
    CHECK_FATAL(!out, "out matrix is null");
    CHECK_FATAL(!a, "a matrix is null");
    CHECK_FATAL(!b, "b matrix is null");
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
void matrix_xply_2(Matrixf* out, const Matrixf* a, const Matrixf* b)
{
    CHECK_FATAL(!out, "out matrix is null");
    CHECK_FATAL(!a, "a matrix is null");
    CHECK_FATAL(!b, "b matrix is null");
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
void matrix_LU_Decomp(Matrixf* L, Matrixf* U, const Matrixf* mat)
{
    CHECK_FATAL(!L, "L mat is null");
    CHECK_FATAL(!U, "U mat is null");
    CHECK_FATAL(!mat, "mat is null");
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
    CHECK_FATAL(!mat, "mat matrix is null");
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
    // for (u64 i = 0; i < n; i++) { det *= U->data[IDX(U, i, i)]; }
    for (u64 i = 0; i < n; i++) { det *= U.data[IDX(&U, i, i)]; }

    return det;
}


void matrix_T(Matrixf* out, const Matrixf* mat)
{
    CHECK_FATAL(!mat, "mat matrix is null");
    CHECK_FATAL(!out, "out matrix is null");
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

void matrix_scale(Matrixf* mat, float val)
{
    CHECK_FATAL(!mat, "matrix is null");

    u64 total = MATRIX_TOTAL(mat);
    for (u64 i = 0; i < total; i++) { mat->data[i] *= val; }
}


void matrix_div(Matrixf* mat, float val)
{
    CHECK_FATAL(!mat, "mat is null");
    CHECK_FATAL(val == 0, "division by zero!");

    u64 total = MATRIX_TOTAL(mat);
    for (u64 i = 0; i < total; i++) { mat->data[i] /= val; }
}

void matrix_copy(Matrixf* dest, const Matrixf* src)
{
    CHECK_FATAL(!dest, "dest matrix is null");
    CHECK_FATAL(!src, "src matrix is null");
    CHECK_FATAL(dest->m != src->m || dest->n != src->n,
                "matrix dimensions don't match");

    memcpy(dest->data, src->data, sizeof(float) * MATRIX_TOTAL(src));
}

void matrix_print(const Matrixf* mat)
{
    CHECK_FATAL(!mat, "matrix is null");

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



