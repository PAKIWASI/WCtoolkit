#include "common.h"
#include "wc_test.h"
#include "wc_macros.h"
#include "hashset.h"
#include "hashmap.h"
#include "queue.h"
#include "stack.h"
#include "wc_string.h"
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
    HashSet* set = HashSet_create(sizeof(int), hash_int, cmp_int, &int_move_ops);

    int* val = malloc(sizeof(int));
    *val = 42;

    SET_INSERT_MOVE(set, val);

    /* source pointer must be nulled after move */
    WC_ASSERT_NULL(val);

    /* element must be present in the set */
    int key = 42;
    WC_ASSERT_TRUE(HashSet_has(set, (u8*)&key));

    HashSet_destroy(set);
}


/* -- Phase 4: QUEUE macros ------------------------------------------------ */

static void test_Queue_macros(void)
{
    Queue* q = QUEUE_CREATE(int, 4);

    QUEUE_PUSH(q, 10);
    QUEUE_PUSH(q, 20);
    QUEUE_PUSH(q, 30);

    WC_ASSERT_EQ_INT(QUEUE_PEEK(q, int), 10);
    WC_ASSERT_EQ_INT(QUEUE_POP(q, int), 10);
    WC_ASSERT_EQ_INT(QUEUE_POP(q, int), 20);
    WC_ASSERT_EQ_INT(QUEUE_POP(q, int), 30);

    Queue_destroy(q);
}

/* -- Phase 4: STACK macros ------------------------------------------------ */

static void test_Stack_macros(void)
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

    Stack_destroy(s);
}

/* -- Phase 4: MAP_GET and MAP_TRY_GET ------------------------------------- */

static void test_map_get_and_try_get(void)
{
    HashMap* m = HashMap_create(sizeof(int), sizeof(int), hash_int, cmp_int, NULL, NULL);

    int k1 = 1, v1 = 10;
    int k2 = 2, v2 = 20;
    HashMap_put(m, (u8*)&k1, (u8*)&v1);
    HashMap_put(m, (u8*)&k2, (u8*)&v2);

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

    HashMap_destroy(m);
}

/* -- Phase 4: SET_FOREACH and SET_FROM_VEC -------------------------------- */

static void test_set_foreach_and_from_vec(void)
{
    GenVec* v = GenVec_create(4, sizeof(int), NULL);
    for (int i = 1; i <= 3; i++) {
        GenVec_push(v, (u8*)&i);
    }

    HashSet* s = SET_FROM_VEC(v, hash_int, cmp_int);
    WC_ASSERT_EQ_U64(HashSet_size(s), 3);

    int sum = 0;
    SET_FOREACH(s, int, elm) {
        sum += *elm;
    }
    WC_ASSERT_EQ_INT(sum, 6);

    HashSet_destroy(s);
    GenVec_destroy(v);
}


/* -- Suite ----------------------------------------------------------------- */

// void macros_suite(void)
// {
//     WC_SUITE("Macros");
//     WC_RUN(test_set_insert_move_compiles_and_works);
//     WC_RUN(test_Queue_macros);
//     WC_RUN(test_Stack_macros);
//     WC_RUN(test_map_get_and_try_get);
//     WC_RUN(test_set_foreach_and_from_vec);
// }

/* -- Phase 6: WC_OPS / VEC_CREATE_OF — ops picked from T ------------------- */

static void test_create_of_pod_uses_null_ops(void)
{
    GenVec* v = VEC_CREATE_OF(int, 8);
    WC_ASSERT_NOT_NULL(v);

    int x = 42;
    GenVec_push(v, (u8*)&x);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 0), 42);

    GenVec_destroy(v);
}

static void test_create_of_String_by_value(void)
{
    /* sizeof(String) bytes per slot; wc_str_ops supplies the del_fn that frees
     * the heap str on destroy. */
    GenVec* v = VEC_CREATE_OF(String, 4);
    WC_ASSERT_NOT_NULL(v);
    WC_ASSERT_EQ_U64(v->data_size, sizeof(String));

    VEC_PUSH_CSTR(v, "hello");
    VEC_PUSH_CSTR(v, "world");
    WC_ASSERT_EQ_U64(v->size, 2);

    GenVec_destroy(v);
}

static void test_create_of_String_by_pointer(void)
{
    /* String* slots (8 bytes); wc_str_ptr_ops frees each heap String on
     * destroy. */
    GenVec* v = VEC_CREATE_OF(String*, 4);
    WC_ASSERT_NOT_NULL(v);
    WC_ASSERT_EQ_U64(v->data_size, sizeof(String*));

    String* a = String_from_cstr("a");
    String* b = String_from_cstr("b");
    VEC_PUSH(v, a);
    VEC_PUSH(v, b);
    WC_ASSERT_EQ_U64(v->size, 2);

    GenVec_destroy(v);
}

static void test_map_create_of(void)
{
    HashMap* m = MAP_CREATE_OF(int, double);
    WC_ASSERT_NOT_NULL(m);

    int    k   = 7;
    double val = 3.5;
    MAP_PUT(m, k, val);

    double out = 0;
    WC_ASSERT_TRUE(MAP_TRY_GET(m, int, k, &out));
    WC_ASSERT_TRUE(out == 3.5);

    HashMap_destroy(m);
}


/* -- Phase 6: WC_ASSERT_ELEM_SIZE — element-size guard in typed macros ----- */

static void test_vec_at_asserts_elem_size_passes_correct_t(void)
{
    /* The assert fires only on mismatch, so VEC_AT with the right T must run
     * the check and still return the element. */
    GenVec* v = GenVec_create(4, sizeof(int), NULL);
    for (int i = 1; i <= 3; i++) {
        GenVec_push(v, (u8*)&i);
    }

    int sum = VEC_AT(v, int, 0) + VEC_AT(v, int, 1) + VEC_AT(v, int, 2);
    WC_ASSERT_EQ_INT(sum, 6);

    *VEC_AT_MUT(v, int, 0) = 10;
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 0), 10);

    GenVec_destroy(v);
}

static void test_vec_front_back_assert_elem_size(void)
{
    GenVec* v = GenVec_create(4, sizeof(int), NULL);
    for (int i = 1; i <= 3; i++) {
        GenVec_push(v, (u8*)&i);
    }

    WC_ASSERT_EQ_INT(VEC_FRONT(v, int), 1);
    WC_ASSERT_EQ_INT(VEC_BACK(v, int), 3);

    GenVec_destroy(v);
}


/* -- Phase 6: VEC_FOREACH — leading if/else usage --------------------------- */

static void test_vec_foreach_if_else_prefix(void)
{
    GenVec* v = GenVec_create(4, sizeof(int), NULL);
    for (int i = 1; i <= 3; i++) {
        GenVec_push(v, (u8*)&i);
    }

    int count = 0;
    // leading if/else must still compile: the foreach is a plain for loop with
    // no dangling-statement traps
    if (v->size > 0) {
        VEC_FOREACH(v, int, p) {
            count++;
        }
    } else {
        count = -1;
    }

    WC_ASSERT_EQ_INT(count, 3);

    GenVec_destroy(v);
}


/* -- Phase 6: GenVec_get_ptr_unsafe / mut_unsafe — in-range use ------------- */

static void test_genvec_unsafe_getters_in_range(void)
{
    GenVec* v = GenVec_create(4, sizeof(int), NULL);
    for (int i = 1; i <= 3; i++) {
        GenVec_push(v, (u8*)&i);
    }

    const u8* cp = GenVec_get_ptr_unsafe(v, 1);
    WC_ASSERT_EQ_INT(*(const int*)cp, 2);

    u8* mp = GenVec_get_ptr_mut_unsafe(v, 2);
    *(int*)mp = 30;
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 2), 30);

    GenVec_destroy(v);
}


/* -- Suite ----------------------------------------------------------------- */

void macros_suite(void)
{
    WC_SUITE("Macros");
    WC_RUN(test_set_insert_move_compiles_and_works);
    WC_RUN(test_Queue_macros);
    WC_RUN(test_Stack_macros);
    WC_RUN(test_map_get_and_try_get);
    WC_RUN(test_set_foreach_and_from_vec);
    WC_RUN(test_create_of_pod_uses_null_ops);
    WC_RUN(test_create_of_String_by_value);
    WC_RUN(test_create_of_String_by_pointer);
    WC_RUN(test_map_create_of);
    WC_RUN(test_vec_at_asserts_elem_size_passes_correct_t);
    WC_RUN(test_vec_front_back_assert_elem_size);
    WC_RUN(test_vec_foreach_if_else_prefix);
    WC_RUN(test_genvec_unsafe_getters_in_range);
}
