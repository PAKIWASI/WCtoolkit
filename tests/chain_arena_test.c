#include "chain_arena.h"
#include "common.h"
#include "wc_allocator.h"
#include "wc_test.h"
#include "wc_test_allocator.h"
#include "wc_test_fatal.h"

#include <stdint.h>

void ChainArena_suite(void);


// Every test runs on a test allocator: node frees must match node allocs exactly.
#define WITH_CHAIN(ca, ta)                  \
    wc_test_alloc ta;                       \
    wc_test_alloc_init2(&ta, WC_LIBC);      \
    ChainArena ca;                          \
    ChainArena_create(&ca, wc_test_alloc_allocator2(&ta))

#define END_CHAIN(ca, ta)                                   \
    do {                                                    \
        ChainArena_destroy(&ca);                            \
        WC_ASSERT_EQ_U64(ta.n_errors, 0);                   \
        WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);    \
    } while (0)

static u64 node_count(const ChainArena* ca)
{
    u64 n = 0;
    for (const ChainArenaNode* x = ca->head; x; x = x->next) {
        n++;
    }
    return n;
}


/* ── Lifecycle ───────────────────────────────────────────────────────────── */

static void test_create_destroy_leak_free(void)
{
    WITH_CHAIN(ca, ta);
    WC_ASSERT_EQ_U64(ta.live_blocks, 1);
    WC_ASSERT(ca.head == ca.tail);
    WC_ASSERT_EQ_U64(ChainArena_used(&ca), 0);
    END_CHAIN(ca, ta);
}

static void test_destroy_zero_safe_and_idempotent(void)
{
    ChainArena z;
    memset(&z, 0, sizeof(z));
    ChainArena_destroy(&z);

    WITH_CHAIN(ca, ta);
    ChainArena_destroy(&ca);
    ChainArena_destroy(&ca); // second destroy: no-op
    WC_ASSERT_EQ_U64(wc_test_alloc_destroy(&ta), 0);
}

static void test_grows_across_nodes(void)
{
    WITH_CHAIN(ca, ta);
    u8* ptrs[64];
    for (int i = 0; i < 64; i++) {
        ptrs[i] = ChainArena_alloc(&ca, 500);
        WC_ASSERT_NOT_NULL(ptrs[i]);
        memset(ptrs[i], i, 500);
    }
    WC_ASSERT(node_count(&ca) > 1);
    for (int i = 0; i < 64; i++) {
        WC_ASSERT_EQ_INT(ptrs[i][0], i);
        WC_ASSERT_EQ_INT(ptrs[i][499], i);
    }
    END_CHAIN(ca, ta);
}


/* ── Alignment (A2) and oversize (A3) ────────────────────────────────────── */

static void test_alignment_by_address(void)
{
    WITH_CHAIN(ca, ta);
    const u64 aligns[] = {1, 2, 4, 8, 16, 32, 64, 256};
    for (int round = 0; round < 40; round++) { // crosses node boundaries
        for (u64 i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++) {
            ChainArena_alloc_aligned(&ca, 3, 1); // odd burn
            u8* p = ChainArena_alloc_aligned(&ca, 24, aligns[i]);
            WC_ASSERT_NOT_NULL(p);
            WC_ASSERT_EQ_U64((uintptr_t)p % aligns[i], 0);
        }
    }
    END_CHAIN(ca, ta);
}

static void test_oversize_request_gets_dedicated_node(void)
{
    WITH_CHAIN(ca, ta);
    u8* small = ChainArena_alloc(&ca, 16);
    u8* big   = ChainArena_alloc_aligned(&ca, nKB(10), 64); // > one node: was fatal (A3)
    WC_ASSERT_NOT_NULL(big);
    WC_ASSERT_EQ_U64((uintptr_t)big % 64, 0);
    memset(big, 0xAB, nKB(10));
    WC_ASSERT(ca.tail->cap >= nKB(10));

    u8* after = ChainArena_alloc(&ca, 32); // next small alloc still works
    WC_ASSERT_NOT_NULL(after);
    memset(after, 1, 32);
    WC_ASSERT_EQ_INT(big[nKB(10) - 1], 0xAB);
    (void)small;
    END_CHAIN(ca, ta);
}


/* ── Allocator backend ───────────────────────────────────────────────────── */

static void test_realloc_in_place_then_moves_across_node(void)
{
    WITH_CHAIN(ca, ta);
    wc_allocator al = ChainArena_allocator(&ca);

    u8* p = wc2_alloc(al, 64, 8);
    for (int i = 0; i < 64; i++) {
        p[i] = (u8)i;
    }
    u8* q = wc2_realloc(al, p, 64, 2000, 8);
    WC_ASSERT(q == p); // in place inside the tail node

    u8* r = wc2_realloc(al, q, 2000, nKB(8), 8); // cannot fit: moves to a new node
    WC_ASSERT(r != q);
    for (int i = 0; i < 64; i++) {
        WC_ASSERT_EQ_INT(r[i], i);
    }
    END_CHAIN(ca, ta);
}

static void test_free_rewinds_last_block(void)
{
    WITH_CHAIN(ca, ta);
    wc_allocator al = ChainArena_allocator(&ca);
    u8*          p1 = wc2_alloc(al, 32, 8);
    u8*          p2 = wc2_alloc(al, 32, 8);
    u64          u  = ChainArena_used(&ca);
    wc2_free(al, p1, 32, 8); // not on top
    WC_ASSERT_EQ_U64(ChainArena_used(&ca), u);
    wc2_free(al, p2, 32, 8);
    WC_ASSERT(ChainArena_used(&ca) < u);
    END_CHAIN(ca, ta);
}

static void test_reset_keeps_one_node(void)
{
    WITH_CHAIN(ca, ta);
    for (int i = 0; i < 20; i++) {
        ChainArena_alloc(&ca, 1000);
    }
    WC_ASSERT(ta.live_blocks > 1);
    ChainArena_reset(&ca);
    WC_ASSERT_EQ_U64(ta.live_blocks, 1);
    WC_ASSERT_EQ_U64(ChainArena_used(&ca), 0);
    WC_ASSERT_NOT_NULL(ChainArena_alloc(&ca, 100));
    END_CHAIN(ca, ta);
}

static void test_clear_reuses_nodes_without_new_allocations(void)
{
    WITH_CHAIN(ca, ta);
    for (int i = 0; i < 20; i++) {
        ChainArena_alloc(&ca, 1000);
    }
    u64 nodes  = node_count(&ca);
    u64 allocs = ta.n_alloc;

    ChainArena_clear(&ca);
    for (int i = 0; i < 20; i++) {
        WC_ASSERT_NOT_NULL(ChainArena_alloc(&ca, 1000));
    }
    WC_ASSERT_EQ_U64(node_count(&ca), nodes);
    WC_ASSERT_EQ_U64(ta.n_alloc, allocs); // same workload, zero backing allocations
    END_CHAIN(ca, ta);
}


/* ── Scratch and floor (D10) ─────────────────────────────────────────────── */

static void test_scratch_across_node_boundary_frees_new_nodes(void)
{
    WITH_CHAIN(ca, ta);
    u8* keep = ChainArena_alloc(&ca, 100);
    memset(keep, 'K', 100);
    u64 blocks = ta.live_blocks;
    u64 used   = ChainArena_used(&ca);

    CHAIN_ARENA_SCRATCH(&ca) {
        for (int i = 0; i < 30; i++) {
            ChainArena_alloc(&ca, 1000); // spans several nodes
        }
        WC_ASSERT(ta.live_blocks > blocks);
    }
    WC_ASSERT_EQ_U64(ta.live_blocks, blocks); // appended nodes released
    WC_ASSERT_EQ_U64(ChainArena_used(&ca), used);
    WC_ASSERT(ca.tail == ca.head);
    for (int i = 0; i < 100; i++) {
        WC_ASSERT_EQ_INT(keep[i], 'K');
    }
    END_CHAIN(ca, ta);
}

#ifndef NDEBUG
static void grow_outer_block_inside_chain_scratch(void)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_LIBC);
    wc_allocator al = ChainArena_allocator(&ca);
    u8*          v  = wc2_alloc(al, 64, 8);
    CHAIN_ARENA_SCRATCH(&ca) {
        v = wc2_realloc(al, v, 64, 512, 8);
    }
    (void)v;
    ChainArena_destroy(&ca);
}

static void grow_block_from_earlier_node_inside_chain_scratch(void)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_LIBC);
    wc_allocator al = ChainArena_allocator(&ca);
    u8*          v  = wc2_alloc(al, 64, 8);
    for (int i = 0; i < 10; i++) {
        ChainArena_alloc(&ca, 1000); // v's node is no longer the tail
    }
    CHAIN_ARENA_SCRATCH(&ca) {
        v = wc2_realloc(al, v, 64, 512, 8);
    }
    (void)v;
    ChainArena_destroy(&ca);
}

#endif

static void test_floor_grow_outer_block_inside_scratch(void)
{
#ifndef NDEBUG
    WC_ASSERT_DIES(grow_outer_block_inside_chain_scratch);
    WC_ASSERT_DIES(grow_block_from_earlier_node_inside_chain_scratch);
#else
    WITH_CHAIN(ca, ta);
    wc_allocator al = ChainArena_allocator(&ca);
    u8*          v  = wc2_alloc(al, 64, 8);
    u64          u  = ChainArena_used(&ca);
    CHAIN_ARENA_SCRATCH(&ca) {
        WC_ASSERT(wc2_realloc(al, v, 64, 512, 8) != v); // never in place past the mark
    }
    WC_ASSERT_EQ_U64(ChainArena_used(&ca), u);
    END_CHAIN(ca, ta);
#endif
}

static void test_floor_free_inside_scratch_does_not_rewind(void)
{
    WITH_CHAIN(ca, ta);
    wc_allocator al    = ChainArena_allocator(&ca);
    u8*          outer = wc2_alloc(al, 64, 8);
    u64          u     = ChainArena_used(&ca);
    CHAIN_ARENA_SCRATCH(&ca) {
        wc2_free(al, outer, 64, 8);
        WC_ASSERT_EQ_U64(ChainArena_used(&ca), u);
        u8* t = wc2_alloc(al, 16, 8);
        WC_ASSERT(wc2_realloc(al, t, 16, 256, 8) == t); // above the floor: in place
    }
    WC_ASSERT_EQ_U64(ChainArena_used(&ca), u);
    END_CHAIN(ca, ta);
}

static void test_floor_nested_scopes(void)
{
    WITH_CHAIN(ca, ta);
    ChainArena_alloc(&ca, 10);
    ChainArenaScratch s1 = ChainArena_scratch_begin(&ca);
    ChainArena_alloc(&ca, 3000);
    ChainArenaScratch s2 = ChainArena_scratch_begin(&ca);
    ChainArena_alloc(&ca, 3000); // new node
    ChainArena_scratch_end(s2);
    WC_ASSERT(ca.floor_node == s1.node);
    ChainArena_scratch_end(s1);
    WC_ASSERT_NULL(ca.floor_node);
    WC_ASSERT_EQ_U64(ta.live_blocks, 1);
    END_CHAIN(ca, ta);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void ChainArena_suite(void)
{
    WC_SUITE("ChainArena");

    WC_RUN(test_create_destroy_leak_free);
    WC_RUN(test_destroy_zero_safe_and_idempotent);
    WC_RUN(test_grows_across_nodes);
    WC_RUN(test_alignment_by_address);
    WC_RUN(test_oversize_request_gets_dedicated_node);
    WC_RUN(test_realloc_in_place_then_moves_across_node);
    WC_RUN(test_free_rewinds_last_block);
    WC_RUN(test_reset_keeps_one_node);
    WC_RUN(test_clear_reuses_nodes_without_new_allocations);

    WC_SUITE("ChainArena scratch floor (D10)");

    WC_RUN(test_scratch_across_node_boundary_frees_new_nodes);
    WC_RUN(test_floor_grow_outer_block_inside_scratch);
    WC_RUN(test_floor_free_inside_scratch_does_not_rewind);
    WC_RUN(test_floor_nested_scopes);
}
