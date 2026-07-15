#include "chain_arena.h"
#include "common.h"
#include "gen_vector.h"
#include <string.h>


// Align `val` up to the next multiple of `align` (power of 2).
#define ALIGN_UP(val, align) \
    (((align) == 0) ? (val) : (((val) + ((align) - 1)) & ~((align) - 1)))

// Access the last node (assumes at least one node exists).
#define LAST_NODE(arena) (*(ArenaNode**)genVec_back(&(arena)->nodes))
#define REMAINING(node)  (ARENA_NODE_INLINE_SIZE - (node)->used)
#define NODE_PTR(node)   ((node)->base + (node)->used)

#define NODES_INIT_SIZE 10


static inline ArenaNode* append_node(ChainArena* arena);


// genVec operations for ArenaNode*
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

// Reset back to initial state: only the first node remains, empty.
void chain_arena_reset(ChainArena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    // Remove all nodes except the first one
    u64 total = genVec_size(&arena->nodes);
    if (total > 1) {
        genVec_remove_range(&arena->nodes, 1, total - 1);
    }

    // Reset the first node
    ArenaNode* first = *(ArenaNode**)genVec_get_ptr_mut(&arena->nodes, 0);
    first->used = 0;

    arena->idx  = 0;
    arena->used = 0;
}

// (chain_arena_clear is defined inline in the header)


// Scratch API

ChainArenaScratch chain_arena_scratch_begin(ChainArena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    u64 node_idx = genVec_size(&arena->nodes) - 1;
    ArenaNode* last = LAST_NODE(arena);

    return (ChainArenaScratch){
        .arena           = arena,
        .node_idx        = node_idx,
        .node_used_mark  = last->used,
        .arena_used_mark = arena->used
    };
}

void chain_arena_scratch_end(ChainArenaScratch scratch)
{
    if (!scratch.arena) { return; }
    ChainArena* a = scratch.arena;

    // 1. Restore the target node’s fill level
    (*(ArenaNode**)genVec_get_ptr_mut(&a->nodes, scratch.node_idx))
        ->used = scratch.node_used_mark;

    // 2. Remove all nodes that were appended after the saved node
    u64 total = genVec_size(&a->nodes);
    if (total > scratch.node_idx + 1) {
        genVec_remove_range(&a->nodes, scratch.node_idx + 1,
                            total - (scratch.node_idx + 1));
    }

    // 3. Restore the global counters
    a->used = scratch.arena_used_mark;
    a->idx  = scratch.node_idx * ARENA_NODE_INLINE_SIZE;

    scratch.arena = NULL;   // mark as consumed
}


// Private helpers

static inline ArenaNode* append_node(ChainArena* arena)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    CHECK_FATAL(!n, "node malloc failed");
    n->used = 0;

    genVec_push(&arena->nodes, castptr(n));

    // Advance the linear offset by one full node size
    arena->idx += ARENA_NODE_INLINE_SIZE;

    return n;
}


