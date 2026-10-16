#include "arena.h"
#include "common.h"
#include "test_support.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_errno.h"
#include "wc_test_allocator.h"

#include <stdalign.h>
#include <stdint.h>
#include <string.h>

#define SIZE nKB(4)

/* ── Basic alloc ─────────────────────────────────────────────────────────── */

UTEST(arena, create_default_size)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 0);
    EXPECT_TRUE((a.base) != NULL);
    EXPECT_EQ(a.off, 0u);
    EXPECT_EQ(a.cap, SIZE);
    EXPECT_TRUE(a.self.ctx == &a); // the embedded allocator points at this arena
    Arena_destroy(&a);
}

UTEST(arena, create_custom_size)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(8));
    EXPECT_EQ(a.cap, nKB(8));
    Arena_destroy(&a);
}

UTEST(arena, alloc_returns_valid_ptr)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u8* ptr = Arena_alloc(&a, 64);
    EXPECT_TRUE((ptr) != NULL);
    EXPECT_TRUE(ptr >= a.base);
    EXPECT_TRUE(ptr + 64 <= a.base + a.cap);
    Arena_destroy(&a);
}

UTEST(arena, alloc_advances_idx)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u8* b = Arena_alloc(&a, 16);
    b[0] = 69;
    EXPECT_TRUE(a.off >= 16);
    Arena_destroy(&a);
}

UTEST(arena, alloc_sequential_no_overlap)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    int* p1 = Arena_alloc(&a, sizeof(int));
    int* p2 = Arena_alloc(&a, sizeof(int));
    EXPECT_TRUE((p1) != NULL);
    EXPECT_TRUE((p2) != NULL);
    *p1 = 111;
    *p2 = 222;
    EXPECT_EQ(*p1, 111);
    EXPECT_EQ(*p2, 222);
    Arena_destroy(&a);
}

UTEST(arena, destroy_is_zero_safe_and_idempotent)
{
    Arena z;
    memset(&z, 0, sizeof(z));
    Arena_destroy(&z); // zeroed: no-op

    Arena a;
    Arena_create(&a, WC_LIBC, 64);
    Arena_destroy(&a);
    EXPECT_TRUE((a.base) == NULL);
    EXPECT_TRUE(a.self.ctx == NULL);
    Arena_destroy(&a); // second destroy: no-op
}


/* ── Backing allocator ───────────────────────────────────────────────────── */

UTEST(arena, region_comes_from_backing)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    Arena a;
    Arena_create(&a, wc_test_alloc_allocator(&ta), 1000);
    EXPECT_EQ(ta.live_blocks, 1u);
    EXPECT_EQ(ta.live_bytes, 1000u);
    EXPECT_TRUE(wc_test_alloc_owns(&ta, a.base));
    Arena_destroy(&a); // must free with the exact size/align it allocated

    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(arena, on_arena)
{
    Arena outer;
    Arena_create(&outer, WC_LIBC, nKB(4));

    Arena inner;
    Arena_create(&inner, Arena_allocator(&outer), nKB(1));
    EXPECT_TRUE(inner.base >= outer.base && inner.base + inner.cap <= outer.base + outer.cap);
    EXPECT_TRUE((Arena_alloc(&inner, 100)) != NULL);
    Arena_destroy(&inner); // last block of outer: rewinds it
    EXPECT_EQ(Arena_used(&outer), 0u);

    Arena_destroy(&outer);
}


/* ── Alignment (by address, A2) ──────────────────────────────────────────── */

UTEST(arena, alloc_aligned)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u8* ptr = Arena_alloc_aligned(&a, sizeof(double), sizeof(double));
    EXPECT_TRUE((ptr) != NULL);
    EXPECT_EQ((uintptr_t)ptr % sizeof(double), 0u);
    Arena_destroy(&a);
}

UTEST(arena, default_alloc_max_aligned)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    Arena_alloc(&a, 1);
    u8* ptr = Arena_alloc(&a, 8);
    EXPECT_TRUE((ptr) != NULL);
    EXPECT_EQ((uintptr_t)ptr % WC_MAX_ALIGN, 0u); // like malloc
    Arena_destroy(&a);
}



/* ── Full Arena ──────────────────────────────────────────────────────────── */

UTEST(arena, alloc_full_returns_null)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 32);
    Arena_alloc(&a, 32);
    wc_errno = WC_OK;
    u8* ptr  = Arena_alloc(&a, 1);
    EXPECT_TRUE((ptr) == NULL);
    EXPECT_EQ(wc_errno, (wc_err)WC_ERR_FULL);
    Arena_destroy(&a);
}

UTEST(arena, alloc_full_sets_errno)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 64);
    Arena_alloc(&a, 64);
    wc_errno = WC_OK;
    u8* p    = Arena_alloc(&a, 1);
    EXPECT_TRUE((p) == NULL);
    EXPECT_EQ(wc_errno, (wc_err)WC_ERR_FULL);
    Arena_destroy(&a);
}

UTEST(arena, used_remaining)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    EXPECT_EQ(Arena_used(&a), 0u);
    EXPECT_EQ(Arena_remaining(&a), nKB(1));

    Arena_alloc(&a, 128);
    EXPECT_TRUE(Arena_used(&a) >= 128);
    EXPECT_TRUE(Arena_remaining(&a) <= nKB(1) - 128);
    Arena_destroy(&a);
}


/* ── Allocator backend: in-place realloc and last-block free (A4) ────────── */

UTEST(arena, realloc_last_block_grows_in_place)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    const wc_allocator* al = Arena_allocator(&a);

    u8* p = wc_alloc(al, 16, 8);
    memset(p, 'x', 16);
    u8* q = wc_realloc(al, p, 16, 400, 8);
    EXPECT_TRUE(q == p);
    EXPECT_EQ(Arena_used(&a), (u64)(p - a.base) + 400);
    EXPECT_EQ(q[15], 'x');

    q = wc_realloc(al, q, 400, 32, 8); // shrink in place
    EXPECT_TRUE(q == p);
    EXPECT_EQ(Arena_used(&a), (u64)(p - a.base) + 32);
    Arena_destroy(&a);
}

UTEST(arena, realloc_non_last_block_copies)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    const wc_allocator* al = Arena_allocator(&a);

    u8*               p           = wc_alloc(al, 16, 8);
    static const char bytes16[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'}; // 16 bytes, not a C string
    memcpy(p, bytes16, sizeof(bytes16));
    u8* other = wc_alloc(al, 8, 8); // p is no longer on top
    (void)other;

    u8* q = wc_realloc(al, p, 16, 64, 8);
    EXPECT_TRUE(q != p);
    EXPECT_TRUE(memcmp(q, "0123456789abcdef", 16) == 0);

    u8* s = wc_realloc(al, other, 8, 4, 8); // shrinking a non-top block keeps it
    EXPECT_TRUE(s == other);
    Arena_destroy(&a);
}

UTEST(arena, realloc_too_big_fails_and_keeps_block)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 128);
    const wc_allocator* al = Arena_allocator(&a);

    u8* p = wc_alloc(al, 64, 8);
    memset(p, 7, 64);
    u64 used = Arena_used(&a);
    EXPECT_TRUE((wc_realloc(al, p, 64, 4096, 8)) == NULL);
    EXPECT_EQ(Arena_used(&a), used);
    EXPECT_EQ(p[63], 7);
    Arena_destroy(&a);
}

UTEST(arena, free_rewinds_only_last_block)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    const wc_allocator* al = Arena_allocator(&a);

    u8* p1  = wc_alloc(al, 32, 8);
    u8* p2  = wc_alloc(al, 32, 8);
    u64 top = Arena_used(&a);

    wc_free(al, p1, 32, 8); // not on top: no-op
    EXPECT_EQ(Arena_used(&a), top);

    wc_free(al, p2, 32, 8); // on top: rewinds
    EXPECT_EQ(Arena_used(&a), (u64)(p2 - a.base));
    Arena_destroy(&a);
}


/* ── Scratch ─────────────────────────────────────────────────────────────── */

UTEST(arena, scratch_create_to_mark)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u64          before = a.off;
    ArenaScratch sc     = ArenaScratch_create(&a);
    Arena_alloc(&a, 256);
    EXPECT_TRUE(a.off > before);
    ArenaScratch_to_mark(sc);
    EXPECT_EQ(a.off, before);
    Arena_destroy(&a);
}

UTEST(arena, scratch_macro)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u64 before = a.off;

    ArenaScratch mark = ArenaScratch_create(&a);
    Arena_alloc(&a, 512);
    EXPECT_TRUE(a.off > before);
    ArenaScratch_to_mark(mark);

    EXPECT_EQ(a.off, before);
    Arena_destroy(&a);
}

UTEST(arena, scratch_outer_alloc_survives)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));

    int* permanent = Arena_alloc(&a, sizeof(int));
    *permanent     = 77;

    ArenaScratch mark = ArenaScratch_create(&a);
    int* tmp = Arena_alloc(&a, sizeof(int));
    *tmp     = 999;
    ArenaScratch_to_mark(mark);

    EXPECT_EQ(*permanent, 77);
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
    const wc_allocator* al  = Arena_allocator(&a);
    u8*          vec = wc_alloc(al, 64, 8);
    ArenaScratch mark = ArenaScratch_create(&a);
    vec = wc_realloc(al, vec, 64, 512, 8);
    ArenaScratch_to_mark(mark);
    (void)vec;
    Arena_destroy(&a);
}

#endif

// control for the death test: the same shape with the block created INSIDE the scope is legal
static void grow_inner_block_inside_scratch(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    const wc_allocator* al = Arena_allocator(&a);
    ArenaScratch mark = ArenaScratch_create(&a);
    u8* vec = wc_alloc(al, 64, 8);
    vec     = wc_realloc(al, vec, 64, 512, 8);
    (void)vec;
    ArenaScratch_to_mark(mark);
    Arena_destroy(&a);
}

UTEST(arena, floor_grow_outer_block_inside_scratch)
{
    EXPECT_EQ(test_dies(grow_inner_block_inside_scratch), 0); // harness sanity
#ifndef NDEBUG
    EXPECT_DIES(grow_outer_block_inside_scratch);
#else
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    const wc_allocator* al   = Arena_allocator(&a);
    u8*          keep = wc_alloc(al, 64, 8);
    memset(keep, 'K', 64);
    u64 mark = Arena_used(&a);
    ArenaScratch inner = ArenaScratch_create(&a);
    u8* grown = wc_realloc(al, keep, 64, 512, 8);
    EXPECT_TRUE(grown != keep); // copied, never extended past the mark
    ArenaScratch_to_mark(inner);
    EXPECT_EQ(Arena_used(&a), mark);
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(keep[i], 'K');
    }
    Arena_destroy(&a);
#endif
}

// shrinking an outer block inside the scope is harmless: kept in place, idx unchanged
UTEST(arena, floor_shrink_outer_block_inside_scratch)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    const wc_allocator* al   = Arena_allocator(&a);
    u8*          keep = wc_alloc(al, 64, 8);
    u64          mark = Arena_used(&a);
    ArenaScratch inner = ArenaScratch_create(&a);
    EXPECT_TRUE(wc_realloc(al, keep, 64, 16, 8) == keep);
    EXPECT_EQ(Arena_used(&a), mark); // not rewound below the mark
    ArenaScratch_to_mark(inner);
    Arena_destroy(&a);
}

// (b) a block created before the scope, freed inside it, is not rewound below the mark
UTEST(arena, floor_free_inside_scratch_does_not_rewind_below_mark)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    const wc_allocator* al = Arena_allocator(&a);

    u8* outer = wc_alloc(al, 64, 8);
    u64 mark  = Arena_used(&a);

    ArenaScratch inner = ArenaScratch_create(&a);
    wc_free(al, outer, 64, 8); // below the floor: no-op
    EXPECT_EQ(Arena_used(&a), mark);

    u8* t = wc_alloc(al, 16, 8); // on top and above the floor: rewinds
    wc_free(al, t, 16, 8);
    EXPECT_EQ(Arena_used(&a), (u64)(t - a.base));
    ArenaScratch_to_mark(inner);
    EXPECT_EQ(Arena_used(&a), mark);
    Arena_destroy(&a);
}

// in-place growth of a block created INSIDE the scope is still allowed
UTEST(arena, floor_allows_in_place_above_mark)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    const wc_allocator* al = Arena_allocator(&a);
    Arena_alloc(&a, 40);

    ArenaScratch mark = ArenaScratch_create(&a);
    u8* p = wc_alloc(al, 16, 8);
    EXPECT_TRUE(wc_realloc(al, p, 16, 256, 8) == p);
    ArenaScratch_to_mark(mark);
    Arena_destroy(&a);
}



// A4: the arena allocator had no realloc, so every growth abandoned the old
// block (about half of a 1 KB arena was dead blocks in the main.c scenario).
UTEST(arena, A4_arena_grows_last_block_in_place)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    const wc_allocator* al = Arena_allocator(&a);

    u8* p = NULL;
    u64 n = 0;
    for (u64 cap = 8; cap <= 512; cap *= 2) {
        u8* q = wc_realloc(al, p, n, cap, 8);
        EXPECT_TRUE(p == NULL || q == p); // never moves: it is always the last block
        p = q;
        n = cap;
    }
    EXPECT_EQ(Arena_used(&a), 512u); // no dead blocks
    Arena_destroy(&a);
}


/* ── Regressions from the review of the first Arena draft ────────────────── */

// Arena_create never stored the backing allocator, so destroy freed through NULL
UTEST(arena, destroy_uses_the_backing_it_was_created_with)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    Arena a;
    Arena_create(&a, wc_test_alloc_allocator(&ta), 512);
    EXPECT_TRUE(a.alloc == wc_test_alloc_allocator(&ta));
    Arena_destroy(&a);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

#ifndef NDEBUG // zero-state use is a WC_ASSERT: dies in Debug only
// Zero state is dead (C3): aborts in Debug.
static void alloc_on_zeroed_arena(void)
{
    Arena z;
    memset(&z, 0, sizeof(z));
    (void)Arena_alloc(&z, 8);
}

static void alloc_aligned_on_zeroed_arena(void)
{
    Arena z;
    memset(&z, 0, sizeof(z));
    (void)Arena_alloc_aligned(&z, 8, 8);
}

static void allocator_of_zeroed_arena(void)
{
    Arena z;
    memset(&z, 0, sizeof(z));
    (void)Arena_allocator(&z); // zeroed self (vt == NULL) would read as WC_LIBC
}

static void alloc_after_arena_destroy(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 64);
    Arena_destroy(&a);
    (void)Arena_alloc(&a, 8);
}

UTEST(arena, zero_state_is_dead)
{
    EXPECT_DIES(alloc_on_zeroed_arena);
    EXPECT_DIES(alloc_aligned_on_zeroed_arena);
    EXPECT_DIES(allocator_of_zeroed_arena);
    EXPECT_DIES(alloc_after_arena_destroy);
}
#endif
