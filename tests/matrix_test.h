#ifndef MATRIX_TEST_H
#define MATRIX_TEST_H

#include "arena.h"
#include "common.h"
#include "matrix.h"


int matrix_test_1(void)
{
    Matrix mat;
    float data[3][4] = {
        {1, 1, 1, 1},
        {2, 2, 2, 2},
        {3, 3, 3, 3}
    };
    matrix_create_stk(&mat, 3, 4, (float*)data);

    matrix_print(&mat);

    // matrix_set_val(&mat, 2, 2, 2, 2,
    //                      2, 2, 2, 2,
    //                      2, 2, 2, 2);           

    matrix_set_elm(&mat, 0, 1, 1);

    matrix_print(&mat);


    return 0;
}

int matrix_test_2(void)
{
    Matrix* mat = matrix_create(3, 3);

    matrix_set_val_arr(mat, 9, (float[9]){
            1, 1, 1,
            2, 2, 2,
            3, 3, 3
    });

    matrix_print(mat);

    // prefer this as much more explicit, hard to make mistake
    matrix_set_val_arr(mat, 9, (float*)(float[3][3]){
        {1,  200, 3},
        {4,  5,   6},
        {70, 8,   8}
    });

    matrix_print(mat);

    matrix_destroy(mat);

    return 0;
}

int matrix_test_3(void)
{
    Matrix* mat = matrix_create_arr(4, 3, (float*)(float[4][3]){
        {1, 1, 1},
        {1, 1, 1},
        {1, 1, 1},
        {1, 1, 1},
    });

    matrix_print(mat);

    Matrix mat2;
    matrix_create_stk(&mat2, 4, 3, (float*)(float[4][3]){
        {2, 2, 2},
        {2, 2, 2},
        {2, 2, 2},
        {2, 2, 2},
    });

    matrix_print(&mat2);

    matrix_add(mat, mat, &mat2);

    matrix_print(mat);

    Matrix out;
    matrix_create_stk(&out, 4, 3, (float*)ZEROS_2D(4, 3));
    matrix_print(&out);

    matrix_sub(&out, mat, &mat2);
    matrix_print(&out);


    matrix_sub(&out, &out, &out);
    matrix_print(&out);


    matrix_destroy(mat);
    return 0;
}

int matrix_test_4(void)
{
    Matrix mat;
    matrix_create_stk(&mat, 4, 3, (float*)(float[4][3]){
        {1, 2, 3},
        {1, 2, 3},
        {1, 2, 3},
        {1, 2, 3},
    });
    matrix_print(&mat);

    Matrix m2;
    matrix_create_stk(&m2, 3, 4, (float*)ZEROS_2D(3, 4));

    matrix_T(&m2, &mat);
    matrix_print(&m2);

    Matrix m3;
    matrix_create_stk(&m3, 3, 3, (float*)ZEROS_2D(3, 3));
    matrix_xply(&m3, &m2, &mat);

    matrix_print(&m3);

    return 0;
}

// lu decomp and det
int matrix_test_5(void)
{
    Matrix* mat = matrix_create_arr(3, 3, (float*)(float[3][3]){
        {3, 2, 4},
        {2, 0, 2},
        {4, 2, 3},
    });

    Matrix L, U;
    matrix_create_stk(&L, 3, 3, (float*)ZEROS_2D(3, 3));
    matrix_create_stk(&U, 3, 3, (float*)ZEROS_2D(3, 3));

    matrix_LU_Decomp(&L, &U, mat);

    matrix_print(&L);
    matrix_print(&U);

    float det = matrix_det(mat);
    printf("Det: %f\n", det);

    Matrix copy;
    matrix_create_stk(&copy, 3, 3, (float*)ZEROS_2D(3, 3));
    matrix_copy(&copy, mat);

    matrix_print(&copy);

    matrix_destroy(mat);
    return 0;
}

// matrix xpy, transpose
int matrix_test_6(void)
{

    Matrix* m1 = matrix_create_arr(4, 3, (float*)(float[4][3]){
        {1, 2, 3},
        {2, 2, 7},
        {1, 0, 1},
        {8, 9, 3},
    });

    Matrix m2;
    matrix_create_stk(&m2, 3, 4, (float*)(float[3][4]){
        {1, 2, 3, 4},
        {1, 0, 9, 6},
        {8, 4, 0, 4},
    });

    Matrix out;
    matrix_create_stk(&out, 4, 4, (float*)ZEROS_2D(4, 4));
    matrix_xply(&out, m1, &m2);
    matrix_print(&out);

    Matrix out_T;
    matrix_create_stk(&out_T, 3, 4, (float*)ZEROS_2D(3, 4));
    matrix_T(&out_T, m1);
    matrix_print(m1);
    matrix_print(&out_T);

    matrix_destroy(m1);

    return 0;
}


// ARENAS ARE SOO COOL
int matrix_test_7(void)
{
    Arena* arena = arena_create(nKB(1));

    Matrix* mat = matrix_arena_alloc(arena, 4, 4);

    ARENA_SCRATCH(xplyy, arena) {
        Matrix* t1 = matrix_arena_arr_alloc(arena, 4, 4, (float*)(float[4][4]){
            {1, 2, 3, 4},
            {1, 2, 3, 4},
            {1, 2, 3, 4},
            {1, 2, 3, 4}
        });

        Matrix* t2 = matrix_arena_arr_alloc(arena, 4, 4, (float*)(float[4][4]){
            {1, 2, 3, 4},
            {1, 2, 3, 4},
            {1, 2, 3, 4},
            {1, 2, 3, 4}
        });

        matrix_xply(mat, t1, t2);
    }

    matrix_print(mat);

    printf("%lu\n%lu\n", arena->idx, arena->size);
    arena_release(arena);
    return 0;
}


#endif // MATRIX_TEST_H
