#include "arena.h"
#include "common.h"
#include "hashmap.h"
#include "map_setup.h"
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
    return HashMap_create(WC_LIBC, sizeof(String), sizeof(String), wyhash_str, str_cmp, &wc_str_ops, &wc_str_ops);
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
    b8 was_update = HashMap_put(&m, &k, &v2);
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
    wc_allocator al = Arena_allocator(&a);

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
    pcg32_rand_seed(42, 54);

    // Queue: interleaved push/pop that wraps the circular buffer and resizes.
    Queue q     = Queue_create(WC_LIBC, 4, sizeof(u32), NULL);
    u64   q_sum = 0;
    for (int i = 0; i < 5000; i++) {
        u32 r = pcg32_rand();
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
        u32 k = pcg32_rand_bounded(3000);
        u64 v = pcg32_rand();
        if (pcg32_rand_bounded(5) == 0) {
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
