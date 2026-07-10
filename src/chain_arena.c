#include "chain_arena.h"


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
// Note: align MUST be power of 2 and >= 1
#define ALIGN_UP(val, align) \
    ((align) == 0 ? (val) : (((val) + ((align) - 1)) & ~((align) - 1)))

// align value to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_UP_DEFAULT(val) \
    ALIGN_UP((val), ARENA_DEFAULT_ALIGNMENT)

#define ARENA_NODE_PTR(node, idx) ((node)->base + (idx))

// given a global idx, find which node number contains it
// each node's used goes up to ARENA_NODE_INLINE_SIZE
// TODO: we do O(n) each time for the node ptr?
#define FIND_NODE(arena, idx) (idx % ARENA_NODE_INLINE_SIZE)


static inline void append_node(ChainArena* arena);



ChainArena* chain_arena_create(void)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    CHECK_FATAL(!n, "node malloc failed");
    n->used = 0;
    n->next = NULL;

    ChainArena* arena = malloc(sizeof(ChainArena));
    CHECK_FATAL(!arena, "arena malloc failed");
    arena->head      = n;
    arena->curr      = n;
    arena->idx       = 0;
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

u8* chain_arena_alloc(ChainArena* arena, u64 size)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    CHECK_FATAL(size > ARENA_NODE_INLINE_SIZE,
                "max possible alloc size is %lu", ARENA_NODE_INLINE_SIZE);
    // TODO: test this by allocating ARENA_NODE_INLINE_SIZE on a fresh node

    // Align the current index first
    u64 aligned_idx = ALIGN_UP_DEFAULT(arena->idx);
    u64 aligned_used = ALIGN_UP_DEFAULT(arena->curr->used);
    // We dont have enough space in current node!
    // make a new one, do allocation there
    if (ARENA_NODE_INLINE_SIZE - aligned_idx < size) {
        append_node(arena);
        // recurse with newly created block
        return chain_arena_alloc(arena, size);
    }

    u8* ptr = ARENA_NODE_PTR(arena->curr, aligned_idx);
    arena->curr->used += aligned_used + size;
    arena->idx = aligned_idx + size;


    return ptr;
}






// create a new node and append it to the chain
static inline void append_node(ChainArena* arena)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    CHECK_FATAL(!n, "node malloc failed");
    n->used = 0;
    n->next = NULL;

    arena->curr->next = n;
    arena->curr       = n;
    arena->num_nodes++;
}






