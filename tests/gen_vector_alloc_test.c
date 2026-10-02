#include "arena.h"
#include "chain_arena.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_macros.h"
#include "wc_test.h"
#include "wc_test_allocator.h"
#include "wc_test_fatal.h"

#include <stdalign.h>
#include <stdint.h>

void gen_vector_alloc_suite(void);


/* ── One workload, every backend ─────────────────────────────────────────── */

// push / grow / insert / remove / shrink, checked element by element
static void workload(GenVec* v)
{
    for (int i = 0; i < 300; i++) {
        GenVec_push(v, cast(i));
    }
    int x = -1;
    GenVec_insert(v, 0, cast(x));
    GenVec_insert(v, 150, cast(x));
    GenVec_remove(v, 150, NULL);
    GenVec_remove(v, 0, NULL);
    GenVec_remove_range(v, 100, 50); // drop 100..149
    GenVec_shrink_to_fit(v);

    WC_EXPECT_EQ_U64(GenVec_size(v), 250);
    for (u64 i = 0; i < 250; i++) {
        int want = (int)(i < 100 ? i : i + 50);
        WC_EXPECT_EQ_INT(*(const int*)GenVec_get_ptr(v, i), want);
    }
}

static void test_workload_libc(void)
{
    GenVec v = GenVec_create(WC_LIBC, 2, sizeof(int), NULL);
    workload(&v);
    GenVec_destroy(&v);
}

static void test_workload_test_allocator(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    GenVec v = GenVec_create(wc_test_alloc_allocator(&ta), 2, sizeof(int), NULL);
    workload(&v);
    WC_EXPECT_EQ_U64(ta.live_blocks, 1);
    GenVec_destroy(&v);
    WC_EXPECT_EQ_U64(ta.n_errors, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_workload_arena(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(8));
    GenVec v = GenVec_create(Arena_allocator(&a), 2, sizeof(int), NULL);
    workload(&v);
    WC_EXPECT(v.data >= a.base && v.data < a.base + a.size);
    GenVec_destroy(&v);
    WC_EXPECT_EQ_U64(Arena_used(&a), 0); // sole block: every growth was in place, destroy rewound it
    Arena_destroy(&a);
}

static void test_workload_arena_over_stack_buffer(void)
{
    alignas(16) u8 buf[4096];
    Arena          a;
    Arena_create_buf(&a, buf, sizeof(buf));
    GenVec v = GenVec_create(Arena_allocator(&a), 2, sizeof(int), NULL);
    workload(&v);
    GenVec_destroy(&v);
    Arena_destroy(&a); // frees nothing: the stack buffer is not ours
}

static void test_workload_chain_arena(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    ChainArena ca;
    ChainArena_create(&ca, wc_test_alloc_allocator(&ta));

    GenVec v = GenVec_create(ChainArena_allocator(&ca), 2, sizeof(int), NULL);
    workload(&v);
    for (int i = 0; i < 3000; i++) { // past one node: was fatal before A3
        GenVec_push(&v, cast(i));
    }
    GenVec_destroy(&v);

    ChainArena_destroy(&ca);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}


/* ── Borrowed buffer (GenVec_create_buf / wc_borrowed) ───────────────────── */

static void test_buf_vector_fills_buffer_in_place(void)
{
    int    buf[8];
    GenVec v = GenVec_create_buf((u8*)buf, 8, sizeof(int), NULL);
    for (int i = 0; i < 8; i++) {
        GenVec_push(&v, cast(i));
    }
    WC_EXPECT(v.data == (u8*)buf);
    WC_EXPECT_EQ_INT(buf[7], 7);
    WC_EXPECT(wc_same(v.alloc, wc_borrowed));
    GenVec_destroy(&v); // must not free the stack buffer (ASAN would catch it)
}

static void push_past_borrowed_capacity(void)
{
    int    buf[2];
    GenVec v = GenVec_create_buf((u8*)buf, 2, sizeof(int), NULL);
    for (int i = 0; i < 3; i++) {
        GenVec_push(&v, cast(i)); // third push must die: a borrowed buffer cannot grow
    }
}

static void test_buf_vector_cannot_grow(void)
{
    WC_EXPECT_DIES(push_past_borrowed_capacity);
}


/* ── Copy across allocators (A7) ─────────────────────────────────────────── */

static void test_copy_arena_to_libc(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    GenVec src = GenVec_create(Arena_allocator(&a), 4, sizeof(int), NULL);
    for (int i = 0; i < 20; i++) {
        GenVec_push(&src, cast(i));
    }

    GenVec dst = GenVec_copy(WC_LIBC, &src);
    WC_EXPECT(wc_is_libc(dst.alloc)); // never inherits src's allocator
    WC_EXPECT(dst.data < a.base || dst.data >= a.base + a.size);

    Arena_destroy(&a); // the arena copy is gone; the libc copy must be intact
    for (int i = 0; i < 20; i++) {
        WC_EXPECT_EQ_INT(*(const int*)GenVec_get_ptr(&dst, (u64)i), i);
    }
    GenVec_destroy(&dst);
}

// every child of a deep copy must belong to the destination allocator
static void test_nested_copy_children_follow_destination(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(16));
    wc_allocator al = Arena_allocator(&a);

    GenVec outer = VEC_OF_IN(al, GenVec, 2);
    for (int r = 0; r < 6; r++) {
        GenVec inner = GenVec_create(al, 1, sizeof(int), NULL);
        for (int c = 0; c <= r; c++) {
            GenVec_push(&inner, cast(c));
        }
        VEC_PUSH_MOVE(&outer, inner); // inner zeroed
        WC_EXPECT_NULL(inner.data);
    }

    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator dst = wc_test_alloc_allocator(&ta);

    GenVec copy = GenVec_copy(dst, &outer);
    WC_EXPECT(wc_test_alloc_owns(&ta, copy.data));
    for (u64 r = 0; r < 6; r++) {
        const GenVec* in = (const GenVec*)GenVec_get_ptr(&copy, r);
        WC_EXPECT(wc_same(in->alloc, dst));
        WC_EXPECT(wc_test_alloc_owns(&ta, in->data));
        WC_EXPECT_EQ_U64(GenVec_size(in), r + 1);
    }

    GenVec_destroy(&outer);
    Arena_destroy(&a);
    GenVec_destroy(&copy);
    WC_EXPECT_EQ_U64(ta.n_errors, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

// by-pointer children: the shell and the data come from the same allocator (D6)
static void test_boxed_children_free_with_their_own_allocator(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator al = wc_test_alloc_allocator(&ta);

    GenVec outer = VEC_OF(GenVec*, 2); // outer on libc, children on the test allocator
    for (int i = 0; i < 5; i++) {
        GenVec* child = WC_BOX_IN(al, GenVec, GenVec_create, 4, sizeof(int), NULL);
        GenVec_push(child, cast(i));
        WC_EXPECT(wc_test_alloc_owns(&ta, child));
        VEC_PUSH_MOVE(&outer, child);
        WC_EXPECT_NULL(child);
    }

    GenVec copy = GenVec_copy(al, &outer); // children deep-copied into `al`, shells too
    WC_EXPECT(wc_test_alloc_owns(&ta, *(GenVec* const*)GenVec_get_ptr(&copy, 0)));

    GenVec_destroy(&outer); // vec_del_ptr: shell freed with each child's own allocator
    GenVec_destroy(&copy);
    WC_EXPECT_EQ_U64(ta.n_errors, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_subarr_into_other_allocator(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    GenVec src = GenVec_create(Arena_allocator(&a), 8, sizeof(int), NULL);
    for (int i = 0; i < 8; i++) {
        GenVec_push(&src, cast(i));
    }
    GenVec sub = GenVec_subarr(&src, WC_LIBC, 2, 3);
    Arena_destroy(&a);
    WC_EXPECT_EQ_U64(GenVec_size(&sub), 3);
    WC_EXPECT_EQ_INT(*(const int*)GenVec_get_ptr(&sub, 0), 2);
    GenVec_destroy(&sub);
}

static void test_vec_like_uses_same_allocator(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    GenVec v = VEC_OF_IN(Arena_allocator(&a), int, 4);
    GenVec w = VEC_LIKE(&v, u64, 4);
    WC_EXPECT(wc_same(w.alloc, v.alloc));
    WC_EXPECT(w.data >= a.base && w.data < a.base + a.size);
    GenVec_destroy(&w);
    GenVec_destroy(&v);
    Arena_destroy(&a);
}


/* ── Alignment (D7) ──────────────────────────────────────────────────────── */

typedef struct {
    alignas(16) u8 b[16];
} Aligned16;

static void test_storage_alignment_is_size_derived(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator al = wc_test_alloc_allocator(&ta);

    GenVec v1  = GenVec_create(al, 3, 1, NULL);                 // align 1
    GenVec v4  = GenVec_create(al, 3, sizeof(int), NULL);       // align 4
    GenVec v12 = GenVec_create(al, 3, 12, NULL);                // align 4
    GenVec v8  = GenVec_create(al, 3, sizeof(u64), NULL);       // align 8
    GenVec v16 = GenVec_create(al, 3, sizeof(Aligned16), NULL); // align 16

    // wc_test_alloc records the align of every block; destroy must pass it back
    // exactly, and the backing honours it as an address alignment.
    WC_EXPECT_EQ_U64((uintptr_t)v16.data % 16, 0);
    GenVec_destroy(&v1);
    GenVec_destroy(&v4);
    GenVec_destroy(&v12);
    GenVec_destroy(&v8);
    GenVec_destroy(&v16);
    WC_EXPECT_EQ_U64(ta.n_errors, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_arena_packs_small_elements(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 256);
    wc_allocator al = Arena_allocator(&a);
    GenVec       b1 = GenVec_create(al, 3, 1, NULL); // 3 bytes
    GenVec       b2 = GenVec_create(al, 5, 1, NULL); // starts right after: no 16-byte padding
    WC_EXPECT(b2.data == b1.data + 3);
    GenVec_destroy(&b2);
    GenVec_destroy(&b1);
    Arena_destroy(&a);
}


/* ── Zero state (D4) ─────────────────────────────────────────────────────── */

static void push_after_move(void)
{
    GenVec a = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    GenVec b;
    GenVec_move(&b, &a);
    int x = 1;
    GenVec_push(&a, cast(x)); // a is zeroed: must die, not allocate from libc
}

static void push_after_destroy(void)
{
    GenVec a = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    GenVec_destroy(&a);
    int x = 1;
    GenVec_push(&a, cast(x));
}

static void reserve_on_zeroed(void)
{
    GenVec z;
    memset(&z, 0, sizeof(z));
    GenVec_reserve(&z, 10);
}

static void create_with_zero_data_size(void)
{
    GenVec v = GenVec_create(WC_LIBC, 4, 0, NULL);
    (void)v;
}

static void test_zero_state_mutation_dies(void)
{
    WC_EXPECT_DIES(push_after_move);
    WC_EXPECT_DIES(push_after_destroy);
    WC_EXPECT_DIES(reserve_on_zeroed);
    WC_EXPECT_DIES(create_with_zero_data_size);
}

static void test_zero_state_reads_and_destroy_are_safe(void)
{
    GenVec z;
    memset(&z, 0, sizeof(z));
    WC_EXPECT_EQ_U64(GenVec_size(&z), 0);
    WC_EXPECT_TRUE(GenVec_empty(&z));
    GenVec_clear(&z);
    GenVec_destroy(&z);
    GenVec_destroy(&z);

    GenVec c = GenVec_copy(WC_LIBC, &z); // copy of zeroed is zeroed
    WC_EXPECT_EQ_U64(c.data_size, 0);
    GenVec_destroy(&c);
}


/* ── Allocation failure (D5): every allocation site dies cleanly ─────────── */

static u64 g_fail_at;

// Touches every GenVec allocation site: create, grow, reserve, shrink, copy,
// subarr, nested element copy.
static u64 alloc_site_scenario(wc_test_alloc* ta)
{
    wc_allocator al = wc_test_alloc_allocator(ta);

    GenVec v = GenVec_create(al, 2, sizeof(int), NULL); // create
    for (int i = 0; i < 20; i++) {
        GenVec_push(&v, cast(i)); // grow
    }
    GenVec_reserve(&v, 100);                   // reserve
    GenVec_shrink_to_fit(&v);                  // shrink
    GenVec c = GenVec_copy(al, &v);            // copy
    GenVec s = GenVec_subarr(&v, al, 0, 5);    // subarr

    GenVec outer = VEC_OF_IN(al, GenVec, 1);
    GenVec_push(&outer, cast(v));              // outer grow + nested element copy

    u64 attempts = ta->n_attempts;
    GenVec_destroy(&outer);
    GenVec_destroy(&s);
    GenVec_destroy(&c);
    GenVec_destroy(&v);
    return attempts;
}

static void run_scenario_failing_at_n(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_test_alloc_fail_at(&ta, g_fail_at, 0);
    (void)alloc_site_scenario(&ta); // must FATAL at attempt g_fail_at
    _exit(0);                       // reached only if the failure was ignored
}

static void test_fail_nth_allocation_dies_at_every_site(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    u64 total = alloc_site_scenario(&ta);
    WC_EXPECT_EQ_U64(ta.n_errors, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
    WC_EXPECT(total >= 8);

    for (g_fail_at = 1; g_fail_at <= total; g_fail_at++) {
        int died = wc_test_dies(run_scenario_failing_at_n);
        if (died != 1) {
            printf("\n    allocation %llu of %llu did not abort cleanly ", (unsigned long long)g_fail_at,
                   (unsigned long long)total);
        }
        WC_EXPECT_EQ_INT(died, 1);
    }
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void gen_vector_alloc_suite(void)
{
    WC_SUITE("GenVec allocators (Phase 2)");

    WC_RUN(test_workload_libc);
    WC_RUN(test_workload_test_allocator);
    WC_RUN(test_workload_arena);
    WC_RUN(test_workload_arena_over_stack_buffer);
    WC_RUN(test_workload_chain_arena);

    WC_RUN(test_buf_vector_fills_buffer_in_place);
    WC_RUN(test_buf_vector_cannot_grow);

    WC_RUN(test_copy_arena_to_libc);
    WC_RUN(test_nested_copy_children_follow_destination);
    WC_RUN(test_boxed_children_free_with_their_own_allocator);
    WC_RUN(test_subarr_into_other_allocator);
    WC_RUN(test_vec_like_uses_same_allocator);

    WC_RUN(test_storage_alignment_is_size_derived);
    WC_RUN(test_arena_packs_small_elements);

    WC_RUN(test_zero_state_mutation_dies);
    WC_RUN(test_zero_state_reads_and_destroy_are_safe);

    WC_RUN(test_fail_nth_allocation_dies_at_every_site);
}
