#include "arena.h"
#include "common.h"
#include "queue.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_errno.h"
#include "wc_test_allocator.h"


/* ═══════════════════════════════════════════════════════════════════════════
 * QUEUE
 * ═══════════════════════════════════════════════════════════════════════════ */

static Queue int_Queue(u64 cap)
{
    return Queue_create(WC_LIBC, cap, sizeof(int), NULL);
}

UTEST(queue, push_pop_fifo)
{
    Queue q      = int_Queue(4);
    int   vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) {
        Queue_push(&q, &vals[i]);
    }

    int out;
    Queue_pop(&q, &out);
    EXPECT_EQ(out, 1);
    Queue_pop(&q, &out);
    EXPECT_EQ(out, 2);
    Queue_pop(&q, &out);
    EXPECT_EQ(out, 3);
    Queue_destroy(&q);
}

UTEST(queue, size)
{
    Queue q = int_Queue(4);
    EXPECT_EQ(Queue_size(&q), 0u);
    EXPECT_TRUE(Queue_empty(&q));
    int x = 1;
    Queue_push(&q, &x);
    EXPECT_EQ(Queue_size(&q), 1u);
    EXPECT_FALSE(Queue_empty(&q));
    Queue_destroy(&q);
}

UTEST(queue, pop_empty_sets_errno)
{
    Queue q  = int_Queue(4);
    wc_errno = WC_OK;
    Queue_pop(&q, NULL);
    EXPECT_EQ(wc_errno, (wc_err)WC_ERR_EMPTY);
    Queue_destroy(&q);
}

UTEST(queue, peek)
{
    Queue q = int_Queue(4);
    int   x = 42;
    Queue_push(&q, &x);
    EXPECT_EQ(*(int*)Queue_peek_ptr(&q), 42);
    /* peek must not pop */
    EXPECT_EQ(Queue_size(&q), 1u);
    Queue_destroy(&q);
}

UTEST(queue, circular_wrap)
{
    /* Push/pop repeatedly to force circular buffer wrap-around */
    Queue q = int_Queue(4);
    for (int round = 0; round < 5; round++) {
        int in = round * 10;
        Queue_push(&q, &in);
        int out = 0;
        Queue_pop(&q, &out);
        EXPECT_EQ(out, in);
    }
    EXPECT_TRUE(Queue_empty(&q));
    Queue_destroy(&q);
}

UTEST(queue, growth)
{
    Queue q = int_Queue(2);
    for (int i = 0; i < 20; i++) {
        Queue_push(&q, &i);
    }
    EXPECT_EQ(Queue_size(&q), 20u);
    /* pop in order */
    for (int i = 0; i < 20; i++) {
        int out = 0;
        Queue_pop(&q, &out);
        EXPECT_EQ(out, i);
    }
    Queue_destroy(&q);
}

UTEST(queue, reset)
{
    Queue q = int_Queue(4);
    int   x = 1;
    for (int i = 0; i < 4; i++) {
        Queue_push(&q, &x);
    }
    Queue_reset(&q);
    EXPECT_EQ(Queue_size(&q), 0u);
    EXPECT_TRUE(Queue_empty(&q));
    Queue_destroy(&q);
}

UTEST(queue, copy_move)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    Queue src = Queue_create(a, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) {
        Queue_push(&src, &i);
    }

    /* copy into libc */
    Queue dst = Queue_copy(WC_LIBC, &src);
    EXPECT_EQ(Queue_size(&dst), Queue_size(&src));
    for (u64 i = 0; i < Queue_size(&src); i++) {
        EXPECT_EQ(*(int*)Queue_get(&src, i), *(int*)Queue_get(&dst, i));
    }
    Queue_destroy(&dst);

    /* move */
    Queue moved;
    Queue_move(&moved, &src);
    EXPECT_EQ(Queue_size(&moved), 4u);
    EXPECT_EQ(Queue_size(&src), 0u); /* zeroed */
    Queue_destroy(&moved);
    Queue_destroy(&src); /* safe on zeroed */

    wc_test_alloc_destroy(&ta);
}

UTEST(queue, arena_wrap)
{
    /* wrap-around resize on an arena — the plan's exit condition */
    Arena a;
    Arena_create(&a, WC_LIBC, 8192);

    Queue q = Queue_create(Arena_allocator(&a), 4, sizeof(int), NULL);

    /* interleave pushes and pops to force wrap-around */
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 8; i++) {
            int v = (round * 10) + i;
            Queue_push(&q, &v);
        }
        for (int i = 0; i < 8; i++) {
            int out = 0;
            Queue_pop(&q, &out);
            EXPECT_EQ(out, (round * 10) + i);
        }
    }

    EXPECT_TRUE(Queue_empty(&q));
    Queue_destroy(&q);
    Arena_destroy(&a);
}
