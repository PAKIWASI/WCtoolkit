#include "arena.h"
#include "common.h"
#include "hashmap.h"
#include "queue.h"
#include "random.h"
#include "test_support.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_helpers.h"
#include "wc_macros.h"
#include "wc_string.h"

#include <stdalign.h>
#include <stdio.h>
#include <string.h>


/* ── Map constructors ────────────────────────────────────────────────────── */

static HashMap int_map(void)
{
    return HashMap_create(WC_LIBC, sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
}

static HashMap int_str_map(void)
{
    return HashMap_create(WC_LIBC, sizeof(int), sizeof(String), NULL, NULL, NULL, &wc_str_ops);
}

static HashMap str_str_map(void)
{
    return HashMap_create(WC_LIBC, sizeof(String), sizeof(String), wc_hash_str, str_cmp, &wc_str_ops, &wc_str_ops);
}


/* ════════════════════════════════════════════════════════════════════════════
 * int -> int  (POD, no copy/move/del)
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashmap, put_and_get)
{
    HashMap m = int_map();
    int     k = 1, v = 100;
    HashMap_put(&m, &k, &v);

    int out = 0;
    EXPECT_TRUE(HashMap_get(&m, &k, &out));
    EXPECT_EQ(out, 100);
    HashMap_destroy(&m);
}

UTEST(hashmap, put_update)
{
    HashMap m = int_map();
    int     k = 1, v1 = 10, v2 = 20;
    HashMap_put(&m, &k, &v1);
    bool was_update = HashMap_put(&m, &k, &v2);
    EXPECT_TRUE(was_update);

    int out = 0;
    HashMap_get(&m, &k, &out);
    EXPECT_EQ(out, 20);
    EXPECT_EQ(HashMap_size(&m), 1u);
    HashMap_destroy(&m);
}

UTEST(hashmap, has)
{
    HashMap m = int_map();
    int     k = 5, v = 0;
    EXPECT_FALSE(HashMap_has(&m, &k));
    HashMap_put(&m, &k, &v);
    EXPECT_TRUE(HashMap_has(&m, &k));
    HashMap_destroy(&m);
}

UTEST(hashmap, del)
{
    HashMap m = int_map();
    int     k = 3, v = 42;
    HashMap_put(&m, &k, &v);
    EXPECT_TRUE(HashMap_del(&m, &k, NULL));
    EXPECT_FALSE(HashMap_has(&m, &k));
    EXPECT_EQ(HashMap_size(&m), 0u);
    HashMap_destroy(&m);
}

UTEST(hashmap, del_copies_out)
{
    HashMap m = int_map();
    int     k = 7, v = 99, out = 0;
    HashMap_put(&m, &k, &v);
    HashMap_del(&m, &k, &out);
    EXPECT_EQ(out, 99);
    HashMap_destroy(&m);
}

UTEST(hashmap, del_missing_returns_false)
{
    HashMap m = int_map();
    int     k = 404;
    EXPECT_FALSE(HashMap_del(&m, &k, NULL));
    HashMap_destroy(&m);
}

UTEST(hashmap, del_on_empty_map)
{
    HashMap m = int_map();
    int     k = 1;
    EXPECT_FALSE(HashMap_del(&m, &k, NULL));
    EXPECT_EQ(HashMap_size(&m), 0u);
    HashMap_destroy(&m);
}

UTEST(hashmap, get_ptr)
{
    HashMap m = int_map();
    int     k = 2, v = 55;
    HashMap_put(&m, &k, &v);
    int* ptr = (int*)HashMap_get_ptr(&m, &k);
    EXPECT_TRUE((ptr) != NULL);
    EXPECT_EQ(*ptr, 55);

    // Mutate through ptr — must be visible via get
    *ptr    = 66;
    int out = 0;
    HashMap_get(&m, &k, &out);
    EXPECT_EQ(out, 66);
    HashMap_destroy(&m);
}

UTEST(hashmap, get_ptr_missing_returns_null)
{
    HashMap m = int_map();
    int     k = 999;
    EXPECT_TRUE((HashMap_get_ptr(&m, &k)) == NULL);
    HashMap_destroy(&m);
}

UTEST(hashmap, get_missing_returns_false)
{
    HashMap m = int_map();
    int     k = 999, out = 0;
    EXPECT_FALSE(HashMap_get(&m, &k, &out));
    HashMap_destroy(&m);
}

UTEST(hashmap, size_tracks_inserts)
{
    HashMap m = int_map();
    EXPECT_EQ(HashMap_size(&m), 0u);
    EXPECT_TRUE(HashMap_empty(&m));

    for (int i = 0; i < 10; i++) {
        int v = i * 10;
        HashMap_put(&m, &i, &v);
    }
    EXPECT_EQ(HashMap_size(&m), 10u);
    EXPECT_FALSE(HashMap_empty(&m));
    HashMap_destroy(&m);
}

UTEST(hashmap, resize_preserves_data)
{
    HashMap m = int_map();
    for (int i = 0; i < 50; i++) {
        int v = i * 2;
        HashMap_put(&m, &i, &v);
    }
    EXPECT_EQ(HashMap_size(&m), 50u);
    for (int i = 0; i < 50; i++) {
        int out = 0;
        EXPECT_TRUE(HashMap_get(&m, &i, &out));
        EXPECT_EQ(out, i * 2);
    }
    HashMap_destroy(&m);
}

// Robin Hood + backward-shift delete doesn't shrink — no LOAD_FACTOR_SHRINK.
// Verify correctness after heavy deletion instead.
UTEST(hashmap, del_correctness_after_many_deletes)
{
    HashMap m = int_map();
    for (int i = 0; i < 50; i++) {
        int v = i;
        HashMap_put(&m, &i, &v);
    }
    for (int i = 0; i < 48; i++) {
        HashMap_del(&m, &i, NULL);
    }
    EXPECT_EQ(HashMap_size(&m), 2u);

    for (int i = 48; i < 50; i++) {
        int out = 0;
        EXPECT_TRUE(HashMap_get(&m, &i, &out));
        EXPECT_EQ(out, i);
    }
    HashMap_destroy(&m);
}


/* ════════════════════════════════════════════════════════════════════════════
 * delete correctness  (backward-shift, no tombstones)
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashmap, del_reinsert)
{
    // Delete a key then reinsert it — must succeed and be findable
    HashMap m = int_map();
    int     k = 42, v1 = 1, v2 = 2;
    HashMap_put(&m, &k, &v1);
    HashMap_del(&m, &k, NULL);
    EXPECT_FALSE(HashMap_has(&m, &k));

    HashMap_put(&m, &k, &v2);
    int out = 0;
    EXPECT_TRUE(HashMap_get(&m, &k, &out));
    EXPECT_EQ(out, 2);
    EXPECT_EQ(HashMap_size(&m), 1u);
    HashMap_destroy(&m);
}

UTEST(hashmap, del_mid_chain)
{
    // Insert keys that will chain during probing, delete one mid-chain,
    // then verify all remaining keys are still reachable.
    // Backward-shift delete keeps the Robin Hood invariant intact.
    HashMap m = int_map();
    for (int i = 0; i < 20; i++) {
        int v = i * 10;
        HashMap_put(&m, &i, &v);
    }

    int mid = 5;
    HashMap_del(&m, &mid, NULL);

    for (int i = 0; i < 20; i++) {
        if (i == mid) {
            continue;
        }
        int out = 0;
        EXPECT_TRUE(HashMap_get(&m, &i, &out));
        EXPECT_EQ(out, i * 10);
    }
    HashMap_destroy(&m);
}

UTEST(hashmap, delete_reinsert_cycle)
{
    // Repeated delete+reinsert must not corrupt or leak
    HashMap m = int_map();
    int     k = 7;
    for (int cycle = 0; cycle < 20; cycle++) {
        int v = cycle;
        HashMap_put(&m, &k, &v);
        int out = 0;
        HashMap_get(&m, &k, &out);
        EXPECT_EQ(out, cycle);
        HashMap_del(&m, &k, NULL);
        EXPECT_EQ(HashMap_size(&m), 0u);
    }
    HashMap_destroy(&m);
}

UTEST(hashmap, del_first_in_chain)
{
    // Deleting the head of a probe chain must leave the rest reachable
    HashMap m = int_map();
    for (int i = 0; i < 15; i++) {
        int v = i;
        HashMap_put(&m, &i, &v);
    }
    int head = 0;
    HashMap_del(&m, &head, NULL);
    EXPECT_FALSE(HashMap_has(&m, &head));

    for (int i = 1; i < 15; i++) {
        int out = 0;
        EXPECT_TRUE(HashMap_get(&m, &i, &out));
        EXPECT_EQ(out, i);
    }
    HashMap_destroy(&m);
}

UTEST(hashmap, del_all_then_reinsert)
{
    // Delete every key then reinsert — map must be fully functional
    HashMap m = int_map();
    for (int i = 0; i < 20; i++) {
        int v = i;
        HashMap_put(&m, &i, &v);
    }
    for (int i = 0; i < 20; i++) {
        HashMap_del(&m, &i, NULL);
    }
    EXPECT_EQ(HashMap_size(&m), 0u);
    EXPECT_TRUE(HashMap_empty(&m));

    for (int i = 0; i < 20; i++) {
        int v = i * 3;
        HashMap_put(&m, &i, &v);
    }
    EXPECT_EQ(HashMap_size(&m), 20u);
    for (int i = 0; i < 20; i++) {
        int out = 0;
        EXPECT_TRUE(HashMap_get(&m, &i, &out));
        EXPECT_EQ(out, i * 3);
    }
    HashMap_destroy(&m);
}


/* ════════════════════════════════════════════════════════════════════════════
 * HashMap_clear
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashmap, clear_empties_map)
{
    HashMap m = int_map();
    for (int i = 0; i < 10; i++) {
        int v = i;
        HashMap_put(&m, &i, &v);
    }
    u64 cap_before = HashMap_capacity(&m);
    HashMap_clear(&m);

    EXPECT_EQ(HashMap_size(&m), 0u);
    EXPECT_TRUE(HashMap_empty(&m));
    EXPECT_EQ(HashMap_capacity(&m), cap_before); // capacity unchanged
    for (int i = 0; i < 10; i++) {
        EXPECT_FALSE(HashMap_has(&m, &i));
    }
    HashMap_destroy(&m);
}

UTEST(hashmap, clear_then_reuse)
{
    HashMap m = int_map();
    for (int i = 0; i < 10; i++) {
        int v = i;
        HashMap_put(&m, &i, &v);
    }
    HashMap_clear(&m);

    for (int i = 100; i < 110; i++) {
        int v = i * 2;
        HashMap_put(&m, &i, &v);
    }
    EXPECT_EQ(HashMap_size(&m), 10u);
    for (int i = 100; i < 110; i++) {
        int out = 0;
        EXPECT_TRUE(HashMap_get(&m, &i, &out));
        EXPECT_EQ(out, i * 2);
    }
    HashMap_destroy(&m);
}

UTEST(hashmap, clear_frees_String_vals)
{
    // clear must properly call del on owned String resources
    HashMap m = int_str_map();
    for (int i = 0; i < 5; i++) {
        String v = String_from_cstr(WC_LIBC, "owned");
        HashMap_put_val_move(&m, &i, &v);
        String_destroy(&v); // safe on zeroed
    }
    HashMap_clear(&m);
    EXPECT_EQ(HashMap_size(&m), 0u);

    // Map must still be usable after clearing owned-resource entries
    int    k = 99;
    String v = String_from_cstr(WC_LIBC, "after_clear");
    HashMap_put_val_move(&m, &k, &v);
    String_destroy(&v); // safe on zeroed
    EXPECT_TRUE(HashMap_has(&m, &k));
    HashMap_destroy(&m);
}

UTEST(hashmap, clear_empty_map)
{
    // clear on an already-empty map must be a safe no-op
    HashMap m = int_map();
    HashMap_clear(&m);
    EXPECT_EQ(HashMap_size(&m), 0u);
    HashMap_destroy(&m);
}


/* ════════════════════════════════════════════════════════════════════════════
 * HashMap_copy
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashmap, copy_int_map)
{
    HashMap src = int_map();
    for (int i = 0; i < 10; i++) {
        int v = i * 3;
        HashMap_put(&src, &i, &v);
    }

    HashMap dest = HashMap_copy(WC_LIBC, &src);
    EXPECT_EQ(HashMap_size(&dest), HashMap_size(&src));

    for (int i = 0; i < 10; i++) {
        int out = 0;
        EXPECT_TRUE(HashMap_get(&dest, &i, &out));
        EXPECT_EQ(out, i * 3);
    }

    HashMap_destroy(&src);
    HashMap_destroy(&dest);
}

UTEST(hashmap, copy_independence)
{
    // Mutating dest must not affect src
    HashMap src = int_map();
    int     k = 1, v = 10;
    HashMap_put(&src, &k, &v);

    HashMap dest = HashMap_copy(WC_LIBC, &src);

    int v2 = 99;
    HashMap_put(&dest, &k, &v2);

    int src_out = 0, dest_out = 0;
    HashMap_get(&src, &k, &src_out);
    HashMap_get(&dest, &k, &dest_out);
    EXPECT_EQ(src_out, 10);
    EXPECT_EQ(dest_out, 99);

    HashMap_destroy(&src);
    HashMap_destroy(&dest);
}

UTEST(hashmap, copy_str_str_map)
{
    // Deep copy: destroying src must not corrupt dest's String data
    HashMap src = str_str_map();
    MAP_PUT_STR_STR(&src, "name", "Alice");
    MAP_PUT_STR_STR(&src, "city", "London");
    MAP_PUT_STR_STR(&src, "color", "blue");

    HashMap dest = HashMap_copy(WC_LIBC, &src);
    EXPECT_EQ(HashMap_size(&dest), 3u);

    HashMap_destroy(&src); // src gone — dest must still be intact

    String  k     = String_from_cstr(WC_LIBC, "city");
    String* found = (String*)HashMap_get_ptr(&dest, &k);
    EXPECT_TRUE((found) != NULL);
    EXPECT_TRUE(String_equals_cstr(found, "London"));
    String_destroy(&k);

    HashMap_destroy(&dest);
}

UTEST(hashmap, copy_empty_map)
{
    HashMap src  = int_map();
    HashMap dest = HashMap_copy(WC_LIBC, &src);
    EXPECT_EQ(HashMap_size(&dest), 0u);
    EXPECT_EQ(HashMap_capacity(&dest), HashMap_capacity(&src));
    HashMap_destroy(&src);
    HashMap_destroy(&dest);
}

UTEST(hashmap, copy_then_del_src_key)
{
    // Deleting from src after copy must not affect dest
    HashMap src = int_map();
    int     k = 5, v = 50;
    HashMap_put(&src, &k, &v);

    HashMap dest = HashMap_copy(WC_LIBC, &src);

    HashMap_del(&src, &k, NULL);
    EXPECT_FALSE(HashMap_has(&src, &k));
    EXPECT_TRUE(HashMap_has(&dest, &k));

    HashMap_destroy(&src);
    HashMap_destroy(&dest);
}


/* ════════════════════════════════════════════════════════════════════════════
 * int -> String  (owned val)
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashmap, str_val_put_copy)
{
    HashMap m  = int_str_map();
    int     k  = 1;
    String  sv = String_from_cstr(WC_LIBC, "hello");
    HashMap_put(&m, &k, &sv);

    String* got = (String*)HashMap_get_ptr(&m, &k);
    EXPECT_TRUE((got) != NULL);
    EXPECT_TRUE(String_equals_cstr(got, "hello"));

    String_destroy(&sv);
    HashMap_destroy(&m);
}

UTEST(hashmap, str_val_independence)
{
    // Mutating source after put must not affect stored copy
    HashMap m  = int_str_map();
    int     k  = 1;
    String  sv = String_from_cstr(WC_LIBC, "original");
    HashMap_put(&m, &k, &sv);
    String_append_cstr(&sv, "_mutated");

    String* stored = (String*)HashMap_get_ptr(&m, &k);
    EXPECT_TRUE(String_equals_cstr(stored, "original"));

    String_destroy(&sv);
    HashMap_destroy(&m);
}

UTEST(hashmap, str_val_move)
{
    HashMap m   = int_str_map();
    int     k   = 2;
    String  src = String_from_cstr(WC_LIBC, "moved");
    HashMap_put_val_move(&m, &k, &src);
    String_destroy(&src); // moved-from: zeroed, safe destroy

    String* stored = (String*)HashMap_get_ptr(&m, &k);
    EXPECT_TRUE(String_equals_cstr(stored, "moved"));
    HashMap_destroy(&m);
}

UTEST(hashmap, str_val_update_frees_old)
{
    // Updating a String val must not leak the old heap buffer
    HashMap m = int_str_map();
    int     k = 1;
    MAP_PUT_INT_STR(&m, k, "first");
    MAP_PUT_INT_STR(&m, k, "second");

    EXPECT_EQ(HashMap_size(&m), 1u);
    String* stored = (String*)HashMap_get_ptr(&m, &k);
    EXPECT_TRUE(String_equals_cstr(stored, "second"));
    HashMap_destroy(&m);
}

UTEST(hashmap, str_val_move_updates_existing)
{
    // put_val_move on an existing key must free old val and store new one
    HashMap m = int_str_map();
    int     k = 5;
    MAP_PUT_INT_STR(&m, k, "old");

    String v = String_from_cstr(WC_LIBC, "new");
    HashMap_put_val_move(&m, &k, &v);
    String_destroy(&v); // moved-from: zeroed, safe destroy

    String* stored = (String*)HashMap_get_ptr(&m, &k);
    EXPECT_TRUE(String_equals_cstr(stored, "new"));
    EXPECT_EQ(HashMap_size(&m), 1u);
    HashMap_destroy(&m);
}

UTEST(hashmap, str_val_del_with_out)
{
    // del with non-null out must copy the String before destroying it
    HashMap m = int_str_map();
    int     k = 3;
    MAP_PUT_INT_STR(&m, k, "goodbye");

    String out = String_create(WC_LIBC);
    HashMap_del(&m, &k, &out);
    EXPECT_TRUE(String_equals_cstr(&out, "goodbye"));

    String_destroy(&out);
    HashMap_destroy(&m);
}

UTEST(hashmap, str_val_many_inserts_and_gets)
{
    // Stress: many int->String pairs across multiple resizes
    HashMap m = int_str_map();
    char    buf[32];
    for (int i = 0; i < 60; i++) {
        snprintf(buf, sizeof(buf), "value_%d", i);
        String v = String_from_cstr(WC_LIBC, buf);
        HashMap_put_val_move(&m, &i, &v);
        String_destroy(&v); // moved-from: zeroed, safe destroy
    }
    EXPECT_EQ(HashMap_size(&m), 60u);
    for (int i = 0; i < 60; i++) {
        snprintf(buf, sizeof(buf), "value_%d", i);
        String* stored = (String*)HashMap_get_ptr(&m, &i);
        EXPECT_TRUE((stored) != NULL);
        EXPECT_TRUE(String_equals_cstr(stored, buf));
    }
    HashMap_destroy(&m);
}


/* ════════════════════════════════════════════════════════════════════════════
 * String -> String  (owned key + val)
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashmap, str_key_lookup)
{
    HashMap m  = str_str_map();
    String  k1 = String_from_cstr(WC_LIBC, "name");
    String  v1 = String_from_cstr(WC_LIBC, "Alice");
    HashMap_put_move(&m, &k1, &v1);
    String_destroy(&k1); // moved-from: zeroed, safe destroy
    String_destroy(&v1); // moved-from: zeroed, safe destroy

    String  key   = String_from_cstr(WC_LIBC, "name");
    String* found = (String*)HashMap_get_ptr(&m, &key);
    EXPECT_TRUE((found) != NULL);
    EXPECT_TRUE(String_equals_cstr(found, "Alice"));

    String_destroy(&key);
    HashMap_destroy(&m);
}

UTEST(hashmap, str_key_miss)
{
    HashMap m = str_str_map();
    String  k = String_from_cstr(WC_LIBC, "missing");
    EXPECT_FALSE(HashMap_has(&m, &k));
    String_destroy(&k);
    HashMap_destroy(&m);
}

UTEST(hashmap, str_key_update_discards_dup_key)
{
    // put_move on an existing key: incoming key freed, new val stored
    HashMap m = str_str_map();
    MAP_PUT_STR_STR(&m, "lang", "C");
    MAP_PUT_STR_STR(&m, "lang", "C11");

    EXPECT_EQ(HashMap_size(&m), 1u);
    String  k = String_from_cstr(WC_LIBC, "lang");
    String* v = (String*)HashMap_get_ptr(&m, &k);
    EXPECT_TRUE(String_equals_cstr(v, "C11"));
    String_destroy(&k);
    HashMap_destroy(&m);
}

UTEST(hashmap, str_key_del)
{
    HashMap m = str_str_map();
    MAP_PUT_STR_STR(&m, "fruit", "apple");

    String k = String_from_cstr(WC_LIBC, "fruit");
    EXPECT_TRUE(HashMap_del(&m, &k, NULL));
    EXPECT_FALSE(HashMap_has(&m, &k));
    EXPECT_EQ(HashMap_size(&m), 0u);
    String_destroy(&k);
    HashMap_destroy(&m);
}

UTEST(hashmap, str_key_put_key_move)
{
    // put_key_move: key is moved in (nulled), val is copied
    HashMap m = str_str_map();
    String  k = String_from_cstr(WC_LIBC, "animal");
    String  v = String_from_cstr(WC_LIBC, "cat");

    HashMap_put_key_move(&m, &k, &v);
    String_destroy(&k); // moved-from: zeroed, safe destroy

    String  lookup = String_from_cstr(WC_LIBC, "animal");
    String* stored = (String*)HashMap_get_ptr(&m, &lookup);
    EXPECT_TRUE((stored) != NULL);
    EXPECT_TRUE(String_equals_cstr(stored, "cat"));

    String_destroy(&v);
    String_destroy(&lookup);
    HashMap_destroy(&m);
}

UTEST(hashmap, str_str_resize_preserves_data)
{
    HashMap m = str_str_map();
    char    key_buf[16], val_buf[16];
    for (int i = 0; i < 40; i++) {
        snprintf(key_buf, sizeof(key_buf), "key%d", i);
        snprintf(val_buf, sizeof(val_buf), "val%d", i);
        MAP_PUT_STR_STR(&m, key_buf, val_buf);
    }
    EXPECT_EQ(HashMap_size(&m), 40u);

    for (int i = 0; i < 40; i++) {
        snprintf(key_buf, sizeof(key_buf), "key%d", i);
        snprintf(val_buf, sizeof(val_buf), "val%d", i);
        String  k = String_from_cstr(WC_LIBC, key_buf);
        String* v = (String*)HashMap_get_ptr(&m, &k);
        EXPECT_TRUE((v) != NULL);
        EXPECT_TRUE(String_equals_cstr(v, val_buf));
        String_destroy(&k);
    }
    HashMap_destroy(&m);
}

UTEST(hashmap, str_str_del_frees_both)
{
    // del on a str->str entry must free both key and val heap buffers
    HashMap m = str_str_map();
    MAP_PUT_STR_STR(&m, "x", "y");

    String k = String_from_cstr(WC_LIBC, "x");
    EXPECT_TRUE(HashMap_del(&m, &k, NULL));
    EXPECT_EQ(HashMap_size(&m), 0u);
    String_destroy(&k);
    HashMap_destroy(&m);
}

UTEST(hashmap, str_str_clear_frees_all)
{
    HashMap m = str_str_map();
    for (int i = 0; i < 10; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "k%d", i);
        MAP_PUT_STR_STR(&m, buf, "v");
    }
    HashMap_clear(&m);
    EXPECT_EQ(HashMap_size(&m), 0u);

    // Usable after clear
    MAP_PUT_STR_STR(&m, "after", "clear");
    String k = String_from_cstr(WC_LIBC, "after");
    EXPECT_TRUE(HashMap_has(&m, &k));
    String_destroy(&k);
    HashMap_destroy(&m);
}


/* ════════════════════════════════════════════════════════════════════════════
 * Phase 5 New Tests: resizes, cross-allocator copy, move, zero-state
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashmap, fill_past_several_resizes)
{
    HashMap   m     = int_map();
    const int count = 2000;
    for (int i = 0; i < count; i++) {
        int v = i * 5;
        HashMap_put(&m, &i, &v);
    }
    EXPECT_EQ(HashMap_size(&m), (u64)count);

    for (int i = 0; i < count; i++) {
        int out = 0;
        EXPECT_TRUE(HashMap_get(&m, &i, &out));
        EXPECT_EQ(out, i * 5);
    }
    HashMap_destroy(&m);
}

UTEST(hashmap, cross_alloc_copy)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(64));
    const wc_allocator* al = Arena_allocator(&a);

    HashMap src = HashMap_create(al, sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
    for (int i = 0; i < 20; i++) {
        int v = i * 11;
        HashMap_put(&src, &i, &v);
    }

    // Copy from Arena to libc
    HashMap dest = HashMap_copy(WC_LIBC, &src);
    EXPECT_EQ(HashMap_size(&dest), 20u);

    // Destroy src and arena, dest must remain valid in libc
    HashMap_destroy(&src);
    Arena_destroy(&a);

    for (int i = 0; i < 20; i++) {
        int out = 0;
        EXPECT_TRUE(HashMap_get(&dest, &i, &out));
        EXPECT_EQ(out, i * 11);
    }
    HashMap_destroy(&dest);
}

UTEST(hashmap, move)
{
    HashMap src = int_map();
    int     k = 10, v = 999;
    HashMap_put(&src, &k, &v);

    HashMap dest;
    HashMap_move(&dest, &src);

    EXPECT_EQ(src.capacity, 0u);
    EXPECT_EQ(HashMap_size(&dest), 1u);
    int out = 0;
    EXPECT_TRUE(HashMap_get(&dest, &k, &out));
    EXPECT_EQ(out, 999);

    HashMap_destroy(&src); // safe on zeroed
    HashMap_destroy(&dest);
}

#if WC_HAS_FORK
static void die_mutating_zero_hashmap(void)
{
    HashMap m;
    memset(&m, 0, sizeof(m));
    int k = 1, v = 2;
    HashMap_put(&m, &k, &v);
}

UTEST(hashmap, zero_state_fatal)
{
    EXPECT_DIES(die_mutating_zero_hashmap);
}
#endif


static inline u64 golden_mix(u64 h, u64 x)
{
    h ^= x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}

// Fixed-seed queue + hashmap workload. The checksums are order-sensitive for
// the queue (FIFO order is part of the contract) and order-independent for the
// map (bucket order is an implementation detail).
#define GOLDEN_QUEUE_CHECKSUM 0x94a3522e12f42c7ULL
#define GOLDEN_MAP_CHECKSUM   0xbd94ebf87ffabaa7ULL
#define GOLDEN_MAP_SIZE       2370ULL

UTEST(hashmap, golden_queue_hashmap_fixed_seed)
{
    WC_Pcg32 rng = PCG32_INITIALIZER;
    pcg32_rand_seed(&rng, 42, 54);

    // Queue: interleaved push/pop that wraps the circular buffer and resizes.
    Queue q     = Queue_create(WC_LIBC, 4, sizeof(u32), NULL);
    u64   q_sum = 0;
    for (int i = 0; i < 5000; i++) {
        u32 r = pcg32_rand(&rng);
        if ((r & 3) != 0 || Queue_size(&q) == 0) {
            Queue_push(&q, &r);
        } else {
            u32 out = 0;
            Queue_pop(&q, &out);
            q_sum = golden_mix(q_sum, out);
        }
    }
    while (Queue_size(&q) > 0) {
        u32 out = 0;
        Queue_pop(&q, &out);
        q_sum = golden_mix(q_sum, out);
    }
    Queue_destroy(&q);

    // HashMap: puts, overwrites and deletes over a small key space.
    HashMap m = HashMap_create(WC_LIBC, sizeof(u32), sizeof(u64), NULL, NULL, NULL, NULL);
    for (int i = 0; i < 20000; i++) {
        u32 k = pcg32_rand_bounded(&rng, 3000);
        u64 v = pcg32_rand(&rng);
        if (pcg32_rand_bounded(&rng, 5) == 0) {
            HashMap_del(&m, &k, NULL);
        } else {
            HashMap_put(&m, &k, &v);
        }
    }
    u64 m_sum = 0;
    for (u64 i = 0; i < HashMap_bucket_count(&m); i++) {
        if (HashMap_bucket_occupied(&m, i)) {
            u32 k = *(const u32*)HashMap_bucket_key_ptr(&m, i);
            u64 v = *(const u64*)HashMap_bucket_val_ptr(&m, i);
            m_sum += golden_mix(k, v); // sum: independent of bucket order
        }
    }
    u64 m_size = HashMap_size(&m);
    HashMap_destroy(&m);

    printf("[q=0x%llx m=0x%llx n=%llu] ", (unsigned long long)q_sum, (unsigned long long)m_sum,
           (unsigned long long)m_size);
    EXPECT_EQ(q_sum, GOLDEN_QUEUE_CHECKSUM);
    EXPECT_EQ(m_sum, GOLDEN_MAP_CHECKSUM);
    EXPECT_EQ(m_size, GOLDEN_MAP_SIZE);
}


/* ════════════════════════════════════════════════════════════════════════════
 * reserve + probe-length guard (u8 PSL must never wrap to "empty")
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashmap, reserve)
{
    HashMap m = int_map();
    for (int i = 0; i < 100; i++) {
        MAP_PUT(&m, i, i * 2);
    }
    HashMap_reserve(&m, 10000); // on a populated map: entries survive
    u64 cap = HashMap_capacity(&m);
    EXPECT_TRUE(cap * 3 > 10000 * 4);
    for (int i = 100; i < 10000; i++) {
        MAP_PUT(&m, i, i * 2);
    }
    EXPECT_EQ(HashMap_capacity(&m), cap); // never resized again
    EXPECT_EQ(HashMap_size(&m), (u64)10000);
    for (int i = 0; i < 10000; i++) {
        const int* v = HashMap_get_ptr(&m, &i);
        ASSERT_TRUE(v != NULL);
        EXPECT_EQ(*v, i * 2);
    }
    HashMap_reserve(&m, 5); // never shrinks
    EXPECT_EQ(HashMap_capacity(&m), cap);
    HashMap_destroy(&m);
}

// 200 keys share one hash value: probe length reaches ~200 (< 250), the table must stay
// correct. Before the guard, anything past 255 wrapped the u8 PSL to "empty".
static u64 two_value_hash(const void* key, u64 len)
{
    (void)len;
    return (u64)(*(const int*)key % 2);
}

UTEST(hashmap, long_probe_chains_stay_correct)
{
    HashMap m = HashMap_create(WC_LIBC, sizeof(int), sizeof(int), two_value_hash, NULL, NULL, NULL);
    for (int i = 0; i < 200; i++) {
        MAP_PUT(&m, i, i + 1);
    }
    EXPECT_EQ(HashMap_size(&m), (u64)200);
    for (int i = 0; i < 200; i++) {
        const int* v = HashMap_get_ptr(&m, &i);
        ASSERT_TRUE(v != NULL);
        EXPECT_EQ(*v, i + 1);
    }
    int missing = 5000;
    EXPECT_TRUE(HashMap_get_ptr(&m, &missing) == NULL);
    HashMap_destroy(&m);
}

static u64 constant_hash(const void* key, u64 len)
{
    (void)key;
    (void)len;
    return 0;
}

static void die_degenerate_hash(void)
{
    HashMap m = HashMap_create(WC_LIBC, sizeof(int), sizeof(int), constant_hash, NULL, NULL, NULL);
    for (int i = 0; i < 1000; i++) {
        MAP_PUT(&m, i, i);
    }
    HashMap_destroy(&m);
}

UTEST(hashmap, degenerate_hash_dies_instead_of_corrupting)
{
    EXPECT_DIES(die_degenerate_hash);
}


/* ════════════════════════════════════════════════════════════════════════════
 * bucket distribution on structured keys (regression: top-bit indexing once made
 * short sequential keys cluster and tripped the probe-length guard)
 * ════════════════════════════════════════════════════════════════════════════ */

static u32 max_stored_psl(const HashMap* m)
{
    u32 mx = 0;
    for (u64 i = 0; i < m->capacity; i++) {
        if (m->psls[i] > mx) {
            mx = m->psls[i];
        }
    }
    return mx;
}

// At load <= 0.75 a good hash keeps the longest probe under ~12 for 100k keys
// (measured: 7 to 9 with wc_hash). The guard fires at 250.
#define PSL_HEALTHY 24

UTEST(hashmap, structured_keys_distribute)
{
    // sequential, negative, strided and shifted keys
    HashMap m = int_map();
    for (int i = 0; i < 100000; i++) {
        MAP_PUT(&m, i, i);
    }
    EXPECT_TRUE(max_stored_psl(&m) < PSL_HEALTHY);
    HashMap_destroy(&m);

    m = int_map();
    for (int i = 0; i < 100000; i++) {
        int k = -i;
        MAP_PUT(&m, k, i);
    }
    EXPECT_TRUE(max_stored_psl(&m) < PSL_HEALTHY);
    HashMap_destroy(&m);

    // keys that differ only in their top bits: (i << 44) for i < 2^20
    static const u64 strides[] = {8, 4096, (u64)1 << 32, (u64)1 << 44};
    for (u64 s = 0; s < sizeof(strides) / sizeof(strides[0]); s++) {
        m = HashMap_create(WC_LIBC, sizeof(u64), sizeof(int), NULL, NULL, NULL, NULL);
        for (u64 i = 0; i < 50000; i++) {
            u64 k = i * strides[s];
            int v = 1;
            HashMap_put(&m, &k, &v);
        }
        EXPECT_EQ(HashMap_size(&m), (u64)50000);
        EXPECT_TRUE(max_stored_psl(&m) < PSL_HEALTHY);
        HashMap_destroy(&m);
    }
}


// Identity hash: the weakest a custom hash gets. The Fibonacci step in map_home
// must still spread sequential keys over the top bits.
static u64 identity_hash(const void* key, u64 size)
{
    (void)size;
    u64 k = 0;
    memcpy(&k, key, sizeof(int));
    return k;
}

UTEST(hashmap, weak_custom_hash_still_distributes)
{
    HashMap m = HashMap_create(WC_LIBC, sizeof(int), sizeof(int), identity_hash, NULL, NULL, NULL);
    for (int i = 0; i < 100000; i++) {
        MAP_PUT(&m, i, i);
    }
    EXPECT_TRUE(max_stored_psl(&m) < PSL_HEALTHY);
    HashMap_destroy(&m);
}

UTEST(hashmap, set_mode_distributes)
{
    HashMap s = SET_OF(u64);
    for (u64 i = 0; i < 100000; i++) {
        u64 k = i << 40;
        SET_INSERT(&s, k);
    }
    EXPECT_EQ(HashMap_size(&s), (u64)100000);
    EXPECT_TRUE(max_stored_psl(&s) < PSL_HEALTHY);
    HashMap_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * wc_hash: rapidhash nano
 * ════════════════════════════════════════════════════════════════════════════ */

// Outputs of upstream rapidhashNano() (seed 0) over bytes i * 7 + 3. Covers every
// length branch: 0, 1-3, 4-7, 8-16, 17-48, > 48 (bulk loop).
UTEST(hash, matches_rapidhash_reference)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    static const struct {
        u64 len;
        u64 hash;
    } ref[] = {
        {0, 0x0338dc4be2cecdaeULL},  {1, 0xc6939e8fb00709ffULL},   {3, 0x5655e9f764e8fb47ULL},
        {4, 0x49227fb2f401a8fcULL},  {7, 0xf8d2bdfe7f388df8ULL},   {8, 0xf5980b646f822e89ULL},
        {15, 0x71bb053c322ab7cbULL}, {16, 0xd0e68844cb24a480ULL},  {17, 0x7998c836066e171bULL},
        {33, 0x50f708a727e0a6f9ULL}, {48, 0x78e892266d502576ULL},  {49, 0x3b64ec2b4166561cULL},
        {100, 0xde3884105eba67a1ULL}, {128, 0xb5e9b5cc7dcaac79ULL},
    };
    u8 buf[128];
    for (u32 i = 0; i < 128; i++) {
        buf[i] = (u8)((i * 7) + 3);
    }
    for (u64 i = 0; i < sizeof(ref) / sizeof(ref[0]); i++) {
        EXPECT_EQ(wc_hash(buf, ref[i].len), ref[i].hash);
    }
#endif
}

// Flip one input bit: every output bit must flip about half the time. The old
// default failed this on 4 and 8 byte keys (some output bits never moved, bias 0.5).
// Sampling noise over 4096 (in, out) pairs at T = 4000 stays under ~0.035.
static double worst_avalanche_bias(u64 len, bool sequential)
{
    enum { T = 4000 };
    static u32 flips[64][64];
    memset(flips, 0, sizeof(flips));

    WC_Pcg32 rng = PCG32_INITIALIZER;
    pcg32_rand_seed(&rng, 42, 54);
    u8 key[8] = {0};

    for (u32 t = 0; t < T; t++) {
        u64 v = sequential ? t : ((u64)pcg32_rand(&rng) << 32) | pcg32_rand(&rng);
        memcpy(key, &v, len);
        u64 h0 = wc_hash(key, len);
        for (u64 bit = 0; bit < len * 8; bit++) {
            key[bit / 8] ^= (u8)(1U << (bit % 8));
            u64 d = wc_hash(key, len) ^ h0;
            key[bit / 8] ^= (u8)(1U << (bit % 8));
            for (u32 o = 0; o < 64; o++) {
                flips[bit][o] += (u32)((d >> o) & 1);
            }
        }
    }

    double worst = 0;
    for (u64 bit = 0; bit < len * 8; bit++) {
        for (u32 o = 0; o < 64; o++) {
            double p   = (double)flips[bit][o] / T;
            double dev = p > 0.5 ? p - 0.5 : 0.5 - p;
            worst      = dev > worst ? dev : worst;
        }
    }
    return worst;
}

UTEST(hash, avalanche_short_keys)
{
    EXPECT_LT(worst_avalanche_bias(4, false), 0.05);
    EXPECT_LT(worst_avalanche_bias(8, false), 0.05);
    EXPECT_LT(worst_avalanche_bias(4, true), 0.05);
    EXPECT_LT(worst_avalanche_bias(8, true), 0.05);
}


/* ════════════════════════════════════════════════════════════════════════════
 * sets (val_size 0) and key access
 * ════════════════════════════════════════════════════════════════════════════ */

static void die_set_with_val_ops(void)
{
    HashMap s = HashMap_create(WC_LIBC, sizeof(int), 0, NULL, NULL, NULL, &wc_str_ops);
    HashMap_destroy(&s);
}

UTEST(hashmap, set_rejects_val_ops)
{
    EXPECT_DIES(die_set_with_val_ops);
}

UTEST(hashmap, set_takes_no_value_memory)
{
    HashMap s = SET_OF(int);
    EXPECT_EQ(s.val_size, 0u);
    EXPECT_TRUE(s.vals == s.scratch); // the values region is 0 bytes
    for (int i = 0; i < 1000; i++) {
        EXPECT_FALSE(HashMap_put(&s, &i, NULL));
    }
    int dup = 7;
    EXPECT_TRUE(HashMap_put(&s, &dup, NULL));
    EXPECT_EQ(HashMap_size(&s), (u64)1000);
    EXPECT_TRUE(HashMap_del(&s, &dup, NULL));
    EXPECT_FALSE(HashMap_has(&s, &dup));
    HashMap_destroy(&s);
}

UTEST(hashmap, get_key_ptr_returns_stored_key)
{
    HashMap m = MAP_OF(String, int);
    MAP_PUT_STR_INT(&m, "interned", 1);

    String probe = String_from_cstr(WC_LIBC, "interned");
    const String* stored = HashMap_get_key_ptr(&m, &probe);
    EXPECT_TRUE(stored != NULL);
    EXPECT_TRUE(stored != &probe);
    EXPECT_TRUE(String_equals(stored, &probe));
    EXPECT_TRUE(stored == HashMap_bucket_key_ptr(&m, (u64)((const u8*)stored - m.keys) / sizeof(String)));

    String absent = String_from_cstr(WC_LIBC, "absent");
    EXPECT_TRUE(HashMap_get_key_ptr(&m, &absent) == NULL);

    String_destroy(&absent);
    String_destroy(&probe);
    HashMap_destroy(&m);
}
