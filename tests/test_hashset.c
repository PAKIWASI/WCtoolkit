#include "arena.h"
#include "common.h"
#include "hashset.h"
#include "map_setup.h"
#include "test_support.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_helpers.h"
#include "wc_macros.h"
#include "wc_string.h"

#include <stdio.h>
#include <string.h>


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

UTEST(hashset, insert_and_has)
{
    HashSet s = int_set();
    int     x = 42;
    EXPECT_FALSE(HashSet_has(&s, &x));
    HashSet_insert(&s, &x);
    EXPECT_TRUE(HashSet_has(&s, &x));
    HashSet_destroy(&s);
}

UTEST(hashset, insert_returns_existed)
{
    HashSet s      = int_set();
    int     x      = 5;
    b8      first  = HashSet_insert(&s, &x);
    b8      second = HashSet_insert(&s, &x);
    EXPECT_FALSE(first); // new insert
    EXPECT_TRUE(second); // already existed
    HashSet_destroy(&s);
}

UTEST(hashset, insert_duplicate_no_growth)
{
    HashSet s = int_set();
    int     x = 10;
    HashSet_insert(&s, &x);
    HashSet_insert(&s, &x);
    HashSet_insert(&s, &x);
    EXPECT_EQ(HashSet_size(&s), 1u);
    HashSet_destroy(&s);
}

UTEST(hashset, has_missing_returns_false)
{
    HashSet s = int_set();
    int     x = 999;
    EXPECT_FALSE(HashSet_has(&s, &x));
    HashSet_destroy(&s);
}

UTEST(hashset, remove)
{
    HashSet s = int_set();
    int     x = 7;
    HashSet_insert(&s, &x);
    EXPECT_TRUE(HashSet_remove(&s, &x));
    EXPECT_FALSE(HashSet_has(&s, &x));
    EXPECT_EQ(HashSet_size(&s), 0u);
    HashSet_destroy(&s);
}

UTEST(hashset, remove_missing_returns_false)
{
    HashSet s = int_set();
    int     x = 999;
    EXPECT_FALSE(HashSet_remove(&s, &x));
    HashSet_destroy(&s);
}

UTEST(hashset, remove_on_empty_set)
{
    HashSet s = int_set();
    int     x = 1;
    EXPECT_FALSE(HashSet_remove(&s, &x));
    EXPECT_EQ(HashSet_size(&s), 0u);
    HashSet_destroy(&s);
}

UTEST(hashset, size_and_empty)
{
    HashSet s = int_set();
    EXPECT_EQ(HashSet_size(&s), 0u);
    EXPECT_TRUE(HashSet_empty(&s));

    for (int i = 0; i < 10; i++) {
        HashSet_insert(&s, &i);
    }
    EXPECT_EQ(HashSet_size(&s), 10u);
    EXPECT_FALSE(HashSet_empty(&s));
    HashSet_destroy(&s);
}

UTEST(hashset, size_tracks_inserts)
{
    HashSet s = int_set();
    for (int i = 0; i < 20; i++) {
        HashSet_insert(&s, &i);
    }
    EXPECT_EQ(HashSet_size(&s), 20u);
    HashSet_destroy(&s);
}

UTEST(hashset, resize_preserves_membership)
{
    HashSet s = int_set();
    for (int i = 0; i < 50; i++) {
        HashSet_insert(&s, &i);
    }
    EXPECT_EQ(HashSet_size(&s), 50u);
    for (int i = 0; i < 50; i++) {
        EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

// Robin Hood + backward-shift delete doesn't shrink — no LOAD_FACTOR_SHRINK.
// Verify correctness after heavy removal instead.
UTEST(hashset, remove_correctness_after_many_removes)
{
    HashSet s = int_set();
    for (int i = 0; i < 50; i++) {
        HashSet_insert(&s, &i);
    }
    for (int i = 0; i < 48; i++) {
        HashSet_remove(&s, &i);
    }
    EXPECT_EQ(HashSet_size(&s), 2u);

    for (int i = 48; i < 50; i++) {
        EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * delete correctness  (backward-shift, no tombstones)
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashset, remove_reinsert)
{
    // Remove an element then re-insert it — must succeed and be findable
    HashSet s = int_set();
    int     x = 42;
    HashSet_insert(&s, &x);
    HashSet_remove(&s, &x);
    EXPECT_FALSE(HashSet_has(&s, &x));

    HashSet_insert(&s, &x);
    EXPECT_TRUE(HashSet_has(&s, &x));
    EXPECT_EQ(HashSet_size(&s), 1u);
    HashSet_destroy(&s);
}

UTEST(hashset, remove_mid_chain)
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
        EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

UTEST(hashset, remove_reinsert_cycle)
{
    // Repeated remove+insert must not corrupt or leak
    HashSet s = int_set();
    int     x = 7;
    for (int cycle = 0; cycle < 20; cycle++) {
        HashSet_insert(&s, &x);
        EXPECT_TRUE(HashSet_has(&s, &x));
        HashSet_remove(&s, &x);
        EXPECT_EQ(HashSet_size(&s), 0u);
    }
    HashSet_destroy(&s);
}

UTEST(hashset, remove_first_in_chain)
{
    // Removing the head of a probe chain must leave the rest reachable
    HashSet s = int_set();
    for (int i = 0; i < 15; i++) {
        HashSet_insert(&s, &i);
    }
    int head = 0;
    HashSet_remove(&s, &head);
    EXPECT_FALSE(HashSet_has(&s, &head));

    for (int i = 1; i < 15; i++) {
        EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

UTEST(hashset, remove_all_then_reinsert)
{
    // Remove every element then reinsert — set must be fully functional
    HashSet s = int_set();
    for (int i = 0; i < 20; i++) {
        HashSet_insert(&s, &i);
    }
    for (int i = 0; i < 20; i++) {
        HashSet_remove(&s, &i);
    }
    EXPECT_EQ(HashSet_size(&s), 0u);
    EXPECT_TRUE(HashSet_empty(&s));

    for (int i = 0; i < 20; i++) {
        HashSet_insert(&s, &i);
    }
    EXPECT_EQ(HashSet_size(&s), 20u);
    for (int i = 0; i < 20; i++) {
        EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * HashSet_clear
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashset, clear_empties_set)
{
    HashSet s = int_set();
    for (int i = 0; i < 10; i++) {
        HashSet_insert(&s, &i);
    }
    u64 cap_before = HashSet_capacity(&s);
    HashSet_clear(&s);

    EXPECT_EQ(HashSet_size(&s), 0u);
    EXPECT_TRUE(HashSet_empty(&s));
    EXPECT_EQ(HashSet_capacity(&s), cap_before); // capacity unchanged
    for (int i = 0; i < 10; i++) {
        EXPECT_FALSE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

UTEST(hashset, clear_then_reuse)
{
    HashSet s = int_set();
    for (int i = 0; i < 10; i++) {
        HashSet_insert(&s, &i);
    }
    HashSet_clear(&s);

    for (int i = 100; i < 110; i++) {
        HashSet_insert(&s, &i);
    }
    EXPECT_EQ(HashSet_size(&s), 10u);
    for (int i = 100; i < 110; i++) {
        EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

UTEST(hashset, clear_frees_String_elms)
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
    EXPECT_EQ(HashSet_size(&s), 0u);

    // Set must still be usable after clearing owned-resource entries
    String v = String_from_cstr(WC_LIBC, "after_clear");
    HashSet_insert_move(&s, &v);
    String_destroy(&v); // safe on zeroed
    EXPECT_EQ(HashSet_size(&s), 1u);
    HashSet_destroy(&s);
}

UTEST(hashset, clear_empty_set)
{
    // clear on an already-empty set must be a safe no-op
    HashSet s = int_set();
    HashSet_clear(&s);
    EXPECT_EQ(HashSet_size(&s), 0u);
    HashSet_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * HashSet_copy
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashset, copy_int_set)
{
    HashSet src = int_set();
    for (int i = 0; i < 10; i++) {
        HashSet_insert(&src, &i);
    }

    HashSet dest = HashSet_copy(WC_LIBC, &src);
    EXPECT_EQ(HashSet_size(&dest), HashSet_size(&src));

    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(HashSet_has(&dest, &i));
    }

    HashSet_destroy(&src);
    HashSet_destroy(&dest);
}

UTEST(hashset, copy_independence)
{
    // Inserting into dest must not affect src
    HashSet src = int_set();
    int     x   = 1;
    HashSet_insert(&src, &x);

    HashSet dest = HashSet_copy(WC_LIBC, &src);

    int y = 99;
    HashSet_insert(&dest, &y);

    EXPECT_FALSE(HashSet_has(&src, &y)); // src unaffected
    EXPECT_TRUE(HashSet_has(&dest, &x)); // dest has original
    EXPECT_TRUE(HashSet_has(&dest, &y)); // dest has new

    HashSet_destroy(&src);
    HashSet_destroy(&dest);
}

UTEST(hashset, copy_str_set)
{
    // Deep copy: destroying src must not corrupt dest's String data
    HashSet     src     = str_set();
    const char* words[] = {"alpha", "beta", "gamma"};
    for (int i = 0; i < 3; i++) {
        String sv = String_from_cstr(WC_LIBC, words[i]);
        HashSet_insert(&src, &sv);
        String_destroy(&sv);
    }

    HashSet dest = HashSet_copy(WC_LIBC, &src);
    EXPECT_EQ(HashSet_size(&dest), 3u);

    HashSet_destroy(&src); // src gone — dest must still be intact

    String probe = String_from_cstr(WC_LIBC, "beta");
    EXPECT_TRUE(HashSet_has(&dest, &probe));
    String_destroy(&probe);

    HashSet_destroy(&dest);
}

UTEST(hashset, copy_empty_set)
{
    HashSet src = int_set();

    HashSet dest = HashSet_copy(WC_LIBC, &src);
    EXPECT_EQ(HashSet_size(&dest), 0u);
    EXPECT_EQ(HashSet_capacity(&dest), HashSet_capacity(&src));

    HashSet_destroy(&src);
    HashSet_destroy(&dest);
}

UTEST(hashset, copy_then_remove_src_elm)
{
    // Removing from src after copy must not affect dest
    HashSet src = int_set();
    int     x   = 5;
    HashSet_insert(&src, &x);

    HashSet dest = HashSet_copy(WC_LIBC, &src);

    HashSet_remove(&src, &x);
    EXPECT_FALSE(HashSet_has(&src, &x));
    EXPECT_TRUE(HashSet_has(&dest, &x));

    HashSet_destroy(&src);
    HashSet_destroy(&dest);
}


/* ════════════════════════════════════════════════════════════════════════════
 * String set  (owns heap memory)
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashset, str_insert_move_nulls_ptr)
{
    HashSet s  = str_set();
    String  s1 = String_from_cstr(WC_LIBC, "hello");
    HashSet_insert_move(&s, &s1);
    EXPECT_EQ(s1.size, 0u); // ownership transferred: source zeroed
    String_destroy(&s1);    // safe on zeroed

    String probe = String_from_cstr(WC_LIBC, "hello");
    EXPECT_TRUE(HashSet_has(&s, &probe));
    String_destroy(&probe);
    HashSet_destroy(&s);
}

UTEST(hashset, str_insert_copy_leaves_src_valid)
{
    // Source String must still be valid and unchanged after copy insert
    HashSet s  = str_set();
    String  s1 = String_from_cstr(WC_LIBC, "world");
    HashSet_insert(&s, &s1);

    EXPECT_TRUE(String_equals_cstr(&s1, "world"));
    String_destroy(&s1);
    HashSet_destroy(&s);
}

UTEST(hashset, str_insert_copy_independence)
{
    // Mutating source String after copy insert must not affect stored copy
    HashSet s  = str_set();
    String  sv = String_from_cstr(WC_LIBC, "original");
    HashSet_insert(&s, &sv);
    String_append_cstr(&sv, "_mutated");

    String probe = String_from_cstr(WC_LIBC, "original");
    EXPECT_TRUE(HashSet_has(&s, &probe));
    String_destroy(&probe);

    String_destroy(&sv);
    HashSet_destroy(&s);
}

UTEST(hashset, str_has_miss)
{
    HashSet s     = str_set();
    String  probe = String_from_cstr(WC_LIBC, "missing");
    EXPECT_FALSE(HashSet_has(&s, &probe));
    String_destroy(&probe);
    HashSet_destroy(&s);
}

UTEST(hashset, str_no_duplicates)
{
    HashSet s      = str_set();
    String  sv     = String_from_cstr(WC_LIBC, "dup");
    b8      first  = HashSet_insert(&s, &sv);
    b8      second = HashSet_insert(&s, &sv);
    EXPECT_FALSE(first);
    EXPECT_TRUE(second);
    EXPECT_EQ(HashSet_size(&s), 1u);
    String_destroy(&sv);
    HashSet_destroy(&s);
}

UTEST(hashset, str_insert_move_duplicate_frees_elm)
{
    // insert_move on a duplicate must free the incoming pointer
    HashSet s  = str_set();
    String  sv = String_from_cstr(WC_LIBC, "dup");
    HashSet_insert(&s, &sv);

    String dup     = String_from_cstr(WC_LIBC, "dup");
    b8     existed = HashSet_insert_move(&s, &dup);
    EXPECT_TRUE(existed);
    EXPECT_EQ(dup.size, 0u); // duplicate destroyed and zeroed
    EXPECT_TRUE((dup.heap) == NULL);
    String_destroy(&dup); // safe on zeroed
    EXPECT_EQ(HashSet_size(&s), 1u);

    String_destroy(&sv);
    HashSet_destroy(&s);
}

UTEST(hashset, str_remove)
{
    HashSet s  = str_set();
    String  s1 = String_from_cstr(WC_LIBC, "remove_me");
    HashSet_insert_move(&s, &s1);
    String_destroy(&s1); // moved-from: zeroed, safe on zeroed

    String probe = String_from_cstr(WC_LIBC, "remove_me");
    EXPECT_TRUE(HashSet_remove(&s, &probe));
    EXPECT_FALSE(HashSet_has(&s, &probe));
    EXPECT_EQ(HashSet_size(&s), 0u);
    String_destroy(&probe);
    HashSet_destroy(&s);
}

UTEST(hashset, str_resize_preserves_membership)
{
    HashSet s = str_set();
    char    buf[16];
    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "word%d", i);
        String sv = String_from_cstr(WC_LIBC, buf);
        HashSet_insert(&s, &sv);
        String_destroy(&sv);
    }
    EXPECT_EQ(HashSet_size(&s), 40u);

    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "word%d", i);
        String probe = String_from_cstr(WC_LIBC, buf);
        EXPECT_TRUE(HashSet_has(&s, &probe));
        String_destroy(&probe);
    }
    HashSet_destroy(&s);
}

UTEST(hashset, str_remove_frees_elm)
{
    // remove must call del_fn on owned String before clearing the slot
    HashSet s  = str_set();
    String  sv = String_from_cstr(WC_LIBC, "owned");
    HashSet_insert(&s, &sv);
    String_destroy(&sv);

    String probe = String_from_cstr(WC_LIBC, "owned");
    EXPECT_TRUE(HashSet_remove(&s, &probe));
    EXPECT_EQ(HashSet_size(&s), 0u);
    String_destroy(&probe);
    HashSet_destroy(&s);
}

UTEST(hashset, str_clear_then_reuse)
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
    EXPECT_EQ(HashSet_size(&s), 0u);

    // Usable after clear
    String v = String_from_cstr(WC_LIBC, "fresh");
    HashSet_insert_move(&s, &v);
    String_destroy(&v); // moved-from: zeroed, safe destroy
    EXPECT_EQ(HashSet_size(&s), 1u);

    String probe = String_from_cstr(WC_LIBC, "fresh");
    EXPECT_TRUE(HashSet_has(&s, &probe));
    String_destroy(&probe);
    HashSet_destroy(&s);
}

/* ── HashSet_insert_move ─────────────────────────────────────────────────── */

UTEST(hashset, insert_move_nulls_src)
{
    HashSet s       = HashSet_create(WC_LIBC, sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
    String  el      = String_from_cstr(WC_LIBC, "owned");
    b8      existed = HashSet_insert_move(&s, &el);
    String_destroy(&el); // moved-from: zeroed, safe destroy
    EXPECT_FALSE(existed);
    EXPECT_EQ(HashSet_size(&s), 1u);

    String k = String_from_cstr(WC_LIBC, "owned");
    EXPECT_TRUE(HashSet_has(&s, &k));
    String_destroy(&k);
    HashSet_destroy(&s);
}

UTEST(hashset, insert_move_duplicate_frees_incoming)
{
    HashSet s = HashSet_create(WC_LIBC, sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
    SET_INSERT_CSTR(&s, "dup");

    String el      = String_from_cstr(WC_LIBC, "dup");
    b8     existed = HashSet_insert_move(&s, &el);
    EXPECT_TRUE(existed);   /* already in set */
    EXPECT_EQ(el.size, 0u); /* incoming consumed and zeroed */
    String_destroy(&el);    /* safe on zeroed */
    EXPECT_EQ(HashSet_size(&s), 1u);
    HashSet_destroy(&s);
}


/* ── HashSet_copy ────────────────────────────────────────────────────────── */

UTEST(hashset, copy_str_set_deep)
{
    HashSet src = HashSet_create(WC_LIBC, sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
    SET_INSERT_CSTR(&src, "alpha");
    SET_INSERT_CSTR(&src, "beta");

    HashSet dest = HashSet_copy(WC_LIBC, &src);
    EXPECT_EQ(HashSet_size(&dest), 2u);

    HashSet_destroy(&src); /* src gone — dest must still be intact */

    String k = String_from_cstr(WC_LIBC, "alpha");
    EXPECT_TRUE(HashSet_has(&dest, &k));
    String_destroy(&k);
    HashSet_destroy(&dest);
}

UTEST(hashset, clear_empty_set_noop)
{
    HashSet s = HashSet_create(WC_LIBC, sizeof(int), NULL, NULL, NULL);
    HashSet_clear(&s);
    EXPECT_EQ(HashSet_size(&s), 0u);
    HashSet_destroy(&s);
}


/* ── SET_FOREACH macro ───────────────────────────────────────────────────── */

UTEST(hashset, set_foreach_visits_all)
{
    HashSet s = HashSet_create(WC_LIBC, sizeof(int), NULL, NULL, NULL);
    for (int i = 0; i < 8; i++) {
        HashSet_insert(&s, &i);
    }

    int count = 0, sum = 0;
    SET_FOREACH(&s, int, el)
    {
        count++;
        sum += *el;
    }
    EXPECT_EQ(count, 8);
    EXPECT_EQ(sum, 0 + 1 + 2 + 3 + 4 + 5 + 6 + 7);
    HashSet_destroy(&s);
}

UTEST(hashset, set_foreach_empty)
{
    HashSet s     = HashSet_create(WC_LIBC, sizeof(int), NULL, NULL, NULL);
    int     count = 0;
    SET_FOREACH(&s, int, el)
    {
        count++;
        (void)el;
    }
    EXPECT_EQ(count, 0);
    HashSet_destroy(&s);
}

UTEST(hashset, set_foreach_after_remove)
{
    HashSet s = HashSet_create(WC_LIBC, sizeof(int), NULL, NULL, NULL);
    for (int i = 0; i < 8; i++) {
        HashSet_insert(&s, &i);
    }
    for (int i = 0; i < 4; i++) {
        HashSet_remove(&s, &i);
    }

    int count = 0;
    SET_FOREACH(&s, int, el)
    {
        EXPECT_TRUE(*el >= 4);
        count++;
    }
    EXPECT_EQ(count, 4);
    HashSet_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * Phase 5 New Tests: resizes, cross-allocator copy, move, zero-state
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(hashset, fill_past_several_resizes)
{
    HashSet   s     = int_set();
    const int count = 2000;
    for (int i = 0; i < count; i++) {
        HashSet_insert(&s, &i);
    }
    EXPECT_EQ(HashSet_size(&s), (u64)count);

    for (int i = 0; i < count; i++) {
        EXPECT_TRUE(HashSet_has(&s, &i));
    }
    HashSet_destroy(&s);
}

UTEST(hashset, cross_alloc_copy)
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
    EXPECT_EQ(HashSet_size(&dest), 20u);

    // Destroy src and arena, dest must remain valid in libc
    HashSet_destroy(&src);
    Arena_destroy(&a);

    for (int i = 0; i < 20; i++) {
        EXPECT_TRUE(HashSet_has(&dest, &i));
    }
    HashSet_destroy(&dest);
}

UTEST(hashset, move)
{
    HashSet src = int_set();
    int     x   = 42;
    HashSet_insert(&src, &x);

    HashSet dest;
    HashSet_move(&dest, &src);

    EXPECT_EQ(src.capacity, 0u);
    EXPECT_EQ(HashSet_size(&dest), 1u);
    EXPECT_TRUE(HashSet_has(&dest, &x));

    HashSet_destroy(&src); // safe on zeroed
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

UTEST(hashset, zero_state_fatal)
{
    EXPECT_DIES(die_mutating_zero_hashset);
}
#endif
