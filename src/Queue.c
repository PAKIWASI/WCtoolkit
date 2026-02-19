#include "Queue.h"
#include "gen_vector.h"
#include "wc_errno.h"
#include <string.h>


#define QUEUE_MIN_CAP   4
#define QUEUE_GROWTH    1.5
#define QUEUE_SHRINK_AT 0.25 // Shrink when less than 25% full
#define QUEUE_SHRINK_BY 0.5  // Reduce capacity by half when shrinking


#define HEAD_UPDATE(q)                                    \
    {                                                     \
        (q)->head = ((q)->head + 1) % (q)->arr->capacity; \
    }

#define TAIL_UPDATE(q)                                              \
    {                                                               \
        (q)->tail = (((q)->head + (q)->size) % (q)->arr->capacity); \
    }

#define MAYBE_GROW(q)                          \
    do {                                       \
        if ((q)->size == (q)->arr->capacity) { \
            queue_grow((q));                   \
        }                                      \
    } while (0)

#define MAYBE_SHRINK(q)                                         \
    do {                                                        \
        u64 capacity = (q)->arr->capacity;                      \
        if (capacity <= 4) {                                    \
            return;                                             \
        }                                                       \
        float load_factor = (float)(q)->size / (float)capacity; \
        if (load_factor < QUEUE_SHRINK_AT) {                    \
            queue_shrink((q));                                  \
        }                                                       \
    } while (0)



static void queue_grow(Queue* q);
static void queue_shrink(Queue* q);
static void queue_compact(Queue* q, u64 new_capacity);


Queue* queue_create(u64 n, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn)
{
    CHECK_FATAL(n == 0, "n can't be 0");
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    Queue* q = malloc(sizeof(Queue));
    CHECK_FATAL(!q, "queue malloc failed");

    q->arr = genVec_init(n, data_size, copy_fn, move_fn, del_fn);

    q->head = 0;
    q->tail = 0;
    q->size = 0;

    return q;
}

Queue* queue_create_val(u64 n, const u8* val, u32 data_size, copy_fn copy_fn, move_fn move_fn,
                        delete_fn del_fn)
{
    CHECK_FATAL(n == 0, "n can't be 0");
    CHECK_FATAL(data_size == 0, "data_size can't be 0");
    CHECK_FATAL(!val, "val is null");

    Queue* q = malloc(sizeof(Queue));
    CHECK_FATAL(!q, "queue malloc failed");

    q->arr = genVec_init_val(n, val, data_size, copy_fn, move_fn, del_fn);

    q->head = 0;
    q->tail = n % genVec_capacity(q->arr);
    q->size = n;

    return q;
}

void queue_destroy(Queue* q)
{
    CHECK_FATAL(!q, "queue is null");

    genVec_destroy(q->arr);
    free(q);
}

void queue_clear(Queue* q)
{
    CHECK_FATAL(!q, "queue is null");

    genVec_clear(q->arr);
    q->size = 0;
    q->head = 0;
    q->tail = 0;
}

void queue_reset(Queue* q)
{
    CHECK_FATAL(!q, "queue is null");

    genVec_reset(q->arr);
    q->size = 0;
    q->head = 0;
    q->tail = 0;
}

// Manual shrink function
void queue_shrink_to_fit(Queue* q)
{
    CHECK_FATAL(!q, "queue is null");

    if (q->size == 0) {
        queue_reset(q);
        return;
    }

    // Don't shrink below minimum useful capacity
    u64 min_capacity     = q->size > QUEUE_MIN_CAP ? q->size : QUEUE_MIN_CAP;
    u64 current_capacity = genVec_capacity(q->arr);

    if (current_capacity > min_capacity) {
        queue_compact(q, min_capacity);
    }
}

void enqueue(Queue* q, const u8* x)
{
    CHECK_FATAL(!q, "queue is null");
    CHECK_FATAL(!x, "x is null");

    MAYBE_GROW(q);

    // If the tail position doesn't exist in the vector yet, push
    if (q->tail >= genVec_size(q->arr)) {
        genVec_push(q->arr, x);
    } else {
        genVec_replace(q->arr, q->tail, x);
    }

    q->size++;
    TAIL_UPDATE(q);
}

void enqueue_move(Queue* q, u8** x)
{
    CHECK_FATAL(!q, "queue is null");
    CHECK_FATAL(!x, "x is null");
    CHECK_FATAL(!*x, "*x is null");

    MAYBE_GROW(q);

    if (q->tail >= genVec_size(q->arr)) {
        genVec_push_move(q->arr, x);
    } else {
        genVec_replace_move(q->arr, q->tail, x);
    }

    q->size++;
    TAIL_UPDATE(q);
}

void dequeue(Queue* q, u8* out)
{
    CHECK_FATAL(!q, "queue is null");

    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );

    if (out) {
        genVec_get(q->arr, q->head, out);
    }

    // Clear the element if del_fn exists
    if (q->arr->del_fn) {
        u8* elem = (u8*)genVec_get_ptr(q->arr, q->head);
        q->arr->del_fn(elem);
        memset(elem, 0, q->arr->data_size);
    }

    HEAD_UPDATE(q);
    q->size--;
    MAYBE_SHRINK(q);
}

void queue_peek(Queue* q, u8* peek)
{
    CHECK_FATAL(!q, "queue is null");
    CHECK_FATAL(!peek, "peek is null");

    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, );

    genVec_get(q->arr, q->head, peek);
}

const u8* queue_peek_ptr(Queue* q)
{
    CHECK_FATAL(!q, "queue is null");

    WC_SET_RET(WC_ERR_EMPTY, q->size == 0, NULL);

    return genVec_get_ptr(q->arr, q->head);
}

void queue_print(Queue* q, print_fn print_fn)
{
    CHECK_FATAL(!q, "queue is empty");
    CHECK_FATAL(!print_fn, "print_fn is empty");

    u64 h   = q->head;
    u64 cap = genVec_capacity(q->arr);

    printf("[ ");
    if (q->size != 0) {
        for (u64 i = 0; i < q->size; i++) {
            const u8* out = genVec_get_ptr(q->arr, h);
            print_fn(out);
            putchar(' ');
            h = (h + 1) % cap;
        }
    }
    putchar(']');
}


static void queue_grow(Queue* q)
{
    u64 old_cap = genVec_capacity(q->arr);
    u64 new_cap = (u64)((float)old_cap * QUEUE_GROWTH);
    if (new_cap <= old_cap) {
        new_cap = old_cap + 1;
    }

    queue_compact(q, new_cap);
}

static void queue_shrink(Queue* q)
{
    u64 current_cap = genVec_capacity(q->arr);
    u64 new_cap     = (u64)((float)current_cap * QUEUE_SHRINK_BY);

    // Don't shrink below current size or minimum capacity
    u64 min_capacity = q->size > QUEUE_MIN_CAP ? q->size : QUEUE_MIN_CAP;
    if (new_cap < min_capacity) {
        new_cap = min_capacity;
    }

    // Only shrink if we're actually reducing capacity
    if (new_cap < current_cap) {
        queue_compact(q, new_cap);
    }
}


static void queue_compact(Queue* q, u64 new_capacity)
{
    CHECK_FATAL(new_capacity < q->size, "new_capacity must be >= current size");

    genVec* new_arr = genVec_init(new_capacity, q->arr->data_size, q->arr->copy_fn, q->arr->move_fn, q->arr->del_fn);

    u64 h       = q->head;
    u64 old_cap = genVec_capacity(q->arr);

    for (u64 i = 0; i < q->size; i++) {
        const u8* elem = genVec_get_ptr(q->arr, h);
        genVec_push(new_arr, elem);
        h = (h + 1) % old_cap;
    }

    genVec_destroy(q->arr);
    q->arr = new_arr;

    q->head = 0;
    q->tail = q->size % new_capacity;
}


