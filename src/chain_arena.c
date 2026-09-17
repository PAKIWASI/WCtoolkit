#include "common.h"
#include "chain_arena.h"
#include "gen_vector.h"
#include <stdlib.h>


// Align `val` up to the next multiple of `align` (power of 2).
#define ALIGN_UP(val, align) \
    (((align) == 0) ? (val) : (((val) + ((align) - 1)) & ~((align) - 1)))

// Access the last node (assumes at least one node exists).
#define LAST_NODE(Arena) (*(ArenaNode**)GenVec_back(&(Arena)->nodes))
#define REMAINING(node)  (ARENA_NODE_INLINE_SIZE - (node)->used)
#define NODE_PTR(node)   ((node)->base + (node)->used)

#define NODES_INIT_SIZE 10


static inline ArenaNode* append_node(ChainArena* Arena);


// GenVec operations for ArenaNode*
static void chain_move(u8* dest, u8** src)
{
    *(ArenaNode**)dest = *(ArenaNode**)src;
    *(ArenaNode**)src  = NULL;
}

static void chain_del(u8* key)
{
    free(*(ArenaNode**)key);
}

static wc_container_ops chain_ops_ptr = {
    .copy_fn = NULL,
    .move_fn = chain_move,
    .del_fn  = chain_del
};


// Public API

ChainArena* chain_Arena_create(void)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    CHECK_FATAL(!n, "node malloc failed");
    n->used = 0;

    ChainArena* Arena = malloc(sizeof(ChainArena));
    CHECK_FATAL(!Arena, "Arena malloc failed");

    GenVec_create_stk(&Arena->nodes, NODES_INIT_SIZE, sizeof(ArenaNode*), &chain_ops_ptr);
    GenVec_push_move(&Arena->nodes, (u8**)&n);   // initial node

    Arena->used = 0;
    return Arena;
}

void chain_Arena_destroy(ChainArena* Arena)
{
    GenVec_destroy_stk(&Arena->nodes);   // frees all nodes via chain_del
    free(Arena);
}

u8* chain_Arena_alloc_aligned(ChainArena* Arena, u64 size, u32 align)
{
    CHECK_FATAL(size == 0, "allocation size must be > 0");
    CHECK_FATAL((align & (align - 1)) != 0, "alignment must be power of two");

    ArenaNode* last = LAST_NODE(Arena);

    u64 aligned_idx = ALIGN_UP(last->used, align);
    u64 required = (aligned_idx - last->used) + size;

    if (required > REMAINING(last)) {
        last = append_node(Arena);       // idx automatically bumps inside append_node
        aligned_idx = ALIGN_UP(0, align);
        required = (aligned_idx - 0) + size;
        CHECK_FATAL(required > ARENA_NODE_INLINE_SIZE,
                    "allocation size exceeds node capacity");
    }

    // Update the actual usage counters
    Arena->used += required;
    last->used = aligned_idx;
    u8* ret = NODE_PTR(last);
    last->used += size;

    return ret;
}

// Reset back to initial state: only the first node remains, empty.
void chain_Arena_reset(ChainArena* Arena)
{
    // Remove all nodes except the first one
    u64 total = GenVec_size(&Arena->nodes);
    if (total > 1) {
        GenVec_remove_range(&Arena->nodes, 1, total - 1);
    }

    // Reset the first node
    (*(ArenaNode**)GenVec_get_ptr_mut(&Arena->nodes, 0))->used = 0;

    Arena->used = 0;
}

// clear all space but dont free any nodes
void chain_Arena_clear(ChainArena* Arena)
{
    u64 node_count = GenVec_size(&Arena->nodes);
    for (u64 i = 0; i < node_count; i++) {
        (*(ArenaNode**)GenVec_get_ptr_mut(&Arena->nodes, i))->used = 0;
    }
    Arena->used = 0;
}


// Scratch API

ChainArenaScratch chain_Arena_scratch_begin(ChainArena* Arena)
{
    u64 node_idx = GenVec_size(&Arena->nodes) - 1;
    ArenaNode* last = LAST_NODE(Arena);

    return (ChainArenaScratch){
        .Arena           = Arena,
        .node_idx        = node_idx,
        .node_used_mark  = last->used,
        .Arena_used_mark = Arena->used
    };
}

void chain_Arena_scratch_end(ChainArenaScratch scratch)
{
    if (!scratch.Arena) { return; }
    ChainArena* a = scratch.Arena;

    // 1. Restore the target node’s fill level
    (*(ArenaNode**)GenVec_get_ptr_mut(&a->nodes, scratch.node_idx))
        ->used = scratch.node_used_mark;

    // 2. Remove all nodes that were appended after the saved node
    u64 total = GenVec_size(&a->nodes);
    if (total > scratch.node_idx + 1) {
        GenVec_remove_range(&a->nodes, scratch.node_idx + 1,
                            total - (scratch.node_idx + 1));
    }

    // 3. Restore the global counters
    a->used = scratch.Arena_used_mark;

    scratch.Arena = NULL;   // mark as consumed
}


// Private helpers

static inline ArenaNode* append_node(ChainArena* Arena)
{
    ArenaNode* n = malloc(sizeof(ArenaNode));
    CHECK_FATAL(!n, "node malloc failed");
    n->used = 0;

    ArenaNode* ret = n;
    GenVec_push_move(&Arena->nodes, (u8**)&n);  // push move consumes n

    return ret;
}


