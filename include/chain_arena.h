#ifndef CHAIN_ARENA_H
#define CHAIN_ARENA_H

#include "common.h"
#include "wc_allocator.h"


/* ChainArena: an arena that never runs out.
 *
 * It is a linked list of blocks. Allocation is a bump pointer in the current block; when
 * the block is full, another one is taken from the backing allocator and becomes current.
 *
 * Requests that are large relative to a block get their own dedicated block, kept on a
 * separate list, so they never waste the rest of the current block.
 *
 * Like Arena, a ChainArena embeds its own wc_allocator (`self`) so containers can store a
 * `const wc_allocator*` that lives exactly as long as the arena. self.ctx is the arena's own
 * address, so it is PINNED.
 */


// the header for each block. the allocation region comes right the aligned header
typedef struct ChainArenaNodeHDR {
    struct ChainArenaNodeHDR* prev; // the prev node in the chain. NULL for last node
    u64                       cap;  // payload bytes
    u64                       off;  // payload bytes used
} ChainArenaNodeHDR;

typedef struct {
    ChainArenaNodeHDR* normal; // nodes with normal capacity. normal points to the newest node,
    // the oldest node (at the end of the chain) points to null
    ChainArenaNodeHDR*  big;   // dedicated blocks for oversized requests, newest first
    const wc_allocator* alloc; // backing allocator every block comes from and goes back to
    wc_allocator        self;  // what containers store; ctx == this ChainArena
} ChainArena;

// Initialises `ca` in place, it must not be moved or copied afterwards.
void ChainArena_create(ChainArena* ca, const wc_allocator* backing, u64 normal_cap) WC_NONULL(1, 2);

// Gives every block back to the backing allocator and zeroes the arena. Safe on a zeroed
// arena and safe to call twice.
void ChainArena_destroy(ChainArena* ca) WC_NONULL(1);

// ChainArena_alloc aligns to WC_MAX_ALIGN (like malloc)
// return NULL only if the backing allocator can't supply a block.
[[nodiscard]] void* ChainArena_alloc(ChainArena* ca, u64 size) WC_NONULL(1);

// ChainArena_alloc_aligned aligns the ADDRESS to `align` (a power of two), including alignments larger than a page
// return NULL only if the backing allocator can't supply a block.
[[nodiscard]] void* ChainArena_alloc_aligned(ChainArena* ca, u64 size, u64 align) WC_NONULL(1);

// The allocator to give to containers. Points into the arena, so it lives as long as the arena.
[[nodiscard]] static inline const wc_allocator* ChainArena_allocator(ChainArena* ca) WC_NONULL(1);

static inline const wc_allocator* ChainArena_allocator(ChainArena* ca)
{
    // zeroed self has vt == NULL, which would silently read as WC_LIBC
    FATAL_IF(!ca->self.ctx, "ChainArena_allocator on zeroed/destroyed ChainArena");
    return &ca->self;
}


typedef struct {
    ChainArena*        chain_arena; // the chain arena used
    ChainArenaNodeHDR* normal;      // the node that was tail when scratch was created
    ChainArenaNodeHDR* big;
    u64                mark; // the offset of node n;
} ChainArenaScratch;

[[nodiscard]] static inline ChainArenaScratch ChainArenaScratch_create(ChainArena* ca)
{
    return (ChainArenaScratch){
        .chain_arena = ca,
        .normal      = ca->normal,
        .big         = ca->big,
        .mark        = ca->normal->off,
    };
}

void ChainArenaScratch_to_mark(ChainArenaScratch scratch);



#endif // CHAIN_ARENA_H
