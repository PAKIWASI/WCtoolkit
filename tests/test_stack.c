#include "arena.h"
#include "common.h"
#include "queue.h"
#include "stack.h"
#include "wc_allocator.h"
#include "wc_errno.h"
#include "wc_macros.h"
#include "test_support.h"
#include "wc_test_allocator.h"



/* ═══════════════════════════════════════════════════════════════════════════
 * STACK
 * ═══════════════════════════════════════════════════════════════════════════ */

static Stack int_Stack(u64 cap)
{
    return Stack_create(WC_LIBC, cap, sizeof(int), NULL);
}

UTEST(stack, push_peek)
{
    Stack s = int_Stack(4);
    int   x = 10;
    Stack_push(&s, &x);
    EXPECT_EQ(*(int*)Stack_peek_ptr(&s), 10);
    Stack_destroy(&s);
}

UTEST(stack, push_pop_lifo)
{
    Stack s      = int_Stack(4);
    int   vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++)
        Stack_push(&s, &vals[i]);

    int out;
    Stack_pop(&s, &out);
    EXPECT_EQ(out, 3);
    Stack_pop(&s, &out);
    EXPECT_EQ(out, 2);
    Stack_pop(&s, &out);
    EXPECT_EQ(out, 1);
    Stack_destroy(&s);
}

UTEST(stack, pop_empty_sets_errno)
{
    Stack s  = int_Stack(4);
    wc_errno = WC_OK;
    Stack_pop(&s, NULL);
    EXPECT_EQ(wc_errno, (wc_err)WC_ERR_EMPTY);
    Stack_destroy(&s);
}

UTEST(stack, peek_empty_sets_errno)
{
    Stack s   = int_Stack(4);
    int   out = 0;
    wc_errno  = WC_OK;
    Stack_peek(&s, &out);
    EXPECT_EQ(wc_errno, (wc_err)WC_ERR_EMPTY);
    Stack_destroy(&s);
}

UTEST(stack, peek_ptr_empty_sets_errno)
{
    Stack     s = int_Stack(4);
    wc_errno    = WC_OK;
    const u8* p = Stack_peek_ptr(&s);
    EXPECT_TRUE((p) == NULL);
    EXPECT_EQ(wc_errno, (wc_err)WC_ERR_EMPTY);
    Stack_destroy(&s);
}

UTEST(stack, size)
{
    Stack s = int_Stack(4);
    EXPECT_EQ(Stack_size(&s), 0u);
    EXPECT_TRUE(Stack_empty(&s));
    int x = 1;
    Stack_push(&s, &x);
    EXPECT_EQ(Stack_size(&s), 1u);
    EXPECT_FALSE(Stack_empty(&s));
    Stack_destroy(&s);
}

UTEST(stack, clear)
{
    Stack s = int_Stack(4);
    int   x = 5;
    for (int i = 0; i < 4; i++)
        Stack_push(&s, &x);
    Stack_clear(&s);
    EXPECT_EQ(Stack_size(&s), 0u);
    EXPECT_TRUE(Stack_empty(&s));
    Stack_destroy(&s);
}

UTEST(stack, growth)
{
    Stack s = int_Stack(2);
    for (int i = 0; i < 20; i++)
        Stack_push(&s, &i);
    EXPECT_EQ(Stack_size(&s), 20u);
    /* LIFO: last pushed = 19 */
    EXPECT_EQ(*(int*)Stack_peek_ptr(&s), 19);
    Stack_destroy(&s);
}

UTEST(stack, copy_move)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    Stack src = Stack_create(a, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++)
        Stack_push(&src, &i);

    /* copy into libc */
    Stack dst = Stack_copy(WC_LIBC, &src);
    EXPECT_EQ(Stack_size(&dst), Stack_size(&src));
    for (u64 i = 0; i < Stack_size(&src); i++) {
        EXPECT_EQ(VEC_AT(&src, int, i), VEC_AT(&dst, int, i));
    }
    Stack_destroy(&dst);

    /* move */
    Stack moved;
    Stack_move(&moved, &src);
    EXPECT_EQ(Stack_size(&moved), 4u);
    EXPECT_EQ(Stack_size(&src), 0u); /* zeroed */
    Stack_destroy(&moved);
    Stack_destroy(&src); /* safe on zeroed */

    wc_test_alloc_destroy(&ta);
}

UTEST(stack, arena)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 4096);

    Stack s = Stack_create(Arena_allocator(&a), 8, sizeof(int), NULL);
    for (int i = 0; i < 8; i++)
        Stack_push(&s, &i);

    EXPECT_EQ(Stack_size(&s), 8u);
    EXPECT_EQ(*(int*)Stack_peek_ptr(&s), 7);

    Stack_destroy(&s);
    Arena_destroy(&a);
}
