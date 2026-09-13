#include "common.h"
#include "map_setup.h"
#include "wc_string.h"
#include "wc_test.h"
#include "hashmap.h"
#include "wc_helpers.h"
#include "wc_macros.h"
#include <stdio.h>
#include <stdlib.h>


/* ── Map constructors ────────────────────────────────────────────────────── */

static HashMap* int_map(void)
{
    return HashMap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
}

static HashMap* int_str_map(void)
{
    return HashMap_create(sizeof(int), sizeof(String), NULL, NULL, NULL, &wc_str_ops);
}

static HashMap* str_str_map(void)
{
    return HashMap_create(sizeof(String), sizeof(String),
                          wyhash_str, str_cmp, &wc_str_ops, &wc_str_ops);
}

// HashMap_copy expects dest to be raw/uninitialised memory — it overwrites
// dest's fields wholesale without freeing anything dest might already own.
// Passing a HashMap already produced by int_map()/str_str_map() (which have
// live, allocated buffers) leaks those buffers. Use this to get an
// uninitialised shell instead; HashMap_copy fills it in completely.
static HashMap* raw_map(void)
{
    HashMap* m = malloc(sizeof(HashMap));
    WC_ASSERT_NOT_NULL(m);
    return m;
}


/* ════════════════════════════════════════════════════════════════════════════
 * int -> int  (POD, no copy/move/del)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_put_and_get(void)
{
    HashMap* m = int_map();
    int k = 1, v = 100;
    HashMap_put(m, (u8*)&k, (u8*)&v);

    int out = 0;
    WC_ASSERT_TRUE(HashMap_get(m, (u8*)&k, (u8*)&out));
    WC_ASSERT_EQ_INT(out, 100);
    HashMap_destroy(m);
}

static void test_put_update(void)
{
    HashMap* m = int_map();
    int k = 1, v1 = 10, v2 = 20;
    HashMap_put(m, (u8*)&k, (u8*)&v1);
    b8 was_update = HashMap_put(m, (u8*)&k, (u8*)&v2);
    WC_ASSERT_TRUE(was_update);

    int out = 0;
    HashMap_get(m, (u8*)&k, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 20);
    WC_ASSERT_EQ_U64(HashMap_size(m), 1);
    HashMap_destroy(m);
}

static void test_has(void)
{
    HashMap* m = int_map();
    int k = 5, v = 0;
    WC_ASSERT_FALSE(HashMap_has(m, (u8*)&k));
    HashMap_put(m, (u8*)&k, (u8*)&v);
    WC_ASSERT_TRUE(HashMap_has(m, (u8*)&k));
    HashMap_destroy(m);
}

static void test_del(void)
{
    HashMap* m = int_map();
    int k = 3, v = 42;
    HashMap_put(m, (u8*)&k, (u8*)&v);
    WC_ASSERT_TRUE(HashMap_del(m, (u8*)&k, NULL));
    WC_ASSERT_FALSE(HashMap_has(m, (u8*)&k));
    WC_ASSERT_EQ_U64(HashMap_size(m), 0);
    HashMap_destroy(m);
}

static void test_del_copies_out(void)
{
    HashMap* m = int_map();
    int k = 7, v = 99, out = 0;
    HashMap_put(m, (u8*)&k, (u8*)&v);
    HashMap_del(m, (u8*)&k, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 99);
    HashMap_destroy(m);
}

static void test_del_missing_returns_false(void)
{
    HashMap* m = int_map();
    int k = 404;
    WC_ASSERT_FALSE(HashMap_del(m, (u8*)&k, NULL));
    HashMap_destroy(m);
}

static void test_del_on_empty_map(void)
{
    HashMap* m = int_map();
    int k = 1;
    WC_ASSERT_FALSE(HashMap_del(m, (u8*)&k, NULL));
    WC_ASSERT_EQ_U64(HashMap_size(m), 0);
    HashMap_destroy(m);
}

static void test_get_ptr(void)
{
    HashMap* m = int_map();
    int k = 2, v = 55;
    HashMap_put(m, (u8*)&k, (u8*)&v);
    int* ptr = (int*)HashMap_get_ptr(m, (u8*)&k);
    WC_ASSERT_NOT_NULL(ptr);
    WC_ASSERT_EQ_INT(*ptr, 55);

    // Mutate through ptr — must be visible via get
    *ptr = 66;
    int out = 0;
    HashMap_get(m, (u8*)&k, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 66);
    HashMap_destroy(m);
}

static void test_get_ptr_missing_returns_null(void)
{
    HashMap* m = int_map();
    int k = 999;
    WC_ASSERT_NULL(HashMap_get_ptr(m, (u8*)&k));
    HashMap_destroy(m);
}

static void test_get_missing_returns_false(void)
{
    HashMap* m = int_map();
    int k = 999, out = 0;
    WC_ASSERT_FALSE(HashMap_get(m, (u8*)&k, (u8*)&out));
    HashMap_destroy(m);
}

static void test_size_tracks_inserts(void)
{
    HashMap* m = int_map();
    WC_ASSERT_EQ_U64(HashMap_size(m), 0);
    WC_ASSERT_TRUE(HashMap_empty(m));

    for (int i = 0; i < 10; i++) {
        int v = i * 10;
        HashMap_put(m, (u8*)&i, (u8*)&v);
    }
    WC_ASSERT_EQ_U64(HashMap_size(m), 10);
    WC_ASSERT_FALSE(HashMap_empty(m));
    HashMap_destroy(m);
}

static void test_resize_preserves_data(void)
{
    HashMap* m = int_map();
    for (int i = 0; i < 50; i++) {
        int v = i * 2;
        HashMap_put(m, (u8*)&i, (u8*)&v);
    }
    WC_ASSERT_EQ_U64(HashMap_size(m), 50);
    for (int i = 0; i < 50; i++) {
        int out = 0;
        WC_ASSERT_TRUE(HashMap_get(m, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i * 2);
    }
    HashMap_destroy(m);
}

// Robin Hood + backward-shift delete doesn't shrink — no LOAD_FACTOR_SHRINK.
// Verify correctness after heavy deletion instead.
static void test_del_correctness_after_many_deletes(void)
{
    HashMap* m = int_map();
    for (int i = 0; i < 50; i++) {
        int v = i;
        HashMap_put(m, (u8*)&i, (u8*)&v);
    }
    for (int i = 0; i < 48; i++) {
        HashMap_del(m, (u8*)&i, NULL);
    }
    WC_ASSERT_EQ_U64(HashMap_size(m), 2);

    for (int i = 48; i < 50; i++) {
        int out = 0;
        WC_ASSERT_TRUE(HashMap_get(m, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i);
    }
    HashMap_destroy(m);
}


/* ════════════════════════════════════════════════════════════════════════════
 * delete correctness  (backward-shift, no tombstones)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_del_reinsert(void)
{
    // Delete a key then reinsert it — must succeed and be findable
    HashMap* m = int_map();
    int k = 42, v1 = 1, v2 = 2;
    HashMap_put(m, (u8*)&k, (u8*)&v1);
    HashMap_del(m, (u8*)&k, NULL);
    WC_ASSERT_FALSE(HashMap_has(m, (u8*)&k));

    HashMap_put(m, (u8*)&k, (u8*)&v2);
    int out = 0;
    WC_ASSERT_TRUE(HashMap_get(m, (u8*)&k, (u8*)&out));
    WC_ASSERT_EQ_INT(out, 2);
    WC_ASSERT_EQ_U64(HashMap_size(m), 1);
    HashMap_destroy(m);
}

static void test_del_mid_chain(void)
{
    // Insert keys that will chain during probing, delete one mid-chain,
    // then verify all remaining keys are still reachable.
    // Backward-shift delete keeps the Robin Hood invariant intact.
    HashMap* m = int_map();
    for (int i = 0; i < 20; i++) {
        int v = i * 10;
        HashMap_put(m, (u8*)&i, (u8*)&v);
    }

    int mid = 5;
    HashMap_del(m, (u8*)&mid, NULL);

    for (int i = 0; i < 20; i++) {
        if (i == mid) {
            continue;
        }
        int out = 0;
        WC_ASSERT_TRUE(HashMap_get(m, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i * 10);
    }
    HashMap_destroy(m);
}

static void test_delete_reinsert_cycle(void)
{
    // Repeated delete+reinsert must not corrupt or leak
    HashMap* m = int_map();
    int k = 7;
    for (int cycle = 0; cycle < 20; cycle++) {
        int v = cycle;
        HashMap_put(m, (u8*)&k, (u8*)&v);
        int out = 0;
        HashMap_get(m, (u8*)&k, (u8*)&out);
        WC_ASSERT_EQ_INT(out, cycle);
        HashMap_del(m, (u8*)&k, NULL);
        WC_ASSERT_EQ_U64(HashMap_size(m), 0);
    }
    HashMap_destroy(m);
}

static void test_del_first_in_chain(void)
{
    // Deleting the head of a probe chain must leave the rest reachable
    HashMap* m = int_map();
    for (int i = 0; i < 15; i++) {
        int v = i;
        HashMap_put(m, (u8*)&i, (u8*)&v);
    }
    int head = 0;
    HashMap_del(m, (u8*)&head, NULL);
    WC_ASSERT_FALSE(HashMap_has(m, (u8*)&head));

    for (int i = 1; i < 15; i++) {
        int out = 0;
        WC_ASSERT_TRUE(HashMap_get(m, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i);
    }
    HashMap_destroy(m);
}

static void test_del_all_then_reinsert(void)
{
    // Delete every key then reinsert — map must be fully functional
    HashMap* m = int_map();
    for (int i = 0; i < 20; i++) {
        int v = i;
        HashMap_put(m, (u8*)&i, (u8*)&v);
    }
    for (int i = 0; i < 20; i++) {
        HashMap_del(m, (u8*)&i, NULL);
    }
    WC_ASSERT_EQ_U64(HashMap_size(m), 0);
    WC_ASSERT_TRUE(HashMap_empty(m));

    for (int i = 0; i < 20; i++) {
        int v = i * 3;
        HashMap_put(m, (u8*)&i, (u8*)&v);
    }
    WC_ASSERT_EQ_U64(HashMap_size(m), 20);
    for (int i = 0; i < 20; i++) {
        int out = 0;
        WC_ASSERT_TRUE(HashMap_get(m, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i * 3);
    }
    HashMap_destroy(m);
}


/* ════════════════════════════════════════════════════════════════════════════
 * HashMap_clear
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_clear_empties_map(void)
{
    HashMap* m = int_map();
    for (int i = 0; i < 10; i++) {
        int v = i;
        HashMap_put(m, (u8*)&i, (u8*)&v);
    }
    u64 cap_before = HashMap_capacity(m);
    HashMap_clear(m);

    WC_ASSERT_EQ_U64(HashMap_size(m), 0);
    WC_ASSERT_TRUE(HashMap_empty(m));
    WC_ASSERT_EQ_U64(HashMap_capacity(m), cap_before); // capacity unchanged
    for (int i = 0; i < 10; i++) {
        WC_ASSERT_FALSE(HashMap_has(m, (u8*)&i));
    }
    HashMap_destroy(m);
}

static void test_clear_then_reuse(void)
{
    HashMap* m = int_map();
    for (int i = 0; i < 10; i++) {
        int v = i;
        HashMap_put(m, (u8*)&i, (u8*)&v);
    }
    HashMap_clear(m);

    for (int i = 100; i < 110; i++) {
        int v = i * 2;
        HashMap_put(m, (u8*)&i, (u8*)&v);
    }
    WC_ASSERT_EQ_U64(HashMap_size(m), 10);
    for (int i = 100; i < 110; i++) {
        int out = 0;
        WC_ASSERT_TRUE(HashMap_get(m, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i * 2);
    }
    HashMap_destroy(m);
}

static void test_clear_frees_String_vals(void)
{
    // clear must properly call del on owned String resources
    HashMap* m = int_str_map();
    for (int i = 0; i < 5; i++) {
        String* v = String_from_cstr("owned");
        HashMap_put_val_move(m, (u8*)&i, (u8**)&v);
    }
    HashMap_clear(m);
    WC_ASSERT_EQ_U64(HashMap_size(m), 0);

    // Map must still be usable after clearing owned-resource entries
    int k = 99;
    String* v = String_from_cstr("after_clear");
    HashMap_put_val_move(m, (u8*)&k, (u8**)&v);
    WC_ASSERT_TRUE(HashMap_has(m, (u8*)&k));
    HashMap_destroy(m);
}

static void test_clear_empty_map(void)
{
    // clear on an already-empty map must be a safe no-op
    HashMap* m = int_map();
    HashMap_clear(m);
    WC_ASSERT_EQ_U64(HashMap_size(m), 0);
    HashMap_destroy(m);
}


/* ════════════════════════════════════════════════════════════════════════════
 * HashMap_copy
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_copy_int_map(void)
{
    HashMap* src = int_map();
    for (int i = 0; i < 10; i++) {
        int v = i * 3;
        HashMap_put(src, (u8*)&i, (u8*)&v);
    }

    // dest must be uninitialised — HashMap_copy allocates everything
    HashMap* dest = raw_map();
    HashMap_copy(dest, src);
    WC_ASSERT_EQ_U64(HashMap_size(dest), HashMap_size(src));

    for (int i = 0; i < 10; i++) {
        int out = 0;
        WC_ASSERT_TRUE(HashMap_get(dest, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i * 3);
    }

    HashMap_destroy(src);
    HashMap_destroy(dest);
}

static void test_copy_independence(void)
{
    // Mutating dest must not affect src
    HashMap* src = int_map();
    int k = 1, v = 10;
    HashMap_put(src, (u8*)&k, (u8*)&v);

    HashMap* dest = raw_map();
    HashMap_copy(dest, src);

    int v2 = 99;
    HashMap_put(dest, (u8*)&k, (u8*)&v2);

    int src_out = 0, dest_out = 0;
    HashMap_get(src,   (u8*)&k, (u8*)&src_out);
    HashMap_get(dest, (u8*)&k, (u8*)&dest_out);
    WC_ASSERT_EQ_INT(src_out,  10);
    WC_ASSERT_EQ_INT(dest_out, 99);

    HashMap_destroy(src);
    HashMap_destroy(dest);
}

static void test_copy_str_str_map(void)
{
    // Deep copy: destroying src must not corrupt dest's String data
    HashMap* src = str_str_map();
    MAP_PUT_STR_STR(src, "name",  "Alice");
    MAP_PUT_STR_STR(src, "city",  "London");
    MAP_PUT_STR_STR(src, "color", "blue");

    HashMap* dest = raw_map();
    HashMap_copy(dest, src);
    WC_ASSERT_EQ_U64(HashMap_size(dest), 3);

    HashMap_destroy(src); // src gone — dest must still be intact

    String k;
    String_create_stk(&k, "city");
    String* found = (String*)HashMap_get_ptr(dest, (u8*)&k);
    WC_ASSERT_NOT_NULL(found);
    WC_ASSERT_TRUE(String_equals_cstr(found, "London"));
    String_destroy_stk(&k);

    HashMap_destroy(dest);
}

static void test_copy_empty_map(void)
{
    HashMap* src = int_map();
    HashMap* dest = raw_map();
    HashMap_copy(dest, src);
    WC_ASSERT_EQ_U64(HashMap_size(dest), 0);
    WC_ASSERT_EQ_U64(HashMap_capacity(dest), HashMap_capacity(src));
    HashMap_destroy(src);
    HashMap_destroy(dest);
}

static void test_copy_then_del_src_key(void)
{
    // Deleting from src after copy must not affect dest
    HashMap* src = int_map();
    int k = 5, v = 50;
    HashMap_put(src, (u8*)&k, (u8*)&v);

    HashMap* dest = raw_map();
    HashMap_copy(dest, src);

    HashMap_del(src, (u8*)&k, NULL);
    WC_ASSERT_FALSE(HashMap_has(src,   (u8*)&k));
    WC_ASSERT_TRUE(HashMap_has(dest, (u8*)&k));

    HashMap_destroy(src);
    HashMap_destroy(dest);
}


// TODO: test macros properly
/* ════════════════════════════════════════════════════════════════════════════
 * macros
 * ════════════════════════════════════════════════════════════════════════════ */

// static void test_foreach_visits_all(void)
// {
//     HashMap* m = int_map();
//     for (int i = 0; i < 8; i++) {
//         int v = i;
//         HashMap_put(m, (u8*)&i, (u8*)&v);
//     }
//
//     int count = 0, key_sum = 0;
//     MAP_FOREACH(m, k, v) {
//         count++;
//         key_sum += *(int*)k;
//         (void)v;
//     }
//     WC_ASSERT_EQ_INT(count,   8);
//     WC_ASSERT_EQ_INT(key_sum, 0+1+2+3+4+5+6+7);
//     HashMap_destroy(m);
// }
//
// static void test_foreach_skips_empty(void)
// {
//     // Deleted slots must not appear during iteration
//     HashMap* m = int_map();
//     for (int i = 0; i < 8; i++) {
//         int v = i;
//         HashMap_put(m, (u8*)&i, (u8*)&v);
//     }
//     for (int i = 0; i < 4; i++) {
//         HashMap_del(m, (u8*)&i, NULL);
//     }
//
//     int count = 0;
//     MAP_FOREACH(m, k, v) {
//         WC_ASSERT_TRUE(*(int*)k >= 4);
//         count++;
//         (void)v;
//     }
//     WC_ASSERT_EQ_INT(count, 4);
//     HashMap_destroy(m);
// }
//
// static void test_foreach_val_typed(void)
// {
//     HashMap* m = int_str_map();
//     MAP_PUT_INT_STR(m, 1, "one");
//     MAP_PUT_INT_STR(m, 2, "two");
//     MAP_PUT_INT_STR(m, 3, "three");
//
//     int count = 0;
//     MAP_FOREACH_VAL(m, String, v) {
//         WC_ASSERT_NOT_NULL(v);
//         WC_ASSERT_TRUE(String_len(v) > 0);
//         count++;
//     }
//     WC_ASSERT_EQ_INT(count, 3);
//     HashMap_destroy(m);
// }
//
// static void test_foreach_empty_map(void)
// {
//     HashMap* m = int_map();
//     int count = 0;
//     MAP_FOREACH(m, k, v) {
//         (void)k; (void)v;
//         count++;
//     }
//     WC_ASSERT_EQ_INT(count, 0);
//     HashMap_destroy(m);
// }
//
// static void test_foreach_key_val_consistent(void)
// {
//     // Every key visited must match its stored val
//     HashMap* m = int_map();
//     for (int i = 0; i < 16; i++) {
//         int v = i * 7;
//         HashMap_put(m, (u8*)&i, (u8*)&v);
//     }
//
//     MAP_FOREACH(m, k, v) {
//         WC_ASSERT_EQ_INT(*(int*)v, *(int*)k * 7);
//     }
//     HashMap_destroy(m);
// }

/* ════════════════════════════════════════════════════════════════════════════
 * int -> String  (owned val)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_str_val_put_copy(void)
{
    HashMap* m = int_str_map();
    int k = 1;
    String sv;
    String_create_stk(&sv, "hello");
    HashMap_put(m, (u8*)&k, (u8*)&sv);

    String* got = (String*)HashMap_get_ptr(m, (u8*)&k);
    WC_ASSERT_NOT_NULL(got);
    WC_ASSERT_TRUE(String_equals_cstr(got, "hello"));

    String_destroy_stk(&sv);
    HashMap_destroy(m);
}

static void test_str_val_independence(void)
{
    // Mutating source after put must not affect stored copy
    HashMap* m = int_str_map();
    int k = 1;
    String sv;
    String_create_stk(&sv, "original");
    HashMap_put(m, (u8*)&k, (u8*)&sv);
    String_append_cstr(&sv, "_mutated");

    String* stored = (String*)HashMap_get_ptr(m, (u8*)&k);
    WC_ASSERT_TRUE(String_equals_cstr(stored, "original"));

    String_destroy_stk(&sv);
    HashMap_destroy(m);
}

static void test_str_val_move(void)
{
    HashMap* m   = int_str_map();
    int      k   = 2;
    String*  src = String_from_cstr("moved");
    HashMap_put_val_move(m, (u8*)&k, (u8**)&src);

    WC_ASSERT_NULL(src);
    String* stored = (String*)HashMap_get_ptr(m, (u8*)&k);
    WC_ASSERT_TRUE(String_equals_cstr(stored, "moved"));
    HashMap_destroy(m);
}

static void test_str_val_update_frees_old(void)
{
    // Updating a String val must not leak the old heap buffer
    HashMap* m = int_str_map();
    int k = 1;
    MAP_PUT_INT_STR(m, k, "first");
    MAP_PUT_INT_STR(m, k, "second");

    WC_ASSERT_EQ_U64(HashMap_size(m), 1);
    String* stored = (String*)HashMap_get_ptr(m, (u8*)&k);
    WC_ASSERT_TRUE(String_equals_cstr(stored, "second"));
    HashMap_destroy(m);
}

static void test_str_val_move_updates_existing(void)
{
    // put_val_move on an existing key must free old val and store new one
    HashMap* m = int_str_map();
    int k = 5;
    MAP_PUT_INT_STR(m, k, "old");

    String* v = String_from_cstr("new");
    HashMap_put_val_move(m, (u8*)&k, (u8**)&v);
    WC_ASSERT_NULL(v);

    String* stored = (String*)HashMap_get_ptr(m, (u8*)&k);
    WC_ASSERT_TRUE(String_equals_cstr(stored, "new"));
    WC_ASSERT_EQ_U64(HashMap_size(m), 1);
    HashMap_destroy(m);
}

static void test_str_val_del_with_out(void)
{
    // del with non-null out must copy the String before destroying it
    HashMap* m = int_str_map();
    int k = 3;
    MAP_PUT_INT_STR(m, k, "goodbye");

    String out;
    String_create_stk(&out, "");
    HashMap_del(m, (u8*)&k, (u8*)&out);
    WC_ASSERT_TRUE(String_equals_cstr(&out, "goodbye"));

    String_destroy_stk(&out);
    HashMap_destroy(m);
}

static void test_str_val_many_inserts_and_gets(void)
{
    // Stress: many int->String pairs across multiple resizes
    HashMap* m = int_str_map();
    char buf[32];
    for (int i = 0; i < 60; i++) {
        snprintf(buf, sizeof(buf), "value_%d", i);
        String* v = String_from_cstr(buf);
        HashMap_put_val_move(m, (u8*)&i, (u8**)&v);
    }
    WC_ASSERT_EQ_U64(HashMap_size(m), 60);
    for (int i = 0; i < 60; i++) {
        snprintf(buf, sizeof(buf), "value_%d", i);
        String* stored = (String*)HashMap_get_ptr(m, (u8*)&i);
        WC_ASSERT_NOT_NULL(stored);
        WC_ASSERT_TRUE(String_equals_cstr(stored, buf));
    }
    HashMap_destroy(m);
}


/* ════════════════════════════════════════════════════════════════════════════
 * String -> String  (owned key + val)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_str_key_lookup(void)
{
    HashMap* m  = str_str_map();
    String*  k1 = String_from_cstr("name");
    String*  v1 = String_from_cstr("Alice");
    HashMap_put_move(m, (u8**)&k1, (u8**)&v1);

    WC_ASSERT_NULL(k1);
    WC_ASSERT_NULL(v1);

    String key;
    String_create_stk(&key, "name");
    String* found = (String*)HashMap_get_ptr(m, (u8*)&key);
    WC_ASSERT_NOT_NULL(found);
    WC_ASSERT_TRUE(String_equals_cstr(found, "Alice"));

    String_destroy_stk(&key);
    HashMap_destroy(m);
}

static void test_str_key_miss(void)
{
    HashMap* m = str_str_map();
    String k;
    String_create_stk(&k, "missing");
    WC_ASSERT_FALSE(HashMap_has(m, (u8*)&k));
    String_destroy_stk(&k);
    HashMap_destroy(m);
}

static void test_str_key_update_discards_dup_key(void)
{
    // put_move on an existing key: incoming key freed, new val stored
    HashMap* m = str_str_map();
    MAP_PUT_STR_STR(m, "lang", "C");
    MAP_PUT_STR_STR(m, "lang", "C11");

    WC_ASSERT_EQ_U64(HashMap_size(m), 1);
    String k;
    String_create_stk(&k, "lang");
    String* v = (String*)HashMap_get_ptr(m, (u8*)&k);
    WC_ASSERT_TRUE(String_equals_cstr(v, "C11"));
    String_destroy_stk(&k);
    HashMap_destroy(m);
}

static void test_str_key_del(void)
{
    HashMap* m = str_str_map();
    MAP_PUT_STR_STR(m, "fruit", "apple");

    String k;
    String_create_stk(&k, "fruit");
    WC_ASSERT_TRUE(HashMap_del(m, (u8*)&k, NULL));
    WC_ASSERT_FALSE(HashMap_has(m, (u8*)&k));
    WC_ASSERT_EQ_U64(HashMap_size(m), 0);
    String_destroy_stk(&k);
    HashMap_destroy(m);
}

static void test_str_key_put_key_move(void)
{
    // put_key_move: key is moved in (nulled), val is copied
    HashMap* m = str_str_map();
    String*  k = String_from_cstr("animal");
    String   v;
    String_create_stk(&v, "cat");

    HashMap_put_key_move(m, (u8**)&k, (u8*)&v);
    WC_ASSERT_NULL(k);

    String lookup;
    String_create_stk(&lookup, "animal");
    String* stored = (String*)HashMap_get_ptr(m, (u8*)&lookup);
    WC_ASSERT_NOT_NULL(stored);
    WC_ASSERT_TRUE(String_equals_cstr(stored, "cat"));

    String_destroy_stk(&v);
    String_destroy_stk(&lookup);
    HashMap_destroy(m);
}

static void test_str_str_resize_preserves_data(void)
{
    HashMap* m = str_str_map();
    char key_buf[16], val_buf[16];
    for (int i = 0; i < 40; i++) {
        snprintf(key_buf, sizeof(key_buf), "key%d", i);
        snprintf(val_buf, sizeof(val_buf), "val%d", i);
        MAP_PUT_STR_STR(m, key_buf, val_buf);
    }
    WC_ASSERT_EQ_U64(HashMap_size(m), 40);

    for (int i = 0; i < 40; i++) {
        snprintf(key_buf, sizeof(key_buf), "key%d", i);
        snprintf(val_buf, sizeof(val_buf), "val%d", i);
        String k;
        String_create_stk(&k, key_buf);
        String* v = (String*)HashMap_get_ptr(m, (u8*)&k);
        WC_ASSERT_NOT_NULL(v);
        WC_ASSERT_TRUE(String_equals_cstr(v, val_buf));
        String_destroy_stk(&k);
    }
    HashMap_destroy(m);
}

static void test_str_str_del_frees_both(void)
{
    // del on a str->str entry must free both key and val heap buffers
    HashMap* m = str_str_map();
    MAP_PUT_STR_STR(m, "x", "y");

    String k;
    String_create_stk(&k, "x");
    WC_ASSERT_TRUE(HashMap_del(m, (u8*)&k, NULL));
    WC_ASSERT_EQ_U64(HashMap_size(m), 0);
    String_destroy_stk(&k);
    HashMap_destroy(m);
}

static void test_str_str_clear_frees_all(void)
{
    HashMap* m = str_str_map();
    for (int i = 0; i < 10; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "k%d", i);
        MAP_PUT_STR_STR(m, buf, "v");
    }
    HashMap_clear(m);
    WC_ASSERT_EQ_U64(HashMap_size(m), 0);

    // Usable after clear
    MAP_PUT_STR_STR(m, "after", "clear");
    String k;
    String_create_stk(&k, "after");
    WC_ASSERT_TRUE(HashMap_has(m, (u8*)&k));
    String_destroy_stk(&k);
    HashMap_destroy(m);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void HashMap_suite(void)
{
    WC_SUITE("HashMap — int->int (POD)");
    WC_RUN(test_put_and_get);
    WC_RUN(test_put_update);
    WC_RUN(test_has);
    WC_RUN(test_del);
    WC_RUN(test_del_copies_out);
    WC_RUN(test_del_missing_returns_false);
    WC_RUN(test_del_on_empty_map);
    WC_RUN(test_get_ptr);
    WC_RUN(test_get_ptr_missing_returns_null);
    WC_RUN(test_get_missing_returns_false);
    WC_RUN(test_size_tracks_inserts);
    WC_RUN(test_resize_preserves_data);
    WC_RUN(test_del_correctness_after_many_deletes);

    WC_SUITE("HashMap — delete correctness");
    WC_RUN(test_del_reinsert);
    WC_RUN(test_del_mid_chain);
    WC_RUN(test_delete_reinsert_cycle);
    WC_RUN(test_del_first_in_chain);
    WC_RUN(test_del_all_then_reinsert);

    WC_SUITE("HashMap — clear");
    WC_RUN(test_clear_empties_map);
    WC_RUN(test_clear_then_reuse);
    WC_RUN(test_clear_frees_String_vals);
    WC_RUN(test_clear_empty_map);

    WC_SUITE("HashMap — copy");
    WC_RUN(test_copy_int_map);
    WC_RUN(test_copy_independence);
    WC_RUN(test_copy_str_str_map);
    WC_RUN(test_copy_empty_map);
    WC_RUN(test_copy_then_del_src_key);

    WC_SUITE("HashMap — Macros");
    // WC_RUN(test_foreach_visits_all);
    // WC_RUN(test_foreach_skips_empty);
    // WC_RUN(test_foreach_val_typed);
    // WC_RUN(test_foreach_empty_map);
    // WC_RUN(test_foreach_key_val_consistent);

    WC_SUITE("HashMap — int->String (owned val)");
    WC_RUN(test_str_val_put_copy);
    WC_RUN(test_str_val_independence);
    WC_RUN(test_str_val_move);
    WC_RUN(test_str_val_update_frees_old);
    WC_RUN(test_str_val_move_updates_existing);
    WC_RUN(test_str_val_del_with_out);
    WC_RUN(test_str_val_many_inserts_and_gets);

    WC_SUITE("HashMap — String->String (owned key+val)");
    WC_RUN(test_str_key_lookup);
    WC_RUN(test_str_key_miss);
    WC_RUN(test_str_key_update_discards_dup_key);
    WC_RUN(test_str_key_del);
    WC_RUN(test_str_key_put_key_move);
    WC_RUN(test_str_str_resize_preserves_data);
    WC_RUN(test_str_str_del_frees_both);
    WC_RUN(test_str_str_clear_frees_all);
}
