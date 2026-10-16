#include "arena.h"
#include "common.h"
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
    Arena_create(&a, wc_test_alloc_allocator(&ta), nKB(4));
    EXPECT_EQ(ta.live_blocks, 1u);
    EXPECT_EQ(ta.live_bytes, nKB(4));
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
    Arena_destroy(&inner);
    EXPECT_EQ(Arena_used(&outer), nKB(4)); // inner's arena free is a no-op: outer keeps the region

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
    Arena_create(&a, WC_LIBC, nKB(4));
    Arena_alloc(&a, nKB(4));
    u8* ptr = Arena_alloc(&a, 1);
    EXPECT_TRUE((ptr) == NULL);
    Arena_destroy(&a);
}

UTEST(arena, used_remaining)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    EXPECT_EQ(Arena_used(&a), 0u);
    EXPECT_EQ(Arena_remaining(&a), nKB(4));

    Arena_alloc(&a, 128);
    EXPECT_TRUE(Arena_used(&a) >= 128);
    EXPECT_TRUE(Arena_remaining(&a) <= nKB(4) - 128);
    Arena_destroy(&a);
}


/* ── Allocator backend: realloc is emulated (always a copy), free is a no-op ─ */

UTEST(arena, realloc_copies_and_preserves)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    const wc_allocator* al = Arena_allocator(&a);

    u8* p = wc_alloc(al, 16, 8);
    memset(p, 'x', 16);
    u8* q = wc_realloc(al, p, 16, 400, 8);
    EXPECT_TRUE(q != p); // emulated: a fresh allocation, never in place
    EXPECT_EQ(q[15], 'x');

    u8* s = wc_realloc(al, q, 400, 32, 8); // shrinking re-allocates too
    EXPECT_TRUE(s != NULL);
    EXPECT_EQ(s[15], 'x');
    Arena_destroy(&a);
}

// Misuses the allocator on purpose (wc_alloc is inline and has a libc branch).
// NOLINTBEGIN(clang-analyzer-unix.Malloc)
UTEST(arena, free_is_a_no_op)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    const wc_allocator* al = Arena_allocator(&a);

    u8* p1  = wc_alloc(al, 32, 8);
    u8* p2  = wc_alloc(al, 32, 8);
    u64 top = Arena_used(&a);

    wc_free(al, p1, 32, 8); // no rewind, on top or not
    wc_free(al, p2, 32, 8);
    EXPECT_EQ(Arena_used(&a), top);
    Arena_destroy(&a);
}
// NOLINTEND(clang-analyzer-unix.Malloc)

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


/* ── Scratch and rewind ──────────────────────────────────────────────────── */

// Growing a block created before the scratch mark, inside the scratch region,
// is a fresh (emulated) allocation: never in place, and the rewind lands on the mark.
UTEST(arena, floor_grow_outer_block_inside_scratch)
{
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
}

// Shrinking an outer block inside the scope is also a fresh allocation; the
// rewind to the mark must not fail because of it.
UTEST(arena, floor_shrink_outer_block_inside_scratch)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    const wc_allocator* al   = Arena_allocator(&a);
    u8*          keep = wc_alloc(al, 64, 8);
    u64          mark = Arena_used(&a);
    ArenaScratch inner = ArenaScratch_create(&a);
    u8* shrunk = wc_realloc(al, keep, 64, 16, 8);
    EXPECT_TRUE(shrunk != keep); // emulated: a fresh block on top
    ArenaScratch_to_mark(inner);
    EXPECT_EQ(Arena_used(&a), mark);
    Arena_destroy(&a);
}

// Freeing inside the scope does not rewind the arena past (or at) the mark:
// free is a no-op, only to_mark brings off back down.
UTEST(arena, floor_free_inside_scratch_does_not_rewind_below_mark)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    const wc_allocator* al = Arena_allocator(&a);

    u8* outer = wc_alloc(al, 64, 8);
    u64 mark  = Arena_used(&a);

    ArenaScratch inner = ArenaScratch_create(&a);
    wc_free(al, outer, 64, 8); // below the mark: no-op
    EXPECT_EQ(Arena_used(&a), mark);

    u8* t = wc_alloc(al, 16, 8); // on top of the mark: still a no-op on free
    wc_free(al, t, 16, 8);
    EXPECT_EQ(Arena_used(&a), (u64)(t - a.base) + 16);
    ArenaScratch_to_mark(inner);
    EXPECT_EQ(Arena_used(&a), mark);
    Arena_destroy(&a);
}

// Growth inside the scope happens on top of the mark and rewinds with it.
UTEST(arena, floor_allows_in_place_above_mark)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    const wc_allocator* al = Arena_allocator(&a);
    Arena_alloc(&a, 40);

    ArenaScratch sc = ArenaScratch_create(&a);
    u8* p = wc_alloc(al, 16, 8);
    u8* q = wc_realloc(al, p, 16, 256, 8);
    EXPECT_TRUE(q != p); // emulated: a fresh block, still above the mark
    ArenaScratch_to_mark(sc);
    EXPECT_EQ(Arena_used(&a), sc.mark);
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

