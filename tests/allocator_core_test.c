#include "common.h"
#include "wc_allocator.h"
#include "wc_test.h"
#include "wc_test_allocator.h"

#include <stdint.h>

// These tests hand stack buffers to allocators on purpose (wc_borrowed, foreign
// pointer detection). The pointer never reaches libc, but GCC cannot prove the
// vtable is non-NULL and warns about the libc branch.
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"

void allocator_core_suite(void);


/* ── libc (WC_LIBC) ──────────────────────────────────────────────────────── */

static void test_libc_is_zero_initialised_allocator(void)
{
    wc_allocator z;
    memset(&z, 0, sizeof(z));
    WC_EXPECT(wc_is_libc(z));
    WC_EXPECT(wc_is_libc(WC_LIBC));
    WC_EXPECT_FALSE(wc_is_libc(wc_borrowed));
}

static void test_libc_alloc_realloc_free(void)
{
    u8* p = wc_alloc(WC_LIBC, 10, 1);
    WC_EXPECT_NOT_NULL(p);
    if (!p) {
        return;
    }
    memcpy(p, "abcdefghij", 10);
    p = wc_realloc(WC_LIBC, p, 10, 1000, 1);
    if (!p) {
        WC_EXPECT_NOT_NULL(p);
        return;
    }
    WC_EXPECT(memcmp(p, "abcdefghij", 10) == 0);
    wc_free(WC_LIBC, p, 1000, 1);
}

static void test_libc_over_aligned(void)
{
    const size_t aligns[] = {32, 64, 256, 4096};
    for (u64 i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++) {
        u8* p = wc_alloc(WC_LIBC, 100, aligns[i]);
        WC_EXPECT_NOT_NULL(p);
        if (!p) {
            return;
        }
        WC_EXPECT_EQ_U64((uintptr_t)p % aligns[i], 0);
        memset(p, 1, 100);
        p = wc_realloc(WC_LIBC, p, 100, 5000, aligns[i]); // realloc keeps over-alignment
        if (!p) {
            WC_EXPECT_NOT_NULL(p);
            return;
        }
        WC_EXPECT_EQ_U64((uintptr_t)p % aligns[i], 0);
        WC_EXPECT_EQ_INT(p[99], 1);
        wc_free(WC_LIBC, p, 5000, aligns[i]);
    }
}


/* ── Wrapper contract (checked by the test allocator in ABORT mode) ──────── */

static void test_zero_size_never_reaches_backend(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    WC_EXPECT_NULL(wc_alloc(a, 0, 8));
    WC_EXPECT_EQ_U64(ta.n_attempts, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_realloc_null_is_alloc_and_zero_is_free(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u8* p = wc_realloc(a, NULL, 0, 32, 8);
    WC_EXPECT_NOT_NULL(p);
    WC_EXPECT_EQ_U64(ta.n_alloc, 1);

    WC_EXPECT_NULL(wc_realloc(a, p, 32, 0, 8));
    WC_EXPECT_EQ_U64(ta.n_free, 1);
    WC_EXPECT_EQ_U64(ta.live_blocks, 0);

    wc_free(a, NULL, 0, 8); // NULL free: no backend call
    WC_EXPECT_EQ_U64(ta.n_errors, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_small_alignment_passes_through(void)
{
    // D7: wrappers no longer raise align to WC_MAX_ALIGN
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u8* p = wc_alloc(a, 12, 4);
    wc_free(a, p, 12, 4); // must match exactly: 4, not 16
    WC_EXPECT_EQ_U64(ta.n_errors, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_typed_macros_and_overflow(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u64* v = WC_NEW_N(a, u64, 8);
    WC_EXPECT_NOT_NULL(v);
    if (!v) {
        return;
    }
    v[7] = 42;
    v    = WC_REALLOC_N(a, u64, v, 8, 16); // this is the A5 shape, now well-formed
    if (!v) {
        WC_EXPECT_NOT_NULL(v);
        return;
    }
    WC_EXPECT_EQ_U64(v[7], 42);

    // overflowing count fails cleanly and leaves v alive (no wrap to a small size)
    WC_EXPECT_NULL(WC_REALLOC_N(a, u64, v, 16, SIZE_MAX / 4));
    WC_EXPECT(wc_test_alloc_owns(&ta, v));
    WC_EXPECT_NULL(WC_NEW_N(a, u64, SIZE_MAX / 2));

    WC_DELETE_N(a, u64, v, 16);

    int* one = WC_NEW(a, int);
    WC_DELETE(a, int, one);

    WC_EXPECT_EQ_U64(ta.n_errors, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

// Backend with no realloc: the wrapper must emulate alloc + copy + free.
static void* noreal_alloc(void* ctx, size_t size, size_t align)
{
    return wc_test_alloc_cb_alloc(ctx, size, align);
}
static void noreal_free(void* ctx, void* p, size_t size, size_t align)
{
    wc_test_alloc_cb_free(ctx, p, size, align);
}
static const wc_alloc_vtable noreal_vt = {.alloc = noreal_alloc, .realloc = NULL, .free = noreal_free};

static void test_missing_realloc_is_emulated(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = {.vt = &noreal_vt, .ctx = &ta};

    u8* p = wc_alloc(a, 16, 8);
    memcpy(p, "0123456789abcdef", 16);
    u8* q = wc_realloc(a, p, 16, 64, 8);
    WC_EXPECT(memcmp(q, "0123456789abcdef", 16) == 0);
    WC_EXPECT_EQ_U64(ta.n_free, 1); // old block freed with its exact size
    wc_free(a, q, 64, 8);

    WC_EXPECT_EQ_U64(ta.n_errors, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}


/* ── wc_borrowed ─────────────────────────────────────────────────────────── */

static void test_borrowed_never_allocates_or_frees(void)
{
    u8 buf[32] = {1, 2, 3};
    WC_EXPECT_NULL(wc_alloc(wc_borrowed, 16, 8));
    WC_EXPECT_NULL(wc_realloc(wc_borrowed, buf, 32, 64, 8)); // growth fails
    WC_EXPECT_EQ_INT(buf[2], 3);                              // buffer untouched
    wc_free(wc_borrowed, buf, 32, 8);                        // no-op (ASAN would catch a real free)
    WC_EXPECT_EQ_INT(buf[0], 1);
}


/* ── Helpers ─────────────────────────────────────────────────────────────── */

static void test_align_for_size(void)
{
    WC_EXPECT_EQ_U64(wc_align_for_size(1), 1);
    WC_EXPECT_EQ_U64(wc_align_for_size(3), 1);
    WC_EXPECT_EQ_U64(wc_align_for_size(4), 4);
    WC_EXPECT_EQ_U64(wc_align_for_size(12), 4);
    WC_EXPECT_EQ_U64(wc_align_for_size(24), 8);
    WC_EXPECT_EQ_U64(wc_align_for_size(40), 8);
    WC_EXPECT_EQ_U64(wc_align_for_size(64), WC_MAX_ALIGN);
    WC_EXPECT_EQ_U64(wc_align_for_size(sizeof(long double)), alignof(long double));
}

static void test_same(void)
{
    int x = 0, y = 0;
    wc_allocator a = {.vt = &noreal_vt, .ctx = &x};
    wc_allocator b = {.vt = &noreal_vt, .ctx = &y};
    WC_EXPECT(wc_same(a, a));
    WC_EXPECT_FALSE(wc_same(a, b));
    WC_EXPECT(wc_same(WC_LIBC, WC_LIBC));
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void allocator_core_suite(void)
{
    WC_SUITE("Allocator core (wc_)");

    WC_RUN(test_libc_is_zero_initialised_allocator);
    WC_RUN(test_libc_alloc_realloc_free);
    WC_RUN(test_libc_over_aligned);
    WC_RUN(test_zero_size_never_reaches_backend);
    WC_RUN(test_realloc_null_is_alloc_and_zero_is_free);
    WC_RUN(test_small_alignment_passes_through);
    WC_RUN(test_typed_macros_and_overflow);
    WC_RUN(test_missing_realloc_is_emulated);
    WC_RUN(test_borrowed_never_allocates_or_frees);
    WC_RUN(test_align_for_size);
    WC_RUN(test_same);
}
