#include "wc_macros.h"
#include "wc_test.h"
#include "hashset.h"
#include "wc_helpers.h"


/* ── Set constructors ────────────────────────────────────────────────────── */

static hashset* int_set(void) {
    return hashset_create(sizeof(int), NULL, NULL, NULL);
}

static hashset* str_set(void) {
    return hashset_create(sizeof(String), murmurhash3_str, str_cmp, &wc_str_ops);
}


/* ════════════════════════════════════════════════════════════════════════════
 * int set  (POD, no copy/move/del)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_insert_and_has(void)
{
    hashset* s = int_set();
    int x = 42;
    WC_ASSERT_FALSE(hashset_has(s, (u8*)&x));
    hashset_insert(s, (u8*)&x);
    WC_ASSERT_TRUE(hashset_has(s, (u8*)&x));
    hashset_destroy(s);
}

static void test_insert_returns_existed(void)
{
    hashset* s = int_set();
    int x = 5;
    b8 first  = hashset_insert(s, (u8*)&x);
    b8 second = hashset_insert(s, (u8*)&x);
    WC_ASSERT_FALSE(first);   /* new insert */
    WC_ASSERT_TRUE(second);   /* already existed */
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

static void test_has_missing_returns_false(void)
{
    hashset* s = int_set();
    int x = 999;
    WC_ASSERT_FALSE(hashset_has(s, (u8*)&x));
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

static void test_remove_on_empty_set(void)
{
    hashset* s = int_set();
    int x = 1;
    WC_ASSERT_FALSE(hashset_remove(s, (u8*)&x));
    WC_ASSERT_EQ_U64(hashset_size(s), 0);
    hashset_destroy(s);
}

static void test_size_and_empty(void)
{
    hashset* s = int_set();
    WC_ASSERT_EQ_U64(hashset_size(s), 0);
    WC_ASSERT_TRUE(hashset_empty(s));

    for (int i = 0; i < 10; i++) {
        hashset_insert(s, (u8*)&i);
    }
    WC_ASSERT_EQ_U64(hashset_size(s), 10);
    WC_ASSERT_FALSE(hashset_empty(s));
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
    WC_ASSERT_EQ_U64(hashset_size(s), 50);
    for (int i = 0; i < 50; i++) {
        WC_ASSERT_TRUE(hashset_has(s, (u8*)&i));
    }
    hashset_destroy(s);
}

static void test_shrink_on_remove(void)
{
    /* Insert enough to grow, remove most — capacity should shrink back */
    hashset* s    = int_set();
    u64      cap0 = hashset_capacity(s);

    for (int i = 0; i < 50; i++) {
        hashset_insert(s, (u8*)&i);
    }
    WC_ASSERT_TRUE(hashset_capacity(s) > cap0);

    for (int i = 0; i < 48; i++) {
        hashset_remove(s, (u8*)&i);
    }
    WC_ASSERT_TRUE(hashset_capacity(s) < 50);

    /* Remaining elements must still be reachable */
    for (int i = 48; i < 50; i++) {
        WC_ASSERT_TRUE(hashset_has(s, (u8*)&i));
    }
    hashset_destroy(s);
}


/* ── Tombstone correctness ───────────────────────────────────────────────── */

static void test_tombstone_reinsert(void)
{
    /* Remove an element then re-insert it — must succeed and be findable */
    hashset* s = int_set();
    int x = 42;
    hashset_insert(s, (u8*)&x);
    hashset_remove(s, (u8*)&x);
    WC_ASSERT_FALSE(hashset_has(s, (u8*)&x));

    hashset_insert(s, (u8*)&x);
    WC_ASSERT_TRUE(hashset_has(s, (u8*)&x));
    WC_ASSERT_EQ_U64(hashset_size(s), 1);
    hashset_destroy(s);
}

static void test_tombstone_probe_chain(void)
{
    /*
     * Insert elements that chain during probing, remove one mid-chain,
     * then verify all remaining elements are still reachable through the tombstone.
     */
    hashset* s = int_set();
    for (int i = 0; i < 20; i++) {
        hashset_insert(s, (u8*)&i);
    }

    int mid = 5;
    hashset_remove(s, (u8*)&mid);

    for (int i = 0; i < 20; i++) {
        if (i == mid) { continue; }
        WC_ASSERT_TRUE(hashset_has(s, (u8*)&i));
    }
    hashset_destroy(s);
}

static void test_remove_reinsert_cycle(void)
{
    /* Repeated remove+insert must not corrupt or leak */
    hashset* s = int_set();
    int x = 7;
    for (int cycle = 0; cycle < 20; cycle++) {
        hashset_insert(s, (u8*)&x);
        WC_ASSERT_TRUE(hashset_has(s, (u8*)&x));
        hashset_remove(s, (u8*)&x);
        WC_ASSERT_EQ_U64(hashset_size(s), 0);
    }
    hashset_destroy(s);
}


/* ── hashset_clear ───────────────────────────────────────────────────────── */

static void test_clear_empties_set(void)
{
    hashset* s = int_set();
    for (int i = 0; i < 10; i++) {
        hashset_insert(s, (u8*)&i);
    }
    u64 cap_before = hashset_capacity(s);
    hashset_clear(s);

    WC_ASSERT_EQ_U64(hashset_size(s), 0);
    WC_ASSERT_TRUE(hashset_empty(s));
    WC_ASSERT_EQ_U64(hashset_capacity(s), cap_before); /* capacity unchanged */
    for (int i = 0; i < 10; i++) {
        WC_ASSERT_FALSE(hashset_has(s, (u8*)&i));
    }
    hashset_destroy(s);
}

static void test_clear_then_reuse(void)
{
    hashset* s = int_set();
    for (int i = 0; i < 10; i++) {
        hashset_insert(s, (u8*)&i);
    }
    hashset_clear(s);

    /* Re-insert after clear — set must be fully functional */
    for (int i = 100; i < 110; i++) {
        hashset_insert(s, (u8*)&i);
    }
    WC_ASSERT_EQ_U64(hashset_size(s), 10);
    for (int i = 100; i < 110; i++) {
        WC_ASSERT_TRUE(hashset_has(s, (u8*)&i));
    }
    hashset_destroy(s);
}

static void test_clear_frees_string_elms(void)
{
    /* clear must properly call del on owned String resources */
    hashset* s = str_set();
    for (int i = 0; i < 5; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "str%d", i);
        String* v = string_from_cstr(buf);
        hashset_insert_move(s, (u8**)&v);
    }
    hashset_clear(s);
    WC_ASSERT_EQ_U64(hashset_size(s), 0);

    /* Set must still be usable after clearing owned-resource entries */
    String* v = string_from_cstr("after_clear");
    hashset_insert_move(s, (u8**)&v);
    WC_ASSERT_EQ_U64(hashset_size(s), 1);
    hashset_destroy(s);
}


/* ── hashset_reset ───────────────────────────────────────────────────────── */

static void test_reset(void)
{
    hashset* s = int_set();
    for (int i = 0; i < 30; i++) {
        hashset_insert(s, (u8*)&i);
    }
    hashset_reset(s);
    WC_ASSERT_EQ_U64(hashset_size(s), 0);
    WC_ASSERT_EQ_U64(hashset_capacity(s), HASHMAP_INIT_CAPACITY);
    hashset_destroy(s);
}

static void test_reset_then_reuse(void)
{
    hashset* s = int_set();
    for (int i = 0; i < 30; i++) {
        hashset_insert(s, (u8*)&i);
    }
    hashset_reset(s);

    for (int i = 200; i < 210; i++) {
        hashset_insert(s, (u8*)&i);
    }
    WC_ASSERT_EQ_U64(hashset_size(s), 10);
    for (int i = 200; i < 210; i++) {
        WC_ASSERT_TRUE(hashset_has(s, (u8*)&i));
    }
    hashset_destroy(s);
}


/* ── hashset_copy ────────────────────────────────────────────────────────── */

static void test_copy_int_set(void)
{
    hashset* src = int_set();
    for (int i = 0; i < 10; i++) {
        hashset_insert(src, (u8*)&i);
    }

    hashset dest;
    hashset_copy(&dest, src);
    WC_ASSERT_EQ_U64(hashset_size(&dest), hashset_size(src));

    for (int i = 0; i < 10; i++) {
        WC_ASSERT_TRUE(hashset_has(&dest, (u8*)&i));
    }

    hashset_destroy(src);
    hashset_clear(&dest);
    free(dest.buckets);
}

static void test_copy_independence(void)
{
    /* Inserting into dest must not affect src, and vice versa */
    hashset* src = int_set();
    int x = 1;
    hashset_insert(src, (u8*)&x);

    hashset dest;
    hashset_copy(&dest, src);

    int y = 99;
    hashset_insert(&dest, (u8*)&y);

    WC_ASSERT_FALSE(hashset_has(src,   (u8*)&y)); /* src unaffected */
    WC_ASSERT_TRUE(hashset_has(&dest,  (u8*)&x)); /* dest has original */
    WC_ASSERT_TRUE(hashset_has(&dest,  (u8*)&y)); /* dest has new */

    hashset_destroy(src);
    hashset_clear(&dest);
    free(dest.buckets);
}

static void test_copy_str_set(void)
{
    /* Deep copy: destroying src must not corrupt dest's String data */
    hashset* src = str_set();
    const char* words[] = { "alpha", "beta", "gamma" };
    for (int i = 0; i < 3; i++) {
        String sv;
        string_create_stk(&sv, words[i]);
        hashset_insert(src, (u8*)&sv);
        string_destroy_stk(&sv);
    }

    hashset dest;
    hashset_copy(&dest, src);
    WC_ASSERT_EQ_U64(hashset_size(&dest), 3);

    hashset_destroy(src); /* src gone — dest must still be intact */

    String probe;
    string_create_stk(&probe, "beta");
    WC_ASSERT_TRUE(hashset_has(&dest, (u8*)&probe));
    string_destroy_stk(&probe);

    hashset_clear(&dest);
    free(dest.buckets);
}

static void test_copy_empty_set(void)
{
    hashset* src = int_set();

    hashset dest;
    hashset_copy(&dest, src);
    WC_ASSERT_EQ_U64(hashset_size(&dest), 0);
    WC_ASSERT_EQ_U64(hashset_capacity(&dest), hashset_capacity(src));

    hashset_destroy(src);
    hashset_clear(&dest);
    free(dest.buckets);
}


/* ── SET_FOREACH_BUCKET iteration ───────────────────────────────────────── */

static void test_foreach_visits_all(void)
{
    hashset* s = int_set();
    for (int i = 0; i < 8; i++) {
        hashset_insert(s, (u8*)&i);
    }

    int count = 0, sum = 0;
    SET_FOREACH_BUCKET(s, elm) {
        count++;
        sum += *(int*)elm->elm;
    }
    WC_ASSERT_EQ_INT(count, 8);
    WC_ASSERT_EQ_INT(sum,   0+1+2+3+4+5+6+7);
    hashset_destroy(s);
}

static void test_foreach_skips_tombstones(void)
{
    /* Removed slots (tombstones) must not appear during iteration */
    hashset* s = int_set();
    for (int i = 0; i < 8; i++) {
        hashset_insert(s, (u8*)&i);
    }
    for (int i = 0; i < 4; i++) {
        hashset_remove(s, (u8*)&i);
    }

    int count = 0;
    SET_FOREACH_BUCKET(s, elm) {
        WC_ASSERT_TRUE(*(int*)elm->elm >= 4);
        count++;
    }
    WC_ASSERT_EQ_INT(count, 4);
    hashset_destroy(s);
}

static void test_foreach_empty_set(void)
{
    hashset* s = int_set();
    int count = 0;
    SET_FOREACH_BUCKET(s, elm) { (void)elm; count++; }
    WC_ASSERT_EQ_INT(count, 0);
    hashset_destroy(s);
}


/* ── String set (owns heap memory) ──────────────────────────────────────── */

static void test_str_insert_move_nulls_ptr(void)
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

static void test_str_insert_copy_leaves_src_valid(void)
{
    /* Source String must still be valid and unchanged after copy insert */
    hashset* s  = str_set();
    String*  s1 = string_from_cstr("world");
    hashset_insert(s, (u8*)s1);

    WC_ASSERT_NOT_NULL(s1);
    WC_ASSERT_TRUE(string_equals_cstr(s1, "world"));
    string_destroy(s1);
    hashset_destroy(s);
}

static void test_str_insert_copy_independence(void)
{
    /* Mutating source String after copy insert must not affect stored copy */
    hashset* s = str_set();
    String   sv;
    string_create_stk(&sv, "original");
    hashset_insert(s, (u8*)&sv);
    string_append_cstr(&sv, "_mutated");

    /* The stored copy should still read "original" */
    String probe;
    string_create_stk(&probe, "original");
    WC_ASSERT_TRUE(hashset_has(s, (u8*)&probe));
    string_destroy_stk(&probe);

    string_destroy_stk(&sv);
    hashset_destroy(s);
}

static void test_str_has_miss(void)
{
    hashset* s = str_set();
    String probe;
    string_create_stk(&probe, "missing");
    WC_ASSERT_FALSE(hashset_has(s, (u8*)&probe));
    string_destroy_stk(&probe);
    hashset_destroy(s);
}

static void test_str_no_duplicates(void)
{
    hashset* s = str_set();
    String   sv;
    string_create_stk(&sv, "dup");
    b8 first  = hashset_insert(s, (u8*)&sv);
    b8 second = hashset_insert(s, (u8*)&sv);
    WC_ASSERT_FALSE(first);
    WC_ASSERT_TRUE(second);
    WC_ASSERT_EQ_U64(hashset_size(s), 1);
    string_destroy_stk(&sv);
    hashset_destroy(s);
}

static void test_str_insert_move_duplicate_frees_elm(void)
{
    /* insert_move on a duplicate must free the incoming pointer */
    hashset* s = str_set();
    String   sv;
    string_create_stk(&sv, "dup");
    hashset_insert(s, (u8*)&sv);

    String* dup = string_from_cstr("dup");
    b8 existed = hashset_insert_move(s, (u8**)&dup);
    WC_ASSERT_TRUE(existed);
    WC_ASSERT_NULL(dup);         /* must be freed and nulled */
    WC_ASSERT_EQ_U64(hashset_size(s), 1);

    string_destroy_stk(&sv);
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
    WC_ASSERT_EQ_U64(hashset_size(s), 0);
    string_destroy_stk(&probe);
    hashset_destroy(s);
}

static void test_str_resize_preserves_membership(void)
{
    hashset* s = str_set();
    char buf[16];
    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "word%d", i);
        String sv;
        string_create_stk(&sv, buf);
        hashset_insert(s, (u8*)&sv);
        string_destroy_stk(&sv);
    }
    WC_ASSERT_EQ_U64(hashset_size(s), 40);

    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "word%d", i);
        String probe;
        string_create_stk(&probe, buf);
        WC_ASSERT_TRUE(hashset_has(s, (u8*)&probe));
        string_destroy_stk(&probe);
    }
    hashset_destroy(s);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void hashset_suite(void)
{
    WC_SUITE("HashSet — int (POD)");
    WC_RUN(test_insert_and_has);
    WC_RUN(test_insert_returns_existed);
    WC_RUN(test_insert_duplicate_no_growth);
    WC_RUN(test_has_missing_returns_false);
    WC_RUN(test_remove);
    WC_RUN(test_remove_missing_returns_false);
    WC_RUN(test_remove_on_empty_set);
    WC_RUN(test_size_and_empty);
    WC_RUN(test_size_tracks_inserts);
    WC_RUN(test_resize_preserves_membership);
    WC_RUN(test_shrink_on_remove);

    WC_SUITE("HashSet — tombstone correctness");
    WC_RUN(test_tombstone_reinsert);
    WC_RUN(test_tombstone_probe_chain);
    WC_RUN(test_remove_reinsert_cycle);

    WC_SUITE("HashSet — clear");
    WC_RUN(test_clear_empties_set);
    WC_RUN(test_clear_then_reuse);
    WC_RUN(test_clear_frees_string_elms);

    WC_SUITE("HashSet — reset");
    WC_RUN(test_reset);
    WC_RUN(test_reset_then_reuse);

    WC_SUITE("HashSet — copy");
    WC_RUN(test_copy_int_set);
    WC_RUN(test_copy_independence);
    WC_RUN(test_copy_str_set);
    WC_RUN(test_copy_empty_set);

    WC_SUITE("HashSet — iteration");
    WC_RUN(test_foreach_visits_all);
    WC_RUN(test_foreach_skips_tombstones);
    WC_RUN(test_foreach_empty_set);

    WC_SUITE("HashSet — String (owned elements)");
    WC_RUN(test_str_insert_move_nulls_ptr);
    WC_RUN(test_str_insert_copy_leaves_src_valid);
    WC_RUN(test_str_insert_copy_independence);
    WC_RUN(test_str_has_miss);
    WC_RUN(test_str_no_duplicates);
    WC_RUN(test_str_insert_move_duplicate_frees_elm);
    WC_RUN(test_str_remove);
    WC_RUN(test_str_resize_preserves_membership);
}


