#include "wc_speed_test.h"
#include "matrix.h"

void matrix_speed_suite(void)
{
    WC_BENCH_SUITE("Matrix");

    /* ── Multiply ─────────────────────────────────────────────────────── */
    WC_BENCH("xply 64x64 x1000 (blocked ikj)", 1000, {
        const u64 N = 64;
        Matrixf* a   = matrix_create(N, N);
        Matrixf* b   = matrix_create(N, N);
        Matrixf* out = matrix_create(N, N);
        for (u64 i = 0; i < N * N; i++) { a->data[i] = 1.0f; b->data[i] = 1.0f; }
        for (int i = 0; i < 1000; i++) { matrix_xply(out, a, b); }
        matrix_destroy(a); matrix_destroy(b); matrix_destroy(out);
    });

    WC_BENCH("xply_2 64x64 x1000 (transpose-B)", 1000, {
        const u64 N = 64;
        Matrixf* a   = matrix_create(N, N);
        Matrixf* b   = matrix_create(N, N);
        Matrixf* out = matrix_create(N, N);
        for (u64 i = 0; i < N * N; i++) { a->data[i] = 1.0f; b->data[i] = 1.0f; }
        for (int i = 0; i < 1000; i++) { matrix_xply_2(out, a, b); }
        matrix_destroy(a); matrix_destroy(b); matrix_destroy(out);
    });

    WC_BENCH("xply 256x256 x10 (blocked ikj)", 10, {
        const u64 N = 256;
        Matrixf* a   = matrix_create(N, N);
        Matrixf* b   = matrix_create(N, N);
        Matrixf* out = matrix_create(N, N);
        for (u64 i = 0; i < N * N; i++) { a->data[i] = 1.0f; b->data[i] = 1.0f; }
        for (int i = 0; i < 10; i++) { matrix_xply(out, a, b); }
        matrix_destroy(a); matrix_destroy(b); matrix_destroy(out);
    });

    WC_BENCH("xply_2 256x256 x10 (transpose-B)", 10, {
        const u64 N = 256;
        Matrixf* a   = matrix_create(N, N);
        Matrixf* b   = matrix_create(N, N);
        Matrixf* out = matrix_create(N, N);
        for (u64 i = 0; i < N * N; i++) { a->data[i] = 1.0f; b->data[i] = 1.0f; }
        for (int i = 0; i < 10; i++) { matrix_xply_2(out, a, b); }
        matrix_destroy(a); matrix_destroy(b); matrix_destroy(out);
    });

    /* ── Transpose ────────────────────────────────────────────────────── */
    WC_BENCH("transpose 512x512 x100", 100, {
        const u64 N = 512;
        Matrixf* m   = matrix_create(N, N);
        Matrixf* out = matrix_create(N, N);
        for (int i = 0; i < 100; i++) { matrix_T(out, m); }
        matrix_destroy(m); matrix_destroy(out);
    });

    /* ── Add ──────────────────────────────────────────────────────────── */
    WC_BENCH("add 1024x1024 x100", 100, {
        const u64 N = 1024;
        Matrixf* a   = matrix_create(N, N);
        Matrixf* b   = matrix_create(N, N);
        Matrixf* out = matrix_create(N, N);
        for (int i = 0; i < 100; i++) { matrix_add(out, a, b); }
        matrix_destroy(a); matrix_destroy(b); matrix_destroy(out);
    });

    /* ── Det / LU ─────────────────────────────────────────────────────── */
    WC_BENCH("det 32x32 x1000", 1000, {
        const u64 N = 32;
        Matrixf* m = matrix_create(N, N);
        for (u64 i = 0; i < N; i++) { m->data[IDX(m, i, i)] = (float)(i + 1); }
        for (int i = 0; i < 1000; i++) { matrix_det(m); }
        matrix_destroy(m);
    });
}
