#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"


/* Priority Queue
    A binary heap (a complete binary tree with the heap property) stored in a
    GenVec: every parent has higher priority than its children.
    cmp(a, b) < 0 means `a` has HIGHER priority (comes out first).

          1
        /   \
       3     5
      / \   /
     9   4 6

    index:  0  1  2  3  4  5
    value: [1, 3, 5, 9, 4, 6]

    for index i:
        parent      = (i - 1) / 2;
        left child  = 2 * i + 1;
        right child = 2 * i + 2;

    Sifting uses the HOLE technique: the moving element waits in the vector's
    spare slot (index size) and each level costs one element move, not a swap.
    The heap therefore keeps capacity > size at all times.

    Memory rules: pop / remove MOVE the element into `out`. Sifting is
    relocation, so it is memcpy only: no copy_fn, no allocation.
*/

typedef struct {
    GenVec        v; // heap order; slot v.size is the sift scratch slot
    wc_compare_fn cmp_fn;
} PriorityQueue;

_Static_assert(sizeof(PriorityQueue) == 56, "PriorityQueue size mismatch");


// Creation: returns by value; the allocator goes into pq->v.alloc
PriorityQueue PriorityQueue_create(const wc_allocator* a, u64 n, u32 data_size, const wc_container_ops* ops,
                                   wc_compare_fn cmp_fn) __attribute__((nonnull(1, 5), warn_unused_result));

void PriorityQueue_destroy(PriorityQueue* pq) __attribute__((nonnull(1)));

// Heap from a deep COPY of vec (storage and elements from `a`). vec is unchanged.
PriorityQueue PriorityQueue_from_vec(const wc_allocator* a, const GenVec* vec, wc_compare_fn cmp_fn)
    __attribute__((nonnull(1, 2, 3), warn_unused_result));

// Heap that TAKES OVER vec's buffer: no copy, vec is left zeroed. The heap keeps
// vec's allocator. At most one realloc (for the sift slot) if vec is full.
PriorityQueue PriorityQueue_from_vec_move(GenVec* vec, wc_compare_fn cmp_fn)
    __attribute__((nonnull(1, 2), warn_unused_result));


// operations

// Highest priority element, without removing it. NULL (WC_ERR_EMPTY) if empty.
const void* PriorityQueue_peek(const PriorityQueue* pq) __attribute__((nonnull(1)));

// Push a deep COPY of *data.
void PriorityQueue_push(PriorityQueue* pq, const void* data) __attribute__((nonnull(1, 2)));

// Push *data by MOVE: *data is zeroed.
void PriorityQueue_push_move(PriorityQueue* pq, void* data) __attribute__((nonnull(1, 2)));

// Remove the highest priority element: MOVED into popped, or deleted if NULL.
void PriorityQueue_pop(PriorityQueue* pq, void* popped) __attribute__((nonnull(1)));

// Element at heap index idx (0 = top).
static inline __attribute__((nonnull(1))) const void* PriorityQueue_get(const PriorityQueue* pq, u64 idx)
{
    WC_ASSERT(idx < pq->v.size, "idx out of range");
    return GenVec_get_ptr_unsafe(&pq->v, idx);
}

// Remove the element at heap index idx: MOVED into out, or deleted if NULL.
void PriorityQueue_remove(PriorityQueue* pq, u64 idx, void* out) __attribute__((nonnull(1)));

static inline __attribute__((nonnull(1))) u64 PriorityQueue_size(const PriorityQueue* pq)
{
    return pq->v.size;
}

static inline __attribute__((nonnull(1))) bool PriorityQueue_empty(const PriorityQueue* pq)
{
    return pq->v.size == 0;
}

void PriorityQueue_print(const PriorityQueue* pq, wc_print_fn print_fn) __attribute__((nonnull(1, 2)));


#endif // PRIORITY_QUEUE_H
