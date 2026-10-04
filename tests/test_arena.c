#include "arena.h"
#include "common.h"
#include "gen_vector.h"
#include "test_support.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_errno.h"
#include "wc_macros.h"
#include "wc_test_allocator.h"

#include <stdalign.h>
#include <stdint.h>
#include <string.h>


/* ── Basic alloc ─────────────────────────────────────────────────────────── */

UTEST(arena, create_default_size)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 0);
    EXPECT_TRUE((a.base) != NULL);
    EXPECT_EQ(a.idx, 0u);
    EXPECT_EQ(a.size, ARENA_DEFAULT_SIZE);
    EXPECT_TRUE(a.self.ctx == &a); // the embedded allocator points at this arena
    Arena_destroy(&a);
}

UTEST(arena, create_custom_size)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(8));
    EXPECT_EQ(a.size, nKB(8));
    Arena_destroy(&a);
}

UTEST(arena, alloc_returns_valid_ptr)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u8* ptr = Arena_alloc(&a, 64);
    EXPECT_TRUE((ptr) != NULL);
    EXPECT_TRUE(ptr >= a.base);
    EXPECT_TRUE(ptr + 64 <= a.base + a.size);
    Arena_destroy(&a);
}

UTEST(arena, alloc_advances_idx)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    Arena_alloc(&a, 16);
    EXPECT_TRUE(a.idx >= 16);
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

UTEST(arena, reset)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 256);
    Arena_alloc(&a, 100);
    ArenaScratch s = Arena_scratch_begin(&a);
    (void)s;
    Arena_reset(&a);
    EXPECT_EQ(a.idx, 0u);
    EXPECT_EQ(a.floor, 0u);
    Arena_destroy(&a);
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
    EXPECT_TRUE(inner.base >= outer.base && inner.base + inner.size <= outer.base + outer.size);
    EXPECT_TRUE((Arena_alloc(&inner, 100)) != NULL);
    Arena_destroy(&inner); // last block of outer: rewinds it
    EXPECT_EQ(Arena_used(&outer), 0u);

    Arena_destroy(&outer);
}

UTEST(arena, create_buf_owns_nothing)
{
    u8    buf[256];
    Arena a;
    Arena_create_buf(&a, buf, sizeof(buf));
    EXPECT_EQ(a.size, 256u);
    EXPECT_EQ(a.idx, 0u);
    EXPECT_TRUE(a.backing == WC_BORROWED); // destroy hands the buffer back to nobody

    int* p = Arena_alloc(&a, sizeof(int));
    EXPECT_TRUE((p) != NULL);
    *p = 55;
    EXPECT_EQ(*p, 55);
    Arena_destroy(&a); // must not free the stack buffer (ASAN would catch it)
}

UTEST(arena, create_buf_macro)
{
    Arena a;
    ARENA_CREATE_BUF(&a, 128);
    EXPECT_EQ(a.size, 128u);
    EXPECT_TRUE((ARENA_ALLOC_N(&a, u64, 4)) != NULL);
    Arena_destroy(&a);
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

UTEST(arena, alignment_matrix_on_misaligned_buffers)
{
    // every base offset 0..15 x every alignment 1..64, after an odd-sized burn
    alignas(64) static u8 buf[1024];
    const u64             aligns[] = {1, 2, 4, 8, 16, 32, 64};

    for (u64 shift = 0; shift < 16; shift++) {
        for (u64 i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++) {
            Arena a;
            Arena_create_buf(&a, buf + shift, 512);
            EXPECT_TRUE((Arena_alloc_aligned(&a, 3, 1)) != NULL); // odd burn
            u8* p = Arena_alloc_aligned(&a, 24, aligns[i]);
            EXPECT_TRUE((p) != NULL);
            EXPECT_EQ((uintptr_t)p % aligns[i], 0u);
            EXPECT_TRUE(p + 24 <= a.base + a.size);
            Arena_destroy(&a);
        }
    }
}

UTEST(arena, typed_alloc_uses_alignof)
{
    typedef struct {
        alignas(32) u8 x[32];
    } Big;
    u8    buf[256];
    Arena a;
    Arena_create_buf(&a, buf + 1, 200);
    Big* b = ARENA_ALLOC(&a, Big);
    EXPECT_TRUE((b) != NULL);
    EXPECT_EQ((uintptr_t)b % 32, 0u);
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

UTEST(arena, exhaustion_exact_fit_then_full)
{
    u8    buf[64];
    Arena a;
    Arena_create_buf(&a, buf, sizeof(buf));
    EXPECT_TRUE((Arena_alloc_aligned(&a, 64, 1)) != NULL); // exact fit
    EXPECT_EQ(Arena_remaining(&a), 0u);
    EXPECT_TRUE((Arena_alloc_aligned(&a, 1, 1)) == NULL);
    EXPECT_EQ(a.idx, 64u); // failed alloc leaves idx alone
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

UTEST(arena, scratch_begin_end)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u64          before = a.idx;
    ArenaScratch sc     = Arena_scratch_begin(&a);
    Arena_alloc(&a, 256);
    EXPECT_TRUE(a.idx > before);
    Arena_scratch_end(sc);
    EXPECT_EQ(a.idx, before);
    Arena_destroy(&a);
}

UTEST(arena, scratch_macro)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    u64 before = a.idx;

    ARENA_SCRATCH (&a) {
        Arena_alloc(&a, 512);
        EXPECT_TRUE(a.idx > before);
    }

    EXPECT_EQ(a.idx, before);
    Arena_destroy(&a);
}

UTEST(arena, scratch_outer_alloc_survives)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));

    int* permanent = Arena_alloc(&a, sizeof(int));
    *permanent     = 77;

    ARENA_SCRATCH (&a) {
        int* tmp = Arena_alloc(&a, sizeof(int));
        *tmp     = 999;
    }

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
    ARENA_SCRATCH (&a) {
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
    const wc_allocator* al = Arena_allocator(&a);
    ARENA_SCRATCH (&a) {
        u8* vec = wc_alloc(al, 64, 8);
        vec     = wc_realloc(al, vec, 64, 512, 8);
        (void)vec;
    }
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
    ARENA_SCRATCH (&a) {
        u8* grown = wc_realloc(al, keep, 64, 512, 8);
        EXPECT_TRUE(grown != keep); // copied, never extended past the mark
    }
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
    ARENA_SCRATCH (&a) {
        EXPECT_TRUE(wc_realloc(al, keep, 64, 16, 8) == keep);
        EXPECT_EQ(Arena_used(&a), mark); // not rewound below the mark
    }
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

    ARENA_SCRATCH (&a) {
        wc_free(al, outer, 64, 8); // below the floor: no-op
        EXPECT_EQ(Arena_used(&a), mark);

        u8* t = wc_alloc(al, 16, 8); // on top and above the floor: rewinds
        wc_free(al, t, 16, 8);
        EXPECT_EQ(Arena_used(&a), (u64)(t - a.base));
    }
    EXPECT_EQ(Arena_used(&a), mark);
    Arena_destroy(&a);
}

// (c) nested scopes restore the floor in LIFO order
UTEST(arena, floor_nested_scopes)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    Arena_alloc(&a, 10);

    ArenaScratch s1 = Arena_scratch_begin(&a);
    u64          f1 = a.floor;
    Arena_alloc(&a, 20);
    ArenaScratch s2 = Arena_scratch_begin(&a);
    EXPECT_TRUE(a.floor > f1);
    Arena_alloc(&a, 30);
    Arena_scratch_end(s2);
    EXPECT_EQ(a.floor, f1);
    Arena_scratch_end(s1);
    EXPECT_EQ(a.floor, 0u);
    EXPECT_EQ(a.idx, 10u);

    // in-place growth works again once the scopes are gone
    const wc_allocator* al = Arena_allocator(&a);
    u8*          p  = wc_alloc(al, 8, 8);
    EXPECT_TRUE(wc_realloc(al, p, 8, 64, 8) == p);
    Arena_destroy(&a);
}

// in-place growth of a block created INSIDE the scope is still allowed
UTEST(arena, floor_allows_in_place_above_mark)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    const wc_allocator* al = Arena_allocator(&a);
    Arena_alloc(&a, 40);

    ARENA_SCRATCH (&a) {
        u8* p = wc_alloc(al, 16, 8);
        EXPECT_TRUE(wc_realloc(al, p, 16, 256, 8) == p);
    }
    Arena_destroy(&a);
}


// ARENA_SCOPE (plan 3.3) and typed helper macros

UTEST(arena, scope_basic)
{
    int sum = 0;
    ARENA_SCOPE (tmp, nKB(1)) {
        EXPECT_TRUE((tmp->ctx) != NULL); // an Arena, not libc
        GenVec v = VEC_OF_IN(tmp, int, 4);
        for (int i = 0; i < 100; i++) {
            VEC_PUSH(&v, i);
        } // grows inside the arena
        VEC_FOREACH (&v, int, x) {
            sum += *x;
        }
        // no destroy: the arena goes away with the block (LSan would flag a leak)
    }
    EXPECT_EQ(sum, 4950);
}

// Misuses the allocator on purpose, or the memory belongs to an arena the
// analyzer cannot see through (wc_alloc is inline and has a libc branch).
// NOLINTBEGIN(clang-analyzer-unix.Malloc)
UTEST(arena, scope_nested_and_break)
{
    int reached = 0;
    ARENA_SCOPE (outer, 256) {
        int* a = wc_alloc(outer, sizeof(int), alignof(int));
        *a     = 1;
        ARENA_SCOPE (inner, 256) {
            EXPECT_TRUE(inner->ctx != outer->ctx); // distinct arenas, no name clash
            int* b = wc_alloc(inner, sizeof(int), alignof(int));
            *b     = 2;
            reached += *a + *b;
            break; // leaves only the inner scope, inner arena destroyed
        }
        reached += 10;
    }
    EXPECT_EQ(reached, 13);
}
// NOLINTEND(clang-analyzer-unix.Malloc)

// Misuses the allocator on purpose, or the memory belongs to an arena the
// analyzer cannot see through (wc_alloc is inline and has a libc branch).
// NOLINTBEGIN(clang-analyzer-unix.Malloc)
static int scope_early_return(void)
{
    ARENA_SCOPE (tmp, 128) {
        char* p = wc_alloc(tmp, 16, 1);
        p[0]    = 'x';
        return p[0]; // cleanup still destroys the arena
    }
    return 0;
}
// NOLINTEND(clang-analyzer-unix.Malloc)

UTEST(arena, scope_return)
{
    EXPECT_EQ(scope_early_return(), 'x');
}

UTEST(arena, typed_macros)
{
    // ARENA_ALLOC_ZERO / _N / ARENA_PUSH_ARRAY used to declare `(T)* x`,
    // which C parses as a cast: they did not compile when used.
    Arena arena;
    Arena_create(&arena, WC_LIBC, 512);

    typedef struct {
        u64 a;
        u32 b;
    } pair;
    pair* p = ARENA_ALLOC_ZERO(&arena, pair);
    ASSERT_TRUE(p != NULL); // stop here rather than dereference NULL
    EXPECT_EQ(p->a, 0u);
    EXPECT_EQ((uintptr_t)p % alignof(pair), 0u);

    u32* z = ARENA_ALLOC_ZERO_N(&arena, u32, 8);
    ASSERT_TRUE(z != NULL); // stop here rather than dereference NULL
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(z[i], 0u);
    }

    int  src[] = {4, 5, 6};
    int* c     = ARENA_PUSH_ARRAY(&arena, int, src, 3);
    ASSERT_TRUE(c != NULL); // stop here rather than dereference NULL
    EXPECT_TRUE(c != src);
    EXPECT_EQ(c[2], 6);

    Arena_destroy(&arena);
}


/* ── Fixed defects (regression tests, written before the fix in Phase 0) ─── */

// A1: capacity check `size - aligned_idx < req` underflows when alignment
// pushes aligned_idx past `size`, so a full arena hands out a pointer past its end.
UTEST(arena, A1_arena_alloc_past_end_returns_null)
{
    alignas(16) u8 buf[13];
    Arena          arena;
    Arena_create_buf(&arena, buf, sizeof(buf));

    EXPECT_TRUE((Arena_alloc_aligned(&arena, 1, 8)) != NULL); // idx 0 -> 1
    EXPECT_TRUE((Arena_alloc_aligned(&arena, 4, 8)) != NULL); // aligned to 8, idx -> 12

    u8* p = Arena_alloc(&arena, 1); // default align 16: aligned_idx = 16 > size 13
    EXPECT_TRUE((p) == NULL);
    EXPECT_TRUE(p == NULL || p < buf + sizeof(buf));
}

// A1 (aligned variant)
UTEST(arena, A1_arena_alloc_aligned_past_end_returns_null)
{
    alignas(16) u8 buf[20];
    Arena          arena;
    Arena_create_buf(&arena, buf, sizeof(buf));

    EXPECT_TRUE((Arena_alloc_aligned(&arena, 17, 1)) != NULL); // idx -> 17
    u8* p = Arena_alloc_aligned(&arena, 1, 32);                // aligned_idx = 32 > 20
    EXPECT_TRUE((p) == NULL);
}

// A2: alignment is applied to the OFFSET, not the ADDRESS.
UTEST(arena, A2_arena_aligns_address_not_offset)
{
    alignas(64) u8 buf[256];
    Arena          arena;
    Arena_create_buf(&arena, buf + 4, sizeof(buf) - 4); // base is 4 mod 16

    u8* p16 = Arena_alloc_aligned(&arena, 8, 16);
    u8* p64 = Arena_alloc_aligned(&arena, 8, 64);
    EXPECT_TRUE((p16) != NULL);
    EXPECT_TRUE((p64) != NULL);
    EXPECT_EQ((uintptr_t)p16 % 16, 0u);
    EXPECT_EQ((uintptr_t)p64 % 64, 0u);
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
