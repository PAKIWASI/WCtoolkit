#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_comp.h"


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
wc_nodiscard wc_nonull(1, 5) PriorityQueue PriorityQueue_create(const wc_allocator* a, u64 n, u32 data_size, const wc_container_ops* ops,
                                                               wc_compare_fn cmp_fn);

wc_nonull(1) void PriorityQueue_destroy(PriorityQueue* pq);

// Heap from a deep COPY of vec (storage and elements from `a`). vec is unchanged.
wc_nodiscard wc_nonull(1, 2, 3) PriorityQueue PriorityQueue_from_vec(const wc_allocator* a, const GenVec* vec, wc_compare_fn cmp_fn);

// Heap that TAKES OVER vec's buffer: no copy, vec is left zeroed. The heap keeps
// vec's allocator. At most one realloc (for the sift slot) if vec is full.
wc_nodiscard wc_nonull(1, 2) PriorityQueue PriorityQueue_from_vec_move(GenVec* vec, wc_compare_fn cmp_fn);


// operations

// Highest priority element, without removing it. NULL (WC_ERR_EMPTY) if empty.
wc_nodiscard wc_nonull(1) const void* PriorityQueue_peek(const PriorityQueue* pq);

// Push a deep COPY of *data.
wc_nonull(1, 2) void PriorityQueue_push(PriorityQueue* pq, const void* data);

// Push *data by MOVE: *data is zeroed.
wc_nonull(1, 2) void PriorityQueue_push_move(PriorityQueue* pq, void* data);

// Remove the highest priority element: MOVED into popped, or deleted if NULL.
wc_nonull(1) void PriorityQueue_pop(PriorityQueue* pq, void* popped);

// Element at heap index idx (0 = top).
wc_nodiscard wc_nonull(1) static wc_always_inline const void* PriorityQueue_get(const PriorityQueue* pq, u64 idx)
{
    WC_ASSERT(pq->v.data_size != 0, "PriorityQueue is in the zero state (moved-from or destroyed): re-create it first");
    WC_ASSERT(idx < pq->v.size, "idx out of range");
    return GenVec_get_ptr_unsafe(&pq->v, idx);
}

// Remove the element at heap index idx: MOVED into out, or deleted if NULL.
wc_nonull(1) void PriorityQueue_remove(PriorityQueue* pq, u64 idx, void* out);

wc_nodiscard wc_nonull(1) static wc_always_inline u64 PriorityQueue_size(const PriorityQueue* pq)
{
    return pq->v.size;
}

wc_nodiscard wc_nonull(1) static wc_always_inline bool PriorityQueue_empty(const PriorityQueue* pq)
{
    return pq->v.size == 0;
}

wc_nonull(1, 2) void PriorityQueue_print(const PriorityQueue* pq, wc_print_fn print_fn);


#endif // PRIORITY_QUEUE_H
