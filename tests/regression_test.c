#include "arena.h"
#include "chain_arena.h"
#include "common.h"
#include "gen_vector.h"
#include "hashmap.h"
#include "queue.h"
#include "random.h"
#include "wc_allocator.h"
#include "wc_macros.h"
#include "wc_test.h"

#include <stdalign.h>
#include <stdint.h>

void regression_suite(void);


/* ── Fixed defects (regression tests, written before the fix in Phase 0) ─── */

// A1: capacity check `size - aligned_idx < req` underflows when alignment
// pushes aligned_idx past `size`, so a full arena hands out a pointer past its end.
static void test_A1_arena_alloc_past_end_returns_null(void)
{
    alignas(16) u8 buf[13];
    Arena arena;
    Arena_create_buf(&arena, buf, sizeof(buf));

    WC_EXPECT_NOT_NULL(Arena_alloc(&arena, 1)); // idx 0 -> 1
    WC_EXPECT_NOT_NULL(Arena_alloc(&arena, 4)); // aligned to 8, idx -> 12

    u8* p = Arena_alloc(&arena, 1); // aligned_idx = 16 > size 13
    WC_EXPECT_NULL(p);
    WC_EXPECT(p == NULL || p < buf + sizeof(buf));
}

// A1 (aligned variant)
static void test_A1_arena_alloc_aligned_past_end_returns_null(void)
{
    alignas(16) u8 buf[20];
    Arena arena;
    Arena_create_buf(&arena, buf, sizeof(buf));

    WC_EXPECT_NOT_NULL(Arena_alloc_aligned(&arena, 17, 1)); // idx -> 17
    u8* p = Arena_alloc_aligned(&arena, 1, 32);              // aligned_idx = 32 > 20
    WC_EXPECT_NULL(p);
}

// A2: alignment is applied to the OFFSET, not the ADDRESS.
static void test_A2_arena_aligns_address_not_offset(void)
{
    alignas(64) u8 buf[256];
    Arena arena;
    Arena_create_buf(&arena, buf + 4, sizeof(buf) - 4); // base is 4 mod 16

    u8* p16 = Arena_alloc_aligned(&arena, 8, 16);
    u8* p64 = Arena_alloc_aligned(&arena, 8, 64);
    WC_EXPECT_NOT_NULL(p16);
    WC_EXPECT_NOT_NULL(p64);
    WC_EXPECT_EQ_U64((uintptr_t)p16 % 16, 0);
    WC_EXPECT_EQ_U64((uintptr_t)p64 % 64, 0);
}

// A2 (ChainArena): blocks start 8 bytes into a malloc'd node (after `u64 used`),
// so any request aligned above 8 is misaligned by construction.
static void test_A2_chain_arena_aligns_address(void)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_LIBC);

    u8* p16 = ChainArena_alloc_aligned(&ca, 8, 16);
    u8* p64 = ChainArena_alloc_aligned(&ca, 8, 64);
    WC_EXPECT_EQ_U64((uintptr_t)p16 % 16, 0);
    WC_EXPECT_EQ_U64((uintptr_t)p64 % 64, 0);

    ChainArena_destroy(&ca);
}

// A3: a request larger than one node used to be fatal, so a GenVec growing past
// 4 KB on a ChainArena aborted. Now it gets a dedicated node.
static void test_A3_chain_arena_oversize_request(void)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_LIBC);
    wc_allocator al = ChainArena_allocator(&ca);

    u8* p = NULL;
    u64 n = 0;
    for (u64 cap = 64; cap <= nKB(64); cap *= 2) { // GenVec-style doubling past a node
        p = wc_realloc(al, p, n, cap, 8);
        WC_EXPECT_NOT_NULL(p);
        memset(p + n, (int)(cap & 0xFF), cap - n);
        n = cap;
    }
    WC_EXPECT_EQ_INT(p[nKB(64) - 1], (int)(nKB(64) & 0xFF));
    ChainArena_destroy(&ca);
}

// A4: the arena allocator had no realloc, so every growth abandoned the old
// block (about half of a 1 KB arena was dead blocks in the main.c scenario).
static void test_A4_arena_grows_last_block_in_place(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    wc_allocator al = Arena_allocator(&a);

    u8* p = NULL;
    u64 n = 0;
    for (u64 cap = 8; cap <= 512; cap *= 2) {
        u8* q = wc_realloc(al, p, n, cap, 8);
        WC_EXPECT(p == NULL || q == p); // never moves: it is always the last block
        p = q;
        n = cap;
    }
    WC_EXPECT_EQ_U64(Arena_used(&a), 512); // no dead blocks
    Arena_destroy(&a);
}

// A5 is a compile-time defect (WC_REALLOC_N had the wrong arity). It cannot
// live in this binary; see tests/compile/a5_realloc_n.c and the
// `a5_realloc_n_compiles` ctest entry.


/* ── Golden scenarios (behaviour to preserve through the refactor) ───────── */

// Baseline history: 964 bytes before the refactor (Phase 0), 340 after
// in-place arena realloc (Phase 1), 292 after Phase 2 (no heap shell in the
// arena, 4-byte alignment for int: exactly 73 x 4 bytes, zero waste).
// The contents must stay identical and the arena must never use MORE than the
// current baseline; lower it when a change improves it.
#define GOLDEN_MAIN_ARENA_USED_BASELINE 292

// src/main.c scenario: 70 pushes of int into a vector whose storage is a 1 KB arena.
static void test_golden_main_70_push(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));

    GenVec v = VEC_OF_IN(Arena_allocator(&a), int, 5);
    for (int i = 0; i < 70; i++) {
        VEC_PUSH(&v, i);
    }

    WC_EXPECT_EQ_U64(GenVec_size(&v), 70);
    for (int i = 0; i < 70; i++) {
        WC_EXPECT_EQ_INT(*(const int*)GenVec_get_ptr(&v, (u64)i), i);
    }
    u64 used = Arena_used(&a);
    WC_EXPECT(used <= GOLDEN_MAIN_ARENA_USED_BASELINE);
    printf("[arena used %llu / baseline %d] ", (unsigned long long)used, GOLDEN_MAIN_ARENA_USED_BASELINE);

    GenVec_destroy(&v);
    WC_EXPECT_EQ_U64(Arena_used(&a), 0); // the vector was the only block: freed by rewinding
    Arena_destroy(&a);
}

// Same 70 pushes on libc: exact contents and final capacity.
#define GOLDEN_MAIN_LIBC_CAPACITY 73

static void test_golden_main_70_push_libc(void)
{
    GenVec v = VEC_OF(int, 5);
    for (int i = 0; i < 70; i++) {
        VEC_PUSH(&v, i);
    }
    WC_EXPECT_EQ_U64(GenVec_size(&v), 70);
    WC_EXPECT_EQ_U64(GenVec_capacity(&v), GOLDEN_MAIN_LIBC_CAPACITY);
    for (int i = 0; i < 70; i++) {
        WC_EXPECT_EQ_INT(*(const int*)GenVec_get_ptr(&v, (u64)i), i);
    }
    GenVec_destroy(&v);
}

static inline u64 golden_mix(u64 h, u64 x)
{
    h ^= x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

// Fixed-seed queue + hashmap workload. The checksums are order-sensitive for
// the queue (FIFO order is part of the contract) and order-independent for the
// map (bucket order is an implementation detail).
#define GOLDEN_QUEUE_CHECKSUM 0x94a3522e12f42c7ULL
#define GOLDEN_MAP_CHECKSUM   0xbd94ebf87ffabaa7ULL
#define GOLDEN_MAP_SIZE       2370ULL

static void test_golden_queue_hashmap_fixed_seed(void)
{
    pcg32_rand_seed(42, 54);

    // Queue: interleaved push/pop that wraps the circular buffer and resizes.
    Queue q     = Queue_create(WC_LIBC, 4, sizeof(u32), NULL);
    u64   q_sum = 0;
    for (int i = 0; i < 5000; i++) {
        u32 r = pcg32_rand();
        if ((r & 3) != 0 || Queue_size(&q) == 0) {
            Queue_push(&q, cast(r));
        } else {
            u32 out = 0;
            Queue_pop(&q, cast(out));
            q_sum = golden_mix(q_sum, out);
        }
    }
    while (Queue_size(&q) > 0) {
        u32 out = 0;
        Queue_pop(&q, cast(out));
        q_sum = golden_mix(q_sum, out);
    }
    Queue_destroy(&q);

    // HashMap: puts, overwrites and deletes over a small key space.
    HashMap m = HashMap_create(WC_LIBC, sizeof(u32), sizeof(u64), NULL, NULL, NULL, NULL);
    for (int i = 0; i < 20000; i++) {
        u32 k = pcg32_rand_bounded(3000);
        u64 v = pcg32_rand();
        if (pcg32_rand_bounded(5) == 0) {
            HashMap_del(&m, cast(k), NULL);
        } else {
            HashMap_put(&m, cast(k), cast(v));
        }
    }
    u64 m_sum = 0;
    for (u64 i = 0; i < HashMap_bucket_count(&m); i++) {
        if (HashMap_bucket_occupied(&m, i)) {
            u32 k = *(const u32*)HashMap_bucket_key_ptr(&m, i);
            u64 v = *(const u64*)HashMap_bucket_val_ptr(&m, i);
            m_sum += golden_mix(k, v); // sum: independent of bucket order
        }
    }
    u64 m_size = HashMap_size(&m);
    HashMap_destroy(&m);

    printf("[q=0x%llx m=0x%llx n=%llu] ", (unsigned long long)q_sum, (unsigned long long)m_sum,
           (unsigned long long)m_size);
    WC_EXPECT_EQ_U64(q_sum, GOLDEN_QUEUE_CHECKSUM);
    WC_EXPECT_EQ_U64(m_sum, GOLDEN_MAP_CHECKSUM);
    WC_EXPECT_EQ_U64(m_size, GOLDEN_MAP_SIZE);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void regression_suite(void)
{
    WC_SUITE("Fixed defects A1-A4");

    WC_RUN(test_A1_arena_alloc_past_end_returns_null);
    WC_RUN(test_A1_arena_alloc_aligned_past_end_returns_null);
    WC_RUN(test_A2_arena_aligns_address_not_offset);
    WC_RUN(test_A2_chain_arena_aligns_address);
    WC_RUN(test_A3_chain_arena_oversize_request);
    WC_RUN(test_A4_arena_grows_last_block_in_place);

    WC_SUITE("Golden scenarios");

    WC_RUN(test_golden_main_70_push);
    WC_RUN(test_golden_main_70_push_libc);
    WC_RUN(test_golden_queue_hashmap_fixed_seed);
}
