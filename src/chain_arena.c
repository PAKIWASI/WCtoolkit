#include "chain_arena.h"
#include <string.h>



/* Align `val` up to the next multiple of `align` (power of 2). */
#define ALIGN_UP(val, align) \
    (((align) == 0) ? (val) : (((val) + ((align) - 1)) & ~((align) - 1)))

/* Access the last node (assumes at least one node exists). */
#define LAST_NODE(arena) (*(ArenaNode**)genVec_back(&(arena)->nodes))
#define REMAINING(node)  (ARENA_NODE_INLINE_SIZE - (node)->used)
#define NODE_PTR(node)   ((node)->base + (node)->used)

#define NODES_INIT_SIZE      10


static inline ArenaNode* append_node(ChainArena* arena);

static void arena_restore_to_used_mark(ChainArena* arena, u64 used_mark);


//  genVec operations for ArenaNode*
static void chain_move(u8* dest, u8** src)
{
    *(ArenaNode**)dest = *(ArenaNode**)src;
    *(ArenaNode**)src  = NULL;
}

static void chain_del(u8* key)
{
    ArenaNode* node = *(ArenaNode**)key;
    free(node);
}

static container_ops chain_ops = {
    .copy_fn = NULL,
    .move_fn = chain_move,
    .del_fn  = chain_del
};


// Public API

ChainArena* chain_arena_create(void)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    CHECK_FATAL(!n, "node malloc failed");
    n->used = 0;

    ChainArena* arena = malloc(sizeof(ChainArena));
    CHECK_FATAL(!arena, "arena malloc failed");

    genVec_init_stk(NODES_INIT_SIZE, sizeof(ArenaNode*), &chain_ops, &arena->nodes);
    genVec_push(&arena->nodes, castptr(n));   // initial node

    arena->idx  = 0;   // first node at offset 0
    arena->used = 0;
    return arena;
}

void chain_arena_release(ChainArena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    genVec_destroy_stk(&arena->nodes);   // frees all nodes via chain_del
    free(arena);
}

u8* chain_arena_alloc_aligned(ChainArena* arena, u64 size, u32 align)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(size == 0, "allocation size must be > 0");
    CHECK_FATAL((align & (align - 1)) != 0, "alignment must be power of two");

    ArenaNode* last = LAST_NODE(arena);
    u64 aligned_idx = ALIGN_UP(last->used, align);
    u64 required = (aligned_idx - last->used) + size;

    if (required > REMAINING(last)) {
        last = append_node(arena);       // idx automatically bumps inside append_node
        aligned_idx = ALIGN_UP(0, align);
        required = (aligned_idx - 0) + size;
        CHECK_FATAL(required > ARENA_NODE_INLINE_SIZE,
                    "allocation size exceeds node capacity");
    }

    // Update the actual usage counters
    arena->used += required;
    last->used = aligned_idx;
    u8* ret = NODE_PTR(last);
    last->used += size;

    return ret;
}

void chain_arena_reset(ChainArena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    genVec_remove_range(&arena->nodes, 1, genVec_size(&arena->nodes)-1);

    (*(ArenaNode**)genVec_get_ptr_mut(&arena->nodes, 0))->used = 0;
    arena->idx  = 0;
    arena->used = 0;
}


inline void chain_arena_scratch_end(ChainArenaScratch scratch)
{
    if (scratch.arena) {
        arena_restore_to_used_mark(scratch.arena, scratch.mark);
        scratch.arena = NULL;
    }
}


// Private helpers

/*
 * Restore the arena to the state where `used` was exactly `used_mark`.
 * This walks the node list and sums `node->used` (actual bytes).  If the
 * total equals `used_mark`, we stop; otherwise we trim the last node.
 */
static void arena_restore_to_used_mark(ChainArena* arena, u64 used_mark)
{
    u64 accumulated = 0;
    u64 node_count  = genVec_size(&arena->nodes);

    for (u64 i = 0; i < node_count; i++) {
        ArenaNode* node = *(ArenaNode**)genVec_get_ptr_mut(&arena->nodes, i);
        u64 next = accumulated + node->used;

        if (next >= used_mark) {
            // Mark falls inside this node. Trim its `used`.
            node->used = used_mark - accumulated;

            // Remove all subsequent nodes.
            while (node_count > i + 1) {
                genVec_pop(&arena->nodes, NULL);  // frees node via chain_del
                node_count--;
            }

            arena->used = used_mark;
            arena->idx  = i * ARENA_NODE_INLINE_SIZE;  // adjust idx to match node offset
            return;
        }
        accumulated = next;
    }

    // If we leave the loop, used_mark == accumulated (exact end of last node).
    arena->used = used_mark;
    arena->idx  = (node_count - 1) * ARENA_NODE_INLINE_SIZE;
}

/*
 * Append a fresh node to the chain.
 * Also bumps `idx` by the full node size for O(1) lookup.
 */
static inline ArenaNode* append_node(ChainArena* arena)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    CHECK_FATAL(!n, "node malloc failed");
    n->used = 0;

    genVec_push(&arena->nodes, castptr(n));

    // Advance the linear offset.
    arena->idx += ARENA_NODE_INLINE_SIZE;

    return n;
}


