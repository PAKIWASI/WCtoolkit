#include "priority_queue.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_errno.h"

#include <stdio.h>
#include <string.h>


#define PARENT(i)     (((i) - 1) / 2)
#define LEFT_NODE(i)  ((2 * (i)) + 1)
#define RIGHT_NODE(i) ((2 * (i)) + 2)

#define DS(pq)           ((u64)(pq)->v.data_size)
#define AT(pq, i)        ((pq)->v.data + ((u64)(i) * DS(pq)))
#define HOLE(pq)         AT((pq), (pq)->v.size) // spare slot: the element being sifted waits here
#define BETTER(pq, a, b) ((pq)->cmp_fn((a), (b), DS(pq)) < 0)


static void pq_sift_up(PriorityQueue* pq, u64 idx);
static void pq_sift_down(PriorityQueue* pq, u64 idx);
static void pq_build_heap(PriorityQueue* pq);

// Keep capacity > size so HOLE(pq) is always a valid slot.
static inline void pq_ensure_hole(PriorityQueue* pq)
{
    if (WC_UNLIKELY(pq->v.size + 1 >= pq->v.capacity)) {
        u64 cap = pq->v.capacity < GENVEC_MIN_CAPACITY ? GENVEC_MIN_CAPACITY : pq->v.capacity + (pq->v.capacity / 2);
        if (cap < pq->v.size + 2) {
            cap = pq->v.size + 2;
        }
        GenVec_reserve(&pq->v, cap);
    }
}


PriorityQueue PriorityQueue_create(const wc_allocator* a, u64 n, u32 data_size, const wc_container_ops* ops,
                                   wc_compare_fn cmp_fn)
{
    return (PriorityQueue){.v = GenVec_create(a, n, data_size, ops), .cmp_fn = cmp_fn};
}

void PriorityQueue_destroy(PriorityQueue* pq)
{
    GenVec_destroy(&pq->v);
    pq->cmp_fn = NULL;
}

PriorityQueue PriorityQueue_from_vec(const wc_allocator* a, const GenVec* vec, wc_compare_fn cmp_fn)
{
    PriorityQueue pq = {.v = GenVec_copy(a, vec), .cmp_fn = cmp_fn};
    pq_build_heap(&pq);
    return pq;
}

PriorityQueue PriorityQueue_from_vec_move(GenVec* vec, wc_compare_fn cmp_fn)
{
    PriorityQueue pq = {.cmp_fn = cmp_fn};
    GenVec_move(&pq.v, vec);
    pq_build_heap(&pq);
    return pq;
}


const void* PriorityQueue_peek(const PriorityQueue* pq)
{
    WC_SET_RET(WC_ERR_EMPTY, pq->v.size == 0, NULL);
    return AT(pq, 0);
}

void PriorityQueue_push(PriorityQueue* pq, const void* data)
{
    pq_ensure_hole(pq);
    GenVec_push(&pq->v, data); // the one deep copy (B8)
    pq_sift_up(pq, pq->v.size - 1);
}

void PriorityQueue_push_move(PriorityQueue* pq, void* data)
{
    pq_ensure_hole(pq);
    GenVec_push_move(&pq->v, data);
    pq_sift_up(pq, pq->v.size - 1);
}

void PriorityQueue_pop(PriorityQueue* pq, void* popped)
{
    PriorityQueue_remove(pq, 0, popped);
}

void PriorityQueue_remove(PriorityQueue* pq, u64 idx, void* out)
{
    WC_SET_RET(WC_ERR_EMPTY, pq->v.size == 0, );
    WC_ASSERT(idx < pq->v.size, "idx out of range");

    // swap_pop: element idx moved out (or deleted), the last element fills idx
    GenVec_swap_pop(&pq->v, idx, out);

    if (idx < pq->v.size) {
        // the filler may need to go either way
        if (idx > 0 && BETTER(pq, AT(pq, idx), AT(pq, PARENT(idx)))) {
            pq_sift_up(pq, idx);
        } else {
            pq_sift_down(pq, idx);
        }
    }
}


static void pq_print_tree(const PriorityQueue* pq, u64 i, u32 depth, wc_print_fn print_fn)
{
    if (i >= pq->v.size) {
        return;
    }

    pq_print_tree(pq, RIGHT_NODE(i), depth + 1, print_fn); // right subtree above

    for (u32 d = 0; d < depth; d++) {
        printf("    "); // 4 spaces per level of depth
    }
    print_fn(AT(pq, i));
    putchar('\n');

    pq_print_tree(pq, LEFT_NODE(i), depth + 1, print_fn); // left subtree below
}

void PriorityQueue_print(const PriorityQueue* pq, wc_print_fn print_fn)
{
    putchar('\n');
    pq_print_tree(pq, 0, 0, print_fn);
    putchar('\n');
}


// Private Functions
// Every sift lifts the moving element into HOLE, shifts parents/children one
// level each (one memcpy per level), then drops the element into place.

static void pq_sift_up(PriorityQueue* pq, u64 idx)
{
    if (idx == 0 || !BETTER(pq, AT(pq, idx), AT(pq, PARENT(idx)))) {
        return; // already in place: no moves at all
    }

    u64 ds   = DS(pq);
    u8* hole = HOLE(pq);
    memcpy(hole, AT(pq, idx), ds);

    while (idx > 0 && BETTER(pq, hole, AT(pq, PARENT(idx)))) {
        memcpy(AT(pq, idx), AT(pq, PARENT(idx)), ds);
        idx = PARENT(idx);
    }
    memcpy(AT(pq, idx), hole, ds);
}

static void pq_sift_down(PriorityQueue* pq, u64 idx)
{
    u64 n    = pq->v.size;
    u64 ds   = DS(pq);
    u8* hole = HOLE(pq);
    memcpy(hole, AT(pq, idx), ds);

    for (;;) {
        u64 best = LEFT_NODE(idx);
        if (best >= n) {
            break;
        }
        u64 right = best + 1;
        if (right < n && BETTER(pq, AT(pq, right), AT(pq, best))) {
            best = right;
        }
        if (!BETTER(pq, AT(pq, best), hole)) {
            break;
        }
        memcpy(AT(pq, idx), AT(pq, best), ds);
        idx = best;
    }
    memcpy(AT(pq, idx), hole, ds);
}

static void pq_build_heap(PriorityQueue* pq)
{
    if (pq->v.size > 1) {
        pq_ensure_hole(pq);
        // sift every internal node down, bottom up: O(n). Leaves are valid heaps.
        for (u64 i = pq->v.size / 2; i-- > 0;) {
            pq_sift_down(pq, i);
        }
    }
}
