#include "common.h"
#include "wc_test.h"
#include "Arena.h"
#include "wc_errno.h"


/* ── Basic alloc ─────────────────────────────────────────────────────────── */

static void test_create_default_size(void)
{
    Arena* a = Arena_create(0);
    WC_ASSERT_NOT_NULL(a);
    WC_ASSERT_NOT_NULL(a->base);
    WC_ASSERT_EQ_U64(a->idx, 0);
    WC_ASSERT_EQ_U64(a->size, ARENA_DEFAULT_SIZE);
    Arena_destroy(a);
}

static void test_create_custom_size(void)
{
    Arena* a = Arena_create(nKB(8));
    WC_ASSERT_EQ_U64(a->size, nKB(8));
    Arena_destroy(a);
}

static void test_alloc_returns_valid_ptr(void)
{
    Arena* a   = Arena_create(nKB(4));
    u8*    ptr = Arena_alloc(a, 64);
    WC_ASSERT_NOT_NULL(ptr);
    /* must be inside the Arena's region */
    WC_ASSERT_TRUE(ptr >= a->base);
    WC_ASSERT_TRUE(ptr < a->base + a->size);
    Arena_destroy(a);
}

static void test_alloc_advances_idx(void)
{
    Arena* a = Arena_create(nKB(4));
    Arena_alloc(a, 16);
    /* idx must have advanced by at least 16 (may be more due to alignment) */
    WC_ASSERT_TRUE(a->idx >= 16);
    Arena_destroy(a);
}

static void test_alloc_sequential_no_overlap(void)
{
    Arena* a  = Arena_create(nKB(4));
    int*   p1 = (int*)Arena_alloc(a, sizeof(int));
    int*   p2 = (int*)Arena_alloc(a, sizeof(int));
    WC_ASSERT_NOT_NULL(p1);
    WC_ASSERT_NOT_NULL(p2);
    *p1 = 111;
    *p2 = 222;
    /* writing to p2 must not corrupt p1 */
    WC_ASSERT_EQ_INT(*p1, 111);
    WC_ASSERT_EQ_INT(*p2, 222);
    Arena_destroy(a);
}


/* ── Alignment ───────────────────────────────────────────────────────────── */

static void test_alloc_aligned(void)
{
    Arena* a   = Arena_create(nKB(4));
    u8*    ptr = Arena_alloc_aligned(a, sizeof(double), sizeof(double));
    WC_ASSERT_NOT_NULL(ptr);
    /* must be aligned to sizeof(double) = 8 */
    WC_ASSERT_EQ_U64((u64)ptr % sizeof(double), 0);
    Arena_destroy(a);
}

static void test_default_alloc_8byte_aligned(void)
{
    Arena* a   = Arena_create(nKB(4));
    /* burn 1 byte to force misalignment */
    Arena_alloc(a, 1);
    u8* ptr = Arena_alloc(a, 8);
    WC_ASSERT_NOT_NULL(ptr);
    WC_ASSERT_EQ_U64((u64)ptr % ARENA_DEFAULT_ALIGNMENT, 0);
    Arena_destroy(a);
}


/* ── Full Arena ──────────────────────────────────────────────────────────── */

static void test_alloc_full_returns_null(void)
{
    Arena* a = Arena_create(32);
    /* exhaust the Arena */
    Arena_alloc(a, 32);
    wc_errno  = WC_OK;
    u8* ptr   = Arena_alloc(a, 1);
    WC_ASSERT_NULL(ptr);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_FULL);
    Arena_destroy(a);
}


/* ── Scratch ─────────────────────────────────────────────────────────────── */

static void test_scratch_begin_end(void)
{
    Arena*       a     = Arena_create(nKB(4));
    u64          before = a->idx;
    ArenaScratch sc   = Arena_scratch_begin(a);
    Arena_alloc(a, 256);
    WC_ASSERT_TRUE(a->idx > before);
    Arena_scratch_end(sc);
    WC_ASSERT_EQ_U64(a->idx, before);
    Arena_destroy(a);
}

static void test_scratch_macro(void)
{
    Arena* a      = Arena_create(nKB(4));
    u64    before = a->idx;

    ARENA_SCRATCH(a) {
        Arena_alloc(a, 512);
        WC_ASSERT_TRUE(a->idx > before);
    }

    WC_ASSERT_EQ_U64(a->idx, before);
    Arena_destroy(a);
}

static void test_scratch_outer_alloc_survives(void)
{
    Arena* a = Arena_create(nKB(4));

    int* permanent = (int*)Arena_alloc(a, sizeof(int));
    *permanent     = 77;

    ARENA_SCRATCH(a) {
        int* tmp = (int*)Arena_alloc(a, sizeof(int));
        *tmp     = 999;
        (void)tmp;
    }

    /* permanent allocation must still hold its value */
    WC_ASSERT_EQ_INT(*permanent, 77);
    Arena_destroy(a);
}


/* ── Stack-based Arena ───────────────────────────────────────────────────── */

static void test_stk_Arena(void)
{
    u8    buf[256];
    Arena a;
    Arena_create_arr_stk(buf, 256, &a);
    WC_ASSERT_EQ_U64(a.size, 256);
    WC_ASSERT_EQ_U64(a.idx,  0);

    int* p = (int*)Arena_alloc(&a, sizeof(int));
    WC_ASSERT_NOT_NULL(p);
    *p = 55;
    WC_ASSERT_EQ_INT(*p, 55);
    /* no release needed — buf is on the Stack */
}

static void test_used_remaining(void)
{
    Arena* a = Arena_create(nKB(1));
    WC_ASSERT_EQ_U64(Arena_used(a), 0);
    WC_ASSERT_EQ_U64(Arena_remaining(a), nKB(1));

    Arena_alloc(a, 128);
    WC_ASSERT_TRUE(Arena_used(a) >= 128);
    WC_ASSERT_TRUE(Arena_remaining(a) <= nKB(1) - 128);

    Arena_destroy(a);
}

static void test_alloc_full_sets_errno(void)
{
    Arena* a = Arena_create(64);
    /* exhaust the Arena */
    Arena_alloc(a, 64);
    /* next alloc must fail with WC_ERR_FULL */
    wc_errno = WC_OK;
    u8* p = Arena_alloc(a, 1);
    WC_ASSERT_NULL(p);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_FULL);
    Arena_destroy(a);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void Arena_suite(void)
{
    WC_SUITE("Arena");

    /* basic alloc */
    WC_RUN(test_create_default_size);
    WC_RUN(test_create_custom_size);
    WC_RUN(test_alloc_returns_valid_ptr);
    WC_RUN(test_alloc_advances_idx);
    WC_RUN(test_alloc_sequential_no_overlap);

    /* alignment */
    WC_RUN(test_alloc_aligned);
    WC_RUN(test_default_alloc_8byte_aligned);

    /* full Arena */
    WC_RUN(test_alloc_full_returns_null);
    WC_RUN(test_alloc_full_sets_errno);

    /* scratch */
    WC_RUN(test_scratch_begin_end);
    WC_RUN(test_scratch_macro);
    WC_RUN(test_scratch_outer_alloc_survives);

    /* Stack-based */
    WC_RUN(test_stk_Arena);
    WC_RUN(test_used_remaining);
}
