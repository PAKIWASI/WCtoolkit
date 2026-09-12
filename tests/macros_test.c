#include "wc_test.h"
#include "wc_macros.h"
#include "wc_helpers.h"
#include "wc_errno.h"
#include "hashset.h"
#include "hashmap.h"
#include "Queue.h"
#include "Stack.h"
#include "String.h"
#include "gen_vector.h"
#include <stdlib.h>
#include <string.h>

/* -- Helpers --------------------------------------------------------------- */

static u64 hash_int(const u8* key, u64 size)
{
    (void)size;
    return (u64)(*(const int*)key);
}

static int cmp_int(const u8* a, const u8* b, u64 size)
{
    (void)size;
    return (*(const int*)a) - (*(const int*)b);
}


/* -- Phase 1-D: SET_INSERT_MOVE ------------------------------------------- */

/* move: copy the int value into the set slot, free the heap source, null it */
static void int_move(u8* dest, u8** src)
{
    memcpy(dest, *src, sizeof(int));
    free(*src);
    *src = NULL;
}

/* del: the set stores the int by value; nothing extra to free */
static void int_del(u8* elm)
{
    (void)elm;
}

static const container_ops int_move_ops = { NULL, int_move, int_del };

/*
 * Before the fix, SET_INSERT_MOVE referenced (vec) instead of (set),
 * causing a compile error or silently binding to an unrelated variable.
 * Verifies the macro compiles and has the correct effect.
 */
static void test_set_insert_move_compiles_and_works(void)
{
    hashset* set = hashset_create(sizeof(int), hash_int, cmp_int, &int_move_ops);

    int* val = malloc(sizeof(int));
    *val = 42;

    SET_INSERT_MOVE(set, val);

    /* source pointer must be nulled after move */
    WC_ASSERT_NULL(val);

    /* element must be present in the set */
    int key = 42;
    WC_ASSERT_TRUE(hashset_has(set, (u8*)&key));

    hashset_destroy(set);
}


/* -- Phase 4: QUEUE macros ------------------------------------------------ */

static void test_queue_macros(void)
{
    Queue* q = QUEUE_CREATE(int, 4);

    QUEUE_PUSH(q, 10);
    QUEUE_PUSH(q, 20);
    QUEUE_PUSH(q, 30);

    WC_ASSERT_EQ_INT(QUEUE_PEEK(q, int), 10);
    WC_ASSERT_EQ_INT(QUEUE_POP(q, int), 10);
    WC_ASSERT_EQ_INT(QUEUE_POP(q, int), 20);
    WC_ASSERT_EQ_INT(QUEUE_POP(q, int), 30);

    queue_destroy(q);
}

/* -- Phase 4: STACK macros ------------------------------------------------ */

static void test_stack_macros(void)
{
    Stack* s = STACK_CREATE(int, 4);

    STACK_PUSH(s, 100);
    STACK_PUSH(s, 200);

    WC_ASSERT_EQ_INT(STACK_AT(s, int, 0), 100);
    WC_ASSERT_EQ_INT(STACK_AT(s, int, 1), 200);

    int sum = 0;
    STACK_FOREACH(s, int, p) {
        sum += *p;
    }
    WC_ASSERT_EQ_INT(sum, 300);

    WC_ASSERT_EQ_INT(STACK_POP(s, int), 200);
    WC_ASSERT_EQ_INT(STACK_POP(s, int), 100);

    stack_destroy(s);
}

/* -- Phase 4: MAP_GET and MAP_TRY_GET ------------------------------------- */

static void test_map_get_and_try_get(void)
{
    hashmap* m = hashmap_create(sizeof(int), sizeof(int), hash_int, cmp_int, NULL, NULL);

    int k1 = 1, v1 = 10;
    int k2 = 2, v2 = 20;
    hashmap_put(m, (u8*)&k1, (u8*)&v1);
    hashmap_put(m, (u8*)&k2, (u8*)&v2);

    /* MAP_GET on hit */
    WC_ASSERT_EQ_INT(MAP_GET(m, int, k1), 10);
    WC_ASSERT_EQ_INT(MAP_GET(m, int, k2), 20);

    /* MAP_TRY_GET */
    int out = 0;
    b8 hit = MAP_TRY_GET(m, int, k1, &out);
    WC_ASSERT_TRUE(hit);
    WC_ASSERT_EQ_INT(out, 10);

    int miss_k = 999;
    b8 miss = MAP_TRY_GET(m, int, miss_k, &out);
    WC_ASSERT_FALSE(miss);

    /* MAP_FOREACH_KEY / VAL */
    int key_sum = 0;
    MAP_FOREACH_KEY(m, int, k) {
        key_sum += *k;
    }
    WC_ASSERT_EQ_INT(key_sum, 3);

    int val_sum = 0;
    MAP_FOREACH_VAL(m, int, v) {
        val_sum += *v;
    }
    WC_ASSERT_EQ_INT(val_sum, 30);

    hashmap_destroy(m);
}

/* -- Phase 4: SET_FOREACH and SET_FROM_VEC -------------------------------- */

static void test_set_foreach_and_from_vec(void)
{
    genVec* v = genVec_create(4, sizeof(int), NULL);
    for (int i = 1; i <= 3; i++) {
        genVec_push(v, (u8*)&i);
    }

    hashset* s = SET_FROM_VEC(v, hash_int, cmp_int);
    WC_ASSERT_EQ_U64(hashset_size(s), 3);

    int sum = 0;
    SET_FOREACH(s, int, elm) {
        sum += *elm;
    }
    WC_ASSERT_EQ_INT(sum, 6);

    hashset_destroy(s);
    genVec_destroy(v);
}


/* -- Suite ----------------------------------------------------------------- */

void macros_suite(void)
{
    WC_SUITE("Macros");
    WC_RUN(test_set_insert_move_compiles_and_works);
    WC_RUN(test_queue_macros);
    WC_RUN(test_stack_macros);
    WC_RUN(test_map_get_and_try_get);
    WC_RUN(test_set_foreach_and_from_vec);
}
