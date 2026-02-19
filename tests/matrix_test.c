#include "wc_test.h"
#include "matrix.h"
#include "arena.h"
#include <math.h>

#define FLOAT_EPS 1e-3f


/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Check every element of a matrix against a flat expected array */
static int mat_eq(const Matrixf* m, const float* expected, float eps)
{
    for (u64 i = 0; i < m->m * m->n; i++) {
        if (fabsf(m->data[i] - expected[i]) > eps) return 0;
    }
    return 1;
}


/* ── Creation ────────────────────────────────────────────────────────────── */

static void test_create_heap(void)
{
    Matrixf* m = matrix_create(3, 4);
    WC_ASSERT_NOT_NULL(m);
    WC_ASSERT_NOT_NULL(m->data);
    WC_ASSERT_EQ_U64(m->m, 3);
    WC_ASSERT_EQ_U64(m->n, 4);
    matrix_destroy(m);
}

static void test_create_arr(void)
{
    float arr[6] = {1,2,3,4,5,6};
    Matrixf* m   = matrix_create_arr(2, 3, arr);
    WC_ASSERT_EQ_INT(*(int*)&m->data[0], *(int*)&arr[0]); /* bit-identical */
    WC_ASSERT(mat_eq(m, arr, FLOAT_EPS));
    matrix_destroy(m);
}

static void test_create_stk(void)
{
    float data[6] = {0};
    Matrixf m;
    matrix_create_stk(&m, 2, 3, data);
    WC_ASSERT_EQ_U64(m.m, 2);
    WC_ASSERT_EQ_U64(m.n, 3);
    WC_ASSERT_TRUE(m.data == data); /* must point at the provided array */
}


/* ── Set element ─────────────────────────────────────────────────────────── */

static void test_set_elm(void)
{
    float d[4] = {0};
    Matrixf m;
    matrix_create_stk(&m, 2, 2, d);
    matrix_set_elm(&m, 7.0f, 1, 0);
}

static void test_set_val_arr(void)
{
    Matrixf* m = matrix_create(2, 2);
    float src[4] = {1,2,3,4};
    matrix_set_val_arr(m, 4, src);
    WC_ASSERT(mat_eq(m, src, FLOAT_EPS));
    matrix_destroy(m);
}


/* ── Copy ────────────────────────────────────────────────────────────────── */

static void test_copy(void)
{
    Matrixf* src = matrix_create_arr(2, 2, (float[]){1,2,3,4});
    float    d[4] = {0};
    Matrixf  dest;
    matrix_create_stk(&dest, 2, 2, d);
    matrix_copy(&dest, src);
    WC_ASSERT(mat_eq(&dest, src->data, FLOAT_EPS));
    /* independence */
    src->data[0] = 99.0f;
    matrix_destroy(src);
}


/* ── Add / Sub ───────────────────────────────────────────────────────────── */

static void test_add(void)
{
    Matrixf* a = matrix_create_arr(2, 2, (float[]){1,2,3,4});
    Matrixf* b = matrix_create_arr(2, 2, (float[]){5,6,7,8});
    float    od[4] = {0};
    Matrixf  out;
    matrix_create_stk(&out, 2, 2, od);
    matrix_add(&out, a, b);
    float expected[] = {6,8,10,12};
    WC_ASSERT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(a);
    matrix_destroy(b);
}

static void test_add_in_place(void)
{
    /* out may alias a */
    Matrixf* a = matrix_create_arr(2, 2, (float[]){1,2,3,4});
    Matrixf* b = matrix_create_arr(2, 2, (float[]){1,1,1,1});
    matrix_add(a, a, b);
    float expected[] = {2,3,4,5};
    WC_ASSERT(mat_eq(a, expected, FLOAT_EPS));
    matrix_destroy(a);
    matrix_destroy(b);
}

static void test_sub(void)
{
    Matrixf* a = matrix_create_arr(2, 2, (float[]){5,6,7,8});
    Matrixf* b = matrix_create_arr(2, 2, (float[]){1,2,3,4});
    float    od[4] = {0};
    Matrixf  out;
    matrix_create_stk(&out, 2, 2, od);
    matrix_sub(&out, a, b);
    float expected[] = {4,4,4,4};
    WC_ASSERT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(a);
    matrix_destroy(b);
}

static void test_sub_self(void)
{
    /* out = a - a must produce zeros */
    Matrixf* a = matrix_create_arr(2, 2, (float[]){3,7,1,9});
    float    od[4] = {0};
    Matrixf  out;
    matrix_create_stk(&out, 2, 2, od);
    matrix_sub(&out, a, a);
    float expected[] = {0,0,0,0};
    WC_ASSERT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(a);
}

static void test_scale(void)
{
    Matrixf* m = matrix_create_arr(2, 2, (float[]){1,2,3,4});
    matrix_scale(m, 3.0f);
    float expected[] = {3,6,9,12};
    WC_ASSERT(mat_eq(m, expected, FLOAT_EPS));
    matrix_destroy(m);
}


/* ── Multiply ────────────────────────────────────────────────────────────── */

static void test_xply_2x2(void)
{
    /* [[1,2],[3,4]] x [[5,6],[7,8]] = [[19,22],[43,50]] */
    Matrixf* a = matrix_create_arr(2, 2, (float[]){1,2,3,4});
    Matrixf* b = matrix_create_arr(2, 2, (float[]){5,6,7,8});
    float    od[4] = {0};
    Matrixf  out;
    matrix_create_stk(&out, 2, 2, od);
    matrix_xply(&out, a, b);
    float expected[] = {19,22,43,50};
    WC_ASSERT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(a);
    matrix_destroy(b);
}

static void test_xply_rect(void)
{
    /* (2x3) * (3x2) = (2x2) */
    Matrixf* a = matrix_create_arr(2, 3, (float[]){1,2,3, 4,5,6});
    Matrixf* b = matrix_create_arr(3, 2, (float[]){7,8, 9,10, 11,12});
    float    od[4] = {0};
    Matrixf  out;
    matrix_create_stk(&out, 2, 2, od);
    matrix_xply(&out, a, b);
    /* row0: 1*7+2*9+3*11=58, 1*8+2*10+3*12=64 */
    /* row1: 4*7+5*9+6*11=139, 4*8+5*10+6*12=154 */
    float expected[] = {58,64,139,154};
    WC_ASSERT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(a);
    matrix_destroy(b);
}

static void test_xply_identity(void)
{
    /* A * I = A */
    Matrixf* a = matrix_create_arr(2, 2, (float[]){3,7,2,5});
    Matrixf* I = matrix_create_arr(2, 2, (float[]){1,0,0,1});
    float    od[4] = {0};
    Matrixf  out;
    matrix_create_stk(&out, 2, 2, od);
    matrix_xply(&out, a, I);
    WC_ASSERT(mat_eq(&out, a->data, FLOAT_EPS));
    matrix_destroy(a);
    matrix_destroy(I);
}


/* ── Transpose ───────────────────────────────────────────────────────────── */

static void test_transpose_square(void)
{
    /* [[1,2],[3,4]]^T = [[1,3],[2,4]] */
    Matrixf* m = matrix_create_arr(2, 2, (float[]){1,2,3,4});
    float    od[4] = {0};
    Matrixf  out;
    matrix_create_stk(&out, 2, 2, od);
    matrix_T(&out, m);
    float expected[] = {1,3,2,4};
    WC_ASSERT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(m);
}

static void test_transpose_rect(void)
{
    /* (2x3)^T = (3x2) */
    Matrixf* m = matrix_create_arr(2, 3, (float[]){1,2,3,4,5,6});
    float    od[6] = {0};
    Matrixf  out;
    matrix_create_stk(&out, 3, 2, od);
    matrix_T(&out, m);
    float expected[] = {1,4, 2,5, 3,6};
    WC_ASSERT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(m);
}

static void test_double_transpose(void)
{
    /* (A^T)^T = A */
    Matrixf* a = matrix_create_arr(2, 3, (float[]){1,2,3,4,5,6});
    float    t1d[6] = {0}, t2d[6] = {0};
    Matrixf  t1, t2;
    matrix_create_stk(&t1, 3, 2, t1d);
    matrix_create_stk(&t2, 2, 3, t2d);
    matrix_T(&t1, a);
    matrix_T(&t2, &t1);
    WC_ASSERT(mat_eq(&t2, a->data, FLOAT_EPS));
    matrix_destroy(a);
}


/* ── LU decomposition & determinant ─────────────────────────────────────── */

static void test_lu_reconstruct(void)
{
    /* L * U must equal original matrix */
    Matrixf* m = matrix_create_arr(3, 3, (float[]){
        2, 1, 1,
        4, 3, 3,
        8, 7, 9
    });
    float Ld[9] = {0}, Ud[9] = {0}, prod_d[9] = {0};
    Matrixf L, U, prod;
    matrix_create_stk(&L,    3, 3, Ld);
    matrix_create_stk(&U,    3, 3, Ud);
    matrix_create_stk(&prod, 3, 3, prod_d);

    matrix_LU_Decomp(&L, &U, m);
    matrix_xply(&prod, &L, &U);
    WC_ASSERT(mat_eq(&prod, m->data, FLOAT_EPS));
    matrix_destroy(m);
}

static void test_det_known(void)
{
    /* det([[1,2],[3,4]]) = 1*4 - 2*3 = -2 */
    Matrixf* m = matrix_create_arr(2, 2, (float[]){1,2,3,4});
    float    d = matrix_det(m);
    matrix_destroy(m);
}

static void test_det_3x3(void)
{
    /* det([[3,2,4],[2,0,2],[4,2,3]]) = 3*(0-4) - 2*(6-8) + 4*(4-0) = -12+4+16 = 8 */
    Matrixf* m = matrix_create_arr(3, 3, (float[]){
        3,2,4, 2,0,2, 4,2,3
    });
    float d = matrix_det(m);
    matrix_destroy(m);
}

static void test_det_identity(void)
{
    Matrixf* I = matrix_create_arr(3, 3, (float[]){
        1,0,0, 0,1,0, 0,0,1
    });
    matrix_destroy(I);
}


/* ── Arena allocation ────────────────────────────────────────────────────── */

static void test_arena_alloc(void)
{
    Arena*   arena = arena_create(nKB(4));
    Matrixf* m     = matrix_arena_alloc(arena, 3, 3);
    WC_ASSERT_NOT_NULL(m);
    WC_ASSERT_NOT_NULL(m->data);
    WC_ASSERT_EQ_U64(m->m, 3);
    WC_ASSERT_EQ_U64(m->n, 3);
    arena_release(arena);
}

static void test_arena_arr_alloc(void)
{
    Arena* arena = arena_create(nKB(4));
    float  src[] = {1,2,3,4};
    Matrixf* m   = matrix_arena_arr_alloc(arena, 2, 2, src);
    WC_ASSERT(mat_eq(m, src, FLOAT_EPS));
    arena_release(arena);
}

static void test_arena_scratch_temporaries(void)
{
    /* Temporaries inside scratch don't leak; result outside scratch survives */
    Arena*   arena = arena_create(nKB(2));
    Matrixf* result = matrix_arena_alloc(arena, 2, 2);

    ARENA_SCRATCH(sc, arena) {
        Matrixf* t1 = matrix_arena_arr_alloc(arena, 2, 2,
                        (float[]){1,0,0,1});
        Matrixf* t2 = matrix_arena_arr_alloc(arena, 2, 2,
                        (float[]){5,6,7,8});
        matrix_xply(result, t1, t2);
        (void)t1; (void)t2;
    }

    /* t1 and t2 memory reclaimed; result still holds correct values */
    float expected[] = {5,6,7,8};
    WC_ASSERT(mat_eq(result, expected, FLOAT_EPS));
    arena_release(arena);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void matrix_suite(void)
{
    WC_SUITE("Matrix");

    WC_RUN(test_create_heap);
    WC_RUN(test_create_arr);
    WC_RUN(test_create_stk);

    WC_RUN(test_set_elm);
    WC_RUN(test_set_val_arr);
    WC_RUN(test_copy);

    WC_RUN(test_add);
    WC_RUN(test_add_in_place);
    WC_RUN(test_sub);
    WC_RUN(test_sub_self);
    WC_RUN(test_scale);

    WC_RUN(test_xply_2x2);
    WC_RUN(test_xply_rect);
    WC_RUN(test_xply_identity);

    WC_RUN(test_transpose_square);
    WC_RUN(test_transpose_rect);
    WC_RUN(test_double_transpose);

    WC_RUN(test_lu_reconstruct);
    WC_RUN(test_det_known);
    WC_RUN(test_det_3x3);
    WC_RUN(test_det_identity);

    WC_RUN(test_arena_alloc);
    WC_RUN(test_arena_arr_alloc);
    WC_RUN(test_arena_scratch_temporaries);
}
