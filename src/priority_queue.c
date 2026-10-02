#include "priority_queue.h"
#include "common.h"
#include "gen_vector.h"
#include "queue.h"

#include <stdio.h>


#define PARENT(i)     ((((i)) - 1) / 2)
#define LEFT_NODE(i)  ((2 * (i)) + 1)
#define RIGHT_NODE(i) ((2 * (i)) + 2)

// CMP(pq, i, j) < 0  means element i has HIGHER priority than element j.
// i and j are LOGICAL heap indices; Queue_get() maps them through `head` to their real physical slot
#define CMP(pq, i, j) ((pq)->cmp_fn(Queue_get(&(pq)->q, i), Queue_get(&(pq)->q, j), (pq)->q.arr.data_size))


static void heapify_down(PriorityQueue* pq, u64 idx);
static void heapify_up(PriorityQueue* pq, u64 idx);
static void build_heap(PriorityQueue* pq);



PriorityQueue PriorityQueue_create(wc_allocator a, u64 n, u32 data_size, const wc_container_ops* ops,
                                   wc_compare_fn cmp_fn)
{
    PriorityQueue pq;
    pq.q      = Queue_create(a, n, data_size, ops);
    pq.cmp_fn = cmp_fn;
    return pq;
}

void PriorityQueue_destroy(PriorityQueue* pq)
{
    Queue_destroy(&pq->q);
    pq->cmp_fn = NULL;
}

PriorityQueue PriorityQueue_from_vec(wc_allocator a, const GenVec* vec, wc_compare_fn cmp_fn)
{
    PriorityQueue pq;

    pq.q.arr = GenVec_copy(a, vec); // deep copy all elements into allocator `a`

    // A freshly-copied GenVec has no wraparound yet, so head starts at 0 and
    // tail/size follow the same convention Queue_create_val uses.
    pq.q.head = 0;
    pq.q.tail = vec->size % GenVec_capacity(&pq.q.arr);
    pq.q.size = vec->size;

    pq.cmp_fn = cmp_fn;

    build_heap(&pq);

    return pq;
}

void PriorityQueue_push(PriorityQueue* pq, void* data)
{
    u64 off = Queue_size(&pq->q); // logical index the new element will land at
    Queue_push(&pq->q, data);
    heapify_up(pq, off);
}

void PriorityQueue_pop(PriorityQueue* pq, void* popped)
{
    WC_ASSERT(!Queue_empty(&pq->q), "queue is empty");

    // Standard heap-extract, adapted for circular storage
    // logical index 0 (== q.head) is the root we want to return.
    // swap it with the last logical element
    u64 last = pq->q.size - 1;
    Queue_swap(&pq->q, 0, last);

    // then pop that last slot (which now holds the old root) off the
    // BACK of the queue. This shrinks size by one without disturbing head
    // disturing the head will void the heap property, as the tree structure is
    // determined by the head index, which is the logical index 0
    Queue_pop_back(&pq->q, popped);

    // sift the new root down to restore the heap property.
    if (!Queue_empty(&pq->q)) {
        heapify_down(pq, 0);
    }
}

void PriorityQueue_remove(PriorityQueue* pq, u64 idx, void* out)
{
    WC_ASSERT(!Queue_empty(&pq->q), "queue is empty");
    WC_ASSERT(idx < pq->q.size, "idx out of range");

    // swap the element to remove with the last one
    Queue_swap(&pq->q, idx, pq->q.size - 1);
    // pop from the back to reduce the size and/or get the element
    Queue_pop_back(&pq->q, out);
    // run heapify down on the swapped element to put it in a valid position
    if (!Queue_empty(&pq->q)) {
        heapify_down(pq, idx);
    }
}

static inline void print_tree(Queue* q, u64 i, u32 depth, wc_print_fn print_fn)
{
    if (i >= q->size) {
        return;
    }

    print_tree(q, RIGHT_NODE(i), depth + 1, print_fn); // right subtree above

    for (u32 d = 0; d < depth; d++) {
        printf("    "); // 4 spaces per level of depth
    }
    print_fn(Queue_get(q, i));
    putchar('\n');

    print_tree(q, LEFT_NODE(i), depth + 1, print_fn); // left subtree below
}



void PriorityQueue_print(PriorityQueue* pq, wc_print_fn print_fn)
{
    putchar('\n');
    print_tree(&pq->q, 0, 0, print_fn);
    putchar('\n');
}


// Private Functions

static void heapify_up(PriorityQueue* pq, u64 idx)
{
    while (idx > 0) {
        if (CMP(pq, idx, PARENT(idx)) >= 0) {
            break; // idx does not have higher priority than its parent, done
        }

        Queue_swap(&pq->q, idx, PARENT(idx));
        idx = PARENT(idx); // we swapped, now check with it's parent
    }
}

static void heapify_down(PriorityQueue* pq, u64 idx)
{
    while (true) {
        u64 left  = LEFT_NODE(idx);
        u64 right = RIGHT_NODE(idx);
        u64 best  = idx;

        if (left < pq->q.size && CMP(pq, left, best) < 0) {
            best = left;
        }
        if (right < pq->q.size && CMP(pq, right, best) < 0) {
            best = right;
        }

        if (best == idx) {
            break;
        }

        Queue_swap(&pq->q, idx, best);
        idx = best;
    }
}

static void build_heap(PriorityQueue* pq)
{
    if (pq->q.size > 1) {
        // calling heapify_down on every internal node, from the bottom up. O(n)
        // last internal node is at size/2 - 1
        // Leaves (size/2 ... size-1) are already valid heaps
        for (u64 i = pq->q.size / 2; i-- > 0;) {
            heapify_down(pq, i);
        }
    }
}
