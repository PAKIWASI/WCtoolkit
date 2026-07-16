#include "chain_arena.h"


int main(void)
{
    ChainArena* arena = chain_arena_create();


    int* a = CHAIN_ARENA_ALLOC_N(arena, int, 1024);
    a[0] = 1;

    CHAIN_ARENA_SCRATCH(arena) {
        int* b = CHAIN_ARENA_ALLOC_N(arena, int, 1024);
        b[0] = 1;
    }

    printf("used: %lu\n", arena->used);
    printf("num nodes: %lu\n", arena->nodes.size);
    chain_arena_release(arena);
    return 0;
}
