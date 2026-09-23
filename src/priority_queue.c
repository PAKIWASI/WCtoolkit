#include "priority_queue.h"
#include "common.h"

#include "gen_vector.h"
#include <stdlib.h>


#define PARENT(i)     ((i - 1) / 2)
#define LEFT_NODE(i)  ((2 * i) + 1)
#define RIGHT_NODE(i) ((2 * i) + 2)
#define GET(pq, i)    (GenVec_get_ptr_mut_unsafe(&(pq)->arr, i))
#define CMP(pq, i, j) ((pq)->cmp_fn(GET(pq, idx), GET(pq, PARENT(idx)), pq->arr.data_size))

static void heapify_down(PriorityQueue* pq, u64 idx);
static void heapify_up(PriorityQueue* pq, u64 idx);



PriorityQueue* PriorityQueue_create(u64 n, u32 data_size, const wc_container_ops* ops, wc_compare_fn cmp_fn)
{
    PriorityQueue* pq = malloc(sizeof(PriorityQueue));
    CHECK_FATAL(!pq, "pq malloc failed");
    GenVec_create_stk(&pq->arr, n, data_size, ops);
    pq->cmp_fn = cmp_fn;
    return pq;
}

void PriorityQueue_destroy(PriorityQueue* pq)
{
    GenVec_destroy_stk(&pq->arr);
    free(pq);
}

void PriorityQueue_create_stk(PriorityQueue* pq, u64 n, u32 data_size, const wc_container_ops* ops,
                              wc_compare_fn cmp_fn)
{
    GenVec_create_stk(&pq->arr, n, data_size, ops);
    pq->cmp_fn = cmp_fn;
}

void PriorityQueue_destroy_stk(PriorityQueue* pq)
{
    GenVec_destroy_stk(&pq->arr);
}

/*
    [4, 2, 3, 1, 5]
    cmp = return a > b

->  []
*/
PriorityQueue* PriorityQueue_from_vec(GenVec* vec, wc_compare_fn cmp_fn)
{
    PriorityQueue* pq = malloc(sizeof(PriorityQueue));
    CHECK_FATAL(!pq, "pq malloc failed");
    GenVec_copy(&pq->arr, vec); // deep copy all elements
    pq->cmp_fn = cmp_fn;

    if (vec->size > 1) {
        // calling heapify_down on every internal node, from the bottom up. O(n)
        // last internal node is at size/2 - 1
        // Leaves (size/2 ... size-1) are already valid heaps
        for (u64 i = vec->size / 2; i --> 0;) {
            heapify_down(pq, i);
        }
    }

    return pq;
}


static void heapify_up(PriorityQueue* pq, u64 idx)
{
    while (idx > 0) {
        if (CMP(pq, idx, PARENT(idx)) <= 0) {
            break; // parent is already the best
        }
        GenVec_swap(&pq->arr, idx, PARENT(idx));
        idx = PARENT(idx); // we swapped, now check with it's parents
    }
}

static void heapify_down(PriorityQueue* pq, u64 idx)
{
    while (true) {
        u64 left  = LEFT_NODE(idx);
        u64 right = RIGHT_NODE(idx);
        u64 best  = idx;

        if (left < pq->arr.size && CMP(pq, left, best) < 0) {
            best = left;
        }
        if (right < pq->arr.size && CMP(pq, right, best) < 0) {
            best = right;
        }

        if (best == idx) {
            break;
        }

        GenVec_swap(&pq->arr, idx, best);
        idx = best;
    }
}
