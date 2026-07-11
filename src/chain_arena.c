#include "chain_arena.h"
#include "common.h"
#include "gen_vector.h"
#include <string.h>


/*'''python
align a 4 byte thing to 8 bytes alignment boundry:
>>> 4 + (8 - 1) & ~(8 - 1)
8
>>> 7 + (8 - 1) & ~(8 - 1)
8
>>> 9 + (8 - 1) & ~(8 - 1)
16 <- how much bytes should a 9 byte thing occupy to align to boundry
>>> 15 + (8 - 1) & ~(8 - 1)
16
>>> 18 + (8 - 1) & ~(8 - 1)
24
'''*/
// Align a value to alignment boundary
// Note: align MUST be power of 2 and >= 1. pow of 2 validated in caller
#define ALIGN_UP(val, align) \
    ((align) == 0 ? (val) : (((val) + ((align) - 1)) & ~((align) - 1)))

#define LAST_NODE(arena) (*(ArenaNode**)genVec_back(&arena->nodes))
#define REMAINING(node)  (ARENA_NODE_INLINE_SIZE - (node)->used)
#define NODE_PTR(node)   ((node)->base + (node)->used)


#define NODES_INIT_SIZE 10


static inline ArenaNode* append_node(ChainArena* arena);


// Ops for Genvec container
static void chain_move(u8* dest, u8** src) {
    *(ArenaNode**)dest = *(ArenaNode**)src;
    *(ArenaNode**)src  = NULL;
}

static void chain_del(u8* key) {
    free(*(ArenaNode**)key);
}

static container_ops chain_ops = {
    .copy_fn = NULL,
    .move_fn = chain_move,
    .del_fn  = chain_del
};


ChainArena* chain_arena_create(void)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    CHECK_FATAL(!n, "node malloc failed");
    n->used = 0;

    ChainArena* arena = malloc(sizeof(ChainArena));
    CHECK_FATAL(!arena, "arena malloc failed");
    genVec_init_stk(NODES_INIT_SIZE, sizeof(ArenaNode*), &chain_ops, &arena->nodes);

    return arena;
}

void chain_arena_release(ChainArena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    // destroyes each node using chain_del function but not itself
    genVec_destroy_stk(&arena->nodes);

    // free the arena struct, including the genVec struct itself
    free(arena);
}


// TODO: we have a global index in ChainArena, we should use that to determine
// if the next allocation happens in an already present last node or a new node is
// needed. We should also use it to implement chain_arena_clear

u8* chain_arena_alloc_aligned(ChainArena* arena, u64 size, u32 align)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    CHECK_FATAL((align & (align - 1)) != 0,
                "alignment must be power of two");

    ArenaNode* last = (REMAINING(LAST_NODE(arena)) < size) ?
                        append_node(arena) : LAST_NODE(arena);

    // align used to alignment boundry
    u64 aligned_idx = ALIGN_UP(last->used, align);
    // global idx incremented by size and the number of bytes taken by alignment
    arena->idx += (aligned_idx - last->used) + size;
    last->used = aligned_idx;   // used moves forward to allocation boundry
    u8* ret = NODE_PTR(last);   // get the ptr to START of alloc (old aligned node->used)
    last->used += size;         // increment used by size
    return ret;                 // ret ptr to start of allocation
}




static inline ArenaNode* append_node(ChainArena* arena)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    n->used = 0;
    genVec_push(&arena->nodes, castptr(n));
    return n;
}










