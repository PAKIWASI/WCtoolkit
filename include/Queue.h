#ifndef QUEUE_H
#define QUEUE_H

#include "gen_vector.h"


typedef struct { // Circular Queue
    genVec* arr;
    u64 head;   // dequeue from (head + 1) % capacity
    u64 tail;   // enqueue at  (head + size) % capacity
    u64 size;
} Queue;


Queue*    queue_create(u64 n, u32 data_size, const container_ops* ops);
Queue*    queue_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops);

void      queue_destroy(Queue* q);
void      queue_clear(Queue* q);
void      queue_reset(Queue* q);
void      queue_shrink_to_fit(Queue* q);

void      enqueue(Queue* q, const u8* x);
void      enqueue_move(Queue* q, u8** x);
void      dequeue(Queue* q, u8* out);
void      queue_peek(Queue* q, u8* peek);
const u8* queue_peek_ptr(Queue* q);

void      queue_print(Queue* q, print_fn print_fn);

static inline u64 queue_size(Queue* q)     { CHECK_FATAL(!q, "queue is null"); return q->size;                    }
static inline u8  queue_empty(Queue* q)    { CHECK_FATAL(!q, "queue is null"); return q->size == 0;               }
static inline u64 queue_capacity(Queue* q) { CHECK_FATAL(!q, "queue is null"); return genVec_capacity(q->arr);    }


#endif // QUEUE_H
