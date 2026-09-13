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
    ChainArena* Arena;
    u64         node_idx;        // index of the last node at scratch start
    u64         node_used_mark;  // that node's `used` value at scratch start
    u64         Arena_used_mark; // Arena->used at scratch start
} ChainArenaScratch;

// TODO: any node release strategies


ChainArena* chain_Arena_create(void) __attribute__((warn_unused_result));

void chain_Arena_destroy(ChainArena* Arena) __attribute__((nonnull(1)));


u8* chain_Arena_alloc_aligned(ChainArena* Arena, u64 size, u32 align) __attribute__((nonnull(1), alloc_size(2)));

static inline __attribute__((nonnull(1), alloc_size(2))) u8* chain_Arena_alloc(ChainArena* Arena, u64 size)
{
    return chain_Arena_alloc_aligned(Arena, size, ARENA_DEFAULT_ALIGNMENT);
}


// reset back to initial state with a single free node
void chain_Arena_reset(ChainArena* Arena) __attribute__((nonnull(1)));

// clear all space but dont free any nodes
void chain_Arena_clear(ChainArena* Arena) __attribute__((nonnull(1)));


ChainArenaScratch chain_Arena_scratch_begin(ChainArena* Arena) __attribute__((nonnull(1)));

void chain_Arena_scratch_end(ChainArenaScratch scratch);

#define CHAIN_ARENA_SCRATCH(c_Arena_ptr)                                                              \
    for (ChainArenaScratch __nme__ = chain_Arena_scratch_begin(c_Arena_ptr); (__nme__).Arena != NULL; \
         chain_Arena_scratch_end((__nme__)), (__nme__).Arena = NULL)


// Typed allocation macros
#define CHAIN_ARENA_ALLOC(Arena, T)           ((T*)chain_Arena_alloc((Arena), sizeof(T)))
#define CHAIN_ARENA_ALLOC_N(Arena, T, n)      ((T*)chain_Arena_alloc((Arena), sizeof(T) * (n)))
#define CHAIN_ARENA_ALLOC_ZERO(Arena, T)      ((T*)memset(CHAIN_ARENA_ALLOC(Arena, T), 0, sizeof(T)))
#define CHAIN_ARENA_ALLOC_ZERO_N(Arena, T, n) ((T*)memset(CHAIN_ARENA_ALLOC_N(Arena, T, n), 0, sizeof(T) * (n)))

#endif // CHAIN_ARENA_H
