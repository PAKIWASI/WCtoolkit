#include "wc_test.h"
#include "Stack.h"
#include "Queue.h"
#include "wc_errno.h"


/* ═══════════════════════════════════════════════════════════════════════════
 * STACK
 * ═══════════════════════════════════════════════════════════════════════════ */

static Stack* int_stack(u64 cap) {
    return stack_create(cap, sizeof(int), NULL);
}

static void test_stack_push_peek(void)
{
    Stack* s = int_stack(4);
    int x = 10;
    stack_push(s, (u8*)&x);
    WC_ASSERT_EQ_INT(*(int*)stack_peek_ptr(s), 10);
    stack_destroy(s);
}

static void test_stack_push_pop_lifo(void)
{
    Stack* s = int_stack(4);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) stack_push(s, (u8*)&vals[i]);

    int out;
    stack_pop(s, (u8*)&out); WC_ASSERT_EQ_INT(out, 3);
    stack_pop(s, (u8*)&out); WC_ASSERT_EQ_INT(out, 2);
    stack_pop(s, (u8*)&out); WC_ASSERT_EQ_INT(out, 1);
    stack_destroy(s);
}

static void test_stack_pop_empty_sets_errno(void)
{
    Stack* s = int_stack(4);
    wc_errno = WC_OK;
    stack_pop(s, NULL);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_EMPTY);
    stack_destroy(s);
}

static void test_stack_size(void)
{
    Stack* s = int_stack(4);
    WC_ASSERT_EQ_U64(stack_size(s), 0);
    WC_ASSERT_TRUE(stack_empty(s));
    int x = 1;
    stack_push(s, (u8*)&x);
    WC_ASSERT_EQ_U64(stack_size(s), 1);
    WC_ASSERT_FALSE(stack_empty(s));
    stack_destroy(s);
}

static void test_stack_clear(void)
{
    Stack* s = int_stack(4);
    int x = 5;
    for (int i = 0; i < 4; i++) stack_push(s, (u8*)&x);
    stack_clear(s);
    WC_ASSERT_EQ_U64(stack_size(s), 0);
    WC_ASSERT_TRUE(stack_empty(s));
    stack_destroy(s);
}

static void test_stack_growth(void)
{
    Stack* s = int_stack(2);
    for (int i = 0; i < 20; i++) stack_push(s, (u8*)&i);
    WC_ASSERT_EQ_U64(stack_size(s), 20);
    /* LIFO: last pushed = 19 */
    WC_ASSERT_EQ_INT(*(int*)stack_peek_ptr(s), 19);
    stack_destroy(s);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * QUEUE
 * ═══════════════════════════════════════════════════════════════════════════ */

static Queue* int_queue(u64 cap) {
    return queue_create(cap, sizeof(int), NULL);
}

static void test_queue_enqueue_dequeue_fifo(void)
{
    Queue* q = int_queue(4);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) enqueue(q, (u8*)&vals[i]);

    int out;
    dequeue(q, (u8*)&out); WC_ASSERT_EQ_INT(out, 1);
    dequeue(q, (u8*)&out); WC_ASSERT_EQ_INT(out, 2);
    dequeue(q, (u8*)&out); WC_ASSERT_EQ_INT(out, 3);
    queue_destroy(q);
}

static void test_queue_size(void)
{
    Queue* q = int_queue(4);
    WC_ASSERT_EQ_U64(queue_size(q), 0);
    WC_ASSERT_TRUE(queue_empty(q));
    int x = 1;
    enqueue(q, (u8*)&x);
    WC_ASSERT_EQ_U64(queue_size(q), 1);
    WC_ASSERT_FALSE(queue_empty(q));
    queue_destroy(q);
}

static void test_queue_dequeue_empty_sets_errno(void)
{
    Queue* q = int_queue(4);
    wc_errno = WC_OK;
    dequeue(q, NULL);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_EMPTY);
    queue_destroy(q);
}

static void test_queue_peek(void)
{
    Queue* q = int_queue(4);
    int x = 42;
    enqueue(q, (u8*)&x);
    WC_ASSERT_EQ_INT(*(int*)queue_peek_ptr(q), 42);
    /* peek must not dequeue */
    WC_ASSERT_EQ_U64(queue_size(q), 1);
    queue_destroy(q);
}

static void test_queue_circular_wrap(void)
{
    /* Enqueue/dequeue repeatedly to force circular buffer wrap-around */
    Queue* q = int_queue(4);
    for (int round = 0; round < 5; round++) {
        int in = round * 10;
        enqueue(q, (u8*)&in);
        int out = 0;
        dequeue(q, (u8*)&out);
        WC_ASSERT_EQ_INT(out, in);
    }
    WC_ASSERT_TRUE(queue_empty(q));
    queue_destroy(q);
}

static void test_queue_growth(void)
{
    Queue* q = int_queue(2);
    for (int i = 0; i < 20; i++) enqueue(q, (u8*)&i);
    WC_ASSERT_EQ_U64(queue_size(q), 20);
    /* dequeue in order */
    for (int i = 0; i < 20; i++) {
        int out = 0;
        dequeue(q, (u8*)&out);
        WC_ASSERT_EQ_INT(out, i);
    }
    queue_destroy(q);
}

static void test_queue_reset(void)
{
    Queue* q = int_queue(4);
    int x = 1;
    for (int i = 0; i < 4; i++) enqueue(q, (u8*)&x);
    queue_reset(q);
    WC_ASSERT_EQ_U64(queue_size(q), 0);
    WC_ASSERT_TRUE(queue_empty(q));
    queue_destroy(q);
}


/* ── Suite entry points ──────────────────────────────────────────────────── */

void stack_suite(void)
{
    WC_SUITE("Stack");
    WC_RUN(test_stack_push_peek);
    WC_RUN(test_stack_push_pop_lifo);
    WC_RUN(test_stack_pop_empty_sets_errno);
    WC_RUN(test_stack_size);
    WC_RUN(test_stack_clear);
    WC_RUN(test_stack_growth);
}

void queue_suite(void)
{
    WC_SUITE("Queue");
    WC_RUN(test_queue_enqueue_dequeue_fifo);
    WC_RUN(test_queue_size);
    WC_RUN(test_queue_dequeue_empty_sets_errno);
    WC_RUN(test_queue_peek);
    WC_RUN(test_queue_circular_wrap);
    WC_RUN(test_queue_growth);
    WC_RUN(test_queue_reset);
}
