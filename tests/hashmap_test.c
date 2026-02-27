#include "wc_test.h"
#include "hashmap.h"
#include "String.h"
#include "wc_helpers.h"
#include "wc_macros.h"


/* ── Map constructors ────────────────────────────────────────────────────── */

static hashmap* int_map(void) {
    return hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
}

static hashmap* int_str_map(void) {
    return hashmap_create(sizeof(int), sizeof(String), NULL, NULL, NULL, &wc_str_ops);
}

static hashmap* str_str_map(void) {
    return hashmap_create(sizeof(String), sizeof(String),
                          murmurhash3_str, str_cmp, &wc_str_ops, &wc_str_ops);
}


/* ════════════════════════════════════════════════════════════════════════════
 * int -> int  (POD, no copy/move/del)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_put_and_get(void)
{
    hashmap* m = int_map();
    int k = 1, v = 100;
    hashmap_put(m, (u8*)&k, (u8*)&v);

    int out = 0;
    WC_ASSERT_TRUE(hashmap_get(m, (u8*)&k, (u8*)&out));
    WC_ASSERT_EQ_INT(out, 100);
    hashmap_destroy(m);
}

static void test_put_update(void)
{
    hashmap* m = int_map();
    int k = 1, v1 = 10, v2 = 20;
    hashmap_put(m, (u8*)&k, (u8*)&v1);
    b8 was_update = hashmap_put(m, (u8*)&k, (u8*)&v2);
    WC_ASSERT_TRUE(was_update);

    int out = 0;
    hashmap_get(m, (u8*)&k, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 20);
    WC_ASSERT_EQ_U64(hashmap_size(m), 1);
    hashmap_destroy(m);
}

static void test_has(void)
{
    hashmap* m = int_map();
    int k = 5, v = 0;
    WC_ASSERT_FALSE(hashmap_has(m, (u8*)&k));
    hashmap_put(m, (u8*)&k, (u8*)&v);
    WC_ASSERT_TRUE(hashmap_has(m, (u8*)&k));
    hashmap_destroy(m);
}

static void test_del(void)
{
    hashmap* m = int_map();
    int k = 3, v = 42;
    hashmap_put(m, (u8*)&k, (u8*)&v);
    WC_ASSERT_TRUE(hashmap_del(m, (u8*)&k, NULL));
    WC_ASSERT_FALSE(hashmap_has(m, (u8*)&k));
    WC_ASSERT_EQ_U64(hashmap_size(m), 0);
    hashmap_destroy(m);
}

static void test_del_copies_out(void)
{
    hashmap* m = int_map();
    int k = 7, v = 99, out = 0;
    hashmap_put(m, (u8*)&k, (u8*)&v);
    hashmap_del(m, (u8*)&k, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 99);
    hashmap_destroy(m);
}

static void test_del_missing_returns_false(void)
{
    hashmap* m = int_map();
    int k = 404;
    WC_ASSERT_FALSE(hashmap_del(m, (u8*)&k, NULL));
    hashmap_destroy(m);
}

static void test_del_on_empty_map(void)
{
    hashmap* m = int_map();
    int k = 1;
    WC_ASSERT_FALSE(hashmap_del(m, (u8*)&k, NULL));
    WC_ASSERT_EQ_U64(hashmap_size(m), 0);
    hashmap_destroy(m);
}

static void test_get_ptr(void)
{
    hashmap* m = int_map();
    int k = 2, v = 55;
    hashmap_put(m, (u8*)&k, (u8*)&v);
    int* ptr = (int*)hashmap_get_ptr(m, (u8*)&k);
    WC_ASSERT_NOT_NULL(ptr);
    WC_ASSERT_EQ_INT(*ptr, 55);

    /* mutate through ptr — must be visible via get */
    *ptr = 66;
    int out = 0;
    hashmap_get(m, (u8*)&k, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 66);
    hashmap_destroy(m);
}

static void test_get_ptr_missing_returns_null(void)
{
    hashmap* m = int_map();
    int k = 999;
    WC_ASSERT_NULL(hashmap_get_ptr(m, (u8*)&k));
    hashmap_destroy(m);
}

static void test_get_missing_returns_false(void)
{
    hashmap* m = int_map();
    int k = 999, out = 0;
    WC_ASSERT_FALSE(hashmap_get(m, (u8*)&k, (u8*)&out));
    hashmap_destroy(m);
}

static void test_size_tracks_inserts(void)
{
    hashmap* m = int_map();
    WC_ASSERT_EQ_U64(hashmap_size(m), 0);
    WC_ASSERT_TRUE(hashmap_empty(m));

    for (int i = 0; i < 10; i++) {
        int v = i * 10;
        hashmap_put(m, (u8*)&i, (u8*)&v);
    }
    WC_ASSERT_EQ_U64(hashmap_size(m), 10);
    WC_ASSERT_FALSE(hashmap_empty(m));
    hashmap_destroy(m);
}

static void test_resize_preserves_data(void)
{
    hashmap* m = int_map();
    for (int i = 0; i < 50; i++) {
        int v = i * 2;
        hashmap_put(m, (u8*)&i, (u8*)&v);
    }
    WC_ASSERT_EQ_U64(hashmap_size(m), 50);
    for (int i = 0; i < 50; i++) {
        int out = 0;
        WC_ASSERT_TRUE(hashmap_get(m, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i * 2);
    }
    hashmap_destroy(m);
}

static void test_shrink_on_delete(void)
{
    /* Insert enough to grow, delete most — capacity should shrink back */
    hashmap* m    = int_map();
    u64      cap0 = hashmap_capacity(m);

    for (int i = 0; i < 50; i++) {
        int v = i;
        hashmap_put(m, (u8*)&i, (u8*)&v);
    }
    WC_ASSERT_TRUE(hashmap_capacity(m) > cap0);

    for (int i = 0; i < 48; i++) {
        hashmap_del(m, (u8*)&i, NULL);
    }
    WC_ASSERT_TRUE(hashmap_capacity(m) < 50);

    /* Remaining keys must still be readable */
    for (int i = 48; i < 50; i++) {
        int out = 0;
        WC_ASSERT_TRUE(hashmap_get(m, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i);
    }
    hashmap_destroy(m);
}


/* ── Tombstone correctness ───────────────────────────────────────────────── */

static void test_tombstone_reinsert(void)
{
    /* Delete a key then reinsert it — must succeed and be findable */
    hashmap* m = int_map();
    int k = 42, v1 = 1, v2 = 2;
    hashmap_put(m, (u8*)&k, (u8*)&v1);
    hashmap_del(m, (u8*)&k, NULL);
    WC_ASSERT_FALSE(hashmap_has(m, (u8*)&k));

    hashmap_put(m, (u8*)&k, (u8*)&v2);
    int out = 0;
    WC_ASSERT_TRUE(hashmap_get(m, (u8*)&k, (u8*)&out));
    WC_ASSERT_EQ_INT(out, 2);
    WC_ASSERT_EQ_U64(hashmap_size(m), 1);
    hashmap_destroy(m);
}

static void test_tombstone_probe_chain(void)
{
    /*
     * Insert keys that will chain during probing, delete one mid-chain,
     * then verify all remaining keys are still reachable through the tombstone.
     */
    hashmap* m = int_map();
    for (int i = 0; i < 20; i++) {
        int v = i * 10;
        hashmap_put(m, (u8*)&i, (u8*)&v);
    }

    int mid = 5;
    hashmap_del(m, (u8*)&mid, NULL);

    for (int i = 0; i < 20; i++) {
        if (i == mid) { continue; }
        int out = 0;
        WC_ASSERT_TRUE(hashmap_get(m, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i * 10);
    }
    hashmap_destroy(m);
}

static void test_delete_reinsert_cycle(void)
{
    /* Repeated delete+reinsert must not corrupt or leak */
    hashmap* m = int_map();
    int k = 7;
    for (int cycle = 0; cycle < 20; cycle++) {
        int v = cycle;
        hashmap_put(m, (u8*)&k, (u8*)&v);
        int out = 0;
        hashmap_get(m, (u8*)&k, (u8*)&out);
        WC_ASSERT_EQ_INT(out, cycle);
        hashmap_del(m, (u8*)&k, NULL);
        WC_ASSERT_EQ_U64(hashmap_size(m), 0);
    }
    hashmap_destroy(m);
}


/* ── hashmap_clear ───────────────────────────────────────────────────────── */

static void test_clear_empties_map(void)
{
    hashmap* m = int_map();
    for (int i = 0; i < 10; i++) {
        int v = i;
        hashmap_put(m, (u8*)&i, (u8*)&v);
    }
    hashmap_clear(m);

    WC_ASSERT_EQ_U64(hashmap_size(m), 0);
    WC_ASSERT_TRUE(hashmap_empty(m));
    for (int i = 0; i < 10; i++) {
        WC_ASSERT_FALSE(hashmap_has(m, (u8*)&i));
    }
    hashmap_destroy(m);
}

static void test_clear_then_reuse(void)
{
    hashmap* m = int_map();
    for (int i = 0; i < 10; i++) {
        int v = i;
        hashmap_put(m, (u8*)&i, (u8*)&v);
    }
    hashmap_clear(m);

    /* Re-insert after clear — map must be fully functional */
    for (int i = 100; i < 110; i++) {
        int v = i * 2;
        hashmap_put(m, (u8*)&i, (u8*)&v);
    }
    WC_ASSERT_EQ_U64(hashmap_size(m), 10);
    for (int i = 100; i < 110; i++) {
        int out = 0;
        WC_ASSERT_TRUE(hashmap_get(m, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i * 2);
    }
    hashmap_destroy(m);
}

static void test_clear_frees_string_vals(void)
{
    /* clear must properly call del on owned String resources */
    hashmap* m = int_str_map();
    for (int i = 0; i < 5; i++) {
        String* v = string_from_cstr("owned");
        hashmap_put_val_move(m, (u8*)&i, (u8**)&v);
    }
    hashmap_clear(m);
    WC_ASSERT_EQ_U64(hashmap_size(m), 0);

    /* Map must still be usable after clearing owned-resource entries */
    int k = 99;
    String* v = string_from_cstr("after_clear");
    hashmap_put_val_move(m, (u8*)&k, (u8**)&v);
    WC_ASSERT_TRUE(hashmap_has(m, (u8*)&k));
    hashmap_destroy(m);
}


/* ── hashmap_copy ────────────────────────────────────────────────────────── */

static void test_copy_int_map(void)
{
    hashmap* src = int_map();
    for (int i = 0; i < 10; i++) {
        int v = i * 3;
        hashmap_put(src, (u8*)&i, (u8*)&v);
    }

    hashmap dest;
    hashmap_copy(&dest, src);
    WC_ASSERT_EQ_U64(hashmap_size(&dest), hashmap_size(src));

    for (int i = 0; i < 10; i++) {
        int out = 0;
        WC_ASSERT_TRUE(hashmap_get(&dest, (u8*)&i, (u8*)&out));
        WC_ASSERT_EQ_INT(out, i * 3);
    }

    hashmap_destroy(src);
    hashmap_clear(&dest);
    free(dest.buckets);
}

static void test_copy_independence(void)
{
    /* Mutating dest must not affect src, and vice versa */
    hashmap* src = int_map();
    int k = 1, v = 10;
    hashmap_put(src, (u8*)&k, (u8*)&v);

    hashmap dest;
    hashmap_copy(&dest, src);

    int v2 = 99;
    hashmap_put(&dest, (u8*)&k, (u8*)&v2);

    int src_out = 0, dest_out = 0;
    hashmap_get(src,   (u8*)&k, (u8*)&src_out);
    hashmap_get(&dest, (u8*)&k, (u8*)&dest_out);
    WC_ASSERT_EQ_INT(src_out,  10);
    WC_ASSERT_EQ_INT(dest_out, 99);

    hashmap_destroy(src);
    hashmap_clear(&dest);
    free(dest.buckets);
}

static void test_copy_str_str_map(void)
{
    /* Deep copy: destroying src must not corrupt dest's String data */
    hashmap* src = str_str_map();
    MAP_PUT_STR_STR(src, "name",  "Alice");
    MAP_PUT_STR_STR(src, "city",  "London");
    MAP_PUT_STR_STR(src, "color", "blue");

    hashmap dest;
    hashmap_copy(&dest, src);
    WC_ASSERT_EQ_U64(hashmap_size(&dest), 3);

    hashmap_destroy(src);

    String k;
    string_create_stk(&k, "city");
    String* found = (String*)hashmap_get_ptr(&dest, (u8*)&k);
    WC_ASSERT_NOT_NULL(found);
    WC_ASSERT_TRUE(string_equals_cstr(found, "London"));
    string_destroy_stk(&k);

    hashmap_clear(&dest);
    free(dest.buckets);
}


/* ── MAP_FOREACH_BUCKET / MAP_FOREACH_VAL ───────────────────────────────── */

static void test_foreach_bucket_visits_all(void)
{
    hashmap* m = int_map();
    for (int i = 0; i < 8; i++) {
        int v = i;
        hashmap_put(m, (u8*)&i, (u8*)&v);
    }

    int count = 0, key_sum = 0;
    MAP_FOREACH_BUCKET(m, kv) {
        count++;
        key_sum += *(int*)kv->key;
    }
    WC_ASSERT_EQ_INT(count,   8);
    WC_ASSERT_EQ_INT(key_sum, 0+1+2+3+4+5+6+7);
    hashmap_destroy(m);
}

static void test_foreach_bucket_skips_tombstones(void)
{
    /* Deleted slots (tombstones) must not appear during iteration */
    hashmap* m = int_map();
    for (int i = 0; i < 8; i++) {
        int v = i;
        hashmap_put(m, (u8*)&i, (u8*)&v);
    }
    for (int i = 0; i < 4; i++) {
        hashmap_del(m, (u8*)&i, NULL);
    }

    int count = 0;
    MAP_FOREACH_BUCKET(m, kv) {
        WC_ASSERT_TRUE(*(int*)kv->key >= 4);
        count++;
    }
    WC_ASSERT_EQ_INT(count, 4);
    hashmap_destroy(m);
}

static void test_foreach_val_typed(void)
{
    hashmap* m = int_str_map();
    MAP_PUT_INT_STR(m, 1, "one");
    MAP_PUT_INT_STR(m, 2, "two");
    MAP_PUT_INT_STR(m, 3, "three");

    int count = 0;
    MAP_FOREACH_VAL(m, String, v) {
        WC_ASSERT_NOT_NULL(v);
        WC_ASSERT_TRUE(string_len(v) > 0);
        count++;
    }
    WC_ASSERT_EQ_INT(count, 3);
    hashmap_destroy(m);
}

static void test_foreach_empty_map(void)
{
    hashmap* m = int_map();
    int count = 0;
    MAP_FOREACH_BUCKET(m, kv) { (void)kv; count++; }
    WC_ASSERT_EQ_INT(count, 0);
    hashmap_destroy(m);
}


/* ── int -> String  (owned val) ─────────────────────────────────────────── */

static void test_str_val_put_copy(void)
{
    hashmap* m = int_str_map();
    int k = 1;
    String sv;
    string_create_stk(&sv, "hello");
    hashmap_put(m, (u8*)&k, (u8*)&sv);

    String* got = (String*)hashmap_get_ptr(m, (u8*)&k);
    WC_ASSERT_NOT_NULL(got);
    WC_ASSERT_TRUE(string_equals_cstr(got, "hello"));

    string_destroy_stk(&sv);
    hashmap_destroy(m);
}

static void test_str_val_independence(void)
{
    /* Mutating source after put must not affect stored copy */
    hashmap* m = int_str_map();
    int k = 1;
    String sv;
    string_create_stk(&sv, "original");
    hashmap_put(m, (u8*)&k, (u8*)&sv);
    string_append_cstr(&sv, "_mutated");

    String* stored = (String*)hashmap_get_ptr(m, (u8*)&k);
    WC_ASSERT_TRUE(string_equals_cstr(stored, "original"));

    string_destroy_stk(&sv);
    hashmap_destroy(m);
}

static void test_str_val_move(void)
{
    hashmap* m   = int_str_map();
    int      k   = 2;
    String*  src = string_from_cstr("moved");
    hashmap_put_val_move(m, (u8*)&k, (u8**)&src);

    WC_ASSERT_NULL(src);
    String* stored = (String*)hashmap_get_ptr(m, (u8*)&k);
    WC_ASSERT_TRUE(string_equals_cstr(stored, "moved"));
    hashmap_destroy(m);
}

static void test_str_val_update_frees_old(void)
{
    /* Updating a String val must not leak the old heap buffer */
    hashmap* m = int_str_map();
    int k = 1;
    MAP_PUT_INT_STR(m, k, "first");
    MAP_PUT_INT_STR(m, k, "second");

    WC_ASSERT_EQ_U64(hashmap_size(m), 1);
    String* stored = (String*)hashmap_get_ptr(m, (u8*)&k);
    WC_ASSERT_TRUE(string_equals_cstr(stored, "second"));
    hashmap_destroy(m);
}

static void test_str_val_move_updates_existing(void)
{
    /* put_val_move on an existing key must free old val and store new one */
    hashmap* m = int_str_map();
    int k = 5;
    MAP_PUT_INT_STR(m, k, "old");

    String* v = string_from_cstr("new");
    hashmap_put_val_move(m, (u8*)&k, (u8**)&v);
    WC_ASSERT_NULL(v);

    String* stored = (String*)hashmap_get_ptr(m, (u8*)&k);
    WC_ASSERT_TRUE(string_equals_cstr(stored, "new"));
    WC_ASSERT_EQ_U64(hashmap_size(m), 1);
    hashmap_destroy(m);
}

static void test_str_val_del_with_out(void)
{
    /* del with non-null out must copy the String before destroying it */
    hashmap* m = int_str_map();
    int k = 3;
    MAP_PUT_INT_STR(m, k, "goodbye");

    String out;
    string_create_stk(&out, "");
    hashmap_del(m, (u8*)&k, (u8*)&out);
    WC_ASSERT_TRUE(string_equals_cstr(&out, "goodbye"));

    string_destroy_stk(&out);
    hashmap_destroy(m);
}


/* ── String -> String  (owned key+val) ──────────────────────────────────── */

static void test_str_key_lookup(void)
{
    hashmap* m  = str_str_map();
    String*  k1 = string_from_cstr("name");
    String*  v1 = string_from_cstr("Alice");
    hashmap_put_move(m, (u8**)&k1, (u8**)&v1);

    WC_ASSERT_NULL(k1);
    WC_ASSERT_NULL(v1);

    String key;
    string_create_stk(&key, "name");
    String* found = (String*)hashmap_get_ptr(m, (u8*)&key);
    WC_ASSERT_NOT_NULL(found);
    WC_ASSERT_TRUE(string_equals_cstr(found, "Alice"));

    string_destroy_stk(&key);
    hashmap_destroy(m);
}

static void test_str_key_miss(void)
{
    hashmap* m = str_str_map();
    String k;
    string_create_stk(&k, "missing");
    WC_ASSERT_FALSE(hashmap_has(m, (u8*)&k));
    string_destroy_stk(&k);
    hashmap_destroy(m);
}

static void test_str_key_update_discards_dup_key(void)
{
    /* put_move on an existing key: incoming key freed, new val stored */
    hashmap* m = str_str_map();
    MAP_PUT_STR_STR(m, "lang", "C");
    MAP_PUT_STR_STR(m, "lang", "C11");

    WC_ASSERT_EQ_U64(hashmap_size(m), 1);
    String k;
    string_create_stk(&k, "lang");
    String* v = (String*)hashmap_get_ptr(m, (u8*)&k);
    WC_ASSERT_TRUE(string_equals_cstr(v, "C11"));
    string_destroy_stk(&k);
    hashmap_destroy(m);
}

static void test_str_key_del(void)
{
    hashmap* m = str_str_map();
    MAP_PUT_STR_STR(m, "fruit", "apple");

    String k;
    string_create_stk(&k, "fruit");
    WC_ASSERT_TRUE(hashmap_del(m, (u8*)&k, NULL));
    WC_ASSERT_FALSE(hashmap_has(m, (u8*)&k));
    WC_ASSERT_EQ_U64(hashmap_size(m), 0);
    string_destroy_stk(&k);
    hashmap_destroy(m);
}

static void test_str_key_put_key_move(void)
{
    /* put_key_move: key is moved in (nulled), val is copied */
    hashmap* m = str_str_map();
    String*  k = string_from_cstr("animal");
    String   v;
    string_create_stk(&v, "cat");

    hashmap_put_key_move(m, (u8**)&k, (u8*)&v);
    WC_ASSERT_NULL(k);

    String lookup;
    string_create_stk(&lookup, "animal");
    String* stored = (String*)hashmap_get_ptr(m, (u8*)&lookup);
    WC_ASSERT_NOT_NULL(stored);
    WC_ASSERT_TRUE(string_equals_cstr(stored, "cat"));

    string_destroy_stk(&v);
    string_destroy_stk(&lookup);
    hashmap_destroy(m);
}

static void test_str_str_resize_preserves_data(void)
{
    hashmap* m = str_str_map();
    char key_buf[16], val_buf[16];
    for (int i = 0; i < 40; i++) {
        snprintf(key_buf, sizeof(key_buf), "key%d", i);
        snprintf(val_buf, sizeof(val_buf), "val%d", i);
        MAP_PUT_STR_STR(m, key_buf, val_buf);
    }
    WC_ASSERT_EQ_U64(hashmap_size(m), 40);

    for (int i = 0; i < 40; i++) {
        snprintf(key_buf, sizeof(key_buf), "key%d", i);
        snprintf(val_buf, sizeof(val_buf), "val%d", i);
        String k;
        string_create_stk(&k, key_buf);
        String* v = (String*)hashmap_get_ptr(m, (u8*)&k);
        WC_ASSERT_NOT_NULL(v);
        WC_ASSERT_TRUE(string_equals_cstr(v, val_buf));
        string_destroy_stk(&k);
    }
    hashmap_destroy(m);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void hashmap_suite(void)
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
    WC_RUN(test_shrink_on_delete);

    WC_SUITE("HashMap — tombstone correctness");
    WC_RUN(test_tombstone_reinsert);
    WC_RUN(test_tombstone_probe_chain);
    WC_RUN(test_delete_reinsert_cycle);

    WC_SUITE("HashMap — clear");
    WC_RUN(test_clear_empties_map);
    WC_RUN(test_clear_then_reuse);
    WC_RUN(test_clear_frees_string_vals);

    WC_SUITE("HashMap — copy");
    WC_RUN(test_copy_int_map);
    WC_RUN(test_copy_independence);
    WC_RUN(test_copy_str_str_map);

    WC_SUITE("HashMap — iteration macros");
    WC_RUN(test_foreach_bucket_visits_all);
    WC_RUN(test_foreach_bucket_skips_tombstones);
    WC_RUN(test_foreach_val_typed);
    WC_RUN(test_foreach_empty_map);

    WC_SUITE("HashMap — int->String (owned val)");
    WC_RUN(test_str_val_put_copy);
    WC_RUN(test_str_val_independence);
    WC_RUN(test_str_val_move);
    WC_RUN(test_str_val_update_frees_old);
    WC_RUN(test_str_val_move_updates_existing);
    WC_RUN(test_str_val_del_with_out);

    WC_SUITE("HashMap — String->String (owned key+val)");
    WC_RUN(test_str_key_lookup);
    WC_RUN(test_str_key_miss);
    WC_RUN(test_str_key_update_discards_dup_key);
    WC_RUN(test_str_key_del);
    WC_RUN(test_str_key_put_key_move);
    WC_RUN(test_str_str_resize_preserves_data);
}

