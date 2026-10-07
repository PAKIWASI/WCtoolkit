#include "chain_arena.h"
#include "common.h"
#include "gen_vector.h"
#include "priority_queue.h"
#include "wc_allocator.h"
#include <string.h>


#define FINAL_BYTE           (CARENA_INLINE_SIZE - 1)
#define IS_EXTERNAL_ALLOC(n) ((n)->inlined[FINAL_BYTE] == 0)

#define FREE_BLOCK_CUTOFF 16  // ignore blocks smaller than this for free list
#define WASTE_THRESHOLD   0.3 // how much wasted space is acceptable (allocation_size/free_block)


typedef struct {
    u8* ptr;
    u64 len;
} Block;

static inline int smaller_block_cmp(const void* a, const void* b, u64 size)
{
    (void)size;
    return memcmp(&((Block*)a)->len, &((Block*)b)->len, sizeof(((Block*)a)->len));
}

static inline ChainArenaNode* new_node(ChainArena* ca)
{
    ChainArenaNode* n = wc_alloc(ca->alloc, sizeof(ChainArenaNode), WC_MAX_ALIGN);
    FATAL_IF(!n, "node alloc failed");

    n->inlined[FINAL_BYTE] = 1; // inline is active
    n->off                 = 0;
    n->next                = NULL;

    if (ca->head == NULL) { // first allocation
        ca->head = n;
    } else {
        ca->tail->next = n;
        // add wasted space to free list
        // dont do anything if last node was externally alloced
        u64 wasted = FINAL_BYTE - ca->tail->off;
        if (!IS_EXTERNAL_ALLOC(ca->tail) && wasted > FREE_BLOCK_CUTOFF) {
            PriorityQueue_push(&ca->free_ranges, &(Block){
                                                     .ptr = ca->tail->inlined + ca->tail->off,
                                                     .len = CARENA_INLINE_SIZE - ca->tail->off,
                                                 });
        }
    }
    ca->tail = n;

    return n;
}



ChainArena ChainArena_create(const wc_allocator* alloc)
{
    ChainArena ca = {
        .head        = NULL,
        .tail        = NULL,
        .alloc       = alloc,
        .free_ranges = PriorityQueue_create(alloc, 8, sizeof(Block), NULL, smaller_block_cmp),
    };
    new_node(&ca);

    return ca;
}

void ChainArena_destroy(ChainArena* ca)
{
    PriorityQueue_destroy(&ca->free_ranges);

    ChainArenaNode* n = ca->head;
    do {
        if (WC_UNLIKELY(n->inlined[FINAL_BYTE] == 0)) { // external alloc is used
            wc_free(ca->alloc, n->ex, n->ex_cap, WC_MAX_ALIGN);
        }
        wc_free(ca->alloc, n, sizeof(ChainArenaNode), WC_MAX_ALIGN);
        n = n->next;
    } while (n != ca->tail);

    memset(ca, 0, sizeof(ChainArena));
}

void* ChainArena_alloc(ChainArena* ca, u64 size)
{
    WC_ASSERT(size != 0, "can't allocate 0 bytes");

    if (WC_UNLIKELY(size > FINAL_BYTE)) { // need to allocate externally
        ChainArenaNode* n      = new_node(ca);
        n->inlined[FINAL_BYTE] = 0; // use external
        n->ex                  = wc_alloc(ca->alloc, size, WC_MAX_ALIGN);
        n->ex_cap              = size;

        // a lot of space is wasted if we allocate externally
        // we can easily add that space into free list, minus the starting struct and the final byte
        PriorityQueue_push(&ca->free_ranges,
                           &(Block){
                               .ptr = ca->tail->inlined + sizeof(Block),      // struct {ex, ex_cap}
                               .len = CARENA_INLINE_SIZE - sizeof(Block) - 1, // sub the struct and the final byte
                           });
        return n->ex;
    }

    // TODO: check the free list
    // iterate down the tree from the leaves and find the block that will waste the least space
    // after finding the best fit, allocate and place the remaining back into the free list
    PriorityQueue* pq = &ca->free_ranges;
    for (u64 i = 0; i < pq->v.size; i++) {
        const Block* b = GenVec_get_ptr_unsafe(&pq->v, i);
        if ((double)size <= WASTE_THRESHOLD * (double)b->len) { // acceptable waste
            break;
        }
    }
}
