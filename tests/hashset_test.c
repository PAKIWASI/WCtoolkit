#include "arena.h"
#include "common.h"
#include "map_setup.h"
#include "wc_macros.h"
#include "wc_string.h"
#include "wc_test.h"
#include "hashset.h"
#include "wc_helpers.h"
#include "wc_test_fatal.h"
#include <stdio.h>
#include <stdlib.h>


/* ── Set constructors ────────────────────────────────────────────────────── */

static HashSet int_set(void)
{
    return HashSet_create(WC_LIBC, sizeof(int), NULL, NULL, NULL);
}

static HashSet str_set(void)
{
    return HashSet_create(WC_LIBC, sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
}


/* ════════════════════════════════════════════════════════════════════════════
 * int set  (POD, no copy/move/del)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_insert_and_has(void)
{
    HashSet s = int_set();
    int x = 42;
    WC_EXPECT_FALSE(HashSet_has(&s, &x));
    HashSet_insert(&s, &x);
    WC_EXPECT_TRUE(HashSet_has(&s, &x));
    HashSet_destroy(&s);
}

static void test_insert_returns_existed(void)
{
    HashSet s = int_set();
    int x = 5;
    b8 first  = HashSet_insert(&s, &x);
    b8 second = HashSet_insert(&s, &x);
    WC_EXPECT_FALSE(first);  // new insert
    WC_EXPECT_TRUE(second);  // already existed
    HashSet_destroy(&s);
}

static void test_insert_duplicate_no_growth(void)
{
    HashSet s = int_set();
    int x = 10;
    HashSet_insert(&s, &x);
    HashSet_insert(&s, &x);
    HashSet_insert(&s, &x);
    WC_EXPECT_EQ_U64(HashSet_size(&s), 1);
    HashSet_destroy(&s);
}

static void test_has_missing_returns_false(void)
{
    HashSet s = int_set();
    int x = 999;
    WC_EXPECT_FALSE(HashSet_has(&s, &x));
    HashSet_destroy(&s);
}

static void test_remove(void)
{
    HashSet s = int_set();
    int x = 7;
    HashSet_insert(&s, &x);
    WC_EXPECT_TRUE(HashSet_remove(&s, &x));
    WC_EXPECT_FALSE(HashSet_has(&s, &x));
    WC_EXPECT_EQ_U64(HashSet_size(&s), 0);
    HashSet_destroy(&s);
}

static void test_remove_missing_returns_false(void)
{
    HashSet s = int_set();
    int x = 999;
    WC_EXPECT_FALSE(HashSet_remove(&s, &x));
    HashSet_destroy(&s);
}

static void test_remove_on_empty_set(void)
{
    HashSet s = int_set();
    int x = 1;
    WC_EXPECT_FALSE(HashSet_remove(&s, &x));
    WC_EXPECT_EQ_U64(HashSet_size(&s), 0);
    HashSet_destroy(&s);
}

static void test_size_and_empty(void)
{
    HashSet s = int_set();
    WC_EXPECT_EQ_U64(HashSet_size(&s), 0);
    WC_EXPECT_TRUE(HashSet_empty(&s));

    for (int i = 0; i < 10; i++) {
        HashSet_insert(&s, &i);
    }
    WC_EXPECT_EQ_U64(HashSet_size(&s), 10);
    WC_EXPECT_FALSE(HashSet_empty(&s));
    HashSet_destroy(&s);
}

static void test_size_tracks_inserts(void)
{
    HashSet s = int_set();
    for (int i = 0; i < 20; i++) {
        HashSet_insert(&s, &i);
    }
    WC_EXPECT_EQ_U64(HashSet_size(&s), 20);
    HashSet_destroy(&s);
}

static void test_resize_preserves_membership(void)
{
    HashSet s = int_set();
    for (int i = 0; i < 50; i++) {
        HashSet_insert(&s, &i);
    }
    WC_EXPECT_EQ_U64(HashSet_size(&s), 50);
    for (int i = 0; i < 50; i++) {
        WC_EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

// Robin Hood + backward-shift delete doesn't shrink — no LOAD_FACTOR_SHRINK.
// Verify correctness after heavy removal instead.
static void test_remove_correctness_after_many_removes(void)
{
    HashSet s = int_set();
    for (int i = 0; i < 50; i++) {
        HashSet_insert(&s, &i);
    }
    for (int i = 0; i < 48; i++) {
        HashSet_remove(&s, &i);
    }
    WC_EXPECT_EQ_U64(HashSet_size(&s), 2);

    for (int i = 48; i < 50; i++) {
        WC_EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * delete correctness  (backward-shift, no tombstones)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_remove_reinsert(void)
{
    // Remove an element then re-insert it — must succeed and be findable
    HashSet s = int_set();
    int x = 42;
    HashSet_insert(&s, &x);
    HashSet_remove(&s, &x);
    WC_EXPECT_FALSE(HashSet_has(&s, &x));

    HashSet_insert(&s, &x);
    WC_EXPECT_TRUE(HashSet_has(&s, &x));
    WC_EXPECT_EQ_U64(HashSet_size(&s), 1);
    HashSet_destroy(&s);
}

static void test_remove_mid_chain(void)
{
    // Insert elements that chain during probing, remove one mid-chain,
    // then verify all remaining elements are still reachable.
    HashSet s = int_set();
    for (int i = 0; i < 20; i++) {
        HashSet_insert(&s, &i);
    }

    int mid = 5;
    HashSet_remove(&s, &mid);

    for (int i = 0; i < 20; i++) {
        if (i == mid) {
            continue;
        }
        WC_EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

static void test_remove_reinsert_cycle(void)
{
    // Repeated remove+insert must not corrupt or leak
    HashSet s = int_set();
    int x = 7;
    for (int cycle = 0; cycle < 20; cycle++) {
        HashSet_insert(&s, &x);
        WC_EXPECT_TRUE(HashSet_has(&s, &x));
        HashSet_remove(&s, &x);
        WC_EXPECT_EQ_U64(HashSet_size(&s), 0);
    }
    HashSet_destroy(&s);
}

static void test_remove_first_in_chain(void)
{
    // Removing the head of a probe chain must leave the rest reachable
    HashSet s = int_set();
    for (int i = 0; i < 15; i++) {
        HashSet_insert(&s, &i);
    }
    int head = 0;
    HashSet_remove(&s, &head);
    WC_EXPECT_FALSE(HashSet_has(&s, &head));

    for (int i = 1; i < 15; i++) {
        WC_EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

static void test_remove_all_then_reinsert(void)
{
    // Remove every element then reinsert — set must be fully functional
    HashSet s = int_set();
    for (int i = 0; i < 20; i++) {
        HashSet_insert(&s, &i);
    }
    for (int i = 0; i < 20; i++) {
        HashSet_remove(&s, &i);
    }
    WC_EXPECT_EQ_U64(HashSet_size(&s), 0);
    WC_EXPECT_TRUE(HashSet_empty(&s));

    for (int i = 0; i < 20; i++) {
        HashSet_insert(&s, &i);
    }
    WC_EXPECT_EQ_U64(HashSet_size(&s), 20);
    for (int i = 0; i < 20; i++) {
        WC_EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * HashSet_clear
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_clear_empties_set(void)
{
    HashSet s = int_set();
    for (int i = 0; i < 10; i++) {
        HashSet_insert(&s, &i);
    }
    u64 cap_before = HashSet_capacity(&s);
    HashSet_clear(&s);

    WC_EXPECT_EQ_U64(HashSet_size(&s), 0);
    WC_EXPECT_TRUE(HashSet_empty(&s));
    WC_EXPECT_EQ_U64(HashSet_capacity(&s), cap_before); // capacity unchanged
    for (int i = 0; i < 10; i++) {
        WC_EXPECT_FALSE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

static void test_clear_then_reuse(void)
{
    HashSet s = int_set();
    for (int i = 0; i < 10; i++) {
        HashSet_insert(&s, &i);
    }
    HashSet_clear(&s);

    for (int i = 100; i < 110; i++) {
        HashSet_insert(&s, &i);
    }
    WC_EXPECT_EQ_U64(HashSet_size(&s), 10);
    for (int i = 100; i < 110; i++) {
        WC_EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

static void test_clear_frees_String_elms(void)
{
    // clear must properly call del on owned String resources
    HashSet s = str_set();
    for (int i = 0; i < 5; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "str%d", i);
        String v = String_from_cstr(WC_LIBC, buf);
        HashSet_insert_move(&s, &v);
        String_destroy(&v); // safe on zeroed
    }
    HashSet_clear(&s);
    WC_EXPECT_EQ_U64(HashSet_size(&s), 0);

    // Set must still be usable after clearing owned-resource entries
    String v = String_from_cstr(WC_LIBC, "after_clear");
    HashSet_insert_move(&s, &v);
    String_destroy(&v); // safe on zeroed
    WC_EXPECT_EQ_U64(HashSet_size(&s), 1);
    HashSet_destroy(&s);
}

static void test_clear_empty_set(void)
{
    // clear on an already-empty set must be a safe no-op
    HashSet s = int_set();
    HashSet_clear(&s);
    WC_EXPECT_EQ_U64(HashSet_size(&s), 0);
    HashSet_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * HashSet_copy
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_copy_int_set(void)
{
    HashSet src = int_set();
    for (int i = 0; i < 10; i++) {
        HashSet_insert(&src, &i);
    }

    HashSet dest = HashSet_copy(WC_LIBC, &src);
    WC_EXPECT_EQ_U64(HashSet_size(&dest), HashSet_size(&src));

    for (int i = 0; i < 10; i++) {
        WC_EXPECT_TRUE(HashSet_has(&dest, &i));
    }

    HashSet_destroy(&src);
    HashSet_destroy(&dest);
}

static void test_copy_independence(void)
{
    // Inserting into dest must not affect src
    HashSet src = int_set();
    int x = 1;
    HashSet_insert(&src, &x);

    HashSet dest = HashSet_copy(WC_LIBC, &src);

    int y = 99;
    HashSet_insert(&dest, &y);

    WC_EXPECT_FALSE(HashSet_has(&src,  &y)); // src unaffected
    WC_EXPECT_TRUE(HashSet_has(&dest, &x)); // dest has original
    WC_EXPECT_TRUE(HashSet_has(&dest, &y)); // dest has new

    HashSet_destroy(&src);
    HashSet_destroy(&dest);
}

static void test_copy_str_set(void)
{
    // Deep copy: destroying src must not corrupt dest's String data
    HashSet src = str_set();
    const char* words[] = { "alpha", "beta", "gamma" };
    for (int i = 0; i < 3; i++) {
        String sv = String_from_cstr(WC_LIBC, words[i]);
        HashSet_insert(&src, &sv);
        String_destroy(&sv);
    }

    HashSet dest = HashSet_copy(WC_LIBC, &src);
    WC_EXPECT_EQ_U64(HashSet_size(&dest), 3);

    HashSet_destroy(&src); // src gone — dest must still be intact

    String probe = String_from_cstr(WC_LIBC, "beta");
    WC_EXPECT_TRUE(HashSet_has(&dest, &probe));
    String_destroy(&probe);

    HashSet_destroy(&dest);
}

static void test_copy_empty_set(void)
{
    HashSet src = int_set();

    HashSet dest = HashSet_copy(WC_LIBC, &src);
    WC_EXPECT_EQ_U64(HashSet_size(&dest), 0);
    WC_EXPECT_EQ_U64(HashSet_capacity(&dest), HashSet_capacity(&src));

    HashSet_destroy(&src);
    HashSet_destroy(&dest);
}

static void test_copy_then_remove_src_elm(void)
{
    // Removing from src after copy must not affect dest
    HashSet src = int_set();
    int x = 5;
    HashSet_insert(&src, &x);

    HashSet dest = HashSet_copy(WC_LIBC, &src);

    HashSet_remove(&src, &x);
    WC_EXPECT_FALSE(HashSet_has(&src,  &x));
    WC_EXPECT_TRUE(HashSet_has(&dest, &x));

    HashSet_destroy(&src);
    HashSet_destroy(&dest);
}


/* ════════════════════════════════════════════════════════════════════════════
 * String set  (owns heap memory)
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_str_insert_move_nulls_ptr(void)
{
    HashSet s  = str_set();
    String   s1 = String_from_cstr(WC_LIBC, "hello");
    HashSet_insert_move(&s, &s1);
    WC_EXPECT_EQ_U64(s1.size, 0); // ownership transferred: source zeroed
    String_destroy(&s1);          // safe on zeroed

    String probe = String_from_cstr(WC_LIBC, "hello");
    WC_EXPECT_TRUE(HashSet_has(&s, &probe));
    String_destroy(&probe);
    HashSet_destroy(&s);
}

static void test_str_insert_copy_leaves_src_valid(void)
{
    // Source String must still be valid and unchanged after copy insert
    HashSet s  = str_set();
    String   s1 = String_from_cstr(WC_LIBC, "world");
    HashSet_insert(&s, &s1);

    WC_EXPECT_TRUE(String_equals_cstr(&s1, "world"));
    String_destroy(&s1);
    HashSet_destroy(&s);
}

static void test_str_insert_copy_independence(void)
{
    // Mutating source String after copy insert must not affect stored copy
    HashSet s = str_set();
    String   sv = String_from_cstr(WC_LIBC, "original");
    HashSet_insert(&s, &sv);
    String_append_cstr(&sv, "_mutated");

    String probe = String_from_cstr(WC_LIBC, "original");
    WC_EXPECT_TRUE(HashSet_has(&s, &probe));
    String_destroy(&probe);

    String_destroy(&sv);
    HashSet_destroy(&s);
}

static void test_str_has_miss(void)
{
    HashSet s = str_set();
    String probe = String_from_cstr(WC_LIBC, "missing");
    WC_EXPECT_FALSE(HashSet_has(&s, &probe));
    String_destroy(&probe);
    HashSet_destroy(&s);
}

static void test_str_no_duplicates(void)
{
    HashSet s = str_set();
    String   sv = String_from_cstr(WC_LIBC, "dup");
    b8 first  = HashSet_insert(&s, &sv);
    b8 second = HashSet_insert(&s, &sv);
    WC_EXPECT_FALSE(first);
    WC_EXPECT_TRUE(second);
    WC_EXPECT_EQ_U64(HashSet_size(&s), 1);
    String_destroy(&sv);
    HashSet_destroy(&s);
}

static void test_str_insert_move_duplicate_frees_elm(void)
{
    // insert_move on a duplicate must free the incoming pointer
    HashSet s = str_set();
    String   sv = String_from_cstr(WC_LIBC, "dup");
    HashSet_insert(&s, &sv);

    String dup = String_from_cstr(WC_LIBC, "dup");
    b8 existed = HashSet_insert_move(&s, &dup);
    WC_EXPECT_TRUE(existed);
    WC_EXPECT_EQ_U64(dup.size, 0); // duplicate destroyed and zeroed
    WC_EXPECT_NULL(dup.heap);
    String_destroy(&dup);          // safe on zeroed
    WC_EXPECT_EQ_U64(HashSet_size(&s), 1);

    String_destroy(&sv);
    HashSet_destroy(&s);
}

static void test_str_remove(void)
{
    HashSet s  = str_set();
    String   s1 = String_from_cstr(WC_LIBC, "remove_me");
    HashSet_insert_move(&s, &s1);
    String_destroy(&s1); // moved-from: zeroed, safe on zeroed

    String probe = String_from_cstr(WC_LIBC, "remove_me");
    WC_EXPECT_TRUE(HashSet_remove(&s, &probe));
    WC_EXPECT_FALSE(HashSet_has(&s, &probe));
    WC_EXPECT_EQ_U64(HashSet_size(&s), 0);
    String_destroy(&probe);
    HashSet_destroy(&s);
}

static void test_str_resize_preserves_membership(void)
{
    HashSet s = str_set();
    char buf[16];
    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "word%d", i);
        String sv = String_from_cstr(WC_LIBC, buf);
        HashSet_insert(&s, &sv);
        String_destroy(&sv);
    }
    WC_EXPECT_EQ_U64(HashSet_size(&s), 40);

    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "word%d", i);
        String probe = String_from_cstr(WC_LIBC, buf);
        WC_EXPECT_TRUE(HashSet_has(&s, &probe));
        String_destroy(&probe);
    }
    HashSet_destroy(&s);
}

static void test_str_remove_frees_elm(void)
{
    // remove must call del_fn on owned String before clearing the slot
    HashSet s = str_set();
    String sv = String_from_cstr(WC_LIBC, "owned");
    HashSet_insert(&s, &sv);
    String_destroy(&sv);

    String probe = String_from_cstr(WC_LIBC, "owned");
    WC_EXPECT_TRUE(HashSet_remove(&s, &probe));
    WC_EXPECT_EQ_U64(HashSet_size(&s), 0);
    String_destroy(&probe);
    HashSet_destroy(&s);
}

static void test_str_clear_then_reuse(void)
{
    HashSet s = str_set();
    for (int i = 0; i < 8; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "item%d", i);
        String sv = String_from_cstr(WC_LIBC, buf);
        HashSet_insert(&s, &sv);
        String_destroy(&sv);
    }
    HashSet_clear(&s);
    WC_EXPECT_EQ_U64(HashSet_size(&s), 0);

    // Usable after clear
    String v = String_from_cstr(WC_LIBC, "fresh");
    HashSet_insert_move(&s, &v);
    String_destroy(&v); // moved-from: zeroed, safe destroy
    WC_EXPECT_EQ_U64(HashSet_size(&s), 1);

    String probe = String_from_cstr(WC_LIBC, "fresh");
    WC_EXPECT_TRUE(HashSet_has(&s, &probe));
    String_destroy(&probe);
    HashSet_destroy(&s);
}

/* ── HashSet_insert_move ─────────────────────────────────────────────────── */

static void test_insert_move_nulls_src(void)
{
    HashSet s  = HashSet_create(WC_LIBC, sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
    String   el = String_from_cstr(WC_LIBC, "owned");
    b8 existed  = HashSet_insert_move(&s, &el);
    String_destroy(&el); // moved-from: zeroed, safe destroy
    WC_EXPECT_FALSE(existed);
    WC_EXPECT_EQ_U64(HashSet_size(&s), 1);

    String k = String_from_cstr(WC_LIBC, "owned");
    WC_EXPECT_TRUE(HashSet_has(&s, &k));
    String_destroy(&k);
    HashSet_destroy(&s);
}

static void test_insert_move_duplicate_frees_incoming(void)
{
    HashSet s  = HashSet_create(WC_LIBC, sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
    SET_INSERT_CSTR(&s, "dup");

    String el  = String_from_cstr(WC_LIBC, "dup");
    b8 existed = HashSet_insert_move(&s, &el);
    WC_EXPECT_TRUE(existed);         /* already in set */
    WC_EXPECT_EQ_U64(el.size, 0);    /* incoming consumed and zeroed */
    String_destroy(&el);             /* safe on zeroed */
    WC_EXPECT_EQ_U64(HashSet_size(&s), 1);
    HashSet_destroy(&s);
}


/* ── HashSet_copy ────────────────────────────────────────────────────────── */

static void test_copy_str_set_deep(void)
{
    HashSet src = HashSet_create(WC_LIBC, sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
    SET_INSERT_CSTR(&src, "alpha");
    SET_INSERT_CSTR(&src, "beta");

    HashSet dest = HashSet_copy(WC_LIBC, &src);
    WC_EXPECT_EQ_U64(HashSet_size(&dest), 2);

    HashSet_destroy(&src); /* src gone — dest must still be intact */

    String k = String_from_cstr(WC_LIBC, "alpha");
    WC_EXPECT_TRUE(HashSet_has(&dest, &k));
    String_destroy(&k);
    HashSet_destroy(&dest);
}

static void test_clear_empty_set_noop(void)
{
    HashSet s = HashSet_create(WC_LIBC, sizeof(int), NULL, NULL, NULL);
    HashSet_clear(&s);
    WC_EXPECT_EQ_U64(HashSet_size(&s), 0);
    HashSet_destroy(&s);
}


/* ── SET_FOREACH macro ───────────────────────────────────────────────────── */

static void test_set_foreach_visits_all(void)
{
    HashSet s = HashSet_create(WC_LIBC, sizeof(int), NULL, NULL, NULL);
    for (int i = 0; i < 8; i++) { HashSet_insert(&s, &i); }

    int count = 0, sum = 0;
    SET_FOREACH(&s, int, el) {
        count++;
        sum += *el;
    }
    WC_EXPECT_EQ_INT(count, 8);
    WC_EXPECT_EQ_INT(sum, 0+1+2+3+4+5+6+7);
    HashSet_destroy(&s);
}

static void test_set_foreach_empty(void)
{
    HashSet s  = HashSet_create(WC_LIBC, sizeof(int), NULL, NULL, NULL);
    int count   = 0;
    SET_FOREACH(&s, int, el) { count++; (void)el; }
    WC_EXPECT_EQ_INT(count, 0);
    HashSet_destroy(&s);
}

static void test_set_foreach_after_remove(void)
{
    HashSet s = HashSet_create(WC_LIBC, sizeof(int), NULL, NULL, NULL);
    for (int i = 0; i < 8; i++) { HashSet_insert(&s, &i); }
    for (int i = 0; i < 4; i++) { HashSet_remove(&s, &i); }

    int count = 0;
    SET_FOREACH(&s, int, el) {
        WC_EXPECT_TRUE(*el >= 4);
        count++;
    }
    WC_EXPECT_EQ_INT(count, 4);
    HashSet_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * Phase 5 New Tests: resizes, cross-allocator copy, move, zero-state
 * ════════════════════════════════════════════════════════════════════════════ */

static void test_hashset_fill_past_several_resizes(void)
{
    HashSet s = int_set();
    const int count = 2000;
    for (int i = 0; i < count; i++) {
        HashSet_insert(&s, &i);
    }
    WC_EXPECT_EQ_U64(HashSet_size(&s), count);

    for (int i = 0; i < count; i++) {
        WC_EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

static void test_hashset_cross_alloc_copy(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(64));
    wc_allocator al = Arena_allocator(&a);

    HashSet src = HashSet_create(al, sizeof(int), NULL, NULL, NULL);
    for (int i = 0; i < 20; i++) {
        HashSet_insert(&src, &i);
    }

    // Copy from Arena to libc
    HashSet dest = HashSet_copy(WC_LIBC, &src);
    WC_EXPECT_EQ_U64(HashSet_size(&dest), 20);

    // Destroy src and arena, dest must remain valid in libc
    HashSet_destroy(&src);
    Arena_destroy(&a);

    for (int i = 0; i < 20; i++) {
        WC_EXPECT_TRUE(HashSet_has(&dest, &i));
    }
    HashSet_destroy(&dest);
}

static void test_hashset_move(void)
{
    HashSet src = int_set();
    int x = 42;
    HashSet_insert(&src, &x);

    HashSet dest;
    HashSet_move(&dest, &src);

    WC_EXPECT_EQ_U64(src.capacity, 0);
    WC_EXPECT_EQ_U64(HashSet_size(&dest), 1);
    WC_EXPECT_TRUE(HashSet_has(&dest, &x));

    HashSet_destroy(&src);  // safe on zeroed
    HashSet_destroy(&dest);
}

#if WC_HAS_FORK
static void die_mutating_zero_hashset(void)
{
    HashSet s;
    memset(&s, 0, sizeof(s));
    int x = 42;
    HashSet_insert(&s, &x);
}

static void test_hashset_zero_state_fatal(void)
{
    WC_EXPECT_DIES(die_mutating_zero_hashset);
}
#endif


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
    WC_RUN(test_clear_frees_String_elms);
    WC_RUN(test_clear_empty_set);

    WC_SUITE("HashSet — copy");
    WC_RUN(test_copy_int_set);
    WC_RUN(test_copy_independence);
    WC_RUN(test_copy_str_set);
    WC_RUN(test_copy_empty_set);
    WC_RUN(test_copy_then_remove_src_elm);

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

    WC_SUITE("HashSet — insert_move & copy deep");
    WC_RUN(test_insert_move_nulls_src);
    WC_RUN(test_insert_move_duplicate_frees_incoming);
    WC_RUN(test_copy_str_set_deep);
    WC_RUN(test_clear_empty_set_noop);

    WC_SUITE("HashSet — Macros");
    WC_RUN(test_set_foreach_visits_all);
    WC_RUN(test_set_foreach_empty);
    WC_RUN(test_set_foreach_after_remove);

    WC_SUITE("HashSet — Phase 5 Allocator & Move");
    WC_RUN(test_hashset_fill_past_several_resizes);
    WC_RUN(test_hashset_cross_alloc_copy);
    WC_RUN(test_hashset_move);
#if WC_HAS_FORK
    WC_RUN(test_hashset_zero_state_fatal);
#endif
}
