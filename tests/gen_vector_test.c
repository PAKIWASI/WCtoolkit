#include "gen_vector.h"
#include "wc_errno.h"
#include "wc_macros.h"
#include "wc_helpers.h"
#include "wc_test.h"


// Helpers 

// Simple int vec — no copy/move/del needed
static genVec* int_vec(u64 cap)
{
    return genVec_init(cap, sizeof(int), NULL);
}

static void push_ints(genVec* v, int count)
{
    for (int i = 0; i < count; i++) {
        genVec_push(v, (u8*)&i);
    }
}


// Init 

static void test_init_zero_cap(void)
{
    genVec* v = genVec_init(0, sizeof(int), NULL);
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
    int     val = 42;
    genVec* v   = genVec_init_val(5, (u8*)&val, sizeof(int), NULL);
    WC_ASSERT_EQ_U64(genVec_size(v), 5);
    for (u64 i = 0; i < 5; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, i), 42);
    }
    genVec_destroy(v);
}

static void test_init_stk(void)
{
    genVec v;
    genVec_init_stk(4, sizeof(int), NULL, &v);
    WC_ASSERT_EQ_U64(genVec_size(&v), 0);
    WC_ASSERT_EQ_U64(genVec_capacity(&v), 4);
    genVec_destroy_stk(&v);
}

static void test_init_arr(void)
{
    genVec* v = VEC_FROM_ARR(int, 4, ((int[4]){1, 2, 3, 4}));

    WC_ASSERT_EQ_U64(genVec_size(v), 4);
    WC_ASSERT_EQ_U64(v->data_size, sizeof(int));

    genVec_destroy(v);
}


// Push / Pop 

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
    genVec* v   = int_vec(4);
    int     val = 99;
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


// Get 

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
    genVec* v   = int_vec(4);
    int     val = 7;
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
    WC_ASSERT_EQ_INT(*(int*)genVec_back(v), 3);
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


// Insert / Remove 

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
    push_ints(v, 6);              /* 0 1 2 3 4 5 */
    genVec_remove_range(v, 1, 3); /* remove 1,2,3 */
    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 4);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 5);
    genVec_destroy(v);
}


// Replace 

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


// Reserve 

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
    genVec* v   = int_vec(0);
    int     val = 5;
    genVec_reserve_val(v, 10, (u8*)&val);
    WC_ASSERT_EQ_U64(genVec_size(v), 10);
    for (u64 i = 0; i < 10; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, i), 5);
    }
    genVec_destroy(v);
}


// Clear / Reset 

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


// Copy / Move 

static void test_copy(void)
{
    genVec* src = int_vec(4);
    push_ints(src, 4);

    genVec dest;
    genVec_init_stk(0, sizeof(int), NULL, &dest);
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
    genVec_init_stk(0, sizeof(int), NULL, &dest);
    genVec_move(&dest, &src);

    WC_ASSERT_NULL(src);
    WC_ASSERT_EQ_U64(genVec_size(&dest), 4);
    genVec_destroy_stk(&dest);
}


// insert_multi 

static void test_insert_multi(void)
{
    genVec* v     = int_vec(4);
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

// swap_pop 

static void test_swap_pop_middle(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); } /* 0 1 2 3 */
    int out = 0;
    genVec_swap_pop(v, 1, (u8*)&out); /* remove 1, last (3) fills its slot */
    WC_ASSERT_EQ_INT(out, 1);
    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    /* element at [1] is now 3 */
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 1), 3);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 0), 0);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 2), 2);
    genVec_destroy(v);
}

static void test_swap_pop_last(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); }
    genVec_swap_pop(v, 3, NULL); /* last element — just shrinks */
    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 2), 2);
    genVec_destroy(v);
}

static void test_swap_pop_single_element(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    int x = 99;
    VEC_PUSH(v, x);
    genVec_swap_pop(v, 0, NULL);
    WC_ASSERT_EQ_U64(genVec_size(v), 0);
    genVec_destroy(v);
}


// swap 

static void test_swap_two_elements(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); } /* 0 1 2 3 */
    genVec_swap(v, 0, 3);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 0), 3);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 3), 0);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 1), 1); /* untouched */
    genVec_destroy(v);
}

static void test_swap_same_index_noop(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    for (int i = 0; i < 3; i++) { VEC_PUSH(v, i); }
    genVec_swap(v, 1, 1);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 1), 1);
    genVec_destroy(v);
}

static void test_swap_adjacent(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); }
    genVec_swap(v, 1, 2);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 1), 2);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 2), 1);
    genVec_destroy(v);
}


// find 

static void test_find_hit(void)
{
    genVec* v = genVec_init(8, sizeof(int), NULL);
    for (int i = 0; i < 8; i++) { VEC_PUSH(v, i); }
    int target = 5;
    WC_ASSERT_EQ_U64(genVec_find(v, (u8*)&target, NULL), 5);
    genVec_destroy(v);
}

static void test_find_first_occurrence(void)
{
    genVec* v = genVec_init(8, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); }
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); } /* duplicate 0..3 */
    int target = 2;
    WC_ASSERT_EQ_U64(genVec_find(v, (u8*)&target, NULL), 2); /* first occurrence */
    genVec_destroy(v);
}

static void test_find_miss(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); }
    int target = 99;
    WC_ASSERT_EQ_U64(genVec_find(v, (u8*)&target, NULL), (u64)-1);
    genVec_destroy(v);
}

static void test_find_empty_vec(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    int target = 0;
    WC_ASSERT_EQ_U64(genVec_find(v, (u8*)&target, NULL), (u64)-1);
    genVec_destroy(v);
}


// subarr 

static void test_subarr_middle(void)
{
    genVec* v = genVec_init(6, sizeof(int), NULL);
    for (int i = 0; i < 6; i++) { VEC_PUSH(v, i); } /* 0..5 */
    genVec* sub = genVec_subarr(v, 2, 3); /* [2, 3, 4] */
    WC_ASSERT_EQ_U64(genVec_size(sub), 3);
    WC_ASSERT_EQ_INT(VEC_AT(sub, int, 0), 2);
    WC_ASSERT_EQ_INT(VEC_AT(sub, int, 1), 3);
    WC_ASSERT_EQ_INT(VEC_AT(sub, int, 2), 4);
    genVec_destroy(v);
    genVec_destroy(sub);
}

static void test_subarr_clamps_to_end(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); }
    genVec* sub = genVec_subarr(v, 2, 100); /* len exceeds bounds */
    WC_ASSERT_EQ_U64(genVec_size(sub), 2); /* clamped: [2, 3] */
    genVec_destroy(v);
    genVec_destroy(sub);
}

static void test_subarr_independent(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); }
    genVec* sub = genVec_subarr(v, 0, 4);
    int x = 999;
    genVec_replace(v, 0, (u8*)&x);
    WC_ASSERT_EQ_INT(VEC_AT(sub, int, 0), 0); /* sub unaffected */
    genVec_destroy(v);
    genVec_destroy(sub);
}


// shrink_to_fit 

static void test_shrink_to_fit_reduces_capacity(void)
{
    genVec* v = genVec_init(100, sizeof(int), NULL);
    for (int i = 0; i < 5; i++) { VEC_PUSH(v, i); }
    genVec_shrink_to_fit(v);
    WC_ASSERT_TRUE(genVec_capacity(v) <= 10); /* <= max(5, GENVEC_MIN_CAPACITY) */
    WC_ASSERT_EQ_U64(genVec_size(v), 5);
    for (int i = 0; i < 5; i++) {
        WC_ASSERT_EQ_INT(VEC_AT(v, int, (u64)i), i);
    }
    genVec_destroy(v);
}

static void test_shrink_to_fit_already_tight(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); }
    u64 cap_before = genVec_capacity(v);
    genVec_shrink_to_fit(v);
    WC_ASSERT_EQ_U64(genVec_capacity(v), cap_before);
    genVec_destroy(v);
}


// push_move 

static void test_push_move_nulls_src(void)
{
    genVec* v  = VEC_OF_STR(4);
    String* s  = string_from_cstr("owned");
    genVec_push_move(v, (u8**)&s);
    WC_ASSERT_NULL(s);
    WC_ASSERT_EQ_U64(genVec_size(v), 1);
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 0), "owned"));
    genVec_destroy(v);
}


// insert_move 

static void test_insert_move_front(void)
{
    genVec* v = VEC_OF_STR(4);
    VEC_PUSH_CSTR(v, "b");
    VEC_PUSH_CSTR(v, "c");
    String* s = string_from_cstr("a");
    genVec_insert_move(v, 0, (u8**)&s);
    WC_ASSERT_NULL(s);
    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 0), "a"));
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 1), "b"));
    genVec_destroy(v);
}


// replace_move 

static void test_replace_move_frees_old(void)
{
    genVec* v = VEC_OF_STR(4);
    VEC_PUSH_CSTR(v, "old");
    String* s = string_from_cstr("new");
    genVec_replace_move(v, 0, (u8**)&s);
    WC_ASSERT_NULL(s);
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 0), "new"));
    genVec_destroy(v);
}


// remove with out-copy 

static void test_remove_with_out(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); } /* 0 1 2 3 */
    int out = 0;
    genVec_remove(v, 1, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 1);
    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    genVec_destroy(v);
}


// init_val_stk 

static void test_init_val_stk(void)
{
    genVec v;
    int val = 7;
    genVec_init_val_stk(5, (u8*)&val, sizeof(int), NULL, &v);
    WC_ASSERT_EQ_U64(genVec_size(&v), 5);
    for (u64 i = 0; i < 5; i++) {
        WC_ASSERT_EQ_INT(VEC_AT(&v, int, i), 7);
    }
    genVec_destroy_stk(&v);
}


// VEC_FOREACH 

static void test_vec_foreach_mutates(void)
{
    genVec* v = genVec_init(4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(v, i); }
    VEC_FOREACH(v, int, p) { (*p) *= 2; }
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 0), 0);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 1), 2);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 2), 4);
    WC_ASSERT_EQ_INT(VEC_AT(v, int, 3), 6);
    genVec_destroy(v);
}

static void test_vec_foreach_empty(void)
{
    genVec* v    = genVec_init(4, sizeof(int), NULL);
    int     count = 0;
    VEC_FOREACH(v, int, p) { count++; (void)p; }
    WC_ASSERT_EQ_INT(count, 0);
    genVec_destroy(v);
}



// Suite entry point 

extern void gen_vector_suite(void)
{
    WC_SUITE("genVec");

    /* init */
    WC_RUN(test_init_zero_cap);
    WC_RUN(test_init_with_cap);
    WC_RUN(test_init_val);
    WC_RUN(test_init_stk);
    WC_RUN(test_init_arr);

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

    // new tests
    WC_RUN(test_swap_pop_middle);
    WC_RUN(test_swap_pop_last);
    WC_RUN(test_swap_pop_single_element);

    WC_RUN(test_swap_two_elements);
    WC_RUN(test_swap_same_index_noop);
    WC_RUN(test_swap_adjacent);

    WC_RUN(test_find_hit);
    WC_RUN(test_find_first_occurrence);
    WC_RUN(test_find_miss);
    WC_RUN(test_find_empty_vec);

    WC_RUN(test_subarr_middle);
    WC_RUN(test_subarr_clamps_to_end);
    WC_RUN(test_subarr_independent);

    WC_RUN(test_shrink_to_fit_reduces_capacity);
    WC_RUN(test_shrink_to_fit_already_tight);

    WC_RUN(test_push_move_nulls_src);
    WC_RUN(test_insert_move_front);
    WC_RUN(test_replace_move_frees_old);
    WC_RUN(test_remove_with_out);
    WC_RUN(test_init_val_stk);

    WC_RUN(test_vec_foreach_mutates);
    WC_RUN(test_vec_foreach_empty);
}
