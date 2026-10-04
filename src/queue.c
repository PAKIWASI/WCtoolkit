#include "queue.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_errno.h"

#include <stdio.h>
#include <string.h>


#define DS(q)          ((u64)(q)->arr.data_size)
#define CAP(q)         ((q)->arr.capacity)
#define SLOT_PTR(q, s) ((q)->arr.data + ((s) * DS(q)))
#define DATA_ALIGN(q)  wc_align_for_size((q)->arr.data_size)

#define ZERO_GUARD(q) \
    FATAL_IF((q)->arr.data_size == 0, "Queue is in the zero state (moved-from or destroyed): re-create it first")


static void queue_grow(Queue* q);
static void queue_relayout(Queue* q, u64 new_capacity);


// Copy one element into a raw slot: copy_fn if any, else memcpy (push only).
static inline void q_copy_into(const Queue* q, u8* dest, const void* src)
{
    wc_copy_fn copy = q->arr.is_pod ? NULL : VEC_COPY_FN(&q->arr);
    if (copy) {
        copy(q->arr.alloc, dest, src);
    } else {
        memcpy(dest, src, DS(q));
    }
}

// Delete every live element (slots stay raw).
static void q_delete_live(Queue* q)
{
    wc_delete_fn del = q->arr.is_pod ? NULL : VEC_DEL_FN(&q->arr);
    if (!del) {
        return;
    }
    for (u64 i = 0; i < q->size; i++) {
        del(SLOT_PTR(q, Queue_slot(q, i)));
    }
}

// Move element out of `slot` into `out`, or delete it when out == NULL (B7).
static inline void q_take_out(const Queue* q, u8* slot, void* out)
{
    if (out) {
        memcpy(out, slot, DS(q));
        return;
    }
    wc_delete_fn del = q->arr.is_pod ? NULL : VEC_DEL_FN(&q->arr);
    if (del) {
        del(slot);
    }
}


Queue Queue_create(const wc_allocator* a, u64 n, u32 data_size, const wc_container_ops* ops)
{
    FATAL_IF(data_size == 0, "Queue: data_size can't be 0");

    return (Queue){
        .arr  = GenVec_create(a, n, data_size, ops),
        .head = 0,
        .size = 0,
    };
}

Queue Queue_create_val(const wc_allocator* a, u64 n, const void* val, u32 data_size, const wc_container_ops* ops)
{
    FATAL_IF(n == 0 || data_size == 0, "Queue: n/data_size can't be 0");

    Queue q = {
        .arr  = GenVec_create_val(a, n, val, data_size, ops),
        .head = 0,
        .size = n,
    };
    q.arr.size = 0; // the ring tracks its own live range
    return q;
}


void Queue_destroy(Queue* q)
{
    if (q->arr.data) {
        q_delete_live(q);
    }
    GenVec_destroy(&q->arr); // arr.size == 0: frees the buffer, deletes nothing
    q->head = 0;
    q->size = 0;
}

void Queue_clear(Queue* q)
{
    if (q->arr.data) {
        q_delete_live(q);
    }
    q->head = 0;
    q->size = 0;
}

void Queue_reset(Queue* q)
{
    Queue_clear(q);
    GenVec_reset(&q->arr);
}

void Queue_shrink_to_fit(Queue* q)
{
    if (q->size == 0) {
        Queue_reset(q);
        return;
    }

    u64 min_capacity = q->size > QUEUE_MIN_CAP ? q->size : QUEUE_MIN_CAP;
    if (CAP(q) > min_capacity) {
        queue_relayout(q, min_capacity);
    }
}


void Queue_push(Queue* q, const void* x)
{
    WC_ASSERT(!(q->arr.data && (const u8*)x >= q->arr.data && (const u8*)x < q->arr.data + (CAP(q) * DS(q))),
              "input element points into this queue's own buffer: copy it out first");
    if (WC_UNLIKELY(q->size == CAP(q))) {
        queue_grow(q);
    }

    q_copy_into(q, SLOT_PTR(q, Queue_slot(q, q->size)), x);
    q->size++;
}

void Queue_push_move(Queue* q, void* x)
{
    WC_ASSERT(!(q->arr.data && (const u8*)x >= q->arr.data && (const u8*)x < q->arr.data + (CAP(q) * DS(q))),
              "input element points into this queue's own buffer: copy it out first");
    if (WC_UNLIKELY(q->size == CAP(q))) {
        queue_grow(q);
    }

    memcpy(SLOT_PTR(q, Queue_slot(q, q->size)), x, DS(q)); // move: memcpy + zero (B6)
    memset(x, 0, DS(q));
    q->size++;
}

void Queue_pop(Queue* q, void* out)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );

    q_take_out(q, SLOT_PTR(q, q->head), out);

    q->size--;
    q->head = q->size == 0 ? 0 : Queue_slot(q, 1);
}

void Queue_pop_back(Queue* q, void* out)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );

    q_take_out(q, SLOT_PTR(q, Queue_slot(q, q->size - 1)), out);

    q->size--;
    if (q->size == 0) {
        q->head = 0;
    }
}

void Queue_swap(Queue* q, u64 i, u64 j)
{
    WC_ASSERT(i < q->size && j < q->size, "Queue_swap: index out of bounds");
    if (i == j) {
        return;
    }

    // chunked swap through a small stack buffer: never allocates
    u8* a = SLOT_PTR(q, Queue_slot(q, i));
    u8* b = SLOT_PTR(q, Queue_slot(q, j));
    u8  tmp[64];
    for (u64 left = DS(q); left > 0;) {
        u64 n = left < sizeof(tmp) ? left : sizeof(tmp);
        memcpy(tmp, a, n);
        memcpy(a, b, n);
        memcpy(b, tmp, n);
        a += n;
        b += n;
        left -= n;
    }
}

void Queue_peek(const Queue* q, void* peek)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );
    q_copy_into(q, peek, SLOT_PTR(q, q->head));
}

const void* Queue_peek_ptr(const Queue* q)
{
    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, NULL);
    return SLOT_PTR(q, q->head);
}

void Queue_print(const Queue* q, wc_print_fn print)
{
    printf("[ ");
    for (u64 i = 0; i < q->size; i++) {
        print(SLOT_PTR(q, Queue_slot(q, i)));
        putchar(' ');
    }
    putchar(']');
}


Queue Queue_copy(const wc_allocator* a, const Queue* src)
{
    if (src->arr.data_size == 0) {
        return (Queue){0};
    }

    u64   cap  = src->size == 0 ? 0 : (src->size > QUEUE_MIN_CAP ? src->size : QUEUE_MIN_CAP);
    Queue dest = Queue_create(a, cap, src->arr.data_size, src->arr.ops);

    // compacted: logical order becomes physical order, head = 0
    for (u64 i = 0; i < src->size; i++) {
        q_copy_into(&dest, SLOT_PTR(&dest, i), SLOT_PTR(src, Queue_slot(src, i)));
    }
    dest.size = src->size;
    return dest;
}

void Queue_move(Queue* dest, Queue* src)
{
    if (dest == src) {
        return;
    }
    *dest = *src;
    memset(src, 0, sizeof(Queue));
}


// Private

// Grow by 1.5x (0 -> QUEUE_MIN_CAP). realloc keeps every byte in place, then
// at most one of the two runs of a wrapped ring is relocated (B5: memcpy).
static void queue_grow(Queue* q)
{
    ZERO_GUARD(q);

    u64 old_cap = CAP(q);
    u64 new_cap = old_cap == 0 ? QUEUE_MIN_CAP : old_cap + (old_cap / 2);
    if (new_cap <= old_cap) {
        new_cap = old_cap + 1;
    }
    u64 ds = DS(q);

    u8* data = wc_realloc(q->arr.alloc, q->arr.data, old_cap * ds, wc_mul(new_cap, ds), DATA_ALIGN(q));
    FATAL_IF(!data, "Queue: growth to %llu elements failed (arena full, or borrowed buffer)",
             (unsigned long long)new_cap);
    q->arr.data     = data;
    q->arr.capacity = new_cap;

    // Wrapped ring: [head, old_cap) then [0, tail). Unwrapped needs nothing.
    if (q->head + q->size > old_cap) {
        u64 front_len = old_cap - q->head;   // run at the end of the old buffer
        u64 tail_len  = q->size - front_len; // run at the start
        u64 room      = new_cap - old_cap;   // fresh slots after the old end
        if (tail_len <= room && tail_len <= front_len) {
            // append the start run after the old end: ring becomes contiguous
            memcpy(data + (old_cap * ds), data, tail_len * ds);
        } else {
            // slide the end run to the end of the new buffer
            u64 new_head = new_cap - front_len;
            memmove(data + (new_head * ds), data + (q->head * ds), front_len * ds);
            q->head = new_head;
        }
    }
}

// Re-lay the ring into a fresh buffer of new_capacity, head at 0 (shrink only).
static void queue_relayout(Queue* q, u64 new_capacity)
{
    u64 ds   = DS(q);
    u8* data = wc_alloc(q->arr.alloc, wc_mul(new_capacity, ds), DATA_ALIGN(q));
    FATAL_IF(!data, "Queue: relayout to %llu elements failed", (unsigned long long)new_capacity);

    u64 first = q->size < CAP(q) - q->head ? q->size : CAP(q) - q->head;
    memcpy(data, SLOT_PTR(q, q->head), first * ds);
    if (q->size > first) {
        memcpy(data + (first * ds), q->arr.data, (q->size - first) * ds);
    }

    wc_free(q->arr.alloc, q->arr.data, CAP(q) * ds, DATA_ALIGN(q));
    q->arr.data     = data;
    q->arr.capacity = new_capacity;
    q->head         = 0;
}
