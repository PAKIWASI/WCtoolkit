#include "wc_test.h"
#include "hashmap.h"
#include "String.h"

/* helpers from wc_helpers.h — redeclared static here so no header dependency */
#include "wc_helpers.h"


/* ── int -> int map (trivial, no copy/move/del) ──────────────────────────── */

static hashmap* int_map(void) {
    return hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
}

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
    WC_ASSERT_EQ_U64(hashmap_size(m), 1); /* size must not grow on update */
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

static void test_get_ptr(void)
{
    hashmap* m = int_map();
    int k = 2, v = 55;
    hashmap_put(m, (u8*)&k, (u8*)&v);
    int* ptr = (int*)hashmap_get_ptr(m, (u8*)&k);
    WC_ASSERT_NOT_NULL(ptr);
    WC_ASSERT_EQ_INT(*ptr, 55);
    /* mutate through ptr */
    *ptr = 66;
    int out = 0;
    hashmap_get(m, (u8*)&k, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 66);
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
    for (int i = 0; i < 10; i++) {
        int v = i * 10;
        hashmap_put(m, (u8*)&i, (u8*)&v);
    }
    WC_ASSERT_EQ_U64(hashmap_size(m), 10);
    hashmap_destroy(m);
}

static void test_resize_preserves_data(void)
{
    /* Insert enough to force at least one resize */
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


/* ── int -> String (val owns heap memory) ────────────────────────────────── */

static hashmap* int_str_map(void) {
    return hashmap_create(sizeof(int), sizeof(String), NULL, NULL, NULL, &wc_str_ops);
}

static void test_str_val_put_copy(void)
{
    hashmap* m  = int_str_map();
    int      k  = 1;
    String   sv;
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
    /* Modifying the source after put must not affect the stored copy */
    hashmap* m = int_str_map();
    int      k = 1;
    String   sv;
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

    WC_ASSERT_NULL(src); /* ownership transferred */
    String* stored = (String*)hashmap_get_ptr(m, (u8*)&k);
    WC_ASSERT_TRUE(string_equals_cstr(stored, "moved"));
    hashmap_destroy(m);
}


/* ── String -> String (both keys and vals own heap) ─────────────────────── */

static hashmap* str_str_map(void) {
    return hashmap_create(sizeof(String), sizeof(String),
                          murmurhash3_str, str_cmp, &wc_str_ops, &wc_str_ops);
}

static void test_str_key_lookup(void)
{
    hashmap* m  = str_str_map();
    String*  k1 = string_from_cstr("name");
    String*  v1 = string_from_cstr("Alice");
    hashmap_put_move(m, (u8**)&k1, (u8**)&v1);

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
    String   k;
    string_create_stk(&k, "missing");
    WC_ASSERT_FALSE(hashmap_has(m, (u8*)&k));
    string_destroy_stk(&k);
    hashmap_destroy(m);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void hashmap_suite(void)
{
    WC_SUITE("HashMap");

    /* int -> int */
    WC_RUN(test_put_and_get);
    WC_RUN(test_put_update);
    WC_RUN(test_has);
    WC_RUN(test_del);
    WC_RUN(test_del_copies_out);
    WC_RUN(test_get_ptr);
    WC_RUN(test_get_missing_returns_false);
    WC_RUN(test_size_tracks_inserts);
    WC_RUN(test_resize_preserves_data);

    /* int -> String */
    WC_RUN(test_str_val_put_copy);
    WC_RUN(test_str_val_independence);
    WC_RUN(test_str_val_move);

    /* String -> String */
    WC_RUN(test_str_key_lookup);
    WC_RUN(test_str_key_miss);
}
