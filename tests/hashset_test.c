#include "wc_test.h"
#include "hashset.h"
#include "String.h"
#include "wc_helpers.h"


/* ── int set (trivial) ───────────────────────────────────────────────────── */

static hashset* int_set(void) {
    return hashset_create(sizeof(int), NULL, NULL, NULL);
}

static void test_insert_and_has(void)
{
    hashset* s = int_set();
    int x = 42;
    WC_ASSERT_FALSE(hashset_has(s, (u8*)&x));
    hashset_insert(s, (u8*)&x);
    WC_ASSERT_TRUE(hashset_has(s, (u8*)&x));
    hashset_destroy(s);
}

static void test_insert_duplicate_no_growth(void)
{
    hashset* s = int_set();
    int x = 10;
    hashset_insert(s, (u8*)&x);
    hashset_insert(s, (u8*)&x);
    hashset_insert(s, (u8*)&x);
    WC_ASSERT_EQ_U64(hashset_size(s), 1);
    hashset_destroy(s);
}

static void test_insert_returns_existed(void)
{
    hashset* s = int_set();
    int x = 5;
    b8 first  = hashset_insert(s, (u8*)&x);
    b8 second = hashset_insert(s, (u8*)&x);
    WC_ASSERT_FALSE(first);  /* new insert */
    WC_ASSERT_TRUE(second);  /* already existed */
    hashset_destroy(s);
}

static void test_remove(void)
{
    hashset* s = int_set();
    int x = 7;
    hashset_insert(s, (u8*)&x);
    WC_ASSERT_TRUE(hashset_remove(s, (u8*)&x));
    WC_ASSERT_FALSE(hashset_has(s, (u8*)&x));
    WC_ASSERT_EQ_U64(hashset_size(s), 0);
    hashset_destroy(s);
}

static void test_remove_missing_returns_false(void)
{
    hashset* s = int_set();
    int x = 999;
    WC_ASSERT_FALSE(hashset_remove(s, (u8*)&x));
    hashset_destroy(s);
}

static void test_size_tracks_inserts(void)
{
    hashset* s = int_set();
    for (int i = 0; i < 20; i++) {
        hashset_insert(s, (u8*)&i);
    }
    WC_ASSERT_EQ_U64(hashset_size(s), 20);
    hashset_destroy(s);
}

static void test_resize_preserves_membership(void)
{
    hashset* s = int_set();
    for (int i = 0; i < 50; i++) {
        hashset_insert(s, (u8*)&i);
    }
    for (int i = 0; i < 50; i++) {
        WC_ASSERT_TRUE(hashset_has(s, (u8*)&i));
    }
    hashset_destroy(s);
}

static void test_clear(void)
{
    hashset* s = int_set();
    for (int i = 0; i < 5; i++) {
        hashset_insert(s, (u8*)&i);
    }
    u64 cap_before = hashset_capacity(s);
    hashset_clear(s);
    WC_ASSERT_EQ_U64(hashset_size(s), 0);
    WC_ASSERT_EQ_U64(hashset_capacity(s), cap_before);
    hashset_destroy(s);
}

static void test_reset(void)
{
    hashset* s = int_set();
    for (int i = 0; i < 30; i++) {
        hashset_insert(s, (u8*)&i);
    }
    hashset_reset(s);
    WC_ASSERT_EQ_U64(hashset_size(s), 0);
    /* capacity should be back to initial */
    WC_ASSERT_EQ_U64(hashset_capacity(s), HASHMAP_INIT_CAPACITY);
    hashset_destroy(s);
}


/* ── String set (owns heap memory) ──────────────────────────────────────── */

static hashset* str_set(void) {
    return hashset_create(sizeof(String),
                          murmurhash3_str, str_cmp, &wc_str_ops);
}

static void test_str_insert_has(void)
{
    hashset* s  = str_set();
    String*  s1 = string_from_cstr("hello");
    hashset_insert_move(s, (u8**)&s1);
    WC_ASSERT_NULL(s1); /* ownership transferred */

    String probe;
    string_create_stk(&probe, "hello");
    WC_ASSERT_TRUE(hashset_has(s, (u8*)&probe));
    string_destroy_stk(&probe);
    hashset_destroy(s);
}

static void test_str_insert_copy(void)
{
    hashset* s  = str_set();
    String*  s1 = string_from_cstr("world");
    hashset_insert(s, (u8*)s1);
    /* s1 still valid after copy insert */
    WC_ASSERT_NOT_NULL(s1);
    WC_ASSERT(string_equals_cstr(s1, "world"));
    string_destroy(s1);
    hashset_destroy(s);
}

static void test_str_remove(void)
{
    hashset* s  = str_set();
    String*  s1 = string_from_cstr("remove_me");
    hashset_insert_move(s, (u8**)&s1);

    String probe;
    string_create_stk(&probe, "remove_me");
    WC_ASSERT_TRUE(hashset_remove(s, (u8*)&probe));
    WC_ASSERT_FALSE(hashset_has(s, (u8*)&probe));
    string_destroy_stk(&probe);
    hashset_destroy(s);
}

static void test_str_no_duplicates(void)
{
    hashset* s = str_set();
    String   sv;
    string_create_stk(&sv, "dup");
    hashset_insert(s, (u8*)&sv);
    hashset_insert(s, (u8*)&sv);
    WC_ASSERT_EQ_U64(hashset_size(s), 1);
    string_destroy_stk(&sv);
    hashset_destroy(s);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void hashset_suite(void)
{
    WC_SUITE("HashSet");

    /* int set */
    WC_RUN(test_insert_and_has);
    WC_RUN(test_insert_duplicate_no_growth);
    WC_RUN(test_insert_returns_existed);
    WC_RUN(test_remove);
    WC_RUN(test_remove_missing_returns_false);
    WC_RUN(test_size_tracks_inserts);
    WC_RUN(test_resize_preserves_membership);
    WC_RUN(test_clear);
    WC_RUN(test_reset);

    /* string set */
    WC_RUN(test_str_insert_has);
    WC_RUN(test_str_insert_copy);
    WC_RUN(test_str_remove);
    WC_RUN(test_str_no_duplicates);
}
