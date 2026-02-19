#include "wc_test.h"
#include "arena.h"
#include "wc_errno.h"


/* ── Basic alloc ─────────────────────────────────────────────────────────── */

static void test_create_default_size(void)
{
    Arena* a = arena_create(0);
    WC_ASSERT_NOT_NULL(a);
    WC_ASSERT_NOT_NULL(a->base);
    WC_ASSERT_EQ_U64(a->idx, 0);
    WC_ASSERT_EQ_U64(a->size, ARENA_DEFAULT_SIZE);
    arena_release(a);
}

static void test_create_custom_size(void)
{
    Arena* a = arena_create(nKB(8));
    WC_ASSERT_EQ_U64(a->size, nKB(8));
    arena_release(a);
}

static void test_alloc_returns_valid_ptr(void)
{
    Arena* a   = arena_create(nKB(4));
    u8*    ptr = arena_alloc(a, 64);
    WC_ASSERT_NOT_NULL(ptr);
    /* must be inside the arena's region */
    WC_ASSERT_TRUE(ptr >= a->base);
    WC_ASSERT_TRUE(ptr < a->base + a->size);
    arena_release(a);
}

static void test_alloc_advances_idx(void)
{
    Arena* a = arena_create(nKB(4));
    arena_alloc(a, 16);
    /* idx must have advanced by at least 16 (may be more due to alignment) */
    WC_ASSERT_TRUE(a->idx >= 16);
    arena_release(a);
}

static void test_alloc_sequential_no_overlap(void)
{
    Arena* a  = arena_create(nKB(4));
    int*   p1 = (int*)arena_alloc(a, sizeof(int));
    int*   p2 = (int*)arena_alloc(a, sizeof(int));
    WC_ASSERT_NOT_NULL(p1);
    WC_ASSERT_NOT_NULL(p2);
    *p1 = 111;
    *p2 = 222;
    /* writing to p2 must not corrupt p1 */
    WC_ASSERT_EQ_INT(*p1, 111);
    WC_ASSERT_EQ_INT(*p2, 222);
    arena_release(a);
}


/* ── Alignment ───────────────────────────────────────────────────────────── */

static void test_alloc_aligned(void)
{
    Arena* a   = arena_create(nKB(4));
    u8*    ptr = arena_alloc_aligned(a, sizeof(double), sizeof(double));
    WC_ASSERT_NOT_NULL(ptr);
    /* must be aligned to sizeof(double) = 8 */
    WC_ASSERT_EQ_U64((u64)ptr % sizeof(double), 0);
    arena_release(a);
}

static void test_default_alloc_8byte_aligned(void)
{
    Arena* a   = arena_create(nKB(4));
    /* burn 1 byte to force misalignment */
    arena_alloc(a, 1);
    u8* ptr = arena_alloc(a, 8);
    WC_ASSERT_NOT_NULL(ptr);
    WC_ASSERT_EQ_U64((u64)ptr % ARENA_DEFAULT_ALIGNMENT, 0);
    arena_release(a);
}


/* ── Full arena ──────────────────────────────────────────────────────────── */

static void test_alloc_full_returns_null(void)
{
    Arena* a = arena_create(32);
    /* exhaust the arena */
    arena_alloc(a, 32);
    wc_errno  = WC_OK;
    u8* ptr   = arena_alloc(a, 1);
    WC_ASSERT_NULL(ptr);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_FULL);
    arena_release(a);
}


/* ── Marks ───────────────────────────────────────────────────────────────── */

static void test_mark_restore(void)
{
    Arena* a    = arena_create(nKB(4));
    u64    mark = arena_get_mark(a);
    WC_ASSERT_EQ_U64(mark, 0);

    arena_alloc(a, 128);
    WC_ASSERT_TRUE(a->idx > 0);

    arena_clear_mark(a, mark);
    WC_ASSERT_EQ_U64(a->idx, 0);
    arena_release(a);
}

static void test_mark_partial_restore(void)
{
    Arena* a = arena_create(nKB(4));
    arena_alloc(a, 64);
    u64 mark = arena_get_mark(a);

    arena_alloc(a, 128);
    u64 idx_after = a->idx;
    WC_ASSERT_TRUE(idx_after > mark);

    arena_clear_mark(a, mark);
    WC_ASSERT_EQ_U64(a->idx, mark);
    arena_release(a);
}

static void test_mark_reuse(void)
{
    /* After clearing to mark, the same memory is reusable */
    Arena* a    = arena_create(nKB(4));
    u64    mark = arena_get_mark(a);

    int* p1 = (int*)arena_alloc(a, sizeof(int));
    *p1     = 42;
    arena_clear_mark(a, mark);

    int* p2 = (int*)arena_alloc(a, sizeof(int));
    *p2     = 99;
    /* p1 and p2 should point to the same location */
    WC_ASSERT_TRUE(p1 == p2);
    WC_ASSERT_EQ_INT(*p2, 99);
    arena_release(a);
}


/* ── Scratch ─────────────────────────────────────────────────────────────── */

static void test_scratch_begin_end(void)
{
    Arena*       a     = arena_create(nKB(4));
    u64          before = a->idx;
    arena_scratch sc   = arena_scratch_begin(a);
    arena_alloc(a, 256);
    WC_ASSERT_TRUE(a->idx > before);
    arena_scratch_end(&sc);
    WC_ASSERT_EQ_U64(a->idx, before);
    arena_release(a);
}

static void test_scratch_macro(void)
{
    Arena* a      = arena_create(nKB(4));
    u64    before = a->idx;

    ARENA_SCRATCH(sc, a) {
        arena_alloc(a, 512);
        WC_ASSERT_TRUE(a->idx > before);
    }

    WC_ASSERT_EQ_U64(a->idx, before);
    arena_release(a);
}

static void test_scratch_outer_alloc_survives(void)
{
    Arena* a = arena_create(nKB(4));

    int* permanent = (int*)arena_alloc(a, sizeof(int));
    *permanent     = 77;

    ARENA_SCRATCH(sc, a) {
        int* tmp = (int*)arena_alloc(a, sizeof(int));
        *tmp     = 999;
        (void)tmp;
    }

    /* permanent allocation must still hold its value */
    WC_ASSERT_EQ_INT(*permanent, 77);
    arena_release(a);
}


/* ── Stack-based arena ───────────────────────────────────────────────────── */

static void test_stk_arena(void)
{
    u8    buf[256];
    Arena a;
    arena_create_arr_stk(&a, buf, 256);
    WC_ASSERT_EQ_U64(a.size, 256);
    WC_ASSERT_EQ_U64(a.idx,  0);

    int* p = (int*)arena_alloc(&a, sizeof(int));
    WC_ASSERT_NOT_NULL(p);
    *p = 55;
    WC_ASSERT_EQ_INT(*p, 55);
    /* no release needed — buf is on the stack */
}

static void test_used_remaining(void)
{
    Arena* a = arena_create(nKB(1));
    WC_ASSERT_EQ_U64(arena_used(a), 0);
    WC_ASSERT_EQ_U64(arena_remaining(a), nKB(1));

    arena_alloc(a, 128);
    WC_ASSERT_TRUE(arena_used(a) >= 128);
    WC_ASSERT_TRUE(arena_remaining(a) <= nKB(1) - 128);

    arena_release(a);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void arena_suite(void)
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

    /* full arena */
    WC_RUN(test_alloc_full_returns_null);

    /* marks */
    WC_RUN(test_mark_restore);
    WC_RUN(test_mark_partial_restore);
    WC_RUN(test_mark_reuse);

    /* scratch */
    WC_RUN(test_scratch_begin_end);
    WC_RUN(test_scratch_macro);
    WC_RUN(test_scratch_outer_alloc_survives);

    /* stack-based */
    WC_RUN(test_stk_arena);
    WC_RUN(test_used_remaining);
}
