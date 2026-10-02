#include "arena.h"
#include "chain_arena.h"
#include "common.h"
#include "gen_vector.h"
#include "hashmap.h"
#include "queue.h"
#include "random.h"
#include "wc_allocator.h"
#include "wc_errno.h"
#include "wc_macros.h"
#include "wc_string.h"
#include "test_support.h"
#include "wc_test_allocator.h"

#include <stdalign.h>
#include <stdint.h>


// Helpers 

// Simple int vec — no copy/move/del needed
static GenVec int_vec(u64 cap)
{
    return GenVec_create(WC_LIBC, cap, sizeof(int), NULL);
}

static void push_ints(GenVec* v, int count)
{
    for (int i = 0; i < count; i++) {
        GenVec_push(v, &i);
    }
}


// Init 

UTEST(gen_vector, init_zero_cap)
{
    GenVec v = GenVec_create(WC_LIBC, 0, sizeof(int), NULL);
    EXPECT_EQ(GenVec_size(&v), 0u);
    EXPECT_EQ(GenVec_capacity(&v), 0u);
    EXPECT_TRUE(GenVec_empty(&v));
    GenVec_destroy(&v);
}

UTEST(gen_vector, init_with_cap)
{
    GenVec v = int_vec(8);
    EXPECT_EQ(GenVec_size(&v), 0u);
    EXPECT_EQ(GenVec_capacity(&v), 8u);
    GenVec_destroy(&v);
}

UTEST(gen_vector, init_val)
{
    int     val = 42;
    GenVec v   = GenVec_create_val(WC_LIBC, 5, &val, sizeof(int), NULL);
    EXPECT_EQ(GenVec_size(&v), 5u);
    for (u64 i = 0; i < 5; i++) {
        EXPECT_EQ(*(int*)GenVec_get_ptr(&v, i), 42);
    }
    GenVec_destroy(&v);
}

UTEST(gen_vector, init_stk)
{
    GenVec v;
    v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    EXPECT_EQ(GenVec_size(&v), 0u);
    EXPECT_EQ(GenVec_capacity(&v), 4u);
    GenVec_destroy(&v);
}

UTEST(gen_vector, init_arr)
{
    GenVec v = VEC_FROM_ARR(int, 4, ((int[4]){1, 2, 3, 4}));

    EXPECT_EQ(GenVec_size(&v), 4u);
    EXPECT_EQ(v.data_size, sizeof(int));

    GenVec_destroy(&v);
}


// Push / Pop 

UTEST(gen_vector, push_grows_size)
{
    GenVec v = int_vec(4);
    push_ints(&v, 3);
    EXPECT_EQ(GenVec_size(&v), 3u);
    GenVec_destroy(&v);
}

UTEST(gen_vector, push_triggers_growth)
{
    GenVec v = int_vec(2);
    push_ints(&v, 10); /* force multiple reallocations */
    EXPECT_EQ(GenVec_size(&v), 10u);
    /* values must survive realloc */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(*(int*)GenVec_get_ptr(&v, (u64)i), i);
    }
    GenVec_destroy(&v);
}

UTEST(gen_vector, pop_reduces_size)
{
    GenVec v = int_vec(4);
    push_ints(&v, 3);
    GenVec_pop(&v, NULL);
    EXPECT_EQ(GenVec_size(&v), 2u);
    GenVec_destroy(&v);
}

UTEST(gen_vector, pop_copies_value)
{
    GenVec v   = int_vec(4);
    int     val = 99;
    GenVec_push(&v, &val);
    int out = 0;
    GenVec_pop(&v, &out);
    EXPECT_EQ(out, 99);
    GenVec_destroy(&v);
}


// Get 

UTEST(gen_vector, get_ptr)
{
    GenVec v = int_vec(4);
    push_ints(&v, 4);
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(*(int*)GenVec_get_ptr(&v, (u64)i), i);
    }
    GenVec_destroy(&v);
}

UTEST(gen_vector, get_copies)
{
    GenVec v   = int_vec(4);
    int     val = 7;
    GenVec_push(&v, &val);
    int out = 0;
    GenVec_get(&v, 0, &out);
    EXPECT_EQ(out, 7);
    GenVec_destroy(&v);
}

UTEST(gen_vector, front_back)
{
    GenVec v = int_vec(4);
    push_ints(&v, 4); /* 0 1 2 3 */
    EXPECT_EQ(*(int*)GenVec_front(&v), 0);
    EXPECT_EQ(*(int*)GenVec_back(&v), 3);
    GenVec_destroy(&v);
}


// Insert / Remove 

UTEST(gen_vector, insert_front)
{
    GenVec v = int_vec(4);
    push_ints(&v, 3); /* 0 1 2 */
    int x = 99;
    GenVec_insert(&v, 0, &x);
    EXPECT_EQ(GenVec_size(&v), 4u);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 0), 99);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 1), 0);
    GenVec_destroy(&v);
}

UTEST(gen_vector, insert_mid)
{
    GenVec v = int_vec(4);
    push_ints(&v, 4); /* 0 1 2 3 */
    int x = 55;
    GenVec_insert(&v, 2, &x);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 2), 55);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 3), 2);
    GenVec_destroy(&v);
}

UTEST(gen_vector, remove_front)
{
    GenVec v = int_vec(4);
    push_ints(&v, 3); /* 0 1 2 */
    GenVec_remove(&v, 0, NULL);
    EXPECT_EQ(GenVec_size(&v), 2u);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 0), 1);
    GenVec_destroy(&v);
}

UTEST(gen_vector, remove_mid)
{
    GenVec v = int_vec(4);
    push_ints(&v, 4); /* 0 1 2 3 */
    GenVec_remove(&v, 1, NULL);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 0), 0);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 1), 2);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 2), 3);
    GenVec_destroy(&v);
}

UTEST(gen_vector, remove_range)
{
    GenVec v = int_vec(8);
    push_ints(&v, 6);              /* 0 1 2 3 4 5 */
    GenVec_remove_range(&v, 1, 3); /* remove 1,2,3 */
    EXPECT_EQ(GenVec_size(&v), 3u);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 0), 0);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 1), 4);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 2), 5);
    GenVec_destroy(&v);
}


// Replace 

UTEST(gen_vector, replace)
{
    GenVec v = int_vec(4);
    push_ints(&v, 3); /* 0 1 2 */
    int x = 77;
    GenVec_replace(&v, 1, &x);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 0), 0);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 1), 77);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 2), 2);
    GenVec_destroy(&v);
}


// Reserve 

UTEST(gen_vector, reserve_grows_capacity)
{
    GenVec v = int_vec(4);
    GenVec_reserve(&v, 100);
    EXPECT_TRUE(GenVec_capacity(&v) >= 100);
    EXPECT_EQ(GenVec_size(&v), 0u); /* size unchanged */
    GenVec_destroy(&v);
}

UTEST(gen_vector, reserve_does_not_shrink)
{
    GenVec v = int_vec(100);
    GenVec_reserve(&v, 4);
    EXPECT_TRUE(GenVec_capacity(&v) >= 100);
    GenVec_destroy(&v);
}

UTEST(gen_vector, reserve_val)
{
    GenVec v   = int_vec(0);
    int     val = 5;
    GenVec_reserve_val(&v, 10, &val);
    EXPECT_EQ(GenVec_size(&v), 10u);
    for (u64 i = 0; i < 10; i++) {
        EXPECT_EQ(*(int*)GenVec_get_ptr(&v, i), 5);
    }
    GenVec_destroy(&v);
}


// Clear / Reset 

UTEST(gen_vector, clear_keeps_capacity)
{
    GenVec v = int_vec(8);
    push_ints(&v, 5);
    u64 cap_before = GenVec_capacity(&v);
    GenVec_clear(&v);
    EXPECT_EQ(GenVec_size(&v), 0u);
    EXPECT_EQ(GenVec_capacity(&v), cap_before);
    GenVec_destroy(&v);
}

UTEST(gen_vector, reset_frees_memory)
{
    GenVec v = int_vec(8);
    push_ints(&v, 5);
    GenVec_reset(&v);
    EXPECT_EQ(GenVec_size(&v), 0u);
    EXPECT_EQ(GenVec_capacity(&v), 0u);
    EXPECT_TRUE((v.data) == NULL);
    GenVec_destroy(&v);
}


// Copy / Move 

UTEST(gen_vector, copy)
{
    GenVec src = int_vec(4);
    push_ints(&src, 4);

    GenVec dest = GenVec_copy(WC_LIBC, &src);

    EXPECT_EQ(GenVec_size(&dest), 4u);
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(*(int*)GenVec_get_ptr(&dest, (u64)i), i);
    }

    /* independence: modify src, dest must be unaffected */
    int x = 999;
    GenVec_replace(&src, 0, &x);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&dest, 0), 0);

    GenVec_destroy(&src);
    GenVec_destroy(&dest);
}

UTEST(gen_vector, copy_raw_dest)
{
    GenVec src = int_vec(4);
    push_ints(&src, 4);

    GenVec dest; /* deliberately uninitialized Stack memory */
    dest = GenVec_copy(WC_LIBC, &src);

    EXPECT_EQ(GenVec_size(&dest), 4u);
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(*(int*)GenVec_get_ptr(&dest, (u64)i), i);
    }

    GenVec_destroy(&src);
    GenVec_destroy(&dest);
}

UTEST(gen_vector, move_zeroes_src)
{
    GenVec src = int_vec(4);
    push_ints(&src, 4);

    GenVec dest;
    GenVec_move(&dest, &src);

    EXPECT_TRUE((src.data) == NULL); // src is left zeroed
    EXPECT_EQ(src.data_size, 0u);
    GenVec_destroy(&src);     // zero-safe
    EXPECT_EQ(GenVec_size(&dest), 4u);
    GenVec_destroy(&dest);
}


// insert_multi 

UTEST(gen_vector, insert_multi)
{
    GenVec v     = int_vec(4);
    int     arr[] = {10, 20, 30};
    GenVec_insert_multi(&v, 0, arr, 3);
    EXPECT_EQ(GenVec_size(&v), 3u);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 0), 10);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 1), 20);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 2), 30);
    GenVec_destroy(&v);
}

UTEST(gen_vector, insert_multi_mid)
{
    GenVec v = int_vec(8);
    push_ints(&v, 3); /* 0 1 2 */
    int arr[] = {10, 20};
    GenVec_insert_multi(&v, 1, arr, 2);
    /* expected: 0 10 20 1 2 */
    EXPECT_EQ(GenVec_size(&v), 5u);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 0), 0);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 1), 10);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 2), 20);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 3), 1);
    EXPECT_EQ(*(int*)GenVec_get_ptr(&v, 4), 2);
    GenVec_destroy(&v);
}

// swap_pop 

UTEST(gen_vector, swap_pop_middle)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); } /* 0 1 2 3 */
    int out = 0;
    GenVec_swap_pop(&v, 1, &out); /* remove 1, last (3) fills its slot */
    EXPECT_EQ(out, 1);
    EXPECT_EQ(GenVec_size(&v), 3u);
    /* element at [1] is now 3 */
    EXPECT_EQ(VEC_AT(&v, int, 1), 3);
    EXPECT_EQ(VEC_AT(&v, int, 0), 0);
    EXPECT_EQ(VEC_AT(&v, int, 2), 2);
    GenVec_destroy(&v);
}

UTEST(gen_vector, swap_pop_last)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); }
    GenVec_swap_pop(&v, 3, NULL); /* last element — just shrinks */
    EXPECT_EQ(GenVec_size(&v), 3u);
    EXPECT_EQ(VEC_AT(&v, int, 2), 2);
    GenVec_destroy(&v);
}

UTEST(gen_vector, swap_pop_single_element)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    int x = 99;
    VEC_PUSH(&v, x);
    GenVec_swap_pop(&v, 0, NULL);
    EXPECT_EQ(GenVec_size(&v), 0u);
    GenVec_destroy(&v);
}


// swap 

UTEST(gen_vector, swap_two_elements)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); } /* 0 1 2 3 */
    GenVec_swap(&v, 0, 3);
    EXPECT_EQ(VEC_AT(&v, int, 0), 3);
    EXPECT_EQ(VEC_AT(&v, int, 3), 0);
    EXPECT_EQ(VEC_AT(&v, int, 1), 1); /* untouched */
    GenVec_destroy(&v);
}

UTEST(gen_vector, swap_same_index_noop)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 3; i++) { VEC_PUSH(&v, i); }
    GenVec_swap(&v, 1, 1);
    EXPECT_EQ(VEC_AT(&v, int, 1), 1);
    GenVec_destroy(&v);
}

UTEST(gen_vector, swap_adjacent)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); }
    GenVec_swap(&v, 1, 2);
    EXPECT_EQ(VEC_AT(&v, int, 1), 2);
    EXPECT_EQ(VEC_AT(&v, int, 2), 1);
    GenVec_destroy(&v);
}


// find 

UTEST(gen_vector, find_hit)
{
    GenVec v = GenVec_create(WC_LIBC, 8, sizeof(int), NULL);
    for (int i = 0; i < 8; i++) { VEC_PUSH(&v, i); }
    int target = 5;
    EXPECT_EQ(GenVec_find(&v, &target, NULL), 5u);
    GenVec_destroy(&v);
}

UTEST(gen_vector, find_first_occurrence)
{
    GenVec v = GenVec_create(WC_LIBC, 8, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); }
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); } /* duplicate 0..3 */
    int target = 2;
    EXPECT_EQ(GenVec_find(&v, &target, NULL), 2u); /* first occurrence */
    GenVec_destroy(&v);
}

UTEST(gen_vector, find_miss)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); }
    int target = 99;
    EXPECT_EQ(GenVec_find(&v, &target, NULL), WC_NOT_FOUND);
    GenVec_destroy(&v);
}

UTEST(gen_vector, find_empty_vec)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    int target = 0;
    EXPECT_EQ(GenVec_find(&v, &target, NULL), WC_NOT_FOUND);
    GenVec_destroy(&v);
}


// subarr 

UTEST(gen_vector, subarr_middle)
{
    GenVec v = GenVec_create(WC_LIBC, 6, sizeof(int), NULL);
    for (int i = 0; i < 6; i++) { VEC_PUSH(&v, i); } /* 0..5 */
    GenVec sub = GenVec_subarr(&v, WC_LIBC, 2, 3); /* [2, 3, 4] */
    EXPECT_EQ(GenVec_size(&sub), 3u);
    EXPECT_EQ(VEC_AT(&sub, int, 0), 2);
    EXPECT_EQ(VEC_AT(&sub, int, 1), 3);
    EXPECT_EQ(VEC_AT(&sub, int, 2), 4);
    GenVec_destroy(&v);
    GenVec_destroy(&sub);
}

UTEST(gen_vector, subarr_clamps_to_end)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); }
    GenVec sub = GenVec_subarr(&v, WC_LIBC, 2, 100); /* len exceeds bounds */
    EXPECT_EQ(GenVec_size(&sub), 2u); /* clamped: [2, 3] */
    GenVec_destroy(&v);
    GenVec_destroy(&sub);
}

UTEST(gen_vector, subarr_independent)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); }
    GenVec sub = GenVec_subarr(&v, WC_LIBC, 0, 4);
    int x = 999;
    GenVec_replace(&v, 0, &x);
    EXPECT_EQ(VEC_AT(&sub, int, 0), 0); /* sub unaffected */
    GenVec_destroy(&v);
    GenVec_destroy(&sub);
}


// shrink_to_fit 

UTEST(gen_vector, shrink_to_fit_reduces_capacity)
{
    GenVec v = GenVec_create(WC_LIBC, 100, sizeof(int), NULL);
    for (int i = 0; i < 5; i++) { VEC_PUSH(&v, i); }
    GenVec_shrink_to_fit(&v);
    EXPECT_TRUE(GenVec_capacity(&v) <= 10); /* <= max(5, GENVEC_MIN_CAPACITY) */
    EXPECT_EQ(GenVec_size(&v), 5u);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(VEC_AT(&v, int, (u64)i), i);
    }
    GenVec_destroy(&v);
}

UTEST(gen_vector, shrink_to_fit_already_tight)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); }
    u64 cap_before = GenVec_capacity(&v);
    GenVec_shrink_to_fit(&v);
    EXPECT_EQ(GenVec_capacity(&v), cap_before);
    GenVec_destroy(&v);
}


// push_move 

UTEST(gen_vector, push_move_zeroes_src)
{
    GenVec  v = VEC_OF_STR(4);
    String  s = String_from_cstr(WC_LIBC, "owned");
    GenVec_push_move(&v, &s);
    EXPECT_EQ(s.size, 0u); // moved-from String is zeroed
    EXPECT_TRUE((s.heap) == NULL);
    String_destroy(&s);          // zero-safe no-op
    EXPECT_EQ(GenVec_size(&v), 1u);
    EXPECT_TRUE(String_equals_cstr(VEC_AT_MUT(&v, String, 0), "owned"));
    GenVec_destroy(&v);
}


// insert_move 

UTEST(gen_vector, insert_move_front)
{
    GenVec v = VEC_OF_STR(4);
    VEC_PUSH_CSTR(&v, "b");
    VEC_PUSH_CSTR(&v, "c");
    String  s = String_from_cstr(WC_LIBC, "a");
    GenVec_insert_move(&v, 0, &s);
    EXPECT_EQ(s.size, 0u);
    String_destroy(&s);
    EXPECT_EQ(GenVec_size(&v), 3u);
    EXPECT_TRUE(String_equals_cstr(VEC_AT_MUT(&v, String, 0), "a"));
    EXPECT_TRUE(String_equals_cstr(VEC_AT_MUT(&v, String, 1), "b"));
    GenVec_destroy(&v);
}


// replace_move 

UTEST(gen_vector, replace_move_frees_old)
{
    GenVec v = VEC_OF_STR(4);
    VEC_PUSH_CSTR(&v, "old");
    String  s = String_from_cstr(WC_LIBC, "new");
    GenVec_replace_move(&v, 0, &s);
    EXPECT_EQ(s.size, 0u);
    String_destroy(&s);
    EXPECT_TRUE(String_equals_cstr(VEC_AT_MUT(&v, String, 0), "new"));
    GenVec_destroy(&v);
}


// remove with out-copy 

UTEST(gen_vector, remove_with_out)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); } /* 0 1 2 3 */
    int out = 0;
    GenVec_remove(&v, 1, &out);
    EXPECT_EQ(out, 1);
    EXPECT_EQ(GenVec_size(&v), 3u);
    GenVec_destroy(&v);
}


// init_val_stk 

UTEST(gen_vector, init_val_stk)
{
    GenVec v;
    int val = 7;
    v = GenVec_create_val(WC_LIBC, 5, &val, sizeof(int), NULL);
    EXPECT_EQ(GenVec_size(&v), 5u);
    for (u64 i = 0; i < 5; i++) {
        EXPECT_EQ(VEC_AT(&v, int, i), 7);
    }
    GenVec_destroy(&v);
}


// VEC_FOREACH 

UTEST(gen_vector, vec_foreach_mutates)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 4; i++) { VEC_PUSH(&v, i); }
    VEC_FOREACH(&v, int, p) { (*p) *= 2; }
    EXPECT_EQ(VEC_AT(&v, int, 0), 0);
    EXPECT_EQ(VEC_AT(&v, int, 1), 2);
    EXPECT_EQ(VEC_AT(&v, int, 2), 4);
    EXPECT_EQ(VEC_AT(&v, int, 3), 6);
    GenVec_destroy(&v);
}

UTEST(gen_vector, vec_foreach_empty)
{
    GenVec v    = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    int     count = 0;
    VEC_FOREACH(&v, int, p) { count++; (void)p; }
    EXPECT_EQ(count, 0);
    GenVec_destroy(&v);
}

/* ── wc_errno: empty-vec operations ─────────────────────────────────────── */

UTEST(gen_vector, pop_empty_sets_errno)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    wc_errno = WC_OK;
    GenVec_pop(&v, NULL);
    EXPECT_EQ(wc_errno, (wc_err)WC_ERR_EMPTY);
    GenVec_destroy(&v);
}

UTEST(gen_vector, front_empty_sets_errno)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    wc_errno = WC_OK;
    const u8* p = GenVec_front(&v);
    EXPECT_TRUE((p) == NULL);
    EXPECT_EQ(wc_errno, (wc_err)WC_ERR_EMPTY);
    GenVec_destroy(&v);
}

UTEST(gen_vector, back_empty_sets_errno)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    wc_errno = WC_OK;
    const u8* p = GenVec_back(&v);
    EXPECT_TRUE((p) == NULL);
    EXPECT_EQ(wc_errno, (wc_err)WC_ERR_EMPTY);
    GenVec_destroy(&v);
}


/* ── One workload, every backend ─────────────────────────────────────────── */

// push / grow / insert / remove / shrink, checked element by element
// Returns 1 if the vector ends in the expected state.
static int workload(GenVec* v)
{
    for (int i = 0; i < 300; i++) {
        GenVec_push(v, &(i));
    }
    int x = -1;
    GenVec_insert(v, 0, &(x));
    GenVec_insert(v, 150, &(x));
    GenVec_remove(v, 150, NULL);
    GenVec_remove(v, 0, NULL);
    GenVec_remove_range(v, 100, 50); // drop 100..149
    GenVec_shrink_to_fit(v);

    if (GenVec_size(v) != 250) {
        return 0;
    }
    for (u64 i = 0; i < 250; i++) {
        int want = (int)(i < 100 ? i : i + 50);
        if (*(const int*)GenVec_get_ptr(v, i) != want) {
            return 0;
        }
    }
    return 1;
}

UTEST(gen_vector, workload_libc)
{
    GenVec v = GenVec_create(WC_LIBC, 2, sizeof(int), NULL);
    EXPECT_TRUE(workload(&v));
    GenVec_destroy(&v);
}

UTEST(gen_vector, workload_test_allocator)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    GenVec v = GenVec_create(wc_test_alloc_allocator(&ta), 2, sizeof(int), NULL);
    EXPECT_TRUE(workload(&v));
    EXPECT_EQ(ta.live_blocks, 1u);
    GenVec_destroy(&v);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(gen_vector, workload_arena)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(8));
    GenVec v = GenVec_create(Arena_allocator(&a), 2, sizeof(int), NULL);
    EXPECT_TRUE(workload(&v));
    EXPECT_TRUE(v.data >= a.base && v.data < a.base + a.size);
    GenVec_destroy(&v);
    EXPECT_EQ(Arena_used(&a), 0u); // sole block: every growth was in place, destroy rewound it
    Arena_destroy(&a);
}

UTEST(gen_vector, workload_arena_over_stack_buffer)
{
    alignas(16) u8 buf[4096];
    Arena          a;
    Arena_create_buf(&a, buf, sizeof(buf));
    GenVec v = GenVec_create(Arena_allocator(&a), 2, sizeof(int), NULL);
    EXPECT_TRUE(workload(&v));
    GenVec_destroy(&v);
    Arena_destroy(&a); // frees nothing: the stack buffer is not ours
}

UTEST(gen_vector, workload_chain_arena)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    ChainArena ca;
    ChainArena_create(&ca, wc_test_alloc_allocator(&ta));

    GenVec v = GenVec_create(ChainArena_allocator(&ca), 2, sizeof(int), NULL);
    EXPECT_TRUE(workload(&v));
    for (int i = 0; i < 3000; i++) { // past one node: was fatal before A3
        GenVec_push(&v, &(i));
    }
    GenVec_destroy(&v);

    ChainArena_destroy(&ca);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}


/* ── Borrowed buffer (GenVec_create_buf / wc_borrowed) ───────────────────── */

UTEST(gen_vector, buf_vector_fills_buffer_in_place)
{
    int    buf[8];
    GenVec v = GenVec_create_buf(buf, 8, sizeof(int), NULL);
    for (int i = 0; i < 8; i++) {
        GenVec_push(&v, &(i));
    }
    EXPECT_TRUE(v.data == (u8*)buf);
    EXPECT_EQ(buf[7], 7);
    EXPECT_TRUE(wc_same(v.alloc, wc_borrowed));
    GenVec_destroy(&v); // must not free the stack buffer (ASAN would catch it)
}

static void push_past_borrowed_capacity(void)
{
    int    buf[2];
    GenVec v = GenVec_create_buf(buf, 2, sizeof(int), NULL);
    for (int i = 0; i < 3; i++) {
        GenVec_push(&v, &(i)); // third push must die: a borrowed buffer cannot grow
    }
}

UTEST(gen_vector, buf_vector_cannot_grow)
{
    EXPECT_DIES(push_past_borrowed_capacity);
}


/* ── Copy across allocators (A7) ─────────────────────────────────────────── */

UTEST(gen_vector, copy_arena_to_libc)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    GenVec src = GenVec_create(Arena_allocator(&a), 4, sizeof(int), NULL);
    for (int i = 0; i < 20; i++) {
        GenVec_push(&src, &(i));
    }

    GenVec dst = GenVec_copy(WC_LIBC, &src);
    EXPECT_TRUE(wc_is_libc(dst.alloc)); // never inherits src's allocator
    EXPECT_TRUE(dst.data < a.base || dst.data >= a.base + a.size);

    Arena_destroy(&a); // the arena copy is gone; the libc copy must be intact
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(*(const int*)GenVec_get_ptr(&dst, (u64)i), i);
    }
    GenVec_destroy(&dst);
}

// every child of a deep copy must belong to the destination allocator
UTEST(gen_vector, nested_copy_children_follow_destination)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(16));
    wc_allocator al = Arena_allocator(&a);

    GenVec outer = VEC_OF_IN(al, GenVec, 2);
    for (int r = 0; r < 6; r++) {
        GenVec inner = GenVec_create(al, 1, sizeof(int), NULL);
        for (int c = 0; c <= r; c++) {
            GenVec_push(&inner, &(c));
        }
        VEC_PUSH_MOVE(&outer, inner); // inner zeroed
        EXPECT_TRUE((inner.data) == NULL);
    }

    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator dst = wc_test_alloc_allocator(&ta);

    GenVec copy = GenVec_copy(dst, &outer);
    EXPECT_TRUE(wc_test_alloc_owns(&ta, copy.data));
    for (u64 r = 0; r < 6; r++) {
        const GenVec* in = GenVec_get_ptr(&copy, r);
        EXPECT_TRUE(wc_same(in->alloc, dst));
        EXPECT_TRUE(wc_test_alloc_owns(&ta, in->data));
        EXPECT_EQ(GenVec_size(in), r + 1);
    }

    GenVec_destroy(&outer);
    Arena_destroy(&a);
    GenVec_destroy(&copy);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

// by-pointer children: the shell and the data come from the same allocator (D6)
UTEST(gen_vector, boxed_children_free_with_their_own_allocator)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator al = wc_test_alloc_allocator(&ta);

    GenVec outer = VEC_OF(GenVec*, 2); // outer on libc, children on the test allocator
    for (int i = 0; i < 5; i++) {
        GenVec* child = WC_BOX_IN(al, GenVec, GenVec_create, 4, sizeof(int), NULL);
        GenVec_push(child, &(i));
        EXPECT_TRUE(wc_test_alloc_owns(&ta, child));
        VEC_PUSH_MOVE(&outer, child);
        EXPECT_TRUE((child) == NULL);
    }

    GenVec copy = GenVec_copy(al, &outer); // children deep-copied into `al`, shells too
    EXPECT_TRUE(wc_test_alloc_owns(&ta, *(GenVec* const*)GenVec_get_ptr(&copy, 0)));

    GenVec_destroy(&outer); // vec_del_ptr: shell freed with each child's own allocator
    GenVec_destroy(&copy);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(gen_vector, subarr_into_other_allocator)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    GenVec src = GenVec_create(Arena_allocator(&a), 8, sizeof(int), NULL);
    for (int i = 0; i < 8; i++) {
        GenVec_push(&src, &(i));
    }
    GenVec sub = GenVec_subarr(&src, WC_LIBC, 2, 3);
    Arena_destroy(&a);
    EXPECT_EQ(GenVec_size(&sub), 3u);
    EXPECT_EQ(*(const int*)GenVec_get_ptr(&sub, 0), 2);
    GenVec_destroy(&sub);
}

UTEST(gen_vector, vec_like_uses_same_allocator)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(4));
    GenVec v = VEC_OF_IN(Arena_allocator(&a), int, 4);
    GenVec w = VEC_LIKE(&v, u64, 4);
    EXPECT_TRUE(wc_same(w.alloc, v.alloc));
    EXPECT_TRUE(w.data >= a.base && w.data < a.base + a.size);
    GenVec_destroy(&w);
    GenVec_destroy(&v);
    Arena_destroy(&a);
}


/* ── Alignment (D7) ──────────────────────────────────────────────────────── */

typedef struct {
    alignas(16) u8 b[16];
} Aligned16;

UTEST(gen_vector, storage_alignment_is_size_derived)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator al = wc_test_alloc_allocator(&ta);

    GenVec v1  = GenVec_create(al, 3, 1, NULL);                 // align 1
    GenVec v4  = GenVec_create(al, 3, sizeof(int), NULL);       // align 4
    GenVec v12 = GenVec_create(al, 3, 12, NULL);                // align 4
    GenVec v8  = GenVec_create(al, 3, sizeof(u64), NULL);       // align 8
    GenVec v16 = GenVec_create(al, 3, sizeof(Aligned16), NULL); // align 16

    // wc_test_alloc records the align of every block; destroy must pass it back
    // exactly, and the backing honours it as an address alignment.
    EXPECT_EQ((uintptr_t)v16.data % 16, 0u);
    GenVec_destroy(&v1);
    GenVec_destroy(&v4);
    GenVec_destroy(&v12);
    GenVec_destroy(&v8);
    GenVec_destroy(&v16);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(gen_vector, arena_packs_small_elements)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 256);
    wc_allocator al = Arena_allocator(&a);
    GenVec       b1 = GenVec_create(al, 3, 1, NULL); // 3 bytes
    GenVec       b2 = GenVec_create(al, 5, 1, NULL); // starts right after: no 16-byte padding
    EXPECT_TRUE(b2.data == b1.data + 3);
    GenVec_destroy(&b2);
    GenVec_destroy(&b1);
    Arena_destroy(&a);
}


/* ── Zero state (D4) ─────────────────────────────────────────────────────── */

static void push_after_move(void)
{
    GenVec a = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    GenVec b;
    GenVec_move(&b, &a);
    int x = 1;
    GenVec_push(&a, &(x)); // a is zeroed: must die, not allocate from libc
}

static void push_after_destroy(void)
{
    GenVec a = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    GenVec_destroy(&a);
    int x = 1;
    GenVec_push(&a, &(x));
}

static void reserve_on_zeroed(void)
{
    GenVec z;
    memset(&z, 0, sizeof(z));
    GenVec_reserve(&z, 10);
}

static void create_with_zero_data_size(void)
{
    GenVec v = GenVec_create(WC_LIBC, 4, 0, NULL);
    (void)v;
}

UTEST(gen_vector, zero_state_mutation_dies)
{
    EXPECT_DIES(push_after_move);
    EXPECT_DIES(push_after_destroy);
    EXPECT_DIES(reserve_on_zeroed);
    EXPECT_DIES(create_with_zero_data_size);
}

UTEST(gen_vector, zero_state_reads_and_destroy_are_safe)
{
    GenVec z;
    memset(&z, 0, sizeof(z));
    EXPECT_EQ(GenVec_size(&z), 0u);
    EXPECT_TRUE(GenVec_empty(&z));
    GenVec_clear(&z);
    GenVec_destroy(&z);
    GenVec_destroy(&z);

    GenVec c = GenVec_copy(WC_LIBC, &z); // copy of zeroed is zeroed
    EXPECT_EQ(c.data_size, 0u);
    GenVec_destroy(&c);
}


/* ── Allocation failure (D5): every allocation site dies cleanly ─────────── */

static u64 g_fail_at;

// Touches every GenVec allocation site: create, grow, reserve, shrink, copy,
// subarr, nested element copy.
static u64 alloc_site_scenario(wc_test_alloc* ta)
{
    wc_allocator al = wc_test_alloc_allocator(ta);

    GenVec v = GenVec_create(al, 2, sizeof(int), NULL); // create
    for (int i = 0; i < 20; i++) {
        GenVec_push(&v, &(i)); // grow
    }
    GenVec_reserve(&v, 100);                   // reserve
    GenVec_shrink_to_fit(&v);                  // shrink
    GenVec c = GenVec_copy(al, &v);            // copy
    GenVec s = GenVec_subarr(&v, al, 0, 5);    // subarr

    GenVec outer = VEC_OF_IN(al, GenVec, 1);
    GenVec_push(&outer, &(v));              // outer grow + nested element copy

    u64 attempts = ta->n_attempts;
    GenVec_destroy(&outer);
    GenVec_destroy(&s);
    GenVec_destroy(&c);
    GenVec_destroy(&v);
    return attempts;
}

static void run_scenario_failing_at_n(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_test_alloc_fail_at(&ta, g_fail_at, 0);
    (void)alloc_site_scenario(&ta); // must FATAL at attempt g_fail_at
    _exit(0);                       // reached only if the failure was ignored
}

UTEST(gen_vector, fail_nth_allocation_dies_at_every_site)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    u64 total = alloc_site_scenario(&ta);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
    EXPECT_TRUE(total >= 8);

    for (g_fail_at = 1; g_fail_at <= total; g_fail_at++) {
        int died = test_dies(run_scenario_failing_at_n);
        if (died != 1) {
            printf("\n    allocation %llu of %llu did not abort cleanly ", (unsigned long long)g_fail_at,
                   (unsigned long long)total);
        }
        EXPECT_EQ(died, 1);
    }
}


// A5 is a compile-time defect (WC_REALLOC_N had the wrong arity). It cannot
// live in this binary; see tests/compile/a5_realloc_n.c and the
// `a5_realloc_n_compiles` ctest entry.


/* ── Golden scenarios (behaviour to preserve through the refactor) ───────── */

// Baseline history: 964 bytes before the refactor (Phase 0), 340 after
// in-place arena realloc (Phase 1), 292 after Phase 2 (no heap shell in the
// arena, 4-byte alignment for int: exactly 73 x 4 bytes, zero waste).
// The contents must stay identical and the arena must never use MORE than the
// current baseline; lower it when a change improves it.
#define GOLDEN_MAIN_ARENA_USED_BASELINE 292

// src/main.c scenario: 70 pushes of int into a vector whose storage is a 1 KB arena.
UTEST(gen_vector, golden_main_70_push)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));

    GenVec v = VEC_OF_IN(Arena_allocator(&a), int, 5);
    for (int i = 0; i < 70; i++) {
        VEC_PUSH(&v, i);
    }

    EXPECT_EQ(GenVec_size(&v), 70u);
    for (int i = 0; i < 70; i++) {
        EXPECT_EQ(*(const int*)GenVec_get_ptr(&v, (u64)i), i);
    }
    u64 used = Arena_used(&a);
    EXPECT_TRUE(used <= GOLDEN_MAIN_ARENA_USED_BASELINE);
    printf("[arena used %llu / baseline %d] ", (unsigned long long)used, GOLDEN_MAIN_ARENA_USED_BASELINE);

    GenVec_destroy(&v);
    EXPECT_EQ(Arena_used(&a), 0u); // the vector was the only block: freed by rewinding
    Arena_destroy(&a);
}

// Same 70 pushes on libc: exact contents and final capacity.
#define GOLDEN_MAIN_LIBC_CAPACITY 73

UTEST(gen_vector, golden_main_70_push_libc)
{
    GenVec v = VEC_OF(int, 5);
    for (int i = 0; i < 70; i++) {
        VEC_PUSH(&v, i);
    }
    EXPECT_EQ(GenVec_size(&v), 70u);
    EXPECT_EQ(GenVec_capacity(&v), (u64)(GOLDEN_MAIN_LIBC_CAPACITY));
    for (int i = 0; i < 70; i++) {
        EXPECT_EQ(*(const int*)GenVec_get_ptr(&v, (u64)i), i);
    }
    GenVec_destroy(&v);
}
