#include "wc_speed_test.h"
#include "arena.h"

#define N 1000000

void arena_speed_suite(void)
{
    WC_BENCH_SUITE("Arena");

    WC_BENCH("alloc 1M x 8B from large arena", N, {
        Arena* a = arena_create((u64)N * 16);
        for (int i = 0; i < N; i++) { arena_alloc(a, 8); }
        arena_release(a);
    });

    WC_BENCH("alloc+clear cycle x1000 (4KB arena)", 1000, {
        Arena* a = arena_create(nKB(4));
        for (int i = 0; i < 1000; i++) {
            for (int j = 0; j < 100; j++) { arena_alloc(a, 32); }
            arena_clear(a);
        }
        arena_release(a);
    });

    WC_BENCH("scratch begin/end x100K", 100000, {
        Arena* a = arena_create(nKB(64));
        for (int i = 0; i < 100000; i++) {
            arena_scratch sc = arena_scratch_begin(a);
            arena_alloc(a, 64);
            arena_scratch_end(&sc);
        }
        arena_release(a);
    });

    WC_BENCH("mark/restore cycle x100K", 100000, {
        Arena* a = arena_create(nKB(64));
        for (int i = 0; i < 100000; i++) {
            u64 mark = arena_get_mark(a);
            arena_alloc(a, 64);
            arena_clear_mark(a, mark);
        }
        arena_release(a);
    });
}
