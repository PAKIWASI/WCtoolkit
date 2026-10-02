#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "common.h"
#include "gen_vector.h"
#include "queue.h"
#include "wc_allocator.h"

/* Priority Queue

    We implement it as a binary heap (a binary tree satisfing the heap property)
    Every parent has higher priority than its children (heap property)
    It is a complete tree: filled left to right, level by level
    Priority is based on a comparison function comparing 2 elements in the heap
    It is not sorted, it only garentees parents are better than the children
    We use a cicular queue as the container

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

*/

typedef struct {
    Queue         q; // circular queue
    wc_compare_fn cmp_fn;
} PriorityQueue;

_Static_assert(sizeof(PriorityQueue) == 88, "PriorityQueue size mismatch");


// Creation — returns by value; allocator goes into pq->q.arr.alloc
PriorityQueue PriorityQueue_create(wc_allocator a, u64 n, u32 data_size, const wc_container_ops* ops,
                                   wc_compare_fn cmp_fn) __attribute__((nonnull(5), warn_unused_result));
void          PriorityQueue_destroy(PriorityQueue* pq) __attribute__((nonnull(1)));

// Build heap from a GenVec; the copy uses allocator `a`
PriorityQueue PriorityQueue_from_vec(wc_allocator a, const GenVec* vec, wc_compare_fn cmp_fn)
    __attribute__((nonnull(2, 3), warn_unused_result));

// operations

// get the highest priority element without popping
static inline const void* __attribute__((nonnull(1))) PriorityQueue_peek(PriorityQueue* pq)
{
    return Queue_peek_ptr(&pq->q);
}

// push into the heap, heap property maintained
void PriorityQueue_push(PriorityQueue* pq, void* data) __attribute__((nonnull(1, 2)));

// pop the highest priority element from the heap, maintian the heap property
void PriorityQueue_pop(PriorityQueue* pq, void* popped) __attribute__((nonnull(1, 2)));

// get the element at the logical index `idx` of the tree, maintain heap property
static inline const void* __attribute__((nonnull(1))) PriorityQueue_get(PriorityQueue* pq, u64 idx)
{
    WC_ASSERT(idx < pq->q.size, "idx out of range");
    return Queue_get(&pq->q, idx);
}

// remove an element at logical index `idx` of the tree, maintain heap property. optional `out` param
void PriorityQueue_remove(PriorityQueue* pq, u64 idx, void* out) __attribute__((nonnull(1)));

static inline u64 __attribute__((nonnull(1))) PriorityQueue_size(PriorityQueue* pq)
{
    return pq->q.size;
}

void PriorityQueue_print(PriorityQueue* pq, wc_print_fn print_fn) __attribute__((nonnull(1, 2)));



#endif // PRIORITY_QUEUE_H
