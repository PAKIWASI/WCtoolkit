#include "chain_arena.h"
#include "common.h"
#include "test_support.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_test_allocator.h"

#include <stdalign.h>
#include <stdint.h>
#include <string.h>


// Every test runs on a test allocator: node frees must match node allocs exactly.
#define WITH_CHAIN(ca, ta)              \
    wc_test_alloc ta;                   \
    wc_test_alloc_init(&(ta), WC_LIBC); \
    ChainArena ca;                      \
    ChainArena_create(&(ca), wc_test_alloc_allocator(&(ta)))

#define END_CHAIN(ca, ta)                            \
    ({                                               \
        ChainArena_destroy(&(ca));                   \
        EXPECT_EQ((ta).n_errors, 0u);                \
        EXPECT_EQ(wc_test_alloc_destroy(&(ta)), 0u); \
    })

static u64 node_count(const ChainArena* ca)
{
    u64 n = 0;
    for (const ChainArenaNode* x = ca->head; x; x = x->next) {
        n++;
    }
    return n;
}


/* ── Lifecycle ───────────────────────────────────────────────────────────── */

UTEST(chain_arena, create_destroy_leak_free)
{
    WITH_CHAIN(ca, ta);
    EXPECT_EQ(ta.live_blocks, 1u);
    EXPECT_TRUE(ca.head == ca.tail);
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
    EXPECT_TRUE(ca.tail->cap >= nKB(10));

    u8* after = ChainArena_alloc(&ca, 32); // next small alloc still works
    EXPECT_TRUE((after) != NULL);
    memset(after, 1, 32);
    EXPECT_EQ(big[nKB(10) - 1], 0xAB);
    (void)small;
    END_CHAIN(ca, ta);
}


/* ── Allocator backend ───────────────────────────────────────────────────── */

UTEST(chain_arena, realloc_in_place_then_moves_across_node)
{
    WITH_CHAIN(ca, ta);
    wc_allocator al = ChainArena_allocator(&ca);

    u8* p = wc_alloc(al, 64, 8);
    for (int i = 0; i < 64; i++) {
        p[i] = (u8)i;
    }
    u8* q = wc_realloc(al, p, 64, 2000, 8);
    EXPECT_TRUE(q == p); // in place inside the tail node

    u8* r = wc_realloc(al, q, 2000, nKB(8), 8); // cannot fit: moves to a new node
    EXPECT_TRUE(r != q);
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(r[i], i);
    }
    END_CHAIN(ca, ta);
}

UTEST(chain_arena, free_rewinds_last_block)
{
    WITH_CHAIN(ca, ta);
    wc_allocator al = ChainArena_allocator(&ca);
    u8*          p1 = wc_alloc(al, 32, 8);
    u8*          p2 = wc_alloc(al, 32, 8);
    u64          u  = ChainArena_used(&ca);
    wc_free(al, p1, 32, 8); // not on top
    EXPECT_EQ(ChainArena_used(&ca), u);
    wc_free(al, p2, 32, 8);
    EXPECT_TRUE(ChainArena_used(&ca) < u);
    END_CHAIN(ca, ta);
}

UTEST(chain_arena, reset_keeps_one_node)
{
    WITH_CHAIN(ca, ta);
    for (int i = 0; i < 20; i++) {
        ChainArena_alloc(&ca, 1000);
    }
    EXPECT_TRUE(ta.live_blocks > 1);
    ChainArena_reset(&ca);
    EXPECT_EQ(ta.live_blocks, 1u);
    EXPECT_EQ(ChainArena_used(&ca), 0u);
    EXPECT_TRUE((ChainArena_alloc(&ca, 100)) != NULL);
    END_CHAIN(ca, ta);
}

UTEST(chain_arena, clear_reuses_nodes_without_new_allocations)
{
    WITH_CHAIN(ca, ta);
    for (int i = 0; i < 20; i++) {
        ChainArena_alloc(&ca, 1000);
    }
    u64 nodes  = node_count(&ca);
    u64 allocs = ta.n_alloc;

    ChainArena_clear(&ca);
    for (int i = 0; i < 20; i++) {
        EXPECT_TRUE((ChainArena_alloc(&ca, 1000)) != NULL);
    }
    EXPECT_EQ(node_count(&ca), nodes);
    EXPECT_EQ(ta.n_alloc, allocs); // same workload, zero backing allocations
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

    CHAIN_ARENA_SCRATCH(&ca)
    {
        for (int i = 0; i < 30; i++) {
            ChainArena_alloc(&ca, 1000); // spans several nodes
        }
        EXPECT_TRUE(ta.live_blocks > blocks);
    }
    EXPECT_EQ(ta.live_blocks, blocks); // appended nodes released
    EXPECT_EQ(ChainArena_used(&ca), used);
    EXPECT_TRUE(ca.tail == ca.head);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(keep[i], 'K');
    }
    END_CHAIN(ca, ta);
}

#ifndef NDEBUG
static void grow_outer_block_inside_chain_scratch(void)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_LIBC);
    wc_allocator al = ChainArena_allocator(&ca);
    u8*          v  = wc_alloc(al, 64, 8);
    CHAIN_ARENA_SCRATCH(&ca)
    {
        v = wc_realloc(al, v, 64, 512, 8);
    }
    (void)v;
    ChainArena_destroy(&ca);
}

static void grow_block_from_earlier_node_inside_chain_scratch(void)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_LIBC);
    wc_allocator al = ChainArena_allocator(&ca);
    u8*          v  = wc_alloc(al, 64, 8);
    for (int i = 0; i < 10; i++) {
        ChainArena_alloc(&ca, 1000); // v's node is no longer the tail
    }
    CHAIN_ARENA_SCRATCH(&ca)
    {
        v = wc_realloc(al, v, 64, 512, 8);
    }
    (void)v;
    ChainArena_destroy(&ca);
}

#endif

UTEST(chain_arena, floor_grow_outer_block_inside_scratch)
{
#ifndef NDEBUG
    EXPECT_DIES(grow_outer_block_inside_chain_scratch);
    EXPECT_DIES(grow_block_from_earlier_node_inside_chain_scratch);
#else
    WITH_CHAIN(ca, ta);
    wc_allocator al = ChainArena_allocator(&ca);
    u8*          v  = wc_alloc(al, 64, 8);
    u64          u  = ChainArena_used(&ca);
    CHAIN_ARENA_SCRATCH(&ca)
    {
        EXPECT_TRUE(wc_realloc(al, v, 64, 512, 8) != v); // never in place past the mark
    }
    EXPECT_EQ(ChainArena_used(&ca), u);
    END_CHAIN(ca, ta);
#endif
}

UTEST(chain_arena, floor_free_inside_scratch_does_not_rewind)
{
    WITH_CHAIN(ca, ta);
    wc_allocator al    = ChainArena_allocator(&ca);
    u8*          outer = wc_alloc(al, 64, 8);
    u64          u     = ChainArena_used(&ca);
    CHAIN_ARENA_SCRATCH(&ca)
    {
        wc_free(al, outer, 64, 8);
        EXPECT_EQ(ChainArena_used(&ca), u);
        u8* t = wc_alloc(al, 16, 8);
        EXPECT_TRUE(wc_realloc(al, t, 16, 256, 8) == t); // above the floor: in place
    }
    EXPECT_EQ(ChainArena_used(&ca), u);
    END_CHAIN(ca, ta);
}

UTEST(chain_arena, floor_nested_scopes)
{
    WITH_CHAIN(ca, ta);
    ChainArena_alloc(&ca, 10);
    ChainArenaScratch s1 = ChainArena_scratch_begin(&ca);
    ChainArena_alloc(&ca, 3000);
    ChainArenaScratch s2 = ChainArena_scratch_begin(&ca);
    ChainArena_alloc(&ca, 3000); // new node
    ChainArena_scratch_end(s2);
    EXPECT_TRUE(ca.floor_node == s1.node);
    ChainArena_scratch_end(s1);
    EXPECT_TRUE((ca.floor_node) == NULL);
    EXPECT_EQ(ta.live_blocks, 1u);
    END_CHAIN(ca, ta);
}


// A2 (ChainArena): blocks start 8 bytes into a malloc'd node (after `u64 used`),
// so any request aligned above 8 is misaligned by construction.
UTEST(chain_arena, A2_chain_arena_aligns_address)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_LIBC);

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
    ChainArena_create(&ca, WC_LIBC);
    wc_allocator al = ChainArena_allocator(&ca);

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
