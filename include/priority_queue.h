#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "common.h"
#include "gen_vector.h"
#include "queue.h"

/* Priority Queue

    We implement it as a binary heap (a binary tree satisfing the heap property)
    Every parent has higher priority than its children (heap property)
    It is a complete tree: filled left to right, level by level
    Priority is based on a comparison function comparing 2 elements in the heap
    It is not sorted, it only garentees parents are better than the children
    We use vector as the container

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


    Main Operations:

    1. peek() — get highest priority
    Just return items[0].
    O(1)

    2. push() — insert
        Put the new item at the end of the array.
        Compare it with its parent.
        If it has higher priority, swap them.
        Keep moving it up until the heap property is restored.
    This is called sift up or bubble up.
    O(log n)

    3. pop() — remove highest priority
        Save items[0] — that’s the answer.
        Move the last item into items[0].
        Shrink the size by one.
        Compare the new root with its children.
        Swap it with the highest-priority child.
        Keep moving it down until the heap property is restored.
    This is called sift down or bubble down.
    O(log n)

    4. heapify_down/up() - (internal) maintains the heap property
    O(log n)

    5. build_heap() - create a new heap from a vector or array
    O(n)
*/

// TODO: test

typedef struct {
    Queue         q; // circular queue
    wc_compare_fn cmp_fn;
} PriorityQueue;


// Creation
PriorityQueue* PriorityQueue_create(u64 n, u32 data_size, const wc_container_ops* ops, wc_compare_fn cmp_fn)
    __attribute__((nonnull(4)));
void PriorityQueue_destroy(PriorityQueue* pq) __attribute__((nonnull(1)));

void PriorityQueue_create_stk(PriorityQueue* pq, u64 n, u32 data_size, const wc_container_ops* ops,
                              wc_compare_fn cmp_fn) __attribute__((nonnull(1, 5)));
void PriorityQueue_destroy_stk(PriorityQueue* pq) __attribute__((nonnull(1)));

// Build heap from other structures
PriorityQueue* PriorityQueue_from_vec(GenVec* vec, wc_compare_fn cmp_fn) __attribute__((nonnull(1, 2)));
void PriorityQueue_from_vec_stk(PriorityQueue* pq, GenVec* vec, wc_compare_fn cmp_fn) __attribute__((nonnull(1, 2, 3)));

// operations

static inline const u8* __attribute__((nonnull(1))) PriorityQueue_peek(PriorityQueue* pq)
{
    return Queue_peek_ptr(&pq->q);
}

void PriorityQueue_push(PriorityQueue* pq, u8* data) __attribute__((nonnull(1, 2)));

void PriorityQueue_pop(PriorityQueue* pq, u8* popped) __attribute__((nonnull(1, 2)));

void PriorityQueue_print(PriorityQueue* pq, wc_print_fn print_fn) __attribute__((nonnull(1, 2)));

#endif // PRIORITY_QUEUE_H
