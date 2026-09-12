#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"
#include "gen_vector.h"


typedef struct { // Circular Queue
    genVec* arr;
    u64 head;   // dequeue from (head + 1) % capacity
    u64 tail;   // enqueue at  (head + size) % capacity
    u64 size;
} Queue;


Queue*    queue_create(u64 n, u32 data_size, const container_ops* ops);
Queue*    queue_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops) __attribute__((nonnull(2)));
void      queue_create_stk(u64 n, u32 data_size, const container_ops* ops, Queue* q) __attribute__((nonnull(4)));

void      queue_destroy(Queue* q) __attribute__((nonnull(1)));
void      queue_destroy_stk(Queue* q) __attribute__((nonnull(1)));
void      queue_clear(Queue* q) __attribute__((nonnull(1)));
void      queue_reset(Queue* q) __attribute__((nonnull(1)));
void      queue_shrink_to_fit(Queue* q) __attribute__((nonnull(1)));

// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void      queue_copy(Queue* dest, const Queue* src) __attribute__((nonnull(1, 2)));
void      queue_move(Queue* dest, Queue** src) __attribute__((nonnull(1, 2)));

void      queue_push(Queue* q, const u8* x) __attribute__((nonnull(1, 2)));
void      queue_push_move(Queue* q, u8** x) __attribute__((nonnull(1, 2)));
void      queue_pop(Queue* q, u8* out) __attribute__((nonnull(1)));
void      queue_peek(Queue* q, u8* peek) __attribute__((nonnull(1, 2)));
const u8* queue_peek_ptr(const Queue* q) __attribute__((nonnull(1)));

void      queue_print(Queue* q, print_fn print_fn) __attribute__((nonnull(1, 2)));

static inline __attribute__((nonnull(1))) u64 queue_size(const Queue* q) { CHECK_FATAL(!q, "queue is null"); return q->size;                    }
static inline __attribute__((nonnull(1))) u8 queue_empty(const Queue* q) { CHECK_FATAL(!q, "queue is null"); return q->size == 0;               }
static inline __attribute__((nonnull(1))) u64 queue_capacity(const Queue* q) { CHECK_FATAL(!q, "queue is null"); return genVec_capacity(q->arr);    }


#endif // QUEUE_H
