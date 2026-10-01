#include "arena.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_macros.h"
#include "wc_test.h"
#include "wc_test_allocator.h"

#include <stdint.h>

// These tests hand stack buffers to allocators on purpose (wc_borrowed, foreign
// pointer detection). The pointer never reaches libc, but GCC cannot prove the
// vtable is non-NULL and warns about the libc branch.
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"

void test_allocator_suite(void);


/* ── Self-tests: the checker must catch what it claims to catch ──────────── */

static void test_ta_counts_and_bytes(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u8* p = wc_alloc(a, 100, 16);
    u8* q = wc_alloc(a, 50, 16);
    WC_ASSERT_EQ_U64(ta.n_alloc, 2);
    WC_ASSERT_EQ_U64(ta.live_blocks, 2);
    WC_ASSERT_EQ_U64(ta.live_bytes, 150);
    WC_ASSERT(wc_test_alloc_owns(&ta, p));

    p = wc_realloc(a, p, 100, 300, 16);
    WC_ASSERT_EQ_U64(ta.n_realloc, 1);
    WC_ASSERT_EQ_U64(ta.live_bytes, 350);
    WC_ASSERT_EQ_U64(ta.peak_bytes, 350);

    wc_free(a, p, 300, 16);
    wc_free(a, q, 50, 16);
    WC_ASSERT_EQ_U64(ta.n_free, 2);
    WC_ASSERT_EQ_U64(ta.live_bytes, 0);
    WC_ASSERT_EQ_U64(ta.peak_bytes, 350);
    WC_ASSERT_EQ_U64(ta.n_errors, 0);
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_ta_fill_and_realloc_preserves(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u8* p = wc_alloc(a, 8, 16);
    WC_ASSERT_EQ_INT(p[0], WC_TA_FILL);
    WC_ASSERT_EQ_INT(p[7], WC_TA_FILL);

    memcpy(p, "abcdefgh", 8);
    u8* q = wc_realloc(a, p, 8, 16, 16);
    WC_ASSERT(q != p); // always moves, so stale-pointer bugs surface
    WC_ASSERT(memcmp(q, "abcdefgh", 8) == 0);
    WC_ASSERT_EQ_INT(q[15], WC_TA_FILL);
    WC_ASSERT_FALSE(wc_test_alloc_owns(&ta, p));

    wc_free(a, q, 16, 16);
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_ta_detects_leak(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    (void)wc_alloc(a, 32, 16);
    (void)wc_alloc(a, 64, 16);
    fprintf(stderr, "    (expected leak report follows)\n");
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 2); // also releases them
}

static void test_ta_detects_double_free(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    ta.mode          = WC_TA_RECORD;
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u8* p = wc_alloc(a, 32, 16);
    wc_free(a, p, 32, 16);
    wc_free(a, p, 32, 16); // second free: must be caught, must not reach libc
    WC_ASSERT_EQ_U64(ta.n_errors, 1);
    WC_ASSERT_EQ_U64(ta.n_free, 1);
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_ta_detects_foreign_pointer(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    ta.mode          = WC_TA_RECORD;
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u8 stack_buf[32];
    wc_free(a, stack_buf, 32, 16);
    WC_ASSERT_EQ_U64(ta.n_errors, 1);

    WC_ASSERT_NULL(wc_realloc(a, stack_buf, 32, 64, 16));
    WC_ASSERT_EQ_U64(ta.n_errors, 2);
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_ta_detects_size_and_align_mismatch(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    ta.mode          = WC_TA_RECORD;
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u8* p = wc_alloc(a, 40, 16);

    wc_free(a, p, 48, 16); // wrong size
    WC_ASSERT_EQ_U64(ta.n_errors, 1);
    WC_ASSERT(wc_test_alloc_owns(&ta, p)); // rejected free leaves the block live

    wc_free(a, p, 40, 32); // wrong align
    WC_ASSERT_EQ_U64(ta.n_errors, 2);

    WC_ASSERT_NULL(wc_realloc(a, p, 41, 80, 16)); // wrong old size
    WC_ASSERT_EQ_U64(ta.n_errors, 3);
    WC_ASSERT(wc_test_alloc_owns(&ta, p));

    wc_free(a, p, 40, 16);
    WC_ASSERT_EQ_U64(ta.n_errors, 3);
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_ta_detects_zero_size(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    ta.mode = WC_TA_RECORD;

    // The wc_alloc wrapper never forwards size 0 (see allocator_core_test);
    // call the backend directly to check the checker still flags it.
    WC_ASSERT_NULL(wc_test_alloc_cb_alloc(&ta, 0, 16));
    WC_ASSERT_EQ_U64(ta.n_errors, 1);
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_ta_fail_nth(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);
    wc_test_alloc_fail_at(&ta, 2, 0);

    u8* p1 = wc_alloc(a, 8, 16);
    u8* p2 = wc_alloc(a, 8, 16); // 2nd attempt fails
    u8* p3 = wc_realloc(a, p1, 8, 16, 16);
    WC_ASSERT_NOT_NULL(p1);
    WC_ASSERT_NULL(p2);
    WC_ASSERT_NOT_NULL(p3);

    wc_test_alloc_fail_at(&ta, 4, 1); // sticky from the 4th attempt on
    WC_ASSERT_NULL(wc_realloc(a, p3, 16, 32, 16));
    WC_ASSERT(wc_test_alloc_owns(&ta, p3)); // failed realloc leaves p valid
    WC_ASSERT_NULL(wc_alloc(a, 8, 16));

    wc_free(a, p3, 16, 16);
    WC_ASSERT_EQ_U64(ta.n_errors, 0);
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_ta_table_survives_churn(void)
{
    // many live blocks + frees: exercises table growth and tombstone reuse
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    enum { N = 2000 };
    static u8* ptrs[N];
    for (int round = 0; round < 3; round++) {
        for (u64 i = 0; i < N; i++) {
            ptrs[i] = wc_alloc(a, 1 + (i % 97), 16);
        }
        for (u64 i = 0; i < N; i += 2) {
            wc_free(a, ptrs[i], 1 + (i % 97), 16);
        }
        for (u64 i = 1; i < N; i += 2) {
            wc_free(a, ptrs[i], 1 + (i % 97), 16);
        }
    }
    WC_ASSERT_EQ_U64(ta.live_blocks, 0);
    WC_ASSERT_EQ_U64(ta.n_errors, 0);
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_ta_over_arena_backing(void)
{
    // backing is an arena: checks contract on top of a non-libc backend
    Arena arena;
    Arena_create(&arena, WC_LIBC, nKB(4));

    wc_test_alloc ta;
    wc_test_alloc_init(&ta, Arena_allocator(&arena));
    wc_allocator a = wc_test_alloc_allocator(&ta);

    u8* p = wc_alloc(a, 24, 16);
    WC_ASSERT_NOT_NULL(p);
    WC_ASSERT(p >= arena.base && p < arena.base + arena.size);
    wc_free(a, p, 24, 16);

    WC_ASSERT_EQ_U64(ta.n_errors, 0);
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
    Arena_destroy(&arena);
}


/* ── GenVec through the test allocator (explicit allocator, no global) ──── */

#define WITH_TEST_ALLOC(ta_name, al_name, body)               \
    do {                                                      \
        wc_test_alloc ta_name;                                \
        wc_test_alloc_init(&ta_name, WC_LIBC);                \
        wc_allocator al_name = wc_test_alloc_allocator(&ta_name); \
        body;                                                 \
        WC_ASSERT_EQ_U64(ta_name.n_errors, 0);                \
        WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta_name), 0); \
    } while (0)

static void test_genvec_lifecycle_is_leak_free(void)
{
    WITH_TEST_ALLOC(ta, al, {
        GenVec v = VEC_OF_IN(al, int, 4);
        for (int i = 0; i < 1000; i++) {
            VEC_PUSH(&v, i);
        }
        GenVec_reserve(&v, 5000);
        for (int i = 0; i < 900; i++) {
            GenVec_pop(&v, NULL);
        }
        GenVec_shrink_to_fit(&v);
        WC_ASSERT_EQ_U64(GenVec_size(&v), 100);
        WC_ASSERT_EQ_INT(*(const int*)GenVec_get_ptr(&v, 99), 99);
        GenVec_destroy(&v);
        WC_ASSERT(ta.n_realloc > 0);
    });
}

static void test_genvec_copy_move_reset_are_leak_free(void)
{
    WITH_TEST_ALLOC(ta, al, {
        GenVec a = VEC_OF_IN(al, u64, 2);
        for (u64 i = 0; i < 50; i++) {
            VEC_PUSH(&a, i);
        }

        GenVec b = GenVec_copy(al, &a);
        WC_ASSERT_EQ_U64(GenVec_size(&b), 50);

        GenVec c;
        GenVec_move(&c, &a); // a is zeroed
        WC_ASSERT_NULL(a.data);

        GenVec_reset(&c);
        WC_ASSERT_EQ_U64(GenVec_capacity(&c), 0);
        u64 x = 7;
        GenVec_push(&c, cast(x)); // grows from 0 after reset, same allocator

        GenVec_destroy(&a); // zero-safe
        GenVec_destroy(&b);
        GenVec_destroy(&c);
    });
}

static void test_genvec_nested_is_leak_free(void)
{
    WITH_TEST_ALLOC(ta, al, {
        GenVec outer = VEC_OF_IN(al, GenVec, 2);
        for (int i = 0; i < 10; i++) {
            GenVec inner = GenVec_create(al, 1, sizeof(int), NULL);
            for (int j = 0; j < i * 7; j++) {
                GenVec_push(&inner, cast(j));
            }
            GenVec_push(&outer, cast(inner)); // deep copy via wc_vec_ops into outer's allocator
            GenVec_destroy(&inner);
        }
        GenVec copy = GenVec_copy(al, &outer);
        GenVec_destroy(&outer);
        GenVec_destroy(&copy);
    });
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void test_allocator_suite(void)
{
    WC_SUITE("wc_test_allocator");

    WC_RUN(test_ta_counts_and_bytes);
    WC_RUN(test_ta_fill_and_realloc_preserves);
    WC_RUN(test_ta_detects_leak);
    WC_RUN(test_ta_detects_double_free);
    WC_RUN(test_ta_detects_foreign_pointer);
    WC_RUN(test_ta_detects_size_and_align_mismatch);
    WC_RUN(test_ta_detects_zero_size);
    WC_RUN(test_ta_fail_nth);
    WC_RUN(test_ta_table_survives_churn);
    WC_RUN(test_ta_over_arena_backing);

    WC_SUITE("GenVec under wc_test_allocator");

    WC_RUN(test_genvec_lifecycle_is_leak_free);
    WC_RUN(test_genvec_copy_move_reset_are_leak_free);
    WC_RUN(test_genvec_nested_is_leak_free);
}
