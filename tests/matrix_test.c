#include "arena.h"
#include "common.h"
#include "matrix.h"
#include "wc_allocator.h"
#include "wc_test.h"
#include "wc_test_allocator.h"
#include "wc_test_fatal.h"
#include <math.h>
#include <string.h>

#define FLOAT_EPS 1e-3f


// Helpers

// Check every element of a matrix against a flat expected array
static int mat_eq(const Matrixf* m, const float* expected, float eps)
{
    for (u64 i = 0; i < m->m * m->n; i++) {
        if (fabsf(m->data[i] - expected[i]) > eps) { return 0; }
    }
    return 1;
}

static int is_zeroed(const Matrixf* m)
{
    static const Matrixf zero = {0};
    return memcmp(m, &zero, sizeof(zero)) == 0;
}


// Creation / destruction

static void test_create(void)
{
    Matrixf m = matrix_create(WC_LIBC, 3, 4);
    WC_EXPECT_NOT_NULL(m.data);
    WC_EXPECT_EQ_U64(m.m, 3);
    WC_EXPECT_EQ_U64(m.n, 4);
    matrix_destroy(&m);
    WC_EXPECT_TRUE(is_zeroed(&m));
}

static void test_create_arr(void)
{
    float   arr[6] = {1, 2, 3, 4, 5, 6};
    Matrixf m      = matrix_create_arr(WC_LIBC, 2, 3, arr);
    WC_EXPECT(memcmp(m.data, arr, sizeof(arr)) == 0); // bit-identical
    WC_EXPECT_TRUE(m.data != arr);                    // owns a copy
    matrix_destroy(&m);
}

static void test_create_buf_borrows(void)
{
    float   data[6] = {0};
    Matrixf m       = matrix_create_buf(2, 3, data);
    WC_EXPECT_EQ_U64(m.m, 2);
    WC_EXPECT_EQ_U64(m.n, 3);
    WC_EXPECT_TRUE(m.data == data); // must point at the provided array
    matrix_destroy(&m);             // frees nothing (wc_borrowed): ASAN would flag a stack free
    WC_EXPECT_TRUE(is_zeroed(&m));
}

static void test_macros(void)
{
    Matrixf a = MATRIX(2, 2);
    WC_EXPECT_EQ_U64(a.m * a.n, 4);
    matrix_destroy(&a);

    Arena arena;
    Arena_create(&arena, WC_LIBC, nKB(1));
    Matrixf b = MATRIX_IN(Arena_allocator(&arena), 3, 3);
    WC_EXPECT_TRUE((u8*)b.data >= arena.base && (u8*)b.data < arena.base + arena.size);
    matrix_destroy(&b);
    Arena_destroy(&arena);
}

static void test_destroy_zeroed_is_safe(void)
{
    Matrixf z;
    memset(&z, 0, sizeof(z));
    matrix_destroy(&z);
    matrix_destroy(&z); // idempotent
    WC_EXPECT_TRUE(is_zeroed(&z));
}


// Set element

static void test_set_elm(void)
{
    float   d[4] = {0};
    Matrixf m    = matrix_create_buf(2, 2, d);
    matrix_set_elm(&m, 7.0f, 1, 0);
    WC_EXPECT(fabsf(matrix_get_elm(&m, 1, 0) - 7.0f) < FLOAT_EPS);
    WC_EXPECT(fabsf(d[2] - 7.0f) < FLOAT_EPS); // row-major, written through to the buffer
}

static void test_set_val_arr(void)
{
    Matrixf m      = matrix_create(WC_LIBC, 2, 2);
    float   src[4] = {1, 2, 3, 4};
    matrix_set_val_arr(&m, 4, src);
    WC_EXPECT(mat_eq(&m, src, FLOAT_EPS));
    matrix_destroy(&m);
}


// Copy / move

static void test_copy_is_independent(void)
{
    Matrixf src  = matrix_create_arr(WC_LIBC, 2, 2, (float[]){1, 2, 3, 4});
    Matrixf dest = matrix_copy(WC_LIBC, &src);
    WC_EXPECT_TRUE(dest.data != src.data);
    WC_EXPECT(mat_eq(&dest, src.data, FLOAT_EPS));
    src.data[0] = 99.0f;
    WC_EXPECT(fabsf(dest.data[0] - 1.0f) < FLOAT_EPS);
    matrix_destroy(&src);
    matrix_destroy(&dest);
}

static void test_copy_across_allocators(void)
{
    // arena -> libc: the copy must not inherit src->alloc (A7 for matrices)
    Arena arena;
    Arena_create(&arena, WC_LIBC, nKB(1));
    Matrixf src  = matrix_create_arr(Arena_allocator(&arena), 2, 2, (float[]){1, 2, 3, 4});
    Matrixf dest = matrix_copy(WC_LIBC, &src);
    WC_EXPECT_TRUE(dest.alloc.vt == NULL); // libc
    Arena_destroy(&arena);                 // src's memory gone; dest must survive
    float expected[] = {1, 2, 3, 4};
    WC_EXPECT(mat_eq(&dest, expected, FLOAT_EPS));
    matrix_destroy(&dest);
}

static void test_copy_from_buf(void)
{
    float   d[4] = {5, 6, 7, 8};
    Matrixf b    = matrix_create_buf(2, 2, d);
    Matrixf c    = matrix_copy(WC_LIBC, &b); // borrowed source, owning copy
    WC_EXPECT_TRUE(c.data != d);
    WC_EXPECT(mat_eq(&c, d, FLOAT_EPS));
    matrix_destroy(&c);
}

static void test_move_zeroes_src(void)
{
    Matrixf src  = matrix_create_arr(WC_LIBC, 2, 2, (float[]){1, 2, 3, 4});
    float*  data = src.data;
    Matrixf dest;
    matrix_move(&dest, &src);
    WC_EXPECT_TRUE(dest.data == data); // storage transferred, not copied
    WC_EXPECT_TRUE(is_zeroed(&src));
    matrix_destroy(&src); // zero-safe
    matrix_destroy(&dest);
}


// Add / Sub

static void test_add(void)
{
    Matrixf a     = matrix_create_arr(WC_LIBC, 2, 2, (float[]){1, 2, 3, 4});
    Matrixf b     = matrix_create_arr(WC_LIBC, 2, 2, (float[]){5, 6, 7, 8});
    float   od[4] = {0};
    Matrixf out   = matrix_create_buf(2, 2, od);
    matrix_add(&out, &a, &b);
    float expected[] = {6, 8, 10, 12};
    WC_EXPECT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(&a);
    matrix_destroy(&b);
}

static void test_sub(void)
{
    Matrixf a     = matrix_create_arr(WC_LIBC, 2, 2, (float[]){5, 6, 7, 8});
    Matrixf b     = matrix_create_arr(WC_LIBC, 2, 2, (float[]){1, 2, 3, 4});
    float   od[4] = {0};
    Matrixf out   = matrix_create_buf(2, 2, od);
    matrix_sub(&out, &a, &b);
    float expected[] = {4, 4, 4, 4};
    WC_EXPECT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(&a);
    matrix_destroy(&b);
}

static void test_sub_self(void)
{
    // out = a - a must produce zeros
    Matrixf a     = matrix_create_arr(WC_LIBC, 2, 2, (float[]){3, 7, 1, 9});
    float   od[4] = {0};
    Matrixf out   = matrix_create_buf(2, 2, od);
    matrix_sub(&out, &a, &a);
    float expected[] = {0, 0, 0, 0};
    WC_EXPECT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(&a);
}

static void test_scale(void)
{
    Matrixf m = matrix_create_arr(WC_LIBC, 2, 2, (float[]){1, 2, 3, 4});
    matrix_scale(&m, 3.0f);
    float expected[] = {3, 6, 9, 12};
    WC_EXPECT(mat_eq(&m, expected, FLOAT_EPS));
    matrix_destroy(&m);
}


// Multiply

static void test_xply_2x2(void)
{
    // [[1,2],[3,4]] x [[5,6],[7,8]] = [[19,22],[43,50]]
    Matrixf a     = matrix_create_arr(WC_LIBC, 2, 2, (float[]){1, 2, 3, 4});
    Matrixf b     = matrix_create_arr(WC_LIBC, 2, 2, (float[]){5, 6, 7, 8});
    float   od[4] = {0};
    Matrixf out   = matrix_create_buf(2, 2, od);
    matrix_xply(&out, &a, &b);
    float expected[] = {19, 22, 43, 50};
    WC_EXPECT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(&a);
    matrix_destroy(&b);
}

static void test_xply_rect(void)
{
    // (2x3) * (3x2) = (2x2)
    Matrixf a     = matrix_create_arr(WC_LIBC, 2, 3, (float[]){1, 2, 3, 4, 5, 6});
    Matrixf b     = matrix_create_arr(WC_LIBC, 3, 2, (float[]){7, 8, 9, 10, 11, 12});
    float   od[4] = {0};
    Matrixf out   = matrix_create_buf(2, 2, od);
    matrix_xply(&out, &a, &b);
    // row0: 1*7+2*9+3*11=58, 1*8+2*10+3*12=64
    // row1: 4*7+5*9+6*11=139, 4*8+5*10+6*12=154
    float expected[] = {58, 64, 139, 154};
    WC_EXPECT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(&a);
    matrix_destroy(&b);
}

static void test_xply_2_matches_xply(void)
{
    Matrixf a = matrix_create_arr(WC_LIBC, 2, 3, (float[]){1, 2, 3, 4, 5, 6});
    Matrixf b = matrix_create_arr(WC_LIBC, 3, 2, (float[]){7, 8, 9, 10, 11, 12});
    Matrixf o = matrix_create(WC_LIBC, 2, 2);
    matrix_xply_2(&o, &a, &b); // uses an internal borrowed transpose buffer
    float expected[] = {58, 64, 139, 154};
    WC_EXPECT(mat_eq(&o, expected, FLOAT_EPS));
    matrix_destroy(&a);
    matrix_destroy(&b);
    matrix_destroy(&o);
}

static void test_xply_identity(void)
{
    // A * I = A
    Matrixf a     = matrix_create_arr(WC_LIBC, 2, 2, (float[]){3, 7, 2, 5});
    Matrixf I     = matrix_create_arr(WC_LIBC, 2, 2, (float[]){1, 0, 0, 1});
    float   od[4] = {0};
    Matrixf out   = matrix_create_buf(2, 2, od);
    matrix_xply(&out, &a, &I);
    WC_EXPECT(mat_eq(&out, a.data, FLOAT_EPS));
    matrix_destroy(&a);
    matrix_destroy(&I);
}


// Transpose

static void test_transpose_square(void)
{
    // [[1,2],[3,4]]^T = [[1,3],[2,4]]
    Matrixf m     = matrix_create_arr(WC_LIBC, 2, 2, (float[]){1, 2, 3, 4});
    float   od[4] = {0};
    Matrixf out   = matrix_create_buf(2, 2, od);
    matrix_T(&out, &m);
    float expected[] = {1, 3, 2, 4};
    WC_EXPECT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(&m);
}

static void test_transpose_rect(void)
{
    // (2x3)^T = (3x2)
    Matrixf m     = matrix_create_arr(WC_LIBC, 2, 3, (float[]){1, 2, 3, 4, 5, 6});
    float   od[6] = {0};
    Matrixf out   = matrix_create_buf(3, 2, od);
    matrix_T(&out, &m);
    float expected[] = {1, 4, 2, 5, 3, 6};
    WC_EXPECT(mat_eq(&out, expected, FLOAT_EPS));
    matrix_destroy(&m);
}

static void test_double_transpose(void)
{
    // (A^T)^T = A
    Matrixf a      = matrix_create_arr(WC_LIBC, 2, 3, (float[]){1, 2, 3, 4, 5, 6});
    float   t1d[6] = {0}, t2d[6] = {0};
    Matrixf t1     = matrix_create_buf(3, 2, t1d);
    Matrixf t2     = matrix_create_buf(2, 3, t2d);
    matrix_T(&t1, &a);
    matrix_T(&t2, &t1);
    WC_EXPECT(mat_eq(&t2, a.data, FLOAT_EPS));
    matrix_destroy(&a);
}


// LU decomposition & determinant

static void test_lu_reconstruct(void)
{
    // L * U must equal original matrix
    Matrixf m     = matrix_create_arr(WC_LIBC, 3, 3, (float[]){2, 1, 1, 4, 3, 3, 8, 7, 9});
    float   Ld[9] = {0}, Ud[9] = {0}, prod_d[9] = {0};
    Matrixf L     = matrix_create_buf(3, 3, Ld);
    Matrixf U     = matrix_create_buf(3, 3, Ud);
    Matrixf prod  = matrix_create_buf(3, 3, prod_d);

    matrix_LU_Decomp(&L, &U, &m);
    matrix_xply(&prod, &L, &U);
    WC_EXPECT(mat_eq(&prod, m.data, FLOAT_EPS));
    matrix_destroy(&m);
}

static void test_det_known(void)
{
    // det([[1,2],[3,4]]) = 1*4 - 2*3 = -2
    Matrixf m = matrix_create_arr(WC_LIBC, 2, 2, (float[]){1, 2, 3, 4});
    float   d = matrix_det(&m);
    WC_EXPECT(fabsf(d - (-2.0f)) < FLOAT_EPS);
    matrix_destroy(&m);
}

static void test_det_3x3(void)
{
    // det([[3,2,4],[2,0,2],[4,2,3]]) = 3*(0-4) - 2*(6-8) + 4*(4-0) = -12+4+16 = 8
    Matrixf m = matrix_create_arr(WC_LIBC, 3, 3, (float[]){3, 2, 4, 2, 0, 2, 4, 2, 3});
    float   d = matrix_det(&m);
    WC_EXPECT(fabsf(d - 8.0f) < FLOAT_EPS);
    matrix_destroy(&m);
}

static void test_det_identity(void)
{
    Matrixf I = matrix_create_arr(WC_LIBC, 3, 3, (float[]){1, 0, 0, 0, 1, 0, 0, 0, 1});
    WC_EXPECT(fabsf(matrix_det(&I) - 1.0f) < FLOAT_EPS);
    matrix_destroy(&I);
}


// Allocators

static void test_arena_backed(void)
{
    Arena arena;
    Arena_create(&arena, WC_LIBC, nKB(4));
    wc_allocator al = Arena_allocator(&arena);
    Matrixf      m  = matrix_create_arr(al, 2, 2, (float[]){1, 2, 3, 4});
    float        expected[] = {1, 2, 3, 4};
    WC_EXPECT(mat_eq(&m, expected, FLOAT_EPS));
    u64 used = arena.idx;
    matrix_destroy(&m); // last block: rewinds the arena
    WC_EXPECT_TRUE(arena.idx < used);
    Arena_destroy(&arena);
}

static void test_arena_scratch_temporaries(void)
{
    // Temporaries inside scratch don't leak; result outside scratch survives
    Arena arena;
    Arena_create(&arena, WC_LIBC, nKB(2));
    wc_allocator al     = Arena_allocator(&arena);
    Matrixf      result = matrix_create(al, 2, 2);

    ARENA_SCRATCH(&arena)
    {
        Matrixf t1 = matrix_create_arr(al, 2, 2, (float[]){1, 0, 0, 1});
        Matrixf t2 = matrix_create_arr(al, 2, 2, (float[]){5, 6, 7, 8});
        matrix_xply(&result, &t1, &t2);
    }

    // t1 and t2 memory reclaimed; result still holds correct values
    float expected[] = {5, 6, 7, 8};
    WC_EXPECT(mat_eq(&result, expected, FLOAT_EPS));
    Arena_destroy(&arena);
}

static void test_test_allocator_leak_free(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator al = wc_test_alloc_allocator(&ta);

    Matrixf a = matrix_create_arr(al, 3, 3, (float[]){3, 2, 4, 2, 0, 2, 4, 2, 3});
    Matrixf b = matrix_copy(al, &a);
    Matrixf c;
    matrix_move(&c, &b);
    WC_EXPECT(fabsf(matrix_det(&c) - 8.0f) < FLOAT_EPS);
    WC_EXPECT_EQ_U64(ta.live_blocks, 2);

    matrix_destroy(&a);
    matrix_destroy(&b); // moved-from: no-op
    matrix_destroy(&c);
    WC_EXPECT_EQ_U64(ta.n_errors, 0); // free sizes/aligns matched their allocs
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}


// Zero state (D4): mutation of a dead matrix is fatal in every build

static void scale_after_move(void)
{
    Matrixf src = matrix_create(WC_LIBC, 2, 2);
    Matrixf dst;
    matrix_move(&dst, &src);
    matrix_scale(&src, 2.0f);
}

static void set_after_destroy(void)
{
    Matrixf m = matrix_create(WC_LIBC, 2, 2);
    matrix_destroy(&m);
    matrix_set_elm(&m, 1.0f, 0, 0);
}

static void create_zero_dims(void)
{
    Matrixf m = matrix_create(WC_LIBC, 0, 3);
    (void)m;
}

static void copy_of_zeroed(void)
{
    Matrixf z;
    memset(&z, 0, sizeof(z));
    Matrixf c = matrix_copy(WC_LIBC, &z);
    (void)c;
}

static void create_fails_on_exhausted_arena(void)
{
    Arena arena;
    Arena_create(&arena, WC_LIBC, 64);
    Matrixf m = matrix_create(Arena_allocator(&arena), 10, 10); // 400 bytes > 64
    (void)m;
}

static void test_zero_state_and_oom_die(void)
{
    WC_EXPECT_DIES(scale_after_move);
    WC_EXPECT_DIES(set_after_destroy);
    WC_EXPECT_DIES(create_zero_dims);
    WC_EXPECT_DIES(copy_of_zeroed);
    WC_EXPECT_DIES(create_fails_on_exhausted_arena);
}


// Suite entry point

void matrix_suite(void);
void matrix_suite(void)
{
    WC_SUITE("Matrix");

    WC_RUN(test_create);
    WC_RUN(test_create_arr);
    WC_RUN(test_create_buf_borrows);
    WC_RUN(test_macros);
    WC_RUN(test_destroy_zeroed_is_safe);

    WC_RUN(test_set_elm);
    WC_RUN(test_set_val_arr);

    WC_RUN(test_copy_is_independent);
    WC_RUN(test_copy_across_allocators);
    WC_RUN(test_copy_from_buf);
    WC_RUN(test_move_zeroes_src);

    WC_RUN(test_add);
    WC_RUN(test_sub);
    WC_RUN(test_sub_self);
    WC_RUN(test_scale);

    WC_RUN(test_xply_2x2);
    WC_RUN(test_xply_rect);
    WC_RUN(test_xply_2_matches_xply);
    WC_RUN(test_xply_identity);

    WC_RUN(test_transpose_square);
    WC_RUN(test_transpose_rect);
    WC_RUN(test_double_transpose);

    WC_RUN(test_lu_reconstruct);
    WC_RUN(test_det_known);
    WC_RUN(test_det_3x3);
    WC_RUN(test_det_identity);

    WC_RUN(test_arena_backed);
    WC_RUN(test_arena_scratch_temporaries);
    WC_RUN(test_test_allocator_leak_free);
    WC_RUN(test_zero_state_and_oom_die);
}
