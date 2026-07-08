#include "chain_arena.h"
#include "common.h"


#define ALIGN_UP(val, align)\
((align) == 0 ? (val) : (((val) + ((align) - 1)) & ~((align) - 1)))

// align value to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_UP_DEFAULT(val) ALIGN_UP((val), ARENA_DEFAULT_ALIGNMENT)

// Align a pointer to alignment boundary
// turn ptr to a u64 val to align, then turn to ptr again
#define ALIGN_PTR(ptr, align) ((u8*)ALIGN_UP((ptr), (align)))

// align a pointer to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_PTR_DEFAULT(ptr)\
ALIGN_PTR((ptr), ARENA_DEFAULT_ALIGNMENT)


#define ARENA_CURR_IDX_PTR(arena) ((arena)->base + (arena)->idx)
#define ARENA_PTR(arena, idx)     ((arena)->base + (idx))


ChainArena* chain_arena_create(void)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    CHECK_FATAL(!n, "node malloc failed");
    n->idx  = 0;
    n->next = NULL;

    ChainArena* arena = malloc(sizeof(ChainArena));
    CHECK_FATAL(!arena, "arena malloc failed");
    arena->head      = n;
    arena->curr      = n;
    arena->num_nodes = 1;

    return arena;
}

void chain_arena_release(ChainArena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    if (arena->num_nodes != 0) {
        ArenaNode* prev = NULL;
        ArenaNode* curr = arena->head;
        ArenaNode* final = arena->curr;
        do {
            prev = curr;
            curr = curr->next;
            free(prev);
        } while (curr != final);
    }

    free(arena);
}

static void append_node(ChainArena* arena)
{

}


