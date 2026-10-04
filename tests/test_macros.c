#include "common.h"
#include "gen_vector.h"
#include "hashmap.h"
#include "hashset.h"
#include "queue.h"
#include "stack.h"
#include "test_support.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_macros.h"
#include "wc_string.h"

#include <stdlib.h>
#include <string.h>


/* -- Helpers --------------------------------------------------------------- */

static u64 hash_int(const void* key, u64 size)
{
    (void)size;
    return (u64)(*(const int*)key);
}

static int cmp_int(const void* a, const void* b, u64 size)
{
    (void)size;
    int x = *(const int*)a, y = *(const int*)b;
    return (x > y) - (x < y); // no overflow, unlike x - y
}


/* -- Phase 1-D: SET_INSERT_MOVE ------------------------------------------- */

/* del: the set stores the int by value; nothing extra to free */
static void int_del(void* elm)
{
    (void)elm;
}

static const wc_container_ops int_move_ops = {.copy_fn = NULL, .del_fn = int_del};

/*
 * Before the fix, SET_INSERT_MOVE referenced (vec) instead of (set),
 * causing a compile error or silently binding to an unrelated variable.
 * Verifies the macro compiles and has the correct effect.
 */
UTEST(macros, set_insert_move_compiles_and_works)
{
    HashSet set = HashSet_create(WC_LIBC, sizeof(int), hash_int, cmp_int, &int_move_ops);

    int val = 42;

    SET_INSERT_MOVE(&set, val);

    /* source element must be zeroed after move */
    EXPECT_EQ(val, 0);

    /* element must be present in the set */
    int key = 42;
    EXPECT_TRUE(HashSet_has(&set, &key));

    HashSet_destroy(&set);
}


/* -- Phase 4: QUEUE macros ------------------------------------------------ */

UTEST(macros, Queue_macros)
{
    Queue q = QUEUE_CREATE(int, 4);

    QUEUE_PUSH(&q, 10);
    QUEUE_PUSH(&q, 20);
    QUEUE_PUSH(&q, 30);

    EXPECT_EQ(QUEUE_PEEK(&q, int), 10);
    EXPECT_EQ(QUEUE_POP(&q, int), 10);
    EXPECT_EQ(QUEUE_POP(&q, int), 20);
    EXPECT_EQ(QUEUE_POP(&q, int), 30);

    Queue_destroy(&q);
}

/* -- Phase 4: STACK macros ------------------------------------------------ */

UTEST(macros, Stack_macros)
{
    Stack s = STACK_CREATE(int, 4);

    STACK_PUSH(&s, 100);
    STACK_PUSH(&s, 200);

    EXPECT_EQ(STACK_AT(&s, int, 0), 100);
    EXPECT_EQ(STACK_AT(&s, int, 1), 200);

    int sum = 0;
    STACK_FOREACH (&s, int, p) {
        sum += *p;
    }
    EXPECT_EQ(sum, 300);

    EXPECT_EQ(STACK_POP(&s, int), 200);
    EXPECT_EQ(STACK_POP(&s, int), 100);

    Stack_destroy(&s);
}

/* -- Phase 4: MAP_GET and MAP_TRY_GET ------------------------------------- */

UTEST(macros, map_get_and_try_get)
{
    HashMap m = HashMap_create(WC_LIBC, sizeof(int), sizeof(int), hash_int, cmp_int, NULL, NULL);

    int k1 = 1, v1 = 10;
    int k2 = 2, v2 = 20;
    HashMap_put(&m, &k1, &v1);
    HashMap_put(&m, &k2, &v2);

    /* MAP_GET on hit */
    EXPECT_EQ(MAP_GET(&m, int, k1), 10);
    EXPECT_EQ(MAP_GET(&m, int, k2), 20);

    /* MAP_TRY_GET */
    int out = 0;
    bool  hit = MAP_TRY_GET(&m, int, k1, &out);
    EXPECT_TRUE(hit);
    EXPECT_EQ(out, 10);

    int miss_k = 999;
    bool  miss   = MAP_TRY_GET(&m, int, miss_k, &out);
    EXPECT_FALSE(miss);

    /* MAP_FOREACH_KEY / VAL */
    int key_sum = 0;
    MAP_FOREACH_KEY (&m, int, k) {
        key_sum += *k;
    }
    EXPECT_EQ(key_sum, 3);

    int val_sum = 0;
    MAP_FOREACH_VAL (&m, int, v) {
        val_sum += *v;
    }
    EXPECT_EQ(val_sum, 30);

    HashMap_destroy(&m);
}

/* -- Phase 4: SET_FOREACH and SET_FROM_VEC -------------------------------- */

UTEST(macros, set_foreach_and_from_vec)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 1; i <= 3; i++) {
        GenVec_push(&v, &i);
    }

    HashSet s = SET_FROM_VEC(&v, hash_int, cmp_int);
    EXPECT_EQ(HashSet_size(&s), 3u);

    int sum = 0;
    SET_FOREACH (&s, int, elm) {
        sum += *elm;
    }
    EXPECT_EQ(sum, 6);

    HashSet_destroy(&s);
    GenVec_destroy(&v);
}


/* -- Phase 6: WC_OPS / VEC_OF — ops picked from T ------------------- */

UTEST(macros, create_of_pod_uses_null_ops)
{
    GenVec v = VEC_OF(int, 8);

    int x = 42;
    GenVec_push(&v, &x);
    EXPECT_EQ(VEC_AT(&v, int, 0), 42);

    GenVec_destroy(&v);
}

UTEST(macros, create_of_String_by_value)
{
    /* sizeof(String) bytes per slot; wc_str_ops supplies the del_fn that frees
     * the heap str on destroy. */
    GenVec v = VEC_OF(String, 4);
    EXPECT_EQ(v.data_size, sizeof(String));

    VEC_PUSH_CSTR(&v, "hello");
    VEC_PUSH_CSTR(&v, "world");
    EXPECT_EQ(v.size, 2u);

    GenVec_destroy(&v);
}

UTEST(macros, create_of_String_by_pointer)
{
    /* String* slots (8 bytes); wc_str_ptr_ops frees each heap String on
     * destroy. */
    GenVec v = VEC_OF(String*, 4);
    EXPECT_EQ(v.data_size, sizeof(String*));

    String* a = WC_BOX_IN(WC_LIBC, String, String_from_cstr, "a");
    String* b = WC_BOX_IN(WC_LIBC, String, String_from_cstr, "b");
    // VEC_PUSH copies (via wc_str_ptr_ops' copy_fn, which deep-duplicates
    // the String) — that would leave `a`/`b` themselves un-freed and
    // unowned. VEC_PUSH_MOVE transfers ownership into the vector instead,
    // matching the "frees each heap String on destroy" intent above.
    VEC_PUSH_MOVE(&v, a);
    VEC_PUSH_MOVE(&v, b);
    EXPECT_EQ(v.size, 2u);

    GenVec_destroy(&v);
}

UTEST(macros, map_of_string_keys_hash_content)
{
    // Regression: MAP_OF(String, V) used NULL hash/cmp, so it hashed the raw
    // String struct and every lookup with an equal-but-distinct key missed.
    HashMap m = MAP_OF(String, int);

    String k1 = String_from_cstr(WC_LIBC, "key");
    int    v  = 1;
    HashMap_put_move(&m, &k1, &v);
    String big = String_from_cstr(WC_LIBC, "a key longer than the inline buffer!!");
    v          = 2;
    HashMap_put_move(&m, &big, &v);

    String q1 = String_create(WC_LIBC); // built differently, same content
    String_append_cstr(&q1, "k");
    String_append_cstr(&q1, "ey");
    String q2 = String_from_cstr(WC_LIBC, "a key longer than the inline buffer!!");

    const int* r1 = HashMap_get_ptr(&m, &q1);
    const int* r2 = HashMap_get_ptr(&m, &q2);
    EXPECT_TRUE((r1) != NULL);
    EXPECT_TRUE((r2) != NULL);
    EXPECT_EQ(*r1, 1);
    EXPECT_EQ(*r2, 2);

    String_destroy(&q1);
    String_destroy(&q2);
    HashMap_destroy(&m);
}

UTEST(macros, map_create_of)
{
    HashMap m = MAP_OF(int, double);

    int    k   = 7;
    double val = 3.5;
    MAP_PUT(&m, k, val);

    double out = 0;
    EXPECT_TRUE(MAP_TRY_GET(&m, double, k, &out));
    EXPECT_TRUE(out == 3.5);

    HashMap_destroy(&m);
}


/* -- Phase 6: WC_ASSERT_ELEM_SIZE — element-size guard in typed macros ----- */

UTEST(macros, vec_at_asserts_elem_size_passes_correct_t)
{
    /* The assert fires only on mismatch, so VEC_AT with the right T must run
     * the check and still return the element. */
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 1; i <= 3; i++) {
        GenVec_push(&v, &i);
    }

    int sum = VEC_AT(&v, int, 0) + VEC_AT(&v, int, 1) + VEC_AT(&v, int, 2);
    EXPECT_EQ(sum, 6);

    *VEC_AT_MUT(&v, int, 0) = 10;
    EXPECT_EQ(VEC_AT(&v, int, 0), 10);

    GenVec_destroy(&v);
}

UTEST(macros, vec_front_back_assert_elem_size)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 1; i <= 3; i++) {
        GenVec_push(&v, &i);
    }

    EXPECT_EQ(VEC_FRONT(&v, int), 1);
    EXPECT_EQ(VEC_BACK(&v, int), 3);

    GenVec_destroy(&v);
}


/* -- Phase 6: VEC_FOREACH — leading if/else usage --------------------------- */

UTEST(macros, vec_foreach_if_else_prefix)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 1; i <= 3; i++) {
        GenVec_push(&v, &i);
    }

    int count = 0;
    // leading if/else must still compile: the foreach is a plain for loop with
    // no dangling-statement traps
    if (v.size > 0) {
        VEC_FOREACH (&v, int, p) {
            count++;
        }
    } else {
        count = -1;
    }

    EXPECT_EQ(count, 3);

    GenVec_destroy(&v);
}


/* -- Phase 6: GenVec_get_ptr_unsafe / mut_unsafe — in-range use ------------- */

UTEST(macros, genvec_unsafe_getters_in_range)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 1; i <= 3; i++) {
        GenVec_push(&v, &i);
    }

    const u8* cp = GenVec_get_ptr_unsafe(&v, 1);
    EXPECT_EQ(*(const int*)cp, 2);

    u8* mp    = GenVec_get_ptr_mut_unsafe(&v, 2);
    *(int*)mp = 30;
    EXPECT_EQ(VEC_AT(&v, int, 2), 30);

    GenVec_destroy(&v);
}


/* -- Suite ----------------------------------------------------------------- */


/* -- Value macros check the value's size against the slot (Debug) ---------- */

#ifndef NDEBUG // the checks are WC_ASSERTs: these only die in Debug

// Each of these used to copy through a temporary of the VALUE's type and
// read past it: an int (4 bytes) into an 8-byte slot.
static void vec_push_int_into_double(void)
{
    GenVec v = VEC(double, 2);
    VEC_PUSH(&v, 1);
}

static void vec_set_int_into_double(void)
{
    GenVec v = VEC(double, 2);
    VEC_PUSH(&v, 1.0);
    VEC_SET(&v, 0, 2);
}

static void queue_push_int_into_double(void)
{
    Queue q = QUEUE_CREATE(double, 2);
    QUEUE_PUSH(&q, 1);
}

static void set_insert_int_into_u64(void)
{
    HashSet s = HashSet_create(WC_LIBC, sizeof(u64), NULL, NULL, NULL);
    SET_INSERT(&s, 1);
}

static void map_put_int_value_into_double(void)
{
    HashMap m = MAP_OF(int, double);
    MAP_PUT(&m, 1, 2);
}

static void map_put_wrong_key(void)
{
    HashMap m = MAP_OF(u64, int);
    MAP_PUT(&m, 1, 2);
}

static void map_get_wrong_value_type(void)
{
    HashMap m = MAP_OF(int, double);
    MAP_PUT(&m, 1, 2.0);
    int v = MAP_GET(&m, int, 1);
    (void)v;
}

static void map_put_move_wrong_key(void)
{
    HashMap m = MAP_OF(u64, int);
    int     k = 1, v = 2;
    MAP_PUT_MOVE(&m, k, v);
}
#endif

UTEST(macros, value_size_mismatch_dies)
{
#ifndef NDEBUG
    EXPECT_DIES(vec_push_int_into_double);
    EXPECT_DIES(vec_set_int_into_double);
    EXPECT_DIES(queue_push_int_into_double);
    EXPECT_DIES(set_insert_int_into_u64);
    EXPECT_DIES(map_put_int_value_into_double);
    EXPECT_DIES(map_put_wrong_key);
    EXPECT_DIES(map_get_wrong_value_type);
    EXPECT_DIES(map_put_move_wrong_key);
#endif
}

UTEST(macros, value_size_match_works)
{
    GenVec v = VEC(double, 2);
    VEC_PUSH(&v, 1.0);
    VEC_PUSH(&v, (double)2);
    EXPECT_EQ(VEC_AT(&v, double, 1), 2.0);
    GenVec_destroy(&v);

    HashMap m = MAP_OF(int, double);
    MAP_PUT(&m, 1, 2.5);
    EXPECT_EQ(MAP_GET(&m, double, 1), 2.5);
    HashMap_destroy(&m);
}


/* -- break and continue in *_FOREACH ------------------------------------- */

UTEST(macros, vec_foreach_break_stops)
{
    GenVec v       = VEC_FROM_ARR(int, 5, ((int[5]){1, 2, 3, 4, 5}));
    int    visited = 0;
    VEC_FOREACH (&v, int, x) {
        visited++;
        if (*x == 2) {
            break;
        }
    }
    EXPECT_EQ(visited, 2); // used to be 5: break acted like continue
    GenVec_destroy(&v);
}

UTEST(macros, vec_foreach_continue_skips)
{
    GenVec v   = VEC_FROM_ARR(int, 5, ((int[5]){1, 2, 3, 4, 5}));
    int    sum = 0;
    VEC_FOREACH (&v, int, x) {
        if (*x % 2 == 0) {
            continue;
        }
        sum += *x;
    }
    EXPECT_EQ(sum, 1 + 3 + 5);
    GenVec_destroy(&v);
}

UTEST(macros, vec_foreach_nested_break_ends_inner_only)
{
    GenVec v     = VEC_FROM_ARR(int, 3, ((int[3]){1, 2, 3}));
    int    pairs = 0;
    VEC_FOREACH (&v, int, a) {
        VEC_FOREACH (&v, int, b) {
            if (*b > *a) {
                break;
            }
            pairs++;
        }
    }
    EXPECT_EQ(pairs, 1 + 2 + 3); // b runs up to a, for each a
    GenVec_destroy(&v);
}

UTEST(macros, stack_foreach_break_stops)
{
    Stack s = STACK_CREATE(int, 4);
    for (int i = 0; i < 4; i++) {
        STACK_PUSH(&s, i);
    }
    int visited = 0;
    STACK_FOREACH (&s, int, x) {
        (void)x;
        if (++visited == 1) {
            break;
        }
    }
    EXPECT_EQ(visited, 1);
    Stack_destroy(&s);
}

UTEST(macros, map_foreach_break_stops)
{
    HashMap m = MAP_OF(int, int);
    for (int i = 0; i < 10; i++) {
        MAP_PUT(&m, i, i);
    }

    int keys = 0;
    MAP_FOREACH_KEY (&m, int, k) {
        (void)k;
        keys++;
        break;
    }
    int vals = 0;
    MAP_FOREACH_VAL (&m, int, val) {
        (void)val;
        vals++;
        break;
    }
    EXPECT_EQ(keys, 1);
    EXPECT_EQ(vals, 1);

    int all = 0;
    MAP_FOREACH_KEY (&m, int, k) {
        (void)k;
        all++;
    } // empty buckets are skipped
    EXPECT_EQ(all, 10);
    HashMap_destroy(&m);
}

UTEST(macros, set_foreach_break_stops)
{
    HashSet s = HashSet_create(WC_LIBC, sizeof(int), NULL, NULL, NULL);
    for (int i = 0; i < 10; i++) {
        SET_INSERT(&s, i);
    }
    int visited = 0;
    SET_FOREACH (&s, int, x) {
        (void)x;
        visited++;
        break;
    }
    EXPECT_EQ(visited, 1);
    HashSet_destroy(&s);
}

UTEST(macros, foreach_on_empty_runs_zero_times)
{
    GenVec  v = VEC(int, 0);
    HashMap m = MAP_OF(int, int);
    int     n = 0;
    VEC_FOREACH (&v, int, x) {
        (void)x;
        n++;
    }
    MAP_FOREACH_VAL (&m, int, x) {
        (void)x;
        n++;
    }
    EXPECT_EQ(n, 0);
    GenVec_destroy(&v);
    HashMap_destroy(&m);
}
