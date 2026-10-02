#include "queue.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_errno.h"

#include <stdio.h>
#include <string.h>


#define QUEUE_MIN_CAP   4
#define QUEUE_GROWTH    1.5f
#define QUEUE_SHRINK_AT 0.25f
#define QUEUE_SHRINK_BY 0.5f

#define REAL_IDX(q, i) ((i) % (q)->arr.capacity)


#define HEAD_UPDATE(q)                            \
    {                                             \
        (q)->head = REAL_IDX((q), (q)->head + 1); \
    }

#define TAIL_UPDATE(q)                                    \
    {                                                     \
        (q)->tail = REAL_IDX((q), (q)->head + (q)->size); \
    }

#define Q_MAYBE_GROW(q)                       \
    do {                                      \
        if ((q)->size == (q)->arr.capacity) { \
            Queue_grow((q));                  \
        }                                     \
    } while (0)

#define Q_MAYBE_SHRINK(q)                                       \
    do {                                                        \
        u64 capacity = (q)->arr.capacity;                       \
        if (capacity <= 4) {                                    \
            return;                                             \
        }                                                       \
        float load_factor = (float)(q)->size / (float)capacity; \
        if (load_factor < QUEUE_SHRINK_AT) {                    \
            Queue_shrink((q));                                  \
        }                                                       \
    } while (0)


static void Queue_grow(Queue* q);
static void Queue_shrink(Queue* q);
static void Queue_compact(Queue* q, u64 new_capacity);


Queue Queue_create(wc_allocator a, u64 n, u32 data_size, const wc_container_ops* ops)
{
    FATAL_IF(n == 0 || data_size == 0, "n/data_size can't be 0");

    Queue q;
    q.arr  = GenVec_create(a, n, data_size, ops);
    q.head = 0;
    q.tail = 0;
    q.size = 0;
    return q;
}

Queue Queue_create_val(wc_allocator a, u64 n, const u8* val, u32 data_size, const wc_container_ops* ops)
{
    FATAL_IF(n == 0 || data_size == 0, "n/data_size can't be 0");

    Queue q;
    q.arr  = GenVec_create_val(a, n, val, data_size, ops);
    q.head = 0;
    q.tail = n % GenVec_capacity(&q.arr);
    q.size = n;
    return q;
}


void Queue_destroy(Queue* q)
{
    GenVec_destroy(&q->arr);
    q->head = 0;
    q->tail = 0;
    q->size = 0;
}

void Queue_clear(Queue* q)
{
    GenVec_clear(&q->arr);
    q->size = 0;
    q->head = 0;
    q->tail = 0;
}

void Queue_reset(Queue* q)
{
    GenVec_reset(&q->arr);
    q->size = 0;
    q->head = 0;
    q->tail = 0;
}

void Queue_shrink_to_fit(Queue* q)
{
    if (q->size == 0) {
        Queue_reset(q);
        return;
    }

    u64 min_capacity     = q->size > QUEUE_MIN_CAP ? q->size : QUEUE_MIN_CAP;
    u64 current_capacity = GenVec_capacity(&q->arr);

    if (current_capacity > min_capacity) {
        Queue_compact(q, min_capacity);
    }
}

void Queue_push(Queue* q, const u8* x)
{
    Q_MAYBE_GROW(q);

    if (q->tail >= GenVec_size(&q->arr)) {
        GenVec_push(&q->arr, x);
    } else {
        GenVec_replace(&q->arr, q->tail, x);
    }

    q->size++;
    TAIL_UPDATE(q);
}

void Queue_push_move(Queue* q, u8* x)
{
    Q_MAYBE_GROW(q);

    if (q->tail >= GenVec_size(&q->arr)) {
        GenVec_push_move(&q->arr, x);
    } else {
        GenVec_replace_move(&q->arr, q->tail, x);
    }

    q->size++;
    TAIL_UPDATE(q);
}

void Queue_pop(Queue* q, u8* out)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );

    if (out) {
        GenVec_get(&q->arr, q->head, out);
    }

    // Clean up the element if del_fn exists
    wc_delete_fn del = VEC_DEL_FN(&q->arr);
    if (del) {
        u8* elem = (u8*)GenVec_get_ptr(&q->arr, q->head);
        del(elem);
        memset(elem, 0, q->arr.data_size);
    }

    HEAD_UPDATE(q);
    q->size--;
    Q_MAYBE_SHRINK(q);
}

void Queue_pop_back(Queue* q, u8* out)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );

    u64 last = REAL_IDX(q, q->head + q->size - 1);

    if (out) {
        GenVec_get(&q->arr, last, out);
    }

    // Clean up the element if del_fn exists
    wc_delete_fn del = VEC_DEL_FN(&q->arr);
    if (del) {
        u8* elem = (u8*)GenVec_get_ptr(&q->arr, last);
        del(elem);
    }

    q->size--;
    TAIL_UPDATE(q);
    Q_MAYBE_SHRINK(q);
}

void Queue_swap(Queue* q, u64 i, u64 j)
{
    CHECK_FATAL(i >= q->size || j >= q->size, "Queue_swap: index out of bounds");

    u64 real_i = REAL_IDX(q, q->head + i);
    u64 real_j = REAL_IDX(q, q->head + j);

    GenVec_swap(&q->arr, real_i, real_j);
}

void Queue_peek(Queue* q, u8* peek)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );
    GenVec_get(&q->arr, q->head, peek);
}

const u8* Queue_peek_ptr(const Queue* q)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, NULL);
    return GenVec_get_ptr(&q->arr, q->head);
}

void Queue_print(Queue* q, wc_print_fn print)
{
    printf("[ ");
    u64 h = q->head;
    for (u64 i = 0; i < q->size; i++, h = REAL_IDX(q, h + 1)) {
        const u8* out = GenVec_get_ptr_unsafe(&q->arr, h);
        print(out);
        putchar(' ');
    }
    putchar(']');
}


Queue Queue_copy(wc_allocator a, const Queue* src)
{
    Queue dest;
    dest.arr  = GenVec_copy(a, &src->arr);
    dest.head = src->head;
    dest.tail = src->tail;
    dest.size = src->size;
    return dest;
}

void Queue_move(Queue* dest, Queue* src)
{
    *dest = *src;
    memset(src, 0, sizeof(Queue));
}

static void Queue_grow(Queue* q)
{
    u64 old_cap = GenVec_capacity(&q->arr);
    u64 new_cap = (u64)((float)old_cap * QUEUE_GROWTH);
    if (new_cap <= old_cap) {
        new_cap = old_cap + 1;
    }

    Queue_compact(q, new_cap);
}

static void Queue_shrink(Queue* q)
{
    u64 current_cap = q->arr.capacity;
    u64 new_cap     = (u64)((float)current_cap * QUEUE_SHRINK_BY);

    u64 min_capacity = q->size > QUEUE_MIN_CAP ? q->size : QUEUE_MIN_CAP;
    if (new_cap < min_capacity) {
        new_cap = min_capacity;
    }

    if (new_cap < current_cap) {
        Queue_compact(q, new_cap);
    }
}

static void Queue_compact(Queue* q, u64 new_capacity)
{
    // Reuse the same allocator stored in arr
    GenVec new_arr = GenVec_create(q->arr.alloc, new_capacity, q->arr.data_size, q->arr.ops);

    u64 h = q->head;
    for (u64 i = 0; i < q->size; i++, h = REAL_IDX(q, h + 1)) {
        const u8* elm = GenVec_get_ptr(&q->arr, h);
        GenVec_push(&new_arr, elm);
    }

    GenVec_destroy(&q->arr);
    q->arr = new_arr;

    q->head = 0;
    q->tail = q->size % new_capacity;
}
