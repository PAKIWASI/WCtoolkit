#include "chain_arena.h"

int main(void)
{
    ChainArena* arena = chain_arena_create();

    chain_arena_release(arena);
    return 0;
}
