#include "arena.h"
#include "bench_support.h"
#include "chain_arena.h"
#include "common.h"
#include "ubench.h"
#include "views.h"
#include "wc_allocator.h"
#include "wc_string.h"

#include <stdlib.h>
#include <string.h>

// N short strings stored four ways: malloc each, Arena, ChainArena, StringStore.
// And String building inline (SSO) vs on the heap.

static const char WORD[] = "key_00042";

// ubench.h registers each benchmark with realloc() inside the UBENCH macro
// NOLINTBEGIN(bugprone-suspicious-realloc-usage)
UBENCH(memory, malloc_each)
{
    char* ptrs[N];
    for (int i = 0; i < N; i++) {
        ptrs[i] = malloc(sizeof(WORD));
        memcpy(ptrs[i], WORD, sizeof(WORD));
    }
    UBENCH_DO_NOTHING(ptrs[N - 1]);
    for (int i = 0; i < N; i++) {
        free(ptrs[i]);
    }
}

UBENCH(memory, arena)
{
    Arena a;
    Arena_create(&a, WC_LIBC, (u64)N * 16);
    for (int i = 0; i < N; i++) {
        char* p = Arena_alloc(&a, sizeof(WORD));
        memcpy(p, WORD, sizeof(WORD));
    }
    UBENCH_DO_NOTHING(a.base);
    Arena_destroy(&a);
}

UBENCH(memory, chain_arena)
{
    ChainArena c;
    ChainArena_create(&c, WC_LIBC);
    for (int i = 0; i < N; i++) {
        char* p = ChainArena_alloc(&c, sizeof(WORD));
        memcpy(p, WORD, sizeof(WORD));
    }
    UBENCH_DO_NOTHING(&c);
    ChainArena_destroy(&c);
}

UBENCH(memory, string_store)
{
    StringStore ss = StringStore_create(WC_LIBC);
    for (int i = 0; i < N; i++) {
        (void)StringStore_cstr(&ss, WORD, sizeof(WORD) - 1);
    }
    UBENCH_DO_NOTHING(ss.head);
    StringStore_destroy(&ss);
}

UBENCH(memory, string_inline)
{
    for (int i = 0; i < N; i++) {
        String s = String_from_cstr(WC_LIBC, WORD); // fits in 31 chars: no allocation
        UBENCH_DO_NOTHING(&s);
        String_destroy(&s);
    }
}

UBENCH(memory, string_heap)
{
    for (int i = 0; i < N; i++) {
        String s = String_from_cstr(WC_LIBC, LONG_STR); // one allocation each
        UBENCH_DO_NOTHING(&s);
        String_destroy(&s);
    }
}

// NOLINTEND(bugprone-suspicious-realloc-usage)
