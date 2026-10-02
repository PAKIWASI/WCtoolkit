#include "common.h"
#include "wc_allocator.h"
#include "test_support.h"
#include "wc_test_allocator.h"

#include <stdint.h>


// These tests hand stack buffers to allocators on purpose (wc_borrowed, foreign
// pointer detection). The pointer never reaches libc, but GCC cannot prove the
// vtable is non-NULL and warns about the libc branch.
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"

/* ── libc (WC_LIBC) ──────────────────────────────────────────────────────── */

UTEST(allocator, libc_is_zero_initialised_allocator)
{
    wc_allocator z;
    memset(&z, 0, sizeof(z));
    EXPECT_TRUE(wc_is_libc(z));
    EXPECT_TRUE(wc_is_libc(WC_LIBC));
    EXPECT_FALSE(wc_is_libc(wc_borrowed));
}

UTEST(allocator, libc_alloc_realloc_free)
{
    u8* p = wc_alloc(WC_LIBC, 10, 1);
    EXPECT_TRUE((p) != NULL);
    if (!p) {
        return;
    }
    memcpy(p, "abcdefghij", 10);
    p = wc_realloc(WC_LIBC, p, 10, 1000, 1);
    if (!p) {
        EXPECT_TRUE((p) != NULL);
        return;
    }
    EXPECT_TRUE(memcmp(p, "abcdefghij", 10) == 0);
    wc_free(WC_LIBC, p, 1000, 1);
}

UTEST(allocator, libc_over_aligned)
{
    const size_t aligns[] = {32, 64, 256, 4096};
    for (u64 i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++) {
        u8* p = wc_alloc(WC_LIBC, 100, aligns[i]);
        EXPECT_TRUE((p) != NULL);
        if (!p) {
            return;
        }
        EXPECT_EQ((uintptr_t)p % aligns[i], 0u);
        memset(p, 1, 100);
        p = wc_realloc(WC_LIBC, p, 100, 5000, aligns[i]); // realloc keeps over-alignment
        if (!p) {
            EXPECT_TRUE((p) != NULL);
            return;
        }
        EXPECT_EQ((uintptr_t)p % aligns[i], 0u);
        EXPECT_EQ(p[99], 1);
        wc_free(WC_LIBC, p, 5000, aligns[i]);
    }
}


/* ── Wrapper contract (checked by the test allocator in ABORT mode) ──────── */

UTEST(allocator, zero_size_never_reaches_backend)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    EXPECT_TRUE((wc_alloc(a, 0, 8)) == NULL);
    EXPECT_EQ(ta.n_attempts, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(allocator, realloc_null_is_alloc_and_zero_is_free)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u8* p = wc_realloc(a, NULL, 0, 32, 8);
    EXPECT_TRUE((p) != NULL);
    EXPECT_EQ(ta.n_alloc, 1u);

    EXPECT_TRUE((wc_realloc(a, p, 32, 0, 8)) == NULL);
    EXPECT_EQ(ta.n_free, 1u);
    EXPECT_EQ(ta.live_blocks, 0u);

    wc_free(a, NULL, 0, 8); // NULL free: no backend call
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(allocator, small_alignment_passes_through)
{
    // D7: wrappers no longer raise align to WC_MAX_ALIGN
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u8* p = wc_alloc(a, 12, 4);
    wc_free(a, p, 12, 4); // must match exactly: 4, not 16
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(allocator, typed_macros_and_overflow)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u64* v = WC_NEW_N(a, u64, 8);
    EXPECT_TRUE((v) != NULL);
    if (!v) {
        return;
    }
    v[7] = 42;
    v    = WC_REALLOC_N(a, u64, v, 8, 16); // this is the A5 shape, now well-formed
    if (!v) {
        EXPECT_TRUE((v) != NULL);
        return;
    }
    EXPECT_EQ(v[7], 42u);

    // overflowing count fails cleanly and leaves v alive (no wrap to a small size)
    EXPECT_TRUE((WC_REALLOC_N(a, u64, v, 16, SIZE_MAX / 4)) == NULL);
    EXPECT_TRUE(wc_test_alloc_owns(&ta, v));
    EXPECT_TRUE((WC_NEW_N(a, u64, SIZE_MAX / 2)) == NULL);

    WC_DELETE_N(a, u64, v, 16);

    int* one = WC_NEW(a, int);
    WC_DELETE(a, int, one);

    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
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

UTEST(allocator, missing_realloc_is_emulated)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = {.vt = &noreal_vt, .ctx = &ta};

    u8* p = wc_alloc(a, 16, 8);
    memcpy(p, "0123456789abcdef", 16);
    u8* q = wc_realloc(a, p, 16, 64, 8);
    EXPECT_TRUE(memcmp(q, "0123456789abcdef", 16) == 0);
    EXPECT_EQ(ta.n_free, 1u); // old block freed with its exact size
    wc_free(a, q, 64, 8);

    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}


/* ── wc_borrowed ─────────────────────────────────────────────────────────── */

UTEST(allocator, borrowed_never_allocates_or_frees)
{
    u8 buf[32] = {1, 2, 3};
    EXPECT_TRUE((wc_alloc(wc_borrowed, 16, 8)) == NULL);
    EXPECT_TRUE((wc_realloc(wc_borrowed, buf, 32, 64, 8)) == NULL); // growth fails
    EXPECT_EQ(buf[2], 3);                              // buffer untouched
    wc_free(wc_borrowed, buf, 32, 8);                        // no-op (ASAN would catch a real free)
    EXPECT_EQ(buf[0], 1);
}


/* ── Helpers ─────────────────────────────────────────────────────────────── */

UTEST(allocator, align_for_size)
{
    EXPECT_EQ(wc_align_for_size(1), 1u);
    EXPECT_EQ(wc_align_for_size(3), 1u);
    EXPECT_EQ(wc_align_for_size(4), 4u);
    EXPECT_EQ(wc_align_for_size(12), 4u);
    EXPECT_EQ(wc_align_for_size(24), 8u);
    EXPECT_EQ(wc_align_for_size(40), 8u);
    EXPECT_EQ(wc_align_for_size(64), WC_MAX_ALIGN);
    EXPECT_EQ(wc_align_for_size(sizeof(long double)), alignof(long double));
}

UTEST(allocator, same)
{
    int x = 0, y = 0;
    wc_allocator a = {.vt = &noreal_vt, .ctx = &x};
    wc_allocator b = {.vt = &noreal_vt, .ctx = &y};
    EXPECT_TRUE(wc_same(a, a));
    EXPECT_FALSE(wc_same(a, b));
    EXPECT_TRUE(wc_same(WC_LIBC, WC_LIBC));
}


/* ── Suite entry point ───────────────────────────────────────────────────── */
