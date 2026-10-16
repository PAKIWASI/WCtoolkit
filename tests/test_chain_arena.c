#include "chain_arena.h"
#include "common.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_test_allocator.h"

#include <stdalign.h>
#include <stdint.h>
#include <string.h>


// Every test runs on a test allocator: node frees must match node allocs exactly.
// Blocks are nKB(4) unless a test says otherwise.
#define WITH_CHAIN(ca, ta)                       \
    wc_test_alloc ta;                            \
    wc_test_alloc_init(&(ta), WC_LIBC);          \
    ChainArena ca;                               \
    ChainArena_create(&(ca), wc_test_alloc_allocator(&(ta)), nKB(4))

#define END_CHAIN(ca, ta)                            \
    ({                                               \
        ChainArena_destroy(&(ca));                   \
        EXPECT_EQ((ta).n_errors, 0u);                \
        EXPECT_EQ(wc_test_alloc_destroy(&(ta)), 0u); \
    })

static u64 node_count(const ChainArena* ca)
{
    u64 n = 0;
    for (const ChainArenaNodeHDR* x = ca->normal; x; x = x->prev) {
        n++;
    }
    return n;
}


/* ── Lifecycle ───────────────────────────────────────────────────────────── */

UTEST(chain_arena, create_destroy_leak_free)
{
    WITH_CHAIN(ca, ta);
    EXPECT_EQ(ta.live_blocks, 1u);
    EXPECT_TRUE(ca.normal->prev == NULL); // exactly one normal node, nothing older
    EXPECT_EQ(ChainArena_used(&ca), 0u);
    END_CHAIN(ca, ta);
}

UTEST(chain_arena, destroy_zero_safe_and_idempotent)
{
    ChainArena z;
    memset(&z, 0, sizeof(z));
    ChainArena_destroy(&z);

    WITH_CHAIN(ca, ta);
    ChainArena_destroy(&ca);
    ChainArena_destroy(&ca); // second destroy: no-op
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(chain_arena, grows_across_nodes)
{
    WITH_CHAIN(ca, ta);
    u8* ptrs[64];
    for (int i = 0; i < 64; i++) {
        ptrs[i] = ChainArena_alloc(&ca, 500);
        EXPECT_TRUE((ptrs[i]) != NULL);
        memset(ptrs[i], i, 500);
    }
    EXPECT_TRUE(node_count(&ca) > 1);
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(ptrs[i][0], i);
        EXPECT_EQ(ptrs[i][499], i);
    }
    END_CHAIN(ca, ta);
}


/* ── Alignment (A2) and oversize (A3) ────────────────────────────────────── */

UTEST(chain_arena, alignment_by_address)
{
    WITH_CHAIN(ca, ta);
    const u64 aligns[] = {1, 2, 4, 8, 16, 32, 64, 256};
    for (int round = 0; round < 40; round++) { // crosses node boundaries
        for (u64 i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++) {
            ChainArena_alloc_aligned(&ca, 3, 1); // odd burn
            u8* p = ChainArena_alloc_aligned(&ca, 24, aligns[i]);
            EXPECT_TRUE((p) != NULL);
            EXPECT_EQ((uintptr_t)p % aligns[i], 0u);
        }
    }
    END_CHAIN(ca, ta);
}

UTEST(chain_arena, oversize_request_gets_dedicated_node)
{
    WITH_CHAIN(ca, ta);
    u8* small = ChainArena_alloc(&ca, 16);
    u8* big   = ChainArena_alloc_aligned(&ca, nKB(10), 64); // > one node: was fatal (A3)
    EXPECT_TRUE((big) != NULL);
    EXPECT_EQ((uintptr_t)big % 64, 0u);
    memset(big, 0xAB, nKB(10));
    EXPECT_TRUE(ca.big->cap >= nKB(10)); // sits in its own dedicated block

    u8* after = ChainArena_alloc(&ca, 32); // next small alloc still works
    EXPECT_TRUE((after) != NULL);
    memset(after, 1, 32);
    EXPECT_EQ(big[nKB(10) - 1], 0xAB);
    (void)small;
    END_CHAIN(ca, ta);
}


/* ── Allocator backend ───────────────────────────────────────────────────── */

// The ChainArena allocator has only alloc: realloc is emulated (always a copy,
// free is a no-op), so a grown block never moves in place and never rewinds.
UTEST(chain_arena, realloc_emulated_moves_and_preserves)
{
    WITH_CHAIN(ca, ta);
    const wc_allocator* al = ChainArena_allocator(&ca);

    u8* p = wc_alloc(al, 64, 8);
    for (int i = 0; i < 64; i++) {
        p[i] = (u8)i;
    }
    u8* q = wc_realloc(al, p, 64, 2000, 8);
    EXPECT_TRUE(q != p); // emulated: always a new bump, never in place
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(q[i], i);
    }

    u8* r = wc_realloc(al, q, 2000, nKB(8), 8); // oversized: its own big node
    EXPECT_TRUE(r != NULL);
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(r[i], i);
    }
    END_CHAIN(ca, ta);
}


/* ── Scratch and floor (D10) ─────────────────────────────────────────────── */

UTEST(chain_arena, scratch_across_node_boundary_frees_new_nodes)
{
    WITH_CHAIN(ca, ta);
    u8* keep = ChainArena_alloc(&ca, 100);
    memset(keep, 'K', 100);
    u64 blocks = ta.live_blocks;
    u64 used   = ChainArena_used(&ca);

    ChainArenaScratch mark = ChainArenaScratch_create(&ca);
    for (int i = 0; i < 30; i++) {
        ChainArena_alloc(&ca, 1000); // spans several nodes
    }
    EXPECT_TRUE(ta.live_blocks > blocks);
    ChainArenaScratch_to_mark(mark);
    EXPECT_EQ(ta.live_blocks, blocks); // appended nodes released
    EXPECT_EQ(ChainArena_used(&ca), used);
    EXPECT_TRUE(ca.normal == mark.normal);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(keep[i], 'K');
    }
    END_CHAIN(ca, ta);
}

// Growing a block created before the scratch mark, inside the scratch region,
// must never extend the block in place past the mark: it is copied instead.
UTEST(chain_arena, floor_grow_outer_block_inside_scratch)
{
    WITH_CHAIN(ca, ta);
    const wc_allocator* al = ChainArena_allocator(&ca);
    u8*          v  = wc_alloc(al, 64, 8);
    u64          u  = ChainArena_used(&ca);
    ChainArenaScratch mark = ChainArenaScratch_create(&ca);
    EXPECT_TRUE(wc_realloc(al, v, 64, 512, 8) != v); // never in place past the mark
    ChainArenaScratch_to_mark(mark);
    EXPECT_EQ(ChainArena_used(&ca), u);
    END_CHAIN(ca, ta);
}

// Misuses the allocator on purpose, or the memory belongs to an arena the
// analyzer cannot see through (wc_alloc is inline and has a libc branch).
// NOLINTBEGIN(clang-analyzer-unix.Malloc)
UTEST(chain_arena, floor_free_inside_scratch_does_not_rewind)
{
    WITH_CHAIN(ca, ta);
    const wc_allocator* al    = ChainArena_allocator(&ca);
    u8*          outer = wc_alloc(al, 64, 8);
    u64          u     = ChainArena_used(&ca);
    ChainArenaScratch mark = ChainArenaScratch_create(&ca);
    wc_free(al, outer, 64, 8); // free is a no-op: nothing rewinds
    EXPECT_EQ(ChainArena_used(&ca), u);
    u8* t = wc_alloc(al, 16, 8);
    EXPECT_TRUE(wc_realloc(al, t, 16, 256, 8) != t); // emulated: a copy, not in place
    ChainArenaScratch_to_mark(mark);
    EXPECT_EQ(ChainArena_used(&ca), u);
    END_CHAIN(ca, ta);
}
// NOLINTEND(clang-analyzer-unix.Malloc)


// A2 (ChainArena): the payload starts aligned at WC_MAX_ALIGN in every node, so
// any alignment at or below it is native; bigger alignments are met by sliding
// the pointer forward inside the payload.
UTEST(chain_arena, A2_chain_arena_aligns_address)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_LIBC, nKB(4));

    u8* p16 = ChainArena_alloc_aligned(&ca, 8, 16);
    u8* p64 = ChainArena_alloc_aligned(&ca, 8, 64);
    EXPECT_EQ((uintptr_t)p16 % 16, 0u);
    EXPECT_EQ((uintptr_t)p64 % 64, 0u);

    ChainArena_destroy(&ca);
}

// A3: a request larger than one node used to be fatal, so a GenVec growing past
// 4 KB on a ChainArena aborted. Now it gets a dedicated node.
UTEST(chain_arena, A3_chain_arena_oversize_request)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_LIBC, nKB(4));
    const wc_allocator* al = ChainArena_allocator(&ca);

    u8* p = NULL;
    u64 n = 0;
    for (u64 cap = 64; cap <= nKB(64); cap *= 2) { // GenVec-style doubling past a node
        p = wc_realloc(al, p, n, cap, 8);
        EXPECT_TRUE((p) != NULL);
        memset(p + n, (int)(cap & 0xFF), cap - n);
        n = cap;
    }
    EXPECT_EQ(p[nKB(64) - 1], (int)(nKB(64) & 0xFF));
    ChainArena_destroy(&ca);
}


/* ── Regressions from the review of the first ChainArena draft ───────────── */

// destroy walked `n = n->next` after freeing n, and never freed the tail node
UTEST(chain_arena, destroy_frees_tail_and_every_node)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    ChainArena ca;
    ChainArena_create(&ca, wc_test_alloc_allocator(&ta), nKB(4));
    for (int i = 0; i < 50; i++) {
        ChainArena_alloc(&ca, 1000);
    }
    ChainArena_alloc(&ca, nKB(20)); // dedicated tail node
    ChainArena_destroy(&ca);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u); // zero leaked blocks
}

// a backing allocator that fails must give NULL back, not abort inside the arena
UTEST(chain_arena, backing_failure_returns_null)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_LIBC, nKB(4));
    EXPECT_TRUE(ChainArena_alloc(&ca, (u64)PTRDIFF_MAX) == NULL);
    EXPECT_TRUE(ChainArena_alloc(&ca, 16) != NULL); // arena still usable
    ChainArena_destroy(&ca);
}
