#ifndef CHAIN_ARENA_H
#define CHAIN_ARENA_H

#include "common.h"
#include "gen_vector.h"


#ifndef ARENA_DEFAULT_ALIGNMENT
#define ARENA_DEFAULT_ALIGNMENT (sizeof(void*)) // 8 bytes
#endif

#ifndef ARENA_NODE_INLINE_SIZE
#define ARENA_NODE_INLINE_SIZE (nKB(4) - 8) // 4096 bytes
#endif

typedef struct ArenaNode {
    u64 used; // bytes consumed inside this node
    u8  base[ARENA_NODE_INLINE_SIZE];
} ArenaNode;

// NOT COPYABLE: backs a bump allocator; external pointers into it would be invalidated.
typedef struct {
    GenVec nodes; // vector of ArenaNode*
    u64    used;  // total bytes allocated (sum of all node->used).
                  // Used for scratch save/restore.
} ChainArena;

typedef struct {
    ChainArena* arena;
    u64         node_idx;        // index of the last node at scratch start
    u64         node_used_mark;  // that node's `used` value at scratch start
    u64         arena_used_mark; // arena->used at scratch start
} ChainArenaScratch;

// TODO: any node release strategies


ChainArena* chain_arena_create(void) __attribute__((warn_unused_result));

void chain_arena_destroy(ChainArena* arena) __attribute__((nonnull(1)));


u8* chain_arena_alloc_aligned(ChainArena* arena, u64 size, u32 align) __attribute__((nonnull(1), alloc_size(2)));

static inline __attribute__((nonnull(1), alloc_size(2))) u8* chain_arena_alloc(ChainArena* arena, u64 size)
{
    return chain_arena_alloc_aligned(arena, size, ARENA_DEFAULT_ALIGNMENT);
}


// reset back to initial state with a single free node
void chain_arena_reset(ChainArena* arena) __attribute__((nonnull(1)));

// clear all space but dont free any nodes
void chain_arena_clear(ChainArena* arena) __attribute__((nonnull(1)));


ChainArenaScratch chain_arena_scratch_begin(ChainArena* arena) __attribute__((nonnull(1)));

void chain_arena_scratch_end(ChainArenaScratch scratch);

#define CHAIN_ARENA_SCRATCH(c_arena_ptr)                                                              \
    for (ChainArenaScratch __nme__ = chain_arena_scratch_begin(c_arena_ptr); (__nme__).arena != NULL; \
         chain_arena_scratch_end((__nme__)), (__nme__).arena = NULL)


// Typed allocation macros
#define CHAIN_ARENA_ALLOC(arena, T)           ((T*)chain_arena_alloc((arena), sizeof(T)))
#define CHAIN_ARENA_ALLOC_N(arena, T, n)      ((T*)chain_arena_alloc((arena), sizeof(T) * (n)))
#define CHAIN_ARENA_ALLOC_ZERO(arena, T)      ((T*)memset(CHAIN_ARENA_ALLOC(arena, T), 0, sizeof(T)))
#define CHAIN_ARENA_ALLOC_ZERO_N(arena, T, n) ((T*)memset(CHAIN_ARENA_ALLOC_N(arena, T, n), 0, sizeof(T) * (n)))

#endif // CHAIN_ARENA_H
