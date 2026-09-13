#include "common.h"
#include "wc_test.h"
#include "stack.h"
#include "queue.h"
#include "wc_errno.h"


/* ═══════════════════════════════════════════════════════════════════════════
 * STACK
 * ═══════════════════════════════════════════════════════════════════════════ */

static Stack* int_Stack(u64 cap) {
    return Stack_create(cap, sizeof(int), NULL);
}

static void test_Stack_push_peek(void)
{
    Stack* s = int_Stack(4);
    int x = 10;
    Stack_push(s, (u8*)&x);
    WC_ASSERT_EQ_INT(*(int*)Stack_peek_ptr(s), 10);
    Stack_destroy(s);
}

static void test_Stack_push_pop_lifo(void)
{
    Stack* s = int_Stack(4);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) Stack_push(s, (u8*)&vals[i]);

    int out;
    Stack_pop(s, (u8*)&out); WC_ASSERT_EQ_INT(out, 3);
    Stack_pop(s, (u8*)&out); WC_ASSERT_EQ_INT(out, 2);
    Stack_pop(s, (u8*)&out); WC_ASSERT_EQ_INT(out, 1);
    Stack_destroy(s);
}

static void test_Stack_pop_empty_sets_errno(void)
{
    Stack* s = int_Stack(4);
    wc_errno = WC_OK;
    Stack_pop(s, NULL);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_EMPTY);
    Stack_destroy(s);
}

static void test_Stack_peek_empty_sets_errno(void)
{
    Stack* s = int_Stack(4);
    int out = 0;
    wc_errno = WC_OK;
    Stack_peek(s, (u8*)&out);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_EMPTY);
    Stack_destroy(s);
}

static void test_Stack_peek_ptr_empty_sets_errno(void)
{
    Stack* s = int_Stack(4);
    wc_errno = WC_OK;
    const u8* p = Stack_peek_ptr(s);
    WC_ASSERT_NULL(p);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_EMPTY);
    Stack_destroy(s);
}

static void test_Stack_size(void)
{
    Stack* s = int_Stack(4);
    WC_ASSERT_EQ_U64(Stack_size(s), 0);
    WC_ASSERT_TRUE(Stack_empty(s));
    int x = 1;
    Stack_push(s, (u8*)&x);
    WC_ASSERT_EQ_U64(Stack_size(s), 1);
    WC_ASSERT_FALSE(Stack_empty(s));
    Stack_destroy(s);
}

static void test_Stack_clear(void)
{
    Stack* s = int_Stack(4);
    int x = 5;
    for (int i = 0; i < 4; i++) Stack_push(s, (u8*)&x);
    Stack_clear(s);
    WC_ASSERT_EQ_U64(Stack_size(s), 0);
    WC_ASSERT_TRUE(Stack_empty(s));
    Stack_destroy(s);
}

static void test_Stack_growth(void)
{
    Stack* s = int_Stack(2);
    for (int i = 0; i < 20; i++) Stack_push(s, (u8*)&i);
    WC_ASSERT_EQ_U64(Stack_size(s), 20);
    /* LIFO: last pushed = 19 */
    WC_ASSERT_EQ_INT(*(int*)Stack_peek_ptr(s), 19);
    Stack_destroy(s);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * QUEUE
 * ═══════════════════════════════════════════════════════════════════════════ */

static Queue* int_Queue(u64 cap) {
    return Queue_create(cap, sizeof(int), NULL);
}

static void test_Queue_push_pop_fifo(void)
{
    Queue* q = int_Queue(4);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) Queue_push(q, (u8*)&vals[i]);

    int out;
    Queue_pop(q, (u8*)&out); WC_ASSERT_EQ_INT(out, 1);
    Queue_pop(q, (u8*)&out); WC_ASSERT_EQ_INT(out, 2);
    Queue_pop(q, (u8*)&out); WC_ASSERT_EQ_INT(out, 3);
    Queue_destroy(q);
}

static void test_Queue_size(void)
{
    Queue* q = int_Queue(4);
    WC_ASSERT_EQ_U64(Queue_size(q), 0);
    WC_ASSERT_TRUE(Queue_empty(q));
    int x = 1;
    Queue_push(q, (u8*)&x);
    WC_ASSERT_EQ_U64(Queue_size(q), 1);
    WC_ASSERT_FALSE(Queue_empty(q));
    Queue_destroy(q);
}

static void test_Queue_pop_empty_sets_errno(void)
{
    Queue* q = int_Queue(4);
    wc_errno = WC_OK;
    Queue_pop(q, NULL);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_EMPTY);
    Queue_destroy(q);
}

static void test_Queue_peek(void)
{
    Queue* q = int_Queue(4);
    int x = 42;
    Queue_push(q, (u8*)&x);
    WC_ASSERT_EQ_INT(*(int*)Queue_peek_ptr(q), 42);
    /* peek must not pop */
    WC_ASSERT_EQ_U64(Queue_size(q), 1);
    Queue_destroy(q);
}

static void test_Queue_circular_wrap(void)
{
    /* Push/pop repeatedly to force circular buffer wrap-around */
    Queue* q = int_Queue(4);
    for (int round = 0; round < 5; round++) {
        int in = round * 10;
        Queue_push(q, (u8*)&in);
        int out = 0;
        Queue_pop(q, (u8*)&out);
        WC_ASSERT_EQ_INT(out, in);
    }
    WC_ASSERT_TRUE(Queue_empty(q));
    Queue_destroy(q);
}

static void test_Queue_growth(void)
{
    Queue* q = int_Queue(2);
    for (int i = 0; i < 20; i++) Queue_push(q, (u8*)&i);
    WC_ASSERT_EQ_U64(Queue_size(q), 20);
    /* pop in order */
    for (int i = 0; i < 20; i++) {
        int out = 0;
        Queue_pop(q, (u8*)&out);
        WC_ASSERT_EQ_INT(out, i);
    }
    Queue_destroy(q);
}

static void test_Queue_reset(void)
{
    Queue* q = int_Queue(4);
    int x = 1;
    for (int i = 0; i < 4; i++) Queue_push(q, (u8*)&x);
    Queue_reset(q);
    WC_ASSERT_EQ_U64(Queue_size(q), 0);
    WC_ASSERT_TRUE(Queue_empty(q));
    Queue_destroy(q);
}


/* ── Suite entry points ──────────────────────────────────────────────────── */

void Stack_suite(void)
{
    WC_SUITE("Stack");
    WC_RUN(test_Stack_push_peek);
    WC_RUN(test_Stack_push_pop_lifo);
    WC_RUN(test_Stack_pop_empty_sets_errno);
    WC_RUN(test_Stack_peek_empty_sets_errno);
    WC_RUN(test_Stack_peek_ptr_empty_sets_errno);
    WC_RUN(test_Stack_size);
    WC_RUN(test_Stack_clear);
    WC_RUN(test_Stack_growth);
}

void Queue_suite(void)
{
    WC_SUITE("Queue");
    WC_RUN(test_Queue_push_pop_fifo);
    WC_RUN(test_Queue_size);
    WC_RUN(test_Queue_pop_empty_sets_errno);
    WC_RUN(test_Queue_peek);
    WC_RUN(test_Queue_circular_wrap);
    WC_RUN(test_Queue_growth);
    WC_RUN(test_Queue_reset);
}
