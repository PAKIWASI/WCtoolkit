// Instantiates matrix_generic.h for one type so the macro bodies are compiled
// and exercised. Separate TU: matrix.h defines float-specific helpers with the
// same names (ZEROS_1D differs in arity).
#include "arena.h"
#include "common.h"
#include "matrix_generic.h"
#include "wc_allocator.h"
#include "wc_test.h"
#include "wc_test_allocator.h"
#include "wc_test_fatal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// The instantiated functions have external linkage and no header.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
INSTANTIATE_MATRIX(double, "%f")
#pragma GCC diagnostic pop

#define EPS 1e-9

static int dmat_eq(const Matrix_double* m, const double* e)
{
    for (u64 i = 0; i < MATRIX_TOTAL(m); i++) {
        if (fabs(m->data[i] - e[i]) > EPS) { return 0; }
    }
    return 1;
}

static void test_generic_create_copy_move_destroy(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator al = wc_test_alloc_allocator(&ta);

    double        src[] = {1, 2, 3, 4};
    Matrix_double a     = matrix_create_arr_double(al, 2, 2, src);
    Matrix_double b     = matrix_copy_double(al, &a);
    WC_EXPECT_TRUE(b.data != a.data);
    WC_EXPECT(dmat_eq(&b, src));

    Matrix_double c;
    matrix_move_double(&c, &b);
    WC_EXPECT_NULL(b.data);
    WC_EXPECT(dmat_eq(&c, src));

    matrix_destroy_double(&a);
    matrix_destroy_double(&b); // moved-from: no-op
    matrix_destroy_double(&c);
    WC_EXPECT_EQ_U64(ta.n_errors, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_generic_ops(void)
{
    Matrix_double a = matrix_create_arr_double(WC_LIBC, 2, 3, (double[]){1, 2, 3, 4, 5, 6});
    Matrix_double b = matrix_create_arr_double(WC_LIBC, 3, 2, (double[]){7, 8, 9, 10, 11, 12});
    double        od[4];
    Matrix_double o = matrix_create_buf_double(2, 2, od);

    matrix_xply_double(&o, &a, &b);
    WC_EXPECT(dmat_eq(&o, (double[]){58, 64, 139, 154}));
    memset(od, 0, sizeof(od));
    matrix_xply_2_double(&o, &a, &b); // libc scratch transpose inside
    WC_EXPECT(dmat_eq(&o, (double[]){58, 64, 139, 154}));

    Matrix_double s = matrix_create_arr_double(WC_LIBC, 3, 3, (double[]){3, 2, 4, 2, 0, 2, 4, 2, 3});
    WC_EXPECT(fabs(matrix_det_double(&s) - 8.0) < 1e-6);

    matrix_destroy_double(&o); // borrowed: frees nothing
    matrix_destroy_double(&a);
    matrix_destroy_double(&b);
    matrix_destroy_double(&s);
}

static void test_generic_on_arena(void)
{
    Arena arena;
    Arena_create(&arena, WC_LIBC, nKB(1));
    Matrix_double m = matrix_create_double(Arena_allocator(&arena), 4, 4);
    WC_EXPECT_TRUE((u8*)m.data >= arena.base && (u8*)m.data < arena.base + arena.size);
    WC_EXPECT_EQ_U64((uintptr_t)m.data % alignof(double), 0);
    matrix_destroy_double(&m);
    Arena_destroy(&arena);
}

static void generic_scale_after_move(void)
{
    Matrix_double a = matrix_create_double(WC_LIBC, 2, 2);
    Matrix_double b;
    matrix_move_double(&b, &a);
    matrix_scale_double(&a, 2.0);
}

static void test_generic_zero_state_dies(void)
{
    WC_EXPECT_DIES(generic_scale_after_move);
}

void matrix_generic_suite(void);
void matrix_generic_suite(void)
{
    WC_SUITE("Matrix (generic)");
    WC_RUN(test_generic_create_copy_move_destroy);
    WC_RUN(test_generic_ops);
    WC_RUN(test_generic_on_arena);
    WC_RUN(test_generic_zero_state_dies);
}
