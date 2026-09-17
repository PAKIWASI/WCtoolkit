#include "queue.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_errno.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define QUEUE_MIN_CAP   4
#define QUEUE_GROWTH    1.5f
#define QUEUE_SHRINK_AT 0.25f
#define QUEUE_SHRINK_BY 0.5f


#define HEAD_UPDATE(q)                                    \
    {                                                     \
        (q)->head = ((q)->head + 1) % (q)->arr->capacity; \
    }

#define TAIL_UPDATE(q)                                              \
    {                                                               \
        (q)->tail = (((q)->head + (q)->size) % (q)->arr->capacity); \
    }

#define Q_MAYBE_GROW(q)                        \
    do {                                       \
        if ((q)->size == (q)->arr->capacity) { \
            Queue_grow((q));                   \
        }                                      \
    } while (0)

#define Q_MAYBE_SHRINK(q)                                       \
    do {                                                        \
        u64 capacity = (q)->arr->capacity;                      \
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


Queue* Queue_create(u64 n, u32 data_size, const wc_container_ops* ops)
{
    CHECK_FATAL(n == 0 || data_size == 0, "n/data_size can't be 0");

    Queue* q = malloc(sizeof(Queue));
    CHECK_FATAL(!q, "Queue malloc failed");

    q->arr = GenVec_create(n, data_size, ops);

    q->head = 0;
    q->tail = 0;
    q->size = 0;

    return q;
}

Queue* Queue_create_val(u64 n, const u8* val, u32 data_size, const wc_container_ops* ops)
{
    CHECK_FATAL(n == 0 || data_size == 0, "n/data_size can't be 0");

    Queue* q = malloc(sizeof(Queue));
    CHECK_FATAL(!q, "Queue malloc failed");

    q->arr = GenVec_create_val(n, val, data_size, ops);

    q->head = 0;
    q->tail = n % GenVec_capacity(q->arr);
    q->size = n;

    return q;
}


void Queue_create_stk(Queue* q, u64 n, u32 data_size, const wc_container_ops* ops)
{
    CHECK_FATAL(n == 0 || data_size == 0, "n/data_size can't be 0");

    q->arr = GenVec_create(n, data_size, ops);

    q->head = 0;
    q->tail = 0;
    q->size = 0;
}

void Queue_destroy(Queue* q)
{
    GenVec_destroy(q->arr);
    free(q);
}

void Queue_destroy_stk(Queue* q)
{
    GenVec_destroy(q->arr);
}

void Queue_clear(Queue* q)
{
    GenVec_clear(q->arr);
    q->size = 0;
    q->head = 0;
    q->tail = 0;
}

void Queue_reset(Queue* q)
{
    GenVec_reset(q->arr);
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
    u64 current_capacity = GenVec_capacity(q->arr);

    if (current_capacity > min_capacity) {
        Queue_compact(q, min_capacity);
    }
}

void Queue_push(Queue* q, const u8* x)
{
    Q_MAYBE_GROW(q);

    if (q->tail >= GenVec_size(q->arr)) {
        GenVec_push(q->arr, x);
    } else {
        GenVec_replace(q->arr, q->tail, x);
    }

    q->size++;
    TAIL_UPDATE(q);
}

void Queue_push_move(Queue* q, u8** x)
{
    CHECK_FATAL(!*x, "*x is null");

    Q_MAYBE_GROW(q);

    if (q->tail >= GenVec_size(q->arr)) {
        GenVec_push_move(q->arr, x);
    } else {
        GenVec_replace_move(q->arr, q->tail, x);
    }

    q->size++;
    TAIL_UPDATE(q);
}

void Queue_pop(Queue* q, u8* out)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );

    if (out) {
        GenVec_get(q->arr, q->head, out);
    }

    // Clean up the element if del_fn exists
    delete_fn del = VEC_DEL_FN(q->arr);
    if (del) {
        u8* elem = (u8*)GenVec_get_ptr(q->arr, q->head);
        del(elem);
        memset(elem, 0, q->arr->data_size);
    }

    HEAD_UPDATE(q);
    q->size--;
    Q_MAYBE_SHRINK(q);
}

void Queue_peek(Queue* q, u8* peek)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );

    GenVec_get(q->arr, q->head, peek);
}

const u8* Queue_peek_ptr(const Queue* q)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, NULL);

    return GenVec_get_ptr(q->arr, q->head);
}

void Queue_print(Queue* q, print_fn print)
{
    u64 h   = q->head;
    u64 cap = GenVec_capacity(q->arr);

    printf("[ ");
    if (q->size != 0) {
        for (u64 i = 0; i < q->size; i++) {
            const u8* out = GenVec_get_ptr(q->arr, h);
            print(out);
            putchar(' ');
            h = (h + 1) % cap;
        }
    }
    putchar(']');
}


void Queue_copy(Queue* dest, const Queue* src)
{
    GenVec_copy(dest->arr, src->arr);
    dest->head = src->head;
    dest->tail = src->tail;
    dest->size = src->size;
}

void Queue_move(Queue* dest, Queue** src)
{
    if (dest == *src) {
        *src = NULL;
        return;
    }

    memcpy(dest, *src, sizeof(Queue));
    free(*src);
    *src = NULL;
}
static void Queue_grow(Queue* q)
{
    u64 old_cap = GenVec_capacity(q->arr);
    u64 new_cap = (u64)((float)old_cap * QUEUE_GROWTH);
    if (new_cap <= old_cap) {
        new_cap = old_cap + 1;
    }

    Queue_compact(q, new_cap);
}

static void Queue_shrink(Queue* q)
{
    u64 current_cap = GenVec_capacity(q->arr);
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
    CHECK_FATAL(new_capacity < q->size, "new_capacity must be >= current size");

    // Share the same ops pointer
    GenVec* new_arr = GenVec_create(new_capacity, q->arr->data_size, q->arr->ops);

    u64 h       = q->head;
    u64 old_cap = GenVec_capacity(q->arr);

    for (u64 i = 0; i < q->size; i++) {
        const u8* elem = GenVec_get_ptr(q->arr, h);
        GenVec_push(new_arr, elem);
        h = (h + 1) % old_cap;
    }

    GenVec_destroy(q->arr);
    q->arr = new_arr;

    q->head = 0;
    q->tail = q->size % new_capacity;
}
