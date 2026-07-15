#ifndef CHAIN_ARENA_H
#define CHAIN_ARENA_H

#include "gen_vector.h"

#ifndef ARENA_DEFAULT_ALIGNMENT
    #define ARENA_DEFAULT_ALIGNMENT (sizeof(void*))   // 8 bytes
#endif

#ifndef ARENA_NODE_INLINE_SIZE
    #define ARENA_NODE_INLINE_SIZE (nKB(4))           // 4096 bytes
#endif

typedef struct ArenaNode {
    u8  base[ARENA_NODE_INLINE_SIZE];
    u64 used;    // bytes consumed inside this node
} ArenaNode;

typedef struct {
    genVec nodes;   // vector of ArenaNode*
    u64 idx;        // linear bump offset: jumps by ARENA_NODE_INLINE_SIZE
                    // when a new node is appended.  Used for O(1) node lookup.
    u64 used;       // total bytes actually allocated (sum of node->used).
                    // This is the scratch save/restore mark.
} ChainArena;

typedef struct {
    ChainArena* arena;
    u64 mark;       // saved `used` value to restore to
} ChainArenaScratch;



ChainArena* chain_arena_create(void);

void chain_arena_release(ChainArena* arena);


u8* chain_arena_alloc_aligned(ChainArena* arena, u64 size, u32 align);

static inline u8* chain_arena_alloc(ChainArena* arena, u64 size)
{
    return chain_arena_alloc_aligned(arena, size, ARENA_DEFAULT_ALIGNMENT);
}


// reset back to initial state with a single free node
void chain_arena_reset(ChainArena* arena);

// clear all space but dont free any nodes
static inline void chain_arena_clear(ChainArena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    u64 node_count = genVec_size(&arena->nodes);
    for (u64 i = 0; i < node_count; i++) {
        ArenaNode* node = *(ArenaNode**)genVec_get_ptr_mut(&arena->nodes, i);
        node->used = 0;
    }
    arena->idx  = 0;
    arena->used = 0;
}


static inline ChainArenaScratch chain_arena_scratch_begin(ChainArena* arena) {
    CHECK_FATAL(!arena, "arena is null");
    return (ChainArenaScratch){ .arena = arena, .mark = arena->used };
}

inline void chain_arena_scratch_end(ChainArenaScratch scratch);

#define CHAIN_ARENA_SCRATCH(c_arena_ptr) \
    for (ChainArenaScratch __nme__ = chain_arena_scratch_begin(c_arena_ptr); \
         (__nme__).arena != NULL; \
         chain_arena_scratch_end((__nme__)), (__nme__).arena = NULL)

// Typed allocation macros
#define CHAIN_ARENA_ALLOC(arena, T)         ((T*)chain_arena_alloc((arena), sizeof(T)))
#define CHAIN_ARENA_ALLOC_N(arena, T, n)    ((T*)chain_arena_alloc((arena), sizeof(T) * (n)))
#define CHAIN_ARENA_ALLOC_ZERO(arena, T)    ((T*)memset(CHAIN_ARENA_ALLOC(arena, T), 0, sizeof(T)))
#define CHAIN_ARENA_ALLOC_ZERO_N(arena, T, n)\
    ((T*)memset(CHAIN_ARENA_ALLOC_N(arena, T, n), 0, sizeof(T) * (n)))

#endif // CHAIN_ARENA_H
