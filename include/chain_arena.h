#ifndef CHAIN_ARENA_H
#define CHAIN_ARENA_H

#include "common.h"
#include "priority_queue.h"
#include "wc_allocator.h"


#ifndef CARENA_INLINE_SIZE
#define CARENA_INLINE_SIZE (nKB(4)) // 4 KB
#endif


// each chain arena node has some inline cap
// or an external buffer if a larger allocation is requested
typedef struct ChainArenaNode {
    union {
        u8 inlined[CARENA_INLINE_SIZE]; // last byte set to zero means data been allocated elsewhere,
        struct {
            u8* ex; // as the requested allocation was bigger than the inline size
            u64 ex_cap;
        };
    };
    u64                    off;  // how much of this node is used
    struct ChainArenaNode* next; // the next node in the chain
} ChainArenaNode;

typedef struct {
    ChainArenaNode*     head; // first node (needed for deallocatoin)
    ChainArenaNode*     tail; // the current node we are allocating from
    const wc_allocator* alloc;
    PriorityQueue       free_ranges; // priority queue for all the memory blocks that were freed.
    // blocks less than 16 bytes not considered
} ChainArena;

[[nodiscard]] ChainArena ChainArena_create(const wc_allocator* alloc) WC_NONULL(1);

void ChainArena_destroy(ChainArena* ca);

[[nodiscard]] void* ChainArena_alloc(ChainArena* ca, u64 size) WC_NONULL(1);

[[nodiscard]] void* ChainArena_alloc_aligned(ChainArena* ca, u64 size, u64 align) WC_NONULL(1);

void ChainArena_free(
    ChainArena* ca,
    u8* ptr); // TODO: use a hidden header to store {size, align} for each allocation before the returned pointer


#endif // CHAIN_ARENA_H
