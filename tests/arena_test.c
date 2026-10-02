#include "arena.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_macros.h"
#include "wc_errno.h"
#include "wc_test.h"
#include "wc_test_allocator.h"
#include "wc_test_fatal.h"

#include <stdalign.h>
#include <stdint.h>

void Arena_suite(void);


/* ── Basic alloc ─────────────────────────────────────────────────────────── */

static void test_create_default_size(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 0);
    WC_ASSERT_NOT_NULL(a.base);
    WC_ASSERT_EQ_U64(a.idx, 0);
    WC_ASSERT_EQ_U64(a.size, ARENA_DEFAULT_SIZE);
    WC_ASSERT(a.self == &a);
    Arena_destroy(&a);
}

static void test_create_custom_size(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(8));
    WC_ASSERT_EQ_U64(a.size, nKB(8));
    Arena_destroy(&a);
}

static void test_alloc_returns_valid_ptr(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u8* ptr = Arena_alloc(&a, 64);
    WC_ASSERT_NOT_NULL(ptr);
    WC_ASSERT_TRUE(ptr >= a.base);
    WC_ASSERT_TRUE(ptr + 64 <= a.base + a.size);
    Arena_destroy(&a);
}

static void test_alloc_advances_idx(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    Arena_alloc(&a, 16);
    WC_ASSERT_TRUE(a.idx >= 16);
    Arena_destroy(&a);
}

static void test_alloc_sequential_no_overlap(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    int* p1 = (int*)Arena_alloc(&a, sizeof(int));
    int* p2 = (int*)Arena_alloc(&a, sizeof(int));
    WC_ASSERT_NOT_NULL(p1);
    WC_ASSERT_NOT_NULL(p2);
    *p1 = 111;
    *p2 = 222;
    WC_ASSERT_EQ_INT(*p1, 111);
    WC_ASSERT_EQ_INT(*p2, 222);
    Arena_destroy(&a);
}

static void test_destroy_is_zero_safe_and_idempotent(void)
{
    Arena z;
    memset(&z, 0, sizeof(z));
    Arena_destroy(&z); // zeroed: no-op

    Arena a;
    Arena_create(&a, WC_LIBC, 64);
    Arena_destroy(&a);
    WC_ASSERT_NULL(a.base);
    WC_ASSERT(a.self == NULL);
    Arena_destroy(&a); // second destroy: no-op
}

static void test_reset(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 256);
    Arena_alloc(&a, 100);
    ArenaScratch s = Arena_scratch_begin(&a);
    (void)s;
    Arena_reset(&a);
    WC_ASSERT_EQ_U64(a.idx, 0);
    WC_ASSERT_EQ_U64(a.floor, 0);
    Arena_destroy(&a);
}


/* ── Backing allocator ───────────────────────────────────────────────────── */

static void test_region_comes_from_backing(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    Arena a;
    Arena_create(&a, wc_test_alloc_allocator(&ta), 1000);
    WC_ASSERT_EQ_U64(ta.live_blocks, 1);
    WC_ASSERT_EQ_U64(ta.live_bytes, 1000);
    WC_ASSERT(wc_test_alloc_owns(&ta, a.base));
    Arena_destroy(&a); // must free with the exact size/align it allocated

    WC_ASSERT_EQ_U64(ta.n_errors, 0);
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_arena_on_arena(void)
{
    Arena outer;
    Arena_create(&outer, WC_LIBC, nKB(4));

    Arena inner;
    Arena_create(&inner, Arena_allocator(&outer), nKB(1));
    WC_ASSERT(inner.base >= outer.base && inner.base + inner.size <= outer.base + outer.size);
    WC_ASSERT_NOT_NULL(Arena_alloc(&inner, 100));
    Arena_destroy(&inner); // last block of outer: rewinds it
    WC_ASSERT_EQ_U64(Arena_used(&outer), 0);

    Arena_destroy(&outer);
}

static void test_create_buf_owns_nothing(void)
{
    u8    buf[256];
    Arena a;
    Arena_create_buf(&a, buf, sizeof(buf));
    WC_ASSERT_EQ_U64(a.size, 256);
    WC_ASSERT_EQ_U64(a.idx, 0);
    WC_ASSERT_FALSE(a.owns_base);

    int* p = (int*)Arena_alloc(&a, sizeof(int));
    WC_ASSERT_NOT_NULL(p);
    *p = 55;
    WC_ASSERT_EQ_INT(*p, 55);
    Arena_destroy(&a); // must not free the stack buffer (ASAN would catch it)
}

static void test_create_buf_macro(void)
{
    Arena a;
    ARENA_CREATE_BUF(&a, 128);
    WC_ASSERT_EQ_U64(a.size, 128);
    WC_ASSERT_NOT_NULL(ARENA_ALLOC_N(&a, u64, 4));
    Arena_destroy(&a);
}


/* ── Alignment (by address, A2) ──────────────────────────────────────────── */

static void test_alloc_aligned(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u8* ptr = Arena_alloc_aligned(&a, sizeof(double), sizeof(double));
    WC_ASSERT_NOT_NULL(ptr);
    WC_ASSERT_EQ_U64((uintptr_t)ptr % sizeof(double), 0);
    Arena_destroy(&a);
}

static void test_default_alloc_8byte_aligned(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    Arena_alloc(&a, 1);
    u8* ptr = Arena_alloc(&a, 8);
    WC_ASSERT_NOT_NULL(ptr);
    WC_ASSERT_EQ_U64((uintptr_t)ptr % ARENA_DEFAULT_ALIGNMENT, 0);
    Arena_destroy(&a);
}

static void test_alignment_matrix_on_misaligned_buffers(void)
{
    // every base offset 0..15 x every alignment 1..64, after an odd-sized burn
    alignas(64) static u8 buf[1024];
    const u64            aligns[] = {1, 2, 4, 8, 16, 32, 64};

    for (u64 shift = 0; shift < 16; shift++) {
        for (u64 i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++) {
            Arena a;
            Arena_create_buf(&a, buf + shift, 512);
            WC_ASSERT_NOT_NULL(Arena_alloc_aligned(&a, 3, 1)); // odd burn
            u8* p = Arena_alloc_aligned(&a, 24, aligns[i]);
            WC_ASSERT_NOT_NULL(p);
            WC_ASSERT_EQ_U64((uintptr_t)p % aligns[i], 0);
            WC_ASSERT(p + 24 <= a.base + a.size);
            Arena_destroy(&a);
        }
    }
}

static void test_typed_alloc_uses_alignof(void)
{
    typedef struct {
        alignas(32) u8 x[32];
    } Big;
    u8    buf[256];
    Arena a;
    Arena_create_buf(&a, buf + 1, 200);
    Big* b = ARENA_ALLOC(&a, Big);
    WC_ASSERT_NOT_NULL(b);
    WC_ASSERT_EQ_U64((uintptr_t)b % 32, 0);
    Arena_destroy(&a);
}


/* ── Full Arena ──────────────────────────────────────────────────────────── */

static void test_alloc_full_returns_null(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 32);
    Arena_alloc(&a, 32);
    wc_errno = WC_OK;
    u8* ptr  = Arena_alloc(&a, 1);
    WC_ASSERT_NULL(ptr);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_FULL);
    Arena_destroy(&a);
}

static void test_alloc_full_sets_errno(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 64);
    Arena_alloc(&a, 64);
    wc_errno = WC_OK;
    u8* p    = Arena_alloc(&a, 1);
    WC_ASSERT_NULL(p);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_FULL);
    Arena_destroy(&a);
}

static void test_exhaustion_exact_fit_then_full(void)
{
    u8    buf[64];
    Arena a;
    Arena_create_buf(&a, buf, sizeof(buf));
    WC_ASSERT_NOT_NULL(Arena_alloc_aligned(&a, 64, 1)); // exact fit
    WC_ASSERT_EQ_U64(Arena_remaining(&a), 0);
    WC_ASSERT_NULL(Arena_alloc_aligned(&a, 1, 1));
    WC_ASSERT_EQ_U64(a.idx, 64); // failed alloc leaves idx alone
    Arena_destroy(&a);
}

static void test_used_remaining(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    WC_ASSERT_EQ_U64(Arena_used(&a), 0);
    WC_ASSERT_EQ_U64(Arena_remaining(&a), nKB(1));

    Arena_alloc(&a, 128);
    WC_ASSERT_TRUE(Arena_used(&a) >= 128);
    WC_ASSERT_TRUE(Arena_remaining(&a) <= nKB(1) - 128);
    Arena_destroy(&a);
}


/* ── Allocator backend: in-place realloc and last-block free (A4) ────────── */

static void test_realloc_last_block_grows_in_place(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    wc_allocator al = Arena_allocator(&a);

    u8* p = wc_alloc(al, 16, 8);
    memset(p, 'x', 16);
    u8* q = wc_realloc(al, p, 16, 400, 8);
    WC_ASSERT(q == p);
    WC_ASSERT_EQ_U64(Arena_used(&a), (u64)(p - a.base) + 400);
    WC_ASSERT_EQ_INT(q[15], 'x');

    q = wc_realloc(al, q, 400, 32, 8); // shrink in place
    WC_ASSERT(q == p);
    WC_ASSERT_EQ_U64(Arena_used(&a), (u64)(p - a.base) + 32);
    Arena_destroy(&a);
}

static void test_realloc_non_last_block_copies(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    wc_allocator al = Arena_allocator(&a);

    u8* p = wc_alloc(al, 16, 8);
    memcpy(p, "0123456789abcdef", 16);
    u8* other = wc_alloc(al, 8, 8); // p is no longer on top
    (void)other;

    u8* q = wc_realloc(al, p, 16, 64, 8);
    WC_ASSERT(q != p);
    WC_ASSERT(memcmp(q, "0123456789abcdef", 16) == 0);

    u8* s = wc_realloc(al, other, 8, 4, 8); // shrinking a non-top block keeps it
    WC_ASSERT(s == other);
    Arena_destroy(&a);
}

static void test_realloc_too_big_fails_and_keeps_block(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 128);
    wc_allocator al = Arena_allocator(&a);

    u8* p = wc_alloc(al, 64, 8);
    memset(p, 7, 64);
    u64 used = Arena_used(&a);
    WC_ASSERT_NULL(wc_realloc(al, p, 64, 4096, 8));
    WC_ASSERT_EQ_U64(Arena_used(&a), used);
    WC_ASSERT_EQ_INT(p[63], 7);
    Arena_destroy(&a);
}

static void test_free_rewinds_only_last_block(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    wc_allocator al = Arena_allocator(&a);

    u8* p1 = wc_alloc(al, 32, 8);
    u8* p2 = wc_alloc(al, 32, 8);
    u64 top = Arena_used(&a);

    wc_free(al, p1, 32, 8); // not on top: no-op
    WC_ASSERT_EQ_U64(Arena_used(&a), top);

    wc_free(al, p2, 32, 8); // on top: rewinds
    WC_ASSERT_EQ_U64(Arena_used(&a), (u64)(p2 - a.base));
    Arena_destroy(&a);
}


/* ── Scratch ─────────────────────────────────────────────────────────────── */

static void test_scratch_begin_end(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u64          before = a.idx;
    ArenaScratch sc     = Arena_scratch_begin(&a);
    Arena_alloc(&a, 256);
    WC_ASSERT_TRUE(a.idx > before);
    Arena_scratch_end(sc);
    WC_ASSERT_EQ_U64(a.idx, before);
    Arena_destroy(&a);
}

static void test_scratch_macro(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u64 before = a.idx;

    ARENA_SCRATCH(&a) {
        Arena_alloc(&a, 512);
        WC_ASSERT_TRUE(a.idx > before);
    }

    WC_ASSERT_EQ_U64(a.idx, before);
    Arena_destroy(&a);
}

static void test_scratch_outer_alloc_survives(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));

    int* permanent = (int*)Arena_alloc(&a, sizeof(int));
    *permanent     = 77;

    ARENA_SCRATCH(&a) {
        int* tmp = (int*)Arena_alloc(&a, sizeof(int));
        *tmp     = 999;
    }

    WC_ASSERT_EQ_INT(*permanent, 77);
    Arena_destroy(&a);
}


/* ── Scratch floor (D10) ─────────────────────────────────────────────────── */

// (a) growing a block created before the scope, inside the scope, is a lifetime
// bug (truncated in place, or dangling if copied). Debug builds must die loudly;
// release builds must at least never grow it in place past the mark.
#ifndef NDEBUG
static void grow_outer_block_inside_scratch(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    wc_allocator al  = Arena_allocator(&a);
    u8*          vec = wc_alloc(al, 64, 8);
    ARENA_SCRATCH(&a) {
        vec = wc_realloc(al, vec, 64, 512, 8);
    }
    (void)vec;
    Arena_destroy(&a);
}

#endif

// control for the death test: the same shape with the block created INSIDE the scope is legal
static void grow_inner_block_inside_scratch(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    wc_allocator al = Arena_allocator(&a);
    ARENA_SCRATCH(&a) {
        u8* vec = wc_alloc(al, 64, 8);
        vec     = wc_realloc(al, vec, 64, 512, 8);
        (void)vec;
    }
    Arena_destroy(&a);
}

static void test_floor_grow_outer_block_inside_scratch(void)
{
    WC_ASSERT_EQ_INT(wc_test_dies(grow_inner_block_inside_scratch), 0); // harness sanity
#ifndef NDEBUG
    WC_ASSERT_DIES(grow_outer_block_inside_scratch);
#else
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    wc_allocator al   = Arena_allocator(&a);
    u8*          keep = wc_alloc(al, 64, 8);
    memset(keep, 'K', 64);
    u64 mark = Arena_used(&a);
    ARENA_SCRATCH(&a) {
        u8* grown = wc_realloc(al, keep, 64, 512, 8);
        WC_ASSERT(grown != keep); // copied, never extended past the mark
    }
    WC_ASSERT_EQ_U64(Arena_used(&a), mark);
    for (int i = 0; i < 64; i++) {
        WC_ASSERT_EQ_INT(keep[i], 'K');
    }
    Arena_destroy(&a);
#endif
}

// shrinking an outer block inside the scope is harmless: kept in place, idx unchanged
static void test_floor_shrink_outer_block_inside_scratch(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    wc_allocator al   = Arena_allocator(&a);
    u8*          keep = wc_alloc(al, 64, 8);
    u64          mark = Arena_used(&a);
    ARENA_SCRATCH(&a) {
        WC_ASSERT(wc_realloc(al, keep, 64, 16, 8) == keep);
        WC_ASSERT_EQ_U64(Arena_used(&a), mark); // not rewound below the mark
    }
    Arena_destroy(&a);
}

// (b) a block created before the scope, freed inside it, is not rewound below the mark
static void test_floor_free_inside_scratch_does_not_rewind_below_mark(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    wc_allocator al = Arena_allocator(&a);

    u8* outer = wc_alloc(al, 64, 8);
    u64 mark  = Arena_used(&a);

    ARENA_SCRATCH(&a) {
        wc_free(al, outer, 64, 8); // below the floor: no-op
        WC_ASSERT_EQ_U64(Arena_used(&a), mark);

        u8* t = wc_alloc(al, 16, 8); // on top and above the floor: rewinds
        wc_free(al, t, 16, 8);
        WC_ASSERT_EQ_U64(Arena_used(&a), (u64)(t - a.base));
    }
    WC_ASSERT_EQ_U64(Arena_used(&a), mark);
    Arena_destroy(&a);
}

// (c) nested scopes restore the floor in LIFO order
static void test_floor_nested_scopes(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    Arena_alloc(&a, 10);

    ArenaScratch s1 = Arena_scratch_begin(&a);
    u64          f1 = a.floor;
    Arena_alloc(&a, 20);
    ArenaScratch s2 = Arena_scratch_begin(&a);
    WC_ASSERT(a.floor > f1);
    Arena_alloc(&a, 30);
    Arena_scratch_end(s2);
    WC_ASSERT_EQ_U64(a.floor, f1);
    Arena_scratch_end(s1);
    WC_ASSERT_EQ_U64(a.floor, 0);
    WC_ASSERT_EQ_U64(a.idx, 10);

    // in-place growth works again once the scopes are gone
    wc_allocator al = Arena_allocator(&a);
    u8*          p  = wc_alloc(al, 8, 8);
    WC_ASSERT(wc_realloc(al, p, 8, 64, 8) == p);
    Arena_destroy(&a);
}

// in-place growth of a block created INSIDE the scope is still allowed
static void test_floor_allows_in_place_above_mark(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    wc_allocator al = Arena_allocator(&a);
    Arena_alloc(&a, 40);

    ARENA_SCRATCH(&a) {
        u8* p = wc_alloc(al, 16, 8);
        WC_ASSERT(wc_realloc(al, p, 16, 256, 8) == p);
    }
    Arena_destroy(&a);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */


// ARENA_SCOPE (plan 3.3) and typed helper macros

static void test_arena_scope_basic(void)
{
    int sum = 0;
    ARENA_SCOPE(tmp, nKB(1))
    {
        WC_ASSERT_NOT_NULL(tmp.ctx); // an Arena, not libc
        GenVec v = VEC_OF_IN(tmp, int, 4);
        for (int i = 0; i < 100; i++) { VEC_PUSH(&v, i); } // grows inside the arena
        VEC_FOREACH(&v, int, x) { sum += *x; }
        // no destroy: the arena goes away with the block (LSan would flag a leak)
    }
    WC_ASSERT_EQ_INT(sum, 4950);
}

static void test_arena_scope_nested_and_break(void)
{
    int reached = 0;
    ARENA_SCOPE(outer, 256)
    {
        int* a = (int*)wc_alloc(outer, sizeof(int), alignof(int));
        *a     = 1;
        ARENA_SCOPE(inner, 256)
        {
            WC_ASSERT_TRUE(inner.ctx != outer.ctx); // distinct arenas, no name clash
            int* b = (int*)wc_alloc(inner, sizeof(int), alignof(int));
            *b     = 2;
            reached += *a + *b;
            break; // leaves only the inner scope, inner arena destroyed
        }
        reached += 10;
    }
    WC_ASSERT_EQ_INT(reached, 13);
}

static int scope_early_return(void)
{
    ARENA_SCOPE(tmp, 128)
    {
        char* p = (char*)wc_alloc(tmp, 16, 1);
        p[0]    = 'x';
        return p[0]; // cleanup still destroys the arena
    }
    return 0;
}

static void test_arena_scope_return(void)
{
    WC_ASSERT_EQ_INT(scope_early_return(), 'x');
}

static void test_arena_typed_macros(void)
{
    // ARENA_ALLOC_ZERO / _N / ARENA_PUSH_ARRAY used to declare `(T)* x`,
    // which C parses as a cast: they did not compile when used.
    Arena arena;
    Arena_create(&arena, WC_LIBC, 512);

    typedef struct { u64 a; u32 b; } pair;
    pair* p = ARENA_ALLOC_ZERO(&arena, pair);
    WC_ASSERT_NOT_NULL(p);
    WC_ASSERT_EQ_U64(p->a, 0);
    WC_ASSERT_EQ_U64((uintptr_t)p % alignof(pair), 0);

    u32* z = ARENA_ALLOC_ZERO_N(&arena, u32, 8);
    WC_ASSERT_NOT_NULL(z);
    for (int i = 0; i < 8; i++) { WC_ASSERT_EQ_U64(z[i], 0); }

    int  src[] = {4, 5, 6};
    int* c     = ARENA_PUSH_ARRAY(&arena, int, src, 3);
    WC_ASSERT_NOT_NULL(c);
    WC_ASSERT_TRUE(c != src);
    WC_ASSERT_EQ_INT(c[2], 6);

    Arena_destroy(&arena);
}

void Arena_suite(void)
{
    WC_SUITE("Arena");

    WC_RUN(test_create_default_size);
    WC_RUN(test_create_custom_size);
    WC_RUN(test_alloc_returns_valid_ptr);
    WC_RUN(test_alloc_advances_idx);
    WC_RUN(test_alloc_sequential_no_overlap);
    WC_RUN(test_destroy_is_zero_safe_and_idempotent);
    WC_RUN(test_reset);

    WC_RUN(test_region_comes_from_backing);
    WC_RUN(test_arena_on_arena);
    WC_RUN(test_create_buf_owns_nothing);
    WC_RUN(test_create_buf_macro);

    WC_RUN(test_alloc_aligned);
    WC_RUN(test_default_alloc_8byte_aligned);
    WC_RUN(test_alignment_matrix_on_misaligned_buffers);
    WC_RUN(test_typed_alloc_uses_alignof);

    WC_RUN(test_alloc_full_returns_null);
    WC_RUN(test_alloc_full_sets_errno);
    WC_RUN(test_exhaustion_exact_fit_then_full);
    WC_RUN(test_used_remaining);

    WC_RUN(test_realloc_last_block_grows_in_place);
    WC_RUN(test_realloc_non_last_block_copies);
    WC_RUN(test_realloc_too_big_fails_and_keeps_block);
    WC_RUN(test_free_rewinds_only_last_block);

    WC_RUN(test_scratch_begin_end);
    WC_RUN(test_scratch_macro);
    WC_RUN(test_scratch_outer_alloc_survives);

    WC_SUITE("Arena scratch floor (D10)");

    WC_RUN(test_floor_grow_outer_block_inside_scratch);
    WC_RUN(test_floor_shrink_outer_block_inside_scratch);
    WC_RUN(test_floor_free_inside_scratch_does_not_rewind_below_mark);
    WC_RUN(test_floor_nested_scopes);
    WC_RUN(test_floor_allows_in_place_above_mark);
    WC_RUN(test_arena_scope_basic);
    WC_RUN(test_arena_scope_nested_and_break);
    WC_RUN(test_arena_scope_return);
    WC_RUN(test_arena_typed_macros);
}
