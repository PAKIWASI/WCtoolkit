#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h> // before utest.h so its QueryPerformanceCounter stubs see _WINDOWS_
#endif

#include "arena.h"
#include "chain_arena.h"
#include "common.h"
#include "utest.h"
#include "wc_allocator.h"

#include <stdint.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static u64 test_page_size(void)
{
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    return (u64)sysconf(_SC_PAGESIZE);
#endif
}

UTEST(mmap, alloc_is_usable_zeroed_and_freed)
{
    u8* p = wc_alloc(WC_MMAP, 10000, 8);
    ASSERT_TRUE(p != NULL);
    EXPECT_EQ((uintptr_t)p % test_page_size(), 0u);
    for (int i = 0; i < 10000; i++) {
        ASSERT_EQ(p[i], 0); // anonymous pages are zero-filled
    }
    memset(p, 0xAB, 10000);
    wc_free(WC_MMAP, p, 10000, 8); // real munmap (ASan/UBSan would flag a bad length)
}

// was: returned MAP_FAILED ((void*)-1), which passes every `if (!p)` check
UTEST(mmap, failure_returns_null_not_map_failed)
{
    void* p = wc_alloc(WC_MMAP, (size_t)PTRDIFF_MAX, 8);
    EXPECT_TRUE(p == NULL);
}

// was: rounded the SIZE to the alignment, which never changed the address
UTEST(mmap, alignment_above_page_size_is_honoured)
{
    const size_t aligns[] = {4096, 8192, 65536, 1u << 20};
    for (size_t i = 0; i < sizeof(aligns) / sizeof(aligns[0]); i++) {
        u8* p = wc_alloc(WC_MMAP, 5000, aligns[i]);
        ASSERT_TRUE(p != NULL);
        EXPECT_EQ((uintptr_t)p % aligns[i], 0u);
        memset(p, 1, 5000);
        wc_free(WC_MMAP, p, 5000, aligns[i]);
    }
}

UTEST(mmap, realloc_is_emulated)
{
    u8* p = wc_alloc(WC_MMAP, 100, 8);
    ASSERT_TRUE(p != NULL);
    memset(p, 7, 100);
    u8* q = wc_realloc(WC_MMAP, p, 100, 20000, 8);
    ASSERT_TRUE(q != NULL);
    EXPECT_EQ(q[99], 7);
    wc_free(WC_MMAP, q, 20000, 8);
}

UTEST(mmap, arena_on_mmap_reserves_and_unmaps)
{
    Arena a;
    Arena_create(&a, WC_MMAP, nMB(64)); // 64 MB of address space, committed lazily
    u8* p = Arena_alloc(&a, nKB(4));
    ASSERT_TRUE(p != NULL);
    memset(p, 3, nKB(4));
    EXPECT_EQ(Arena_used(&a), nKB(4));
    Arena_destroy(&a);
    EXPECT_TRUE(a.base == NULL);
}

UTEST(mmap, chain_arena_on_mmap_frees_every_node)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_MMAP, nKB(4));
    for (int i = 0; i < 200; i++) {
        u8* p = ChainArena_alloc(&ca, 1000);
        ASSERT_TRUE(p != NULL);
        memset(p, i, 1000);
    }
    u8* big = ChainArena_alloc_aligned(&ca, nMB(1), 4096);
    ASSERT_TRUE(big != NULL);
    EXPECT_EQ((uintptr_t)big % 4096, 0u);
    memset(big, 1, nMB(1));
    ChainArena_destroy(&ca);
}
