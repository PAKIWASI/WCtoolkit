#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"


/* Circular queue over a GenVec buffer.
 *
 * `arr` is only the storage: arr.size stays 0 and arr.capacity is the ring
 * size. Live elements are the `size` slots starting at `head`, wrapping at
 * capacity. Every slot outside that range is RAW memory: push writes into it
 * without deleting anything, pop moves out of it and forgets it.
 *
 * Memory rules:
 *   - pop / pop_back MOVE the element into `out`, or delete it (out NULL)
 *   - growth relocates with realloc + memcpy, never copy_fn,
 *   - the queue never shrinks on its own; call Queue_shrink_to_fit
 *   - element pointers (peek_ptr, get) are invalidated by push and pop
 */
typedef struct {
    GenVec arr;  // ring storage (arr.size unused, always 0)
    u64    head; // index of the front element
    u64    size; // number of live elements
} Queue;

_Static_assert(sizeof(Queue) == 64, "Queue size mismatch");

#ifndef QUEUE_MIN_CAP
#define QUEUE_MIN_CAP 8
#endif


Queue Queue_create(const wc_allocator* a, u64 n, u32 data_size, const wc_container_ops* ops)
    __attribute__((nonnull(1), warn_unused_result));
Queue Queue_create_val(const wc_allocator* a, u64 n, const void* val, u32 data_size, const wc_container_ops* ops)
    __attribute__((nonnull(1, 3), warn_unused_result));

void Queue_destroy(Queue* q) __attribute__((nonnull(1)));
// Delete every element, keep the storage.
void Queue_clear(Queue* q) __attribute__((nonnull(1)));
// Delete every element and free the storage; the allocator is kept.
void Queue_reset(Queue* q) __attribute__((nonnull(1)));
// Shrink storage to max(size, QUEUE_MIN_CAP). Only ever on request (F2).
void Queue_shrink_to_fit(Queue* q) __attribute__((nonnull(1)));

// Deep copy into `a`, compacted: head at 0, capacity max(size, QUEUE_MIN_CAP) (F5).
Queue Queue_copy(const wc_allocator* a, const Queue* src) __attribute__((nonnull(1, 2), warn_unused_result));
void  Queue_move(Queue* dest, Queue* src) __attribute__((nonnull(1, 2)));

void Queue_push(Queue* q, const void* x) __attribute__((nonnull(1, 2)));      // deep copy of *x
void Queue_push_move(Queue* q, void* x) __attribute__((nonnull(1, 2)));       // *x moved in and zeroed
void Queue_pop(Queue* q, void* out) __attribute__((nonnull(1)));              // front, MOVED into out
void Queue_pop_back(Queue* q, void* out) __attribute__((nonnull(1)));         // back, MOVED into out
void Queue_peek(const Queue* q, void* peek) __attribute__((nonnull(1, 2)));   // deep COPY of the front
const void* Queue_peek_ptr(const Queue* q) __attribute__((nonnull(1)));

// Swap the elements at LOGICAL positions i and j. Never allocates.
void Queue_swap(Queue* q, u64 i, u64 j) __attribute__((nonnull(1)));

void Queue_print(const Queue* q, wc_print_fn print_fn) __attribute__((nonnull(1, 2)));


static inline __attribute__((nonnull(1))) u64 Queue_size(const Queue* q)
{
    return q->size;
}

static inline __attribute__((nonnull(1))) bool Queue_empty(const Queue* q)
{
    return q->size == 0;
}

static inline __attribute__((nonnull(1))) u64 Queue_capacity(const Queue* q)
{
    return q->arr.capacity;
}


// access and iteration

// Physical slot of logical position idx. head < cap and idx < cap, so the sum
// is below 2 * cap: one conditional subtract instead of a division.
static inline __attribute__((nonnull(1))) u64 Queue_slot(const Queue* q, u64 idx)
{
    u64 i = q->head + idx;
    return i >= q->arr.capacity ? i - q->arr.capacity : i;
}

// Pointer to the element at LOGICAL position idx (0 = front).
static inline __attribute__((nonnull(1))) const void* Queue_get(const Queue* q, u64 idx)
{
    WC_ASSERT(idx < q->size, "Queue_get: idx out of bounds");
    return GenVec_get_ptr_unsafe(&q->arr, Queue_slot(q, idx));
}


#endif // QUEUE_H
