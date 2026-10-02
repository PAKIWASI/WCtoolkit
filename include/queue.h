#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"


typedef struct { // Circular Queue
    GenVec arr;
    u64    head; // pop from (head + 1) % capacity
    u64    tail; // push at  (head + size) % capacity
    u64    size;
} Queue;

_Static_assert(sizeof(Queue) == 80, "Queue size mismatch");


Queue Queue_create(wc_allocator a, u64 n, u32 data_size, const wc_container_ops* ops) __attribute__((warn_unused_result));
Queue Queue_create_val(wc_allocator a, u64 n, const void* val, u32 data_size, const wc_container_ops* ops)
    __attribute__((nonnull(3), warn_unused_result));

void Queue_destroy(Queue* q) __attribute__((nonnull(1)));
void Queue_clear(Queue* q) __attribute__((nonnull(1)));
void Queue_reset(Queue* q) __attribute__((nonnull(1)));
void Queue_shrink_to_fit(Queue* q) __attribute__((nonnull(1)));

Queue Queue_copy(wc_allocator a, const Queue* src) __attribute__((nonnull(2), warn_unused_result));
void  Queue_move(Queue* dest, Queue* src) __attribute__((nonnull(1, 2)));

void        Queue_push(Queue* q, const void* x) __attribute__((nonnull(1, 2)));
void        Queue_push_move(Queue* q, void* x) __attribute__((nonnull(1, 2))); // *x is moved in and zeroed
void        Queue_pop(Queue* q, void* out) __attribute__((nonnull(1)));
void        Queue_pop_back(Queue* q, void* out) __attribute__((nonnull(1)));
void        Queue_peek(Queue* q, void* peek) __attribute__((nonnull(1, 2)));
const void* Queue_peek_ptr(const Queue* q) __attribute__((nonnull(1)));

// Swap the elements at LOGICAL positions i and j
void Queue_swap(Queue* q, u64 i, u64 j) __attribute__((nonnull(1)));

void Queue_print(Queue* q, wc_print_fn print_fn) __attribute__((nonnull(1, 2)));

static inline __attribute__((nonnull(1))) u64 Queue_size(const Queue* q)
{
    return q->size;
}
static inline __attribute__((nonnull(1))) u8 Queue_empty(const Queue* q)
{
    return q->size == 0;
}
static inline __attribute__((nonnull(1))) u64 Queue_capacity(const Queue* q)
{
    return q->arr.capacity;
}

// access and iteration

// converts the element at the logical `idx` position. Circular queue nuance is handeled
static inline __attribute__((nonnull(1))) const void* Queue_get(Queue* q, u64 idx)
{
    WC_ASSERT(idx < q->size, "Queue_get: idx out of bounds");
    return GenVec_get_ptr_unsafe(&q->arr, (q->head + idx) % q->arr.capacity);
}


#endif // QUEUE_H
