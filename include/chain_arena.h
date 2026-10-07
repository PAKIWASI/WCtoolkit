#ifndef CHAIN_ARENA_H
#define CHAIN_ARENA_H

#include "common.h"
#include "wc_allocator.h"


#ifndef CARENA_MIN_SIZE
#define CARENA_INLINE_SIZE (nKB(4)) // 4 KB
#endif


// each chain arena node has some inline cap
typedef struct ChainArenaNode {
    union {
        u8  inlined[CARENA_INLINE_SIZE]; // last byte set to zero means data been allocated elsewhere,
        u8* ex;                          // as the requested allocation was bigger than the inline size
    };
    u64                    off;  // how much of this node is used
    struct ChainArenaNode* next; // the next node in the chain
} ChainArenaNode;

typedef struct {
    ChainArenaNode*     head; // first node (needed for deallocatoin)
    ChainArenaNode*     tail; // the current node we are allocating from
    const wc_allocator* alloc;
} ChainArena;


[[nodiscard]] ChainArena ChainArena_create(const wc_allocator* alloc) WC_NONULL(1);

[[nodiscard]] void* ChainArena_alloc(ChainArena* ca, u64 size) WC_NONULL(1);

[[nodiscard]] void* ChainArena_alloc_aligned(ChainArena* ca, u64 size, u64 align) WC_NONULL(1);


#endif // CHAIN_ARENA_H
