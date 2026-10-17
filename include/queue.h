#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_comp.h"


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


wc_nodiscard wc_nonull(1) Queue Queue_create(const wc_allocator* a, u64 n, u32 data_size, const wc_container_ops* ops);
wc_nodiscard wc_nonull(1, 3) Queue Queue_create_val(const wc_allocator* a, u64 n, const void* val, u32 data_size, const wc_container_ops* ops);

wc_nonull(1) void Queue_destroy(Queue* q);
// Delete every element, keep the storage.
wc_nonull(1) void Queue_clear(Queue* q);
// Delete every element and free the storage; the allocator is kept.
wc_nonull(1) void Queue_reset(Queue* q);
// Shrink storage to max(size, QUEUE_MIN_CAP). Only ever on request (F2).
wc_nonull(1) void Queue_shrink_to_fit(Queue* q);

// Deep copy into `a`, compacted: head at 0, capacity max(size, QUEUE_MIN_CAP) (F5).
wc_nodiscard wc_nonull(1, 2) Queue Queue_copy(const wc_allocator* a, const Queue* src);
wc_nonull(1, 2) void  Queue_move(Queue* dest, Queue* src);

wc_nonull(1, 2) void        Queue_push(Queue* q, const void* x);    // deep copy of *x
wc_nonull(1, 2) void        Queue_push_move(Queue* q, void* x);     // *x moved in and zeroed
wc_nonull(1) void        Queue_pop(Queue* q, void* out);            // front, MOVED into out
wc_nonull(1) void        Queue_pop_back(Queue* q, void* out);       // back, MOVED into out
wc_nonull(1, 2) void        Queue_peek(const Queue* q, void* peek); // deep COPY of the front
wc_nodiscard wc_nonull(1) const void* Queue_peek_ptr(const Queue* q);

// Swap the elements at LOGICAL positions i and j. Never allocates.
wc_nonull(1) void Queue_swap(Queue* q, u64 i, u64 j);

wc_nonull(1, 2) void Queue_print(const Queue* q, wc_print_fn print_fn);


wc_nodiscard wc_nonull(1) static wc_always_inline u64 Queue_size(const Queue* q)
{
    return q->size;
}

wc_nodiscard wc_nonull(1) static wc_always_inline bool Queue_empty(const Queue* q)
{
    return q->size == 0;
}

wc_nodiscard wc_nonull(1) static wc_always_inline u64 Queue_capacity(const Queue* q)
{
    return q->arr.capacity;
}


// access and iteration

// Physical slot of logical position idx. head < cap and idx < cap, so the sum
// is below 2 * cap: one conditional subtract instead of a division.
wc_nodiscard wc_nonull(1) static wc_always_inline u64 Queue_slot(const Queue* q, u64 idx)
{
    u64 i = q->head + idx;
    return i >= q->arr.capacity ? i - q->arr.capacity : i;
}

// Pointer to the element at LOGICAL position idx (0 = front).
wc_nodiscard wc_nonull(1) static wc_always_inline const void* Queue_get(const Queue* q, u64 idx)
{
    WC_ASSERT(idx < q->size, "Queue_get: idx out of bounds");
    return GenVec_get_ptr_unsafe(&q->arr, Queue_slot(q, idx));
}


#endif // QUEUE_H
