#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"
#include "gen_vector.h"


typedef struct { // Circular Queue
    GenVec* arr;
    u64 head;   // pop from (head + 1) % capacity
    u64 tail;   // push at  (head + size) % capacity
    u64 size;
} Queue;


Queue*    Queue_create(u64 n, u32 data_size, const container_ops* ops) __attribute__((warn_unused_result));
Queue*    Queue_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops) __attribute__((nonnull(2), warn_unused_result));
void      Queue_create_stk(Queue* q, u64 n, u32 data_size, const container_ops* ops) __attribute__((nonnull(1)));

void      Queue_destroy(Queue* q) __attribute__((nonnull(1)));
void      Queue_destroy_stk(Queue* q) __attribute__((nonnull(1)));
void      Queue_clear(Queue* q) __attribute__((nonnull(1)));
void      Queue_reset(Queue* q) __attribute__((nonnull(1)));
void      Queue_shrink_to_fit(Queue* q) __attribute__((nonnull(1)));

// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void      Queue_copy(Queue* dest, const Queue* src) __attribute__((nonnull(1, 2)));
void      Queue_move(Queue* dest, Queue** src) __attribute__((nonnull(1, 2)));

void      Queue_push(Queue* q, const u8* x) __attribute__((nonnull(1, 2)));
void      Queue_push_move(Queue* q, u8** x) __attribute__((nonnull(1, 2)));
void      Queue_pop(Queue* q, u8* out) __attribute__((nonnull(1)));
void      Queue_peek(Queue* q, u8* peek) __attribute__((nonnull(1, 2)));
const u8* Queue_peek_ptr(const Queue* q) __attribute__((nonnull(1)));

void      Queue_print(Queue* q, print_fn print_fn) __attribute__((nonnull(1, 2)));

// 6-J: nonnull-validated — no CHECK_FATAL(!q) re-checks (mirrors gen_vector.c)
static inline __attribute__((nonnull(1))) u64 Queue_size(const Queue* q) { return q->size;                  }
static inline __attribute__((nonnull(1))) u8 Queue_empty(const Queue* q) { return q->size == 0;             }
static inline __attribute__((nonnull(1))) u64 Queue_capacity(const Queue* q) { return GenVec_capacity(q->arr); }


#endif // QUEUE_H
