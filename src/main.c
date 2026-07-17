#include "common.h"
#define ARENA_NODE_INLINE_SIZE nKB(8)
#include "chain_arena.h"
#include <stdio.h>

// BUG: ARENA_NODE_INLINE_SIZE override not working


int main(void)
{
    ChainArena* arena = chain_arena_create();

    int* a = CHAIN_ARENA_ALLOC_N(arena, int, 1000);
    int* b = CHAIN_ARENA_ALLOC_N(arena, int, 1000);

    printf("arena->used: %lu\n", arena->used);
    printf("num nodes: %lu\n", arena->nodes.size);

    chain_arena_release(arena);
    return 0;
}
