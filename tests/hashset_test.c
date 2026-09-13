#include "wc_macros.h"
#include "wc_test.h"
#include "HashSet.h"
#include "wc_helpers.h"


/* ── Set constructors ────────────────────────────────────────────────────── */

static HashSet* int_set(void)
{
    return HashSet_create(sizeof(int), NULL, NULL, NULL);
}

static HashSet* str_set(void)
{
    return HashSet_create(sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
}


/* ════════════════════════════════════════════════════════════════════════════
 * int set  (POD, no copy/move/del)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_insert_and_has(void)
{
    HashSet* s = int_set();
    int x = 42;
    WC_ASSERT_FALSE(HashSet_has(s, (u8*)&x));
    HashSet_insert(s, (u8*)&x);
    WC_ASSERT_TRUE(HashSet_has(s, (u8*)&x));
    HashSet_destroy(s);
}

static void test_insert_returns_existed(void)
{
    HashSet* s = int_set();
    int x = 5;
    b8 first  = HashSet_insert(s, (u8*)&x);
    b8 second = HashSet_insert(s, (u8*)&x);
    WC_ASSERT_FALSE(first);  // new insert
    WC_ASSERT_TRUE(second);  // already existed
    HashSet_destroy(s);
}

static void test_insert_duplicate_no_growth(void)
{
    HashSet* s = int_set();
    int x = 10;
    HashSet_insert(s, (u8*)&x);
    HashSet_insert(s, (u8*)&x);
    HashSet_insert(s, (u8*)&x);
    WC_ASSERT_EQ_U64(HashSet_size(s), 1);
    HashSet_destroy(s);
}

static void test_has_missing_returns_false(void)
{
    HashSet* s = int_set();
    int x = 999;
    WC_ASSERT_FALSE(HashSet_has(s, (u8*)&x));
    HashSet_destroy(s);
}

static void test_remove(void)
{
    HashSet* s = int_set();
    int x = 7;
    HashSet_insert(s, (u8*)&x);
    WC_ASSERT_TRUE(HashSet_remove(s, (u8*)&x));
    WC_ASSERT_FALSE(HashSet_has(s, (u8*)&x));
    WC_ASSERT_EQ_U64(HashSet_size(s), 0);
    HashSet_destroy(s);
}

static void test_remove_missing_returns_false(void)
{
    HashSet* s = int_set();
    int x = 999;
    WC_ASSERT_FALSE(HashSet_remove(s, (u8*)&x));
    HashSet_destroy(s);
}

static void test_remove_on_empty_set(void)
{
    HashSet* s = int_set();
    int x = 1;
    WC_ASSERT_FALSE(HashSet_remove(s, (u8*)&x));
    WC_ASSERT_EQ_U64(HashSet_size(s), 0);
    HashSet_destroy(s);
}

static void test_size_and_empty(void)
{
    HashSet* s = int_set();
    WC_ASSERT_EQ_U64(HashSet_size(s), 0);
    WC_ASSERT_TRUE(HashSet_empty(s));

    for (int i = 0; i < 10; i++) {
        HashSet_insert(s, (u8*)&i);
    }
    WC_ASSERT_EQ_U64(HashSet_size(s), 10);
    WC_ASSERT_FALSE(HashSet_empty(s));
    HashSet_destroy(s);
}

static void test_size_tracks_inserts(void)
{
    HashSet* s = int_set();
    for (int i = 0; i < 20; i++) {
        HashSet_insert(s, (u8*)&i);
    }
    WC_ASSERT_EQ_U64(HashSet_size(s), 20);
    HashSet_destroy(s);
}

static void test_resize_preserves_membership(void)
{
    HashSet* s = int_set();
    for (int i = 0; i < 50; i++) {
        HashSet_insert(s, (u8*)&i);
    }
    WC_ASSERT_EQ_U64(HashSet_size(s), 50);
    for (int i = 0; i < 50; i++) {
        WC_ASSERT_TRUE(HashSet_has(s, (u8*)&i));
    }
    HashSet_destroy(s);
}

// Robin Hood + backward-shift delete doesn't shrink — no LOAD_FACTOR_SHRINK.
// Verify correctness after heavy removal instead.
static void test_remove_correctness_after_many_removes(void)
{
    HashSet* s = int_set();
    for (int i = 0; i < 50; i++) {
        HashSet_insert(s, (u8*)&i);
    }
    for (int i = 0; i < 48; i++) {
        HashSet_remove(s, (u8*)&i);
    }
    WC_ASSERT_EQ_U64(HashSet_size(s), 2);

    for (int i = 48; i < 50; i++) {
        WC_ASSERT_TRUE(HashSet_has(s, (u8*)&i));
    }
    HashSet_destroy(s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * delete correctness  (backward-shift, no tombstones)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_remove_reinsert(void)
{
    // Remove an element then re-insert it — must succeed and be findable
    HashSet* s = int_set();
    int x = 42;
    HashSet_insert(s, (u8*)&x);
    HashSet_remove(s, (u8*)&x);
    WC_ASSERT_FALSE(HashSet_has(s, (u8*)&x));

    HashSet_insert(s, (u8*)&x);
    WC_ASSERT_TRUE(HashSet_has(s, (u8*)&x));
    WC_ASSERT_EQ_U64(HashSet_size(s), 1);
    HashSet_destroy(s);
}

static void test_remove_mid_chain(void)
{
    // Insert elements that chain during probing, remove one mid-chain,
    // then verify all remaining elements are still reachable.
    HashSet* s = int_set();
    for (int i = 0; i < 20; i++) {
        HashSet_insert(s, (u8*)&i);
    }

    int mid = 5;
    HashSet_remove(s, (u8*)&mid);

    for (int i = 0; i < 20; i++) {
        if (i == mid) {
            continue;
        }
        WC_ASSERT_TRUE(HashSet_has(s, (u8*)&i));
    }
    HashSet_destroy(s);
}

static void test_remove_reinsert_cycle(void)
{
    // Repeated remove+insert must not corrupt or leak
    HashSet* s = int_set();
    int x = 7;
    for (int cycle = 0; cycle < 20; cycle++) {
        HashSet_insert(s, (u8*)&x);
        WC_ASSERT_TRUE(HashSet_has(s, (u8*)&x));
        HashSet_remove(s, (u8*)&x);
        WC_ASSERT_EQ_U64(HashSet_size(s), 0);
    }
    HashSet_destroy(s);
}

static void test_remove_first_in_chain(void)
{
    // Removing the head of a probe chain must leave the rest reachable
    HashSet* s = int_set();
    for (int i = 0; i < 15; i++) {
        HashSet_insert(s, (u8*)&i);
    }
    int head = 0;
    HashSet_remove(s, (u8*)&head);
    WC_ASSERT_FALSE(HashSet_has(s, (u8*)&head));

    for (int i = 1; i < 15; i++) {
        WC_ASSERT_TRUE(HashSet_has(s, (u8*)&i));
    }
    HashSet_destroy(s);
}

static void test_remove_all_then_reinsert(void)
{
    // Remove every element then reinsert — set must be fully functional
    HashSet* s = int_set();
    for (int i = 0; i < 20; i++) {
        HashSet_insert(s, (u8*)&i);
    }
    for (int i = 0; i < 20; i++) {
        HashSet_remove(s, (u8*)&i);
    }
    WC_ASSERT_EQ_U64(HashSet_size(s), 0);
    WC_ASSERT_TRUE(HashSet_empty(s));

    for (int i = 0; i < 20; i++) {
        HashSet_insert(s, (u8*)&i);
    }
    WC_ASSERT_EQ_U64(HashSet_size(s), 20);
    for (int i = 0; i < 20; i++) {
        WC_ASSERT_TRUE(HashSet_has(s, (u8*)&i));
    }
    HashSet_destroy(s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * HashSet_clear
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_clear_empties_set(void)
{
    HashSet* s = int_set();
    for (int i = 0; i < 10; i++) {
        HashSet_insert(s, (u8*)&i);
    }
    u64 cap_before = HashSet_capacity(s);
    HashSet_clear(s);

    WC_ASSERT_EQ_U64(HashSet_size(s), 0);
    WC_ASSERT_TRUE(HashSet_empty(s));
    WC_ASSERT_EQ_U64(HashSet_capacity(s), cap_before); // capacity unchanged
    for (int i = 0; i < 10; i++) {
        WC_ASSERT_FALSE(HashSet_has(s, (u8*)&i));
    }
    HashSet_destroy(s);
}

static void test_clear_then_reuse(void)
{
    HashSet* s = int_set();
    for (int i = 0; i < 10; i++) {
        HashSet_insert(s, (u8*)&i);
    }
    HashSet_clear(s);

    for (int i = 100; i < 110; i++) {
        HashSet_insert(s, (u8*)&i);
    }
    WC_ASSERT_EQ_U64(HashSet_size(s), 10);
    for (int i = 100; i < 110; i++) {
        WC_ASSERT_TRUE(HashSet_has(s, (u8*)&i));
    }
    HashSet_destroy(s);
}

static void test_clear_frees_string_elms(void)
{
    // clear must properly call del on owned String resources
    HashSet* s = str_set();
    for (int i = 0; i < 5; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "str%d", i);
        String* v = string_from_cstr(buf);
        HashSet_insert_move(s, (u8**)&v);
    }
    HashSet_clear(s);
    WC_ASSERT_EQ_U64(HashSet_size(s), 0);

    // Set must still be usable after clearing owned-resource entries
    String* v = string_from_cstr("after_clear");
    HashSet_insert_move(s, (u8**)&v);
    WC_ASSERT_EQ_U64(HashSet_size(s), 1);
    HashSet_destroy(s);
}

static void test_clear_empty_set(void)
{
    // clear on an already-empty set must be a safe no-op
    HashSet* s = int_set();
    HashSet_clear(s);
    WC_ASSERT_EQ_U64(HashSet_size(s), 0);
    HashSet_destroy(s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * HashSet_copy
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_copy_int_set(void)
{
    HashSet* src = int_set();
    for (int i = 0; i < 10; i++) {
        HashSet_insert(src, (u8*)&i);
    }

    // dest must be uninitialised — HashSet_copy allocates everything
    HashSet* dest = int_set();
    HashSet_copy(dest, src);
    WC_ASSERT_EQ_U64(HashSet_size(dest), HashSet_size(src));

    for (int i = 0; i < 10; i++) {
        WC_ASSERT_TRUE(HashSet_has(dest, (u8*)&i));
    }

    HashSet_destroy(src);
    HashSet_destroy(dest);
}

static void test_copy_independence(void)
{
    // Inserting into dest must not affect src
    HashSet* src = int_set();
    int x = 1;
    HashSet_insert(src, (u8*)&x);

    HashSet* dest = int_set();
    HashSet_copy(dest, src);

    int y = 99;
    HashSet_insert(dest, (u8*)&y);

    WC_ASSERT_FALSE(HashSet_has(src,   (u8*)&y)); // src unaffected
    WC_ASSERT_TRUE(HashSet_has(dest,  (u8*)&x)); // dest has original
    WC_ASSERT_TRUE(HashSet_has(dest,  (u8*)&y)); // dest has new

    HashSet_destroy(src);
    HashSet_destroy(dest);
}

static void test_copy_str_set(void)
{
    // Deep copy: destroying src must not corrupt dest's String data
    HashSet* src = str_set();
    const char* words[] = { "alpha", "beta", "gamma" };
    for (int i = 0; i < 3; i++) {
        String sv;
        string_create_stk(words[i], &sv);
        HashSet_insert(src, (u8*)&sv);
        string_destroy_stk(&sv);
    }

    HashSet* dest = str_set();
    HashSet_copy(dest, src);
    WC_ASSERT_EQ_U64(HashSet_size(dest), 3);

    HashSet_destroy(src); // src gone — dest must still be intact

    String probe;
    string_create_stk("beta", &probe);
    WC_ASSERT_TRUE(HashSet_has(dest, (u8*)&probe));
    string_destroy_stk(&probe);

    HashSet_destroy(dest);
}

static void test_copy_empty_set(void)
{
    HashSet* src = int_set();

    HashSet* dest = int_set();
    HashSet_copy(dest, src);
    WC_ASSERT_EQ_U64(HashSet_size(dest), 0);
    WC_ASSERT_EQ_U64(HashSet_capacity(dest), HashSet_capacity(src));

    HashSet_destroy(src);
    HashSet_destroy(dest);
}

static void test_copy_then_remove_src_elm(void)
{
    // Removing from src after copy must not affect dest
    HashSet* src = int_set();
    int x = 5;
    HashSet_insert(src, (u8*)&x);

    HashSet* dest = int_set();
    HashSet_copy(dest, src);

    HashSet_remove(src, (u8*)&x);
    WC_ASSERT_FALSE(HashSet_has(src,   (u8*)&x));
    WC_ASSERT_TRUE(HashSet_has(dest, (u8*)&x));

    HashSet_destroy(src);
    HashSet_destroy(dest);
}


// TODO: properly test all macros

/* ════════════════════════════════════════════════════════════════════════════
 * String set  (owns heap memory)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_str_insert_move_nulls_ptr(void)
{
    HashSet* s  = str_set();
    String*  s1 = string_from_cstr("hello");
    HashSet_insert_move(s, (u8**)&s1);
    WC_ASSERT_NULL(s1); // ownership transferred

    String probe;
    string_create_stk("hello", &probe);
    WC_ASSERT_TRUE(HashSet_has(s, (u8*)&probe));
    string_destroy_stk(&probe);
    HashSet_destroy(s);
}

static void test_str_insert_copy_leaves_src_valid(void)
{
    // Source String must still be valid and unchanged after copy insert
    HashSet* s  = str_set();
    String*  s1 = string_from_cstr("world");
    HashSet_insert(s, (u8*)s1);

    WC_ASSERT_NOT_NULL(s1);
    WC_ASSERT_TRUE(string_equals_cstr(s1, "world"));
    string_destroy(s1);
    HashSet_destroy(s);
}

static void test_str_insert_copy_independence(void)
{
    // Mutating source String after copy insert must not affect stored copy
    HashSet* s = str_set();
    String   sv;
    string_create_stk("original", &sv);
    HashSet_insert(s, (u8*)&sv);
    string_append_cstr(&sv, "_mutated");

    String probe;
    string_create_stk("original", &probe);
    WC_ASSERT_TRUE(HashSet_has(s, (u8*)&probe));
    string_destroy_stk(&probe);

    string_destroy_stk(&sv);
    HashSet_destroy(s);
}

static void test_str_has_miss(void)
{
    HashSet* s = str_set();
    String probe;
    string_create_stk("missing", &probe);
    WC_ASSERT_FALSE(HashSet_has(s, (u8*)&probe));
    string_destroy_stk(&probe);
    HashSet_destroy(s);
}

static void test_str_no_duplicates(void)
{
    HashSet* s = str_set();
    String   sv;
    string_create_stk("dup", &sv);
    b8 first  = HashSet_insert(s, (u8*)&sv);
    b8 second = HashSet_insert(s, (u8*)&sv);
    WC_ASSERT_FALSE(first);
    WC_ASSERT_TRUE(second);
    WC_ASSERT_EQ_U64(HashSet_size(s), 1);
    string_destroy_stk(&sv);
    HashSet_destroy(s);
}

static void test_str_insert_move_duplicate_frees_elm(void)
{
    // insert_move on a duplicate must free the incoming pointer
    HashSet* s = str_set();
    String   sv;
    string_create_stk("dup", &sv);
    HashSet_insert(s, (u8*)&sv);

    String* dup = string_from_cstr("dup");
    b8 existed = HashSet_insert_move(s, (u8**)&dup);
    WC_ASSERT_TRUE(existed);
    WC_ASSERT_NULL(dup);        // must be freed and nulled
    WC_ASSERT_EQ_U64(HashSet_size(s), 1);

    string_destroy_stk(&sv);
    HashSet_destroy(s);
}

static void test_str_remove(void)
{
    HashSet* s  = str_set();
    String*  s1 = string_from_cstr("remove_me");
    HashSet_insert_move(s, (u8**)&s1);

    String probe;
    string_create_stk("remove_me", &probe);
    WC_ASSERT_TRUE(HashSet_remove(s, (u8*)&probe));
    WC_ASSERT_FALSE(HashSet_has(s, (u8*)&probe));
    WC_ASSERT_EQ_U64(HashSet_size(s), 0);
    string_destroy_stk(&probe);
    HashSet_destroy(s);
}

static void test_str_resize_preserves_membership(void)
{
    HashSet* s = str_set();
    char buf[16];
    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "word%d", i);
        String sv;
        string_create_stk(buf, &sv);
        HashSet_insert(s, (u8*)&sv);
        string_destroy_stk(&sv);
    }
    WC_ASSERT_EQ_U64(HashSet_size(s), 40);

    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "word%d", i);
        String probe;
        string_create_stk(buf, &probe);
        WC_ASSERT_TRUE(HashSet_has(s, (u8*)&probe));
        string_destroy_stk(&probe);
    }
    HashSet_destroy(s);
}

static void test_str_remove_frees_elm(void)
{
    // remove must call del_fn on owned String before clearing the slot
    HashSet* s = str_set();
    String sv;
    string_create_stk("owned", &sv);
    HashSet_insert(s, (u8*)&sv);
    string_destroy_stk(&sv);

    String probe;
    string_create_stk("owned", &probe);
    WC_ASSERT_TRUE(HashSet_remove(s, (u8*)&probe));
    WC_ASSERT_EQ_U64(HashSet_size(s), 0);
    string_destroy_stk(&probe);
    HashSet_destroy(s);
}

static void test_str_clear_then_reuse(void)
{
    HashSet* s = str_set();
    for (int i = 0; i < 8; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "item%d", i);
        String sv;
        string_create_stk(buf, &sv);
        HashSet_insert(s, (u8*)&sv);
        string_destroy_stk(&sv);
    }
    HashSet_clear(s);
    WC_ASSERT_EQ_U64(HashSet_size(s), 0);

    // Usable after clear
    String* v = string_from_cstr("fresh");
    HashSet_insert_move(s, (u8**)&v);
    WC_ASSERT_EQ_U64(HashSet_size(s), 1);

    String probe;
    string_create_stk("fresh", &probe);
    WC_ASSERT_TRUE(HashSet_has(s, (u8*)&probe));
    string_destroy_stk(&probe);
    HashSet_destroy(s);
}

/* ── HashSet_insert_move ─────────────────────────────────────────────────── */

static void test_insert_move_nulls_src(void)
{
    HashSet* s  = HashSet_create(sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
    String*  el = string_from_cstr("owned");
    b8 existed  = HashSet_insert_move(s, (u8**)&el);
    WC_ASSERT_FALSE(existed);
    WC_ASSERT_NULL(el);
    WC_ASSERT_EQ_U64(HashSet_size(s), 1);

    String k; string_create_stk("owned", &k);
    WC_ASSERT_TRUE(HashSet_has(s, (u8*)&k));
    string_destroy_stk(&k);
    HashSet_destroy(s);
}

static void test_insert_move_duplicate_frees_incoming(void)
{
    HashSet* s  = HashSet_create(sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
    SET_INSERT_CSTR(s, "dup");

    String* el  = string_from_cstr("dup");
    b8 existed  = HashSet_insert_move(s, (u8**)&el);
    WC_ASSERT_TRUE(existed);    /* already in set */
    WC_ASSERT_NULL(el);         /* incoming consumed */
    WC_ASSERT_EQ_U64(HashSet_size(s), 1);
    HashSet_destroy(s);
}


/* ── HashSet_copy ────────────────────────────────────────────────────────── */


static void test_copy_str_set_deep(void)
{
    HashSet* src = HashSet_create(sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
    SET_INSERT_CSTR(src, "alpha");
    SET_INSERT_CSTR(src, "beta");

    HashSet* dest = HashSet_create(sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
    HashSet_copy(dest, src);
    WC_ASSERT_EQ_U64(HashSet_size(dest), 2);

    HashSet_destroy(src); /* src gone — dest must still be intact */

    String k; string_create_stk("alpha", &k);
    WC_ASSERT_TRUE(HashSet_has(dest, (u8*)&k));
    string_destroy_stk(&k);
    HashSet_destroy(dest);
}

static void test_clear_empty_set_noop(void)
{
    HashSet* s = HashSet_create(sizeof(int), NULL, NULL, NULL);
    HashSet_clear(s);
    WC_ASSERT_EQ_U64(HashSet_size(s), 0);
    HashSet_destroy(s);
}


/* ── SET_FOREACH macro ───────────────────────────────────────────────────── */

static void test_set_foreach_visits_all(void)
{
    HashSet* s = HashSet_create(sizeof(int), NULL, NULL, NULL);
    for (int i = 0; i < 8; i++) { HashSet_insert(s, (u8*)&i); }

    int count = 0, sum = 0;
    SET_FOREACH(s, int, el) {
        count++;
        sum += *el;
    }
    WC_ASSERT_EQ_INT(count, 8);
    WC_ASSERT_EQ_INT(sum, 0+1+2+3+4+5+6+7);
    HashSet_destroy(s);
}

static void test_set_foreach_empty(void)
{
    HashSet* s  = HashSet_create(sizeof(int), NULL, NULL, NULL);
    int count   = 0;
    SET_FOREACH(s, int, el) { count++; (void)el; }
    WC_ASSERT_EQ_INT(count, 0);
    HashSet_destroy(s);
}

static void test_set_foreach_after_remove(void)
{
    HashSet* s = HashSet_create(sizeof(int), NULL, NULL, NULL);
    for (int i = 0; i < 8; i++) { HashSet_insert(s, (u8*)&i); }
    for (int i = 0; i < 4; i++) { HashSet_remove(s, (u8*)&i); }

    int count = 0;
    SET_FOREACH(s, int, el) {
        WC_ASSERT_TRUE(*el >= 4);
        count++;
    }
    WC_ASSERT_EQ_INT(count, 4);
    HashSet_destroy(s);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void HashSet_extra_suite(void)
{
    WC_SUITE("HashSet — extra coverage");

}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void HashSet_suite(void)
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
    WC_RUN(test_remove_correctness_after_many_removes);

    WC_SUITE("HashSet — delete correctness");
    WC_RUN(test_remove_reinsert);
    WC_RUN(test_remove_mid_chain);
    WC_RUN(test_remove_reinsert_cycle);
    WC_RUN(test_remove_first_in_chain);
    WC_RUN(test_remove_all_then_reinsert);

    WC_SUITE("HashSet — clear");
    WC_RUN(test_clear_empties_set);
    WC_RUN(test_clear_then_reuse);
    WC_RUN(test_clear_frees_string_elms);
    WC_RUN(test_clear_empty_set);

    WC_SUITE("HashSet — copy");
    WC_RUN(test_copy_int_set);
    WC_RUN(test_copy_independence);
    WC_RUN(test_copy_str_set);
    WC_RUN(test_copy_empty_set);
    WC_RUN(test_copy_then_remove_src_elm);

    WC_SUITE("HashSet — Macros");
    // TODO:

    WC_SUITE("HashSet — String (owned elements)");
    WC_RUN(test_str_insert_move_nulls_ptr);
    WC_RUN(test_str_insert_copy_leaves_src_valid);
    WC_RUN(test_str_insert_copy_independence);
    WC_RUN(test_str_has_miss);
    WC_RUN(test_str_no_duplicates);
    WC_RUN(test_str_insert_move_duplicate_frees_elm);
    WC_RUN(test_str_remove);
    WC_RUN(test_str_resize_preserves_membership);
    WC_RUN(test_str_remove_frees_elm);
    WC_RUN(test_str_clear_then_reuse);


    // new tests
    WC_RUN(test_insert_move_nulls_src);
    WC_RUN(test_insert_move_duplicate_frees_incoming);

    WC_RUN(test_copy_int_set);
    WC_RUN(test_copy_independence);
    WC_RUN(test_copy_str_set_deep);
    WC_RUN(test_copy_empty_set);

    WC_RUN(test_clear_frees_string_elms);
    WC_RUN(test_clear_empty_set_noop);

    WC_RUN(test_set_foreach_visits_all);
    WC_RUN(test_set_foreach_empty);
    WC_RUN(test_set_foreach_after_remove);
}


