#include "arena.h"


int main(void)
{
    Arena* arena = arena_create(0);

    ARENA_SCRATCH(arena) {
        int* n = ARENA_ALLOC_N(arena, int, 10);

        n[0] = 67;
    }

    printf("%lu\n", arena_remaining(arena));
    arena_release(arena);
    return 0;
}
