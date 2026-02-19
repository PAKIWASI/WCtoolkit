#include "wc_test.h"
#include "gen_vector.h"
#include "wc_errno.h"


/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Simple int vec — no copy/move/del needed */
static genVec* int_vec(u64 cap) {
    return genVec_init(cap, sizeof(int), NULL, NULL, NULL);
}

static void push_ints(genVec* v, int count) {
    for (int i = 0; i < count; i++) {
        genVec_push(v, (u8*)&i);
    }
}


/* ── Init ────────────────────────────────────────────────────────────────── */

static void test_init_zero_cap(void)
{
    genVec* v = genVec_init(0, sizeof(int), NULL, NULL, NULL);
    WC_ASSERT_NOT_NULL(v);
    WC_ASSERT_EQ_U64(genVec_size(v), 0);
    WC_ASSERT_EQ_U64(genVec_capacity(v), 0);
    WC_ASSERT_TRUE(genVec_empty(v));
    genVec_destroy(v);
}

static void test_init_with_cap(void)
{
    genVec* v = int_vec(8);
    WC_ASSERT_EQ_U64(genVec_size(v), 0);
    WC_ASSERT_EQ_U64(genVec_capacity(v), 8);
    genVec_destroy(v);
}

static void test_init_val(void)
{
    int val  = 42;
    genVec* v = genVec_init_val(5, (u8*)&val, sizeof(int), NULL, NULL, NULL);
    WC_ASSERT_EQ_U64(genVec_size(v), 5);
    for (u64 i = 0; i < 5; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, i), 42);
    }
    genVec_destroy(v);
}

static void test_init_stk(void)
{
    genVec v;
    genVec_init_stk(4, sizeof(int), NULL, NULL, NULL, &v);
    WC_ASSERT_EQ_U64(genVec_size(&v), 0);
    WC_ASSERT_EQ_U64(genVec_capacity(&v), 4);
    genVec_destroy_stk(&v);
}


/* ── Push / Pop ──────────────────────────────────────────────────────────── */

static void test_push_grows_size(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 3);
    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    genVec_destroy(v);
}

static void test_push_triggers_growth(void)
{
    genVec* v = int_vec(2);
    push_ints(v, 10); /* force multiple reallocations */
    WC_ASSERT_EQ_U64(genVec_size(v), 10);
    /* values must survive realloc */
    for (int i = 0; i < 10; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, (u64)i), i);
    }
    genVec_destroy(v);
}

static void test_pop_reduces_size(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 3);
    genVec_pop(v, NULL);
    WC_ASSERT_EQ_U64(genVec_size(v), 2);
    genVec_destroy(v);
}

static void test_pop_copies_value(void)
{
    genVec* v = int_vec(4);
    int val   = 99;
    genVec_push(v, (u8*)&val);
    int out = 0;
    genVec_pop(v, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 99);
    genVec_destroy(v);
}

static void test_pop_empty_sets_errno(void)
{
    genVec* v = int_vec(4);
    wc_errno  = WC_OK;
    genVec_pop(v, NULL);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_EMPTY);
    genVec_destroy(v);
}


/* ── Get ─────────────────────────────────────────────────────────────────── */

static void test_get_ptr(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 4);
    for (int i = 0; i < 4; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, (u64)i), i);
    }
    genVec_destroy(v);
}

static void test_get_copies(void)
{
    genVec* v = int_vec(4);
    int val   = 7;
    genVec_push(v, (u8*)&val);
    int out = 0;
    genVec_get(v, 0, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 7);
    genVec_destroy(v);
}

static void test_front_back(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 4); /* 0 1 2 3 */
    WC_ASSERT_EQ_INT(*(int*)genVec_front(v), 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_back(v),  3);
    genVec_destroy(v);
}

static void test_front_empty_sets_errno(void)
{
    genVec* v = int_vec(4);
    wc_errno  = WC_OK;
    genVec_front(v);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_EMPTY);
    genVec_destroy(v);
}


/* ── Insert / Remove ─────────────────────────────────────────────────────── */

static void test_insert_front(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 3); /* 0 1 2 */
    int x = 99;
    genVec_insert(v, 0, (u8*)&x);
    WC_ASSERT_EQ_U64(genVec_size(v), 4);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 99);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 0);
    genVec_destroy(v);
}

static void test_insert_mid(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 4); /* 0 1 2 3 */
    int x = 55;
    genVec_insert(v, 2, (u8*)&x);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 55);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 3), 2);
    genVec_destroy(v);
}

static void test_remove_front(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 3); /* 0 1 2 */
    genVec_remove(v, 0, NULL);
    WC_ASSERT_EQ_U64(genVec_size(v), 2);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 1);
    genVec_destroy(v);
}

static void test_remove_mid(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 4); /* 0 1 2 3 */
    genVec_remove(v, 1, NULL);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 2);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 3);
    genVec_destroy(v);
}

static void test_remove_range(void)
{
    genVec* v = int_vec(8);
    push_ints(v, 6); /* 0 1 2 3 4 5 */
    genVec_remove_range(v, 1, 3); /* remove 1,2,3 */
    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 4);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 5);
    genVec_destroy(v);
}


/* ── Replace ─────────────────────────────────────────────────────────────── */

static void test_replace(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 3); /* 0 1 2 */
    int x = 77;
    genVec_replace(v, 1, (u8*)&x);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 77);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 2);
    genVec_destroy(v);
}


/* ── Reserve ─────────────────────────────────────────────────────────────── */

static void test_reserve_grows_capacity(void)
{
    genVec* v = int_vec(4);
    genVec_reserve(v, 100);
    WC_ASSERT_TRUE(genVec_capacity(v) >= 100);
    WC_ASSERT_EQ_U64(genVec_size(v), 0); /* size unchanged */
    genVec_destroy(v);
}

static void test_reserve_does_not_shrink(void)
{
    genVec* v = int_vec(100);
    genVec_reserve(v, 4);
    WC_ASSERT_TRUE(genVec_capacity(v) >= 100);
    genVec_destroy(v);
}

static void test_reserve_val(void)
{
    genVec* v = int_vec(0);
    int val   = 5;
    genVec_reserve_val(v, 10, (u8*)&val);
    WC_ASSERT_EQ_U64(genVec_size(v), 10);
    for (u64 i = 0; i < 10; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, i), 5);
    }
    genVec_destroy(v);
}


/* ── Clear / Reset ───────────────────────────────────────────────────────── */

static void test_clear_keeps_capacity(void)
{
    genVec* v = int_vec(8);
    push_ints(v, 5);
    u64 cap_before = genVec_capacity(v);
    genVec_clear(v);
    WC_ASSERT_EQ_U64(genVec_size(v), 0);
    WC_ASSERT_EQ_U64(genVec_capacity(v), cap_before);
    genVec_destroy(v);
}

static void test_reset_frees_memory(void)
{
    genVec* v = int_vec(8);
    push_ints(v, 5);
    genVec_reset(v);
    WC_ASSERT_EQ_U64(genVec_size(v), 0);
    WC_ASSERT_EQ_U64(genVec_capacity(v), 0);
    WC_ASSERT_NULL(v->data);
    genVec_destroy(v);
}


/* ── Copy / Move ─────────────────────────────────────────────────────────── */

static void test_copy(void)
{
    genVec* src = int_vec(4);
    push_ints(src, 4);

    genVec dest;
    genVec_init_stk(0, sizeof(int), NULL, NULL, NULL, &dest);
    genVec_copy(&dest, src);

    WC_ASSERT_EQ_U64(genVec_size(&dest), 4);
    for (int i = 0; i < 4; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(&dest, (u64)i), i);
    }

    /* independence: modify src, dest must be unaffected */
    int x = 999;
    genVec_replace(src, 0, (u8*)&x);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(&dest, 0), 0);

    genVec_destroy(src);
    genVec_destroy_stk(&dest);
}

static void test_move_nulls_src(void)
{
    genVec* src = int_vec(4);
    push_ints(src, 4);

    genVec dest;
    genVec_init_stk(0, sizeof(int), NULL, NULL, NULL, &dest);
    genVec_move(&dest, &src);

    WC_ASSERT_NULL(src);
    WC_ASSERT_EQ_U64(genVec_size(&dest), 4);
    genVec_destroy_stk(&dest);
}


/* ── insert_multi ────────────────────────────────────────────────────────── */

static void test_insert_multi(void)
{
    genVec* v   = int_vec(4);
    int     arr[] = {10, 20, 30};
    genVec_insert_multi(v, 0, (u8*)arr, 3);
    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 10);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 20);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 30);
    genVec_destroy(v);
}

static void test_insert_multi_mid(void)
{
    genVec* v = int_vec(8);
    push_ints(v, 3); /* 0 1 2 */
    int arr[] = {10, 20};
    genVec_insert_multi(v, 1, (u8*)arr, 2);
    /* expected: 0 10 20 1 2 */
    WC_ASSERT_EQ_U64(genVec_size(v), 5);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 10);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 20);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 3), 1);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 4), 2);
    genVec_destroy(v);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void gen_vector_suite(void)
{
    WC_SUITE("genVec");

    /* init */
    WC_RUN(test_init_zero_cap);
    WC_RUN(test_init_with_cap);
    WC_RUN(test_init_val);
    WC_RUN(test_init_stk);

    /* push / pop */
    WC_RUN(test_push_grows_size);
    WC_RUN(test_push_triggers_growth);
    WC_RUN(test_pop_reduces_size);
    WC_RUN(test_pop_copies_value);
    WC_RUN(test_pop_empty_sets_errno);

    /* get */
    WC_RUN(test_get_ptr);
    WC_RUN(test_get_copies);
    WC_RUN(test_front_back);
    WC_RUN(test_front_empty_sets_errno);

    /* insert / remove */
    WC_RUN(test_insert_front);
    WC_RUN(test_insert_mid);
    WC_RUN(test_remove_front);
    WC_RUN(test_remove_mid);
    WC_RUN(test_remove_range);

    /* replace */
    WC_RUN(test_replace);

    /* reserve */
    WC_RUN(test_reserve_grows_capacity);
    WC_RUN(test_reserve_does_not_shrink);
    WC_RUN(test_reserve_val);

    /* clear / reset */
    WC_RUN(test_clear_keeps_capacity);
    WC_RUN(test_reset_frees_memory);

    /* copy / move */
    WC_RUN(test_copy);
    WC_RUN(test_move_nulls_src);

    /* insert_multi */
    WC_RUN(test_insert_multi);
    WC_RUN(test_insert_multi_mid);
}
