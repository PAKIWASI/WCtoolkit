#include "chain_arena.h"
#include "common.h"
#include "wc_allocator.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>


// Each block is ONE backing allocation: [ header | payload ]. The header is
// padded at the end to WC_MAX_ALIGN so the payload starts aligned like malloc memory
#define HDR ((u64)((sizeof(ChainArenaNodeHDR) + WC_MAX_ALIGN - 1) / WC_MAX_ALIGN * WC_MAX_ALIGN))
// go to the start of the payload
#define PAYLOAD(n) ((u8*)(n) + HDR)
// Total bytes of the backing allocation for a node: what wc_free needs to be told.
#define NODE_BYTES(n) (HDR + (n)->cap)
// At most, only 25% of a normal block is wasted. Any size bigger than BIG_THRESHOLD get's it's own allocation
#define BIG_THRESHOLD(cap) ((cap) / 4)


// Take one block from the backing allocator. NULL if it can't.
static ChainArenaNodeHDR* new_node(ChainArena* ca, u64 payload_cap)
{
    ChainArenaNodeHDR* n = wc_alloc(ca->alloc, HDR + payload_cap, WC_MAX_ALIGN);
    if (!n) {
        return NULL;
    }
    n->prev = NULL;
    n->cap  = payload_cap;
    n->off  = 0;
    return n;
}

// Carve `size` bytes, aligned by ADDRESS to `align`, out of one block. NULL if it doesn't fit
static void* node_bump(ChainArenaNodeHDR* n, u64 size, u64 align)
{
    u64       room = n->cap - n->off;
    uintptr_t cur  = (uintptr_t)PAYLOAD(n) + (uintptr_t)n->off;
    u64       pad  = (u64)((0 - cur) & (uintptr_t)(align - 1));

    if (pad > room || size > room - pad) {
        return NULL;
    }

    u8* p = PAYLOAD(n) + n->off + pad;
    n->off += pad + size;
    return p;
}

// walk back from `start` and delete until (but not including) `end` node. pass `end = NULL` to delete the whole chain
// check the return value to get the new latest node (matches `end` if reached, otherwise NULL if end reached first).
static ChainArenaNodeHDR* free_list(ChainArena* ca, ChainArenaNodeHDR* start, const ChainArenaNodeHDR* end)
{
    ChainArenaNodeHDR* prev;
    while (start && start != end) {
        prev = start->prev; // save prev before deallocating the current start
        wc_free(ca->alloc, start, NODE_BYTES(start), WC_MAX_ALIGN);
        start = prev;
    }
    return start;
}


// wc_allocator interface: alloc only. free is a no-op and realloc is emulated (alloc + copy).
static inline void* carena_alloc(void* ctx, size_t size, size_t align)
{
    return ChainArena_alloc_aligned((ChainArena*)ctx, size, align);
}

static const wc_alloc_vtable carena_vtable = {.alloc = carena_alloc, .realloc = NULL, .free = NULL};


void ChainArena_create(ChainArena* ca, const wc_allocator* backing, u64 normal_cap)
{
    ca->big   = NULL;
    ca->alloc = backing;
    // `self` has to be built here, in place, because its ctx is this arena's own address.
    ca->self = (wc_allocator){.vt = &carena_vtable, .ctx = ca};
    // need to set allocator before calling this
    ca->normal = new_node(ca, normal_cap);
    FATAL_IF(!ca->normal, "first node allocation failed (cap %lu)", normal_cap);
}

void ChainArena_destroy(ChainArena* ca)
{
    if (!ca->self.ctx) {
        return; // zeroed or already destroyed
    }
    free_list(ca, ca->normal, NULL);
    free_list(ca, ca->big, NULL);
    memset(ca, 0, sizeof(ChainArena)); // also clears self.ctx, so a stale allocator is detectable
}

void* ChainArena_alloc(ChainArena* ca, u64 size)
{
    return ChainArena_alloc_aligned(ca, size, WC_MAX_ALIGN);
}

void* ChainArena_alloc_aligned(ChainArena* ca, u64 size, u64 align)
{
    WC_ASSERT(ca->self.ctx != NULL, "ChainArena is in the zero state (moved-from or destroyed): re-create it first");
    WC_ASSERT(size != 0, "can't allocate 0 bytes");
    WC_ALLOC_ASSERT_ALIGN(align);
    WC_ASSERT(ca->self.ctx == ca, "chain arena was moved or copied after ChainArena_create");

    // Block memory is aligned to WC_MAX_ALIGN already. Only a bigger alignment needs room
    // to slide the pointer forward inside the payload.
    u64 slack = align > WC_MAX_ALIGN ? align : 0;
    // huge sizes can overflow
    if (slack > PTRDIFF_MAX - HDR || size > PTRDIFF_MAX - HDR - slack) {
        return NULL;
    }
    u64 need = size + slack; // actual allocation size

    ChainArenaNodeHDR* curr = ca->normal;

    // current node can satisfy the request
    if (need <= curr->cap - curr->off) {
        void* p = node_bump(curr, size, align);
        if (p) {
            return p;
        }
    }

    // a new node can satisfy the request. whatever is left in the old block is abandoned
    // we only go for a new node if `need` is greater than 25% of a normal block's capacity
    // so we only waste 25% of a block. any bigger allocation get's it's own `big` block
    if (need <= BIG_THRESHOLD(curr->cap)) {
        ChainArenaNodeHDR* n = new_node(ca, curr->cap); // each normal node saves the cap supplied by the user
        if (!n) {
            return NULL;
        }

        n->prev    = ca->normal;          // save the current latest normal node as the new node's prev
        ca->normal = n;                   // update the latest node
        return node_bump(n, size, align); // fits need <= node capacity
    }

    // any allocation needing more than BIG_THRESHOLD get's it's own block. It's sized to fit
    // The current regular block is untouched, so its remaining space is still available to later small requests.
    ChainArenaNodeHDR* n = new_node(ca, need);
    if (!n) {
        return NULL;
    }
    n->prev = ca->big;
    ca->big = n;
    return node_bump(n, size, align); // always fits: cap == size + slack
}

void ChainArenaScratch_to_mark(ChainArenaScratch scratch)
{
    // delete all newer nodes till (but not including) scratch.big. reset big.
    scratch.chain_arena->big = free_list(scratch.chain_arena, scratch.chain_arena->big, scratch.big);
    // delete all newer nodes till (but not including) scratch.normal. reset normal.
    scratch.chain_arena->normal = free_list(scratch.chain_arena, scratch.chain_arena->normal, scratch.normal);
    // set the mark
    scratch.chain_arena->normal->off = scratch.mark;
}
