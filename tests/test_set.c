#include "arena.h"
#include "common.h"
#include "hashmap.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_macros.h"
#include "wc_string.h"

#include <stdio.h>


/* ── Set constructors ────────────────────────────────────────────────────── */

static HashMap int_set(void)
{
    return HashMap_create(WC_LIBC, sizeof(int), 0, NULL, NULL, NULL, NULL);
}

static HashMap str_set(void)
{
    // StrView keys: the set stores views; the bytes must outlive the set.
    return HashMap_create(WC_LIBC, sizeof(StrView), 0, wc_strview_hash, wc_strview_cmp, NULL, NULL);
}


/* ════════════════════════════════════════════════════════════════════════════
 * int set  (POD, no copy/move/del)
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(set, insert_and_has)
{
    HashMap s = int_set();
    int     x = 42;
    EXPECT_FALSE(HashMap_has(&s, &x));
    HashMap_put(&s, &x, NULL);
    EXPECT_TRUE(HashMap_has(&s, &x));
    HashMap_destroy(&s);
}

UTEST(set, insert_returns_existed)
{
    HashMap s      = int_set();
    int     x      = 5;
    bool      first  = HashMap_put(&s, &x, NULL);
    bool      second = HashMap_put(&s, &x, NULL);
    EXPECT_FALSE(first); // new insert
    EXPECT_TRUE(second); // already existed
    HashMap_destroy(&s);
}

UTEST(set, insert_duplicate_no_growth)
{
    HashMap s = int_set();
    int     x = 10;
    HashMap_put(&s, &x, NULL);
    HashMap_put(&s, &x, NULL);
    HashMap_put(&s, &x, NULL);
    EXPECT_EQ(HashMap_size(&s), 1u);
    HashMap_destroy(&s);
}

UTEST(set, has_missing_returns_false)
{
    HashMap s = int_set();
    int     x = 999;
    EXPECT_FALSE(HashMap_has(&s, &x));
    HashMap_destroy(&s);
}

UTEST(set, remove)
{
    HashMap s = int_set();
    int     x = 7;
    HashMap_put(&s, &x, NULL);
    EXPECT_TRUE(HashMap_del(&s, &x, NULL));
    EXPECT_FALSE(HashMap_has(&s, &x));
    EXPECT_EQ(HashMap_size(&s), 0u);
    HashMap_destroy(&s);
}

UTEST(set, remove_missing_returns_false)
{
    HashMap s = int_set();
    int     x = 999;
    EXPECT_FALSE(HashMap_del(&s, &x, NULL));
    HashMap_destroy(&s);
}

UTEST(set, remove_on_empty_set)
{
    HashMap s = int_set();
    int     x = 1;
    EXPECT_FALSE(HashMap_del(&s, &x, NULL));
    EXPECT_EQ(HashMap_size(&s), 0u);
    HashMap_destroy(&s);
}

UTEST(set, size_and_empty)
{
    HashMap s = int_set();
    EXPECT_EQ(HashMap_size(&s), 0u);
    EXPECT_TRUE(HashMap_empty(&s));

    for (int i = 0; i < 10; i++) {
        HashMap_put(&s, &i, NULL);
    }
    EXPECT_EQ(HashMap_size(&s), 10u);
    EXPECT_FALSE(HashMap_empty(&s));
    HashMap_destroy(&s);
}

UTEST(set, size_tracks_inserts)
{
    HashMap s = int_set();
    for (int i = 0; i < 20; i++) {
        HashMap_put(&s, &i, NULL);
    }
    EXPECT_EQ(HashMap_size(&s), 20u);
    HashMap_destroy(&s);
}

UTEST(set, resize_preserves_membership)
{
    HashMap s = int_set();
    for (int i = 0; i < 50; i++) {
        HashMap_put(&s, &i, NULL);
    }
    EXPECT_EQ(HashMap_size(&s), 50u);
    for (int i = 0; i < 50; i++) {
        EXPECT_TRUE(HashMap_has(&s, &i));
    }
    HashMap_destroy(&s);
}

// Robin Hood + backward-shift delete doesn't shrink — no LOAD_FACTOR_SHRINK.
// Verify correctness after heavy removal instead.
UTEST(set, remove_correctness_after_many_removes)
{
    HashMap s = int_set();
    for (int i = 0; i < 50; i++) {
        HashMap_put(&s, &i, NULL);
    }
    for (int i = 0; i < 48; i++) {
        HashMap_del(&s, &i, NULL);
    }
    EXPECT_EQ(HashMap_size(&s), 2u);

    for (int i = 48; i < 50; i++) {
        EXPECT_TRUE(HashMap_has(&s, &i));
    }
    HashMap_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * delete correctness  (backward-shift, no tombstones)
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(set, remove_reinsert)
{
    // Remove an element then re-insert it — must succeed and be findable
    HashMap s = int_set();
    int     x = 42;
    HashMap_put(&s, &x, NULL);
    HashMap_del(&s, &x, NULL);
    EXPECT_FALSE(HashMap_has(&s, &x));

    HashMap_put(&s, &x, NULL);
    EXPECT_TRUE(HashMap_has(&s, &x));
    EXPECT_EQ(HashMap_size(&s), 1u);
    HashMap_destroy(&s);
}

UTEST(set, remove_mid_chain)
{
    // Insert elements that chain during probing, remove one mid-chain,
    // then verify all remaining elements are still reachable.
    HashMap s = int_set();
    for (int i = 0; i < 20; i++) {
        HashMap_put(&s, &i, NULL);
    }

    int mid = 5;
    HashMap_del(&s, &mid, NULL);

    for (int i = 0; i < 20; i++) {
        if (i == mid) {
            continue;
        }
        EXPECT_TRUE(HashMap_has(&s, &i));
    }
    HashMap_destroy(&s);
}

UTEST(set, remove_reinsert_cycle)
{
    // Repeated remove+insert must not corrupt or leak
    HashMap s = int_set();
    int     x = 7;
    for (int cycle = 0; cycle < 20; cycle++) {
        HashMap_put(&s, &x, NULL);
        EXPECT_TRUE(HashMap_has(&s, &x));
        HashMap_del(&s, &x, NULL);
        EXPECT_EQ(HashMap_size(&s), 0u);
    }
    HashMap_destroy(&s);
}

UTEST(set, remove_first_in_chain)
{
    // Removing the head of a probe chain must leave the rest reachable
    HashMap s = int_set();
    for (int i = 0; i < 15; i++) {
        HashMap_put(&s, &i, NULL);
    }
    int head = 0;
    HashMap_del(&s, &head, NULL);
    EXPECT_FALSE(HashMap_has(&s, &head));

    for (int i = 1; i < 15; i++) {
        EXPECT_TRUE(HashMap_has(&s, &i));
    }
    HashMap_destroy(&s);
}

UTEST(set, remove_all_then_reinsert)
{
    // Remove every element then reinsert — set must be fully functional
    HashMap s = int_set();
    for (int i = 0; i < 20; i++) {
        HashMap_put(&s, &i, NULL);
    }
    for (int i = 0; i < 20; i++) {
        HashMap_del(&s, &i, NULL);
    }
    EXPECT_EQ(HashMap_size(&s), 0u);
    EXPECT_TRUE(HashMap_empty(&s));

    for (int i = 0; i < 20; i++) {
        HashMap_put(&s, &i, NULL);
    }
    EXPECT_EQ(HashMap_size(&s), 20u);
    for (int i = 0; i < 20; i++) {
        EXPECT_TRUE(HashMap_has(&s, &i));
    }
    HashMap_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * Set clear
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(set, clear_empties_set)
{
    HashMap s = int_set();
    for (int i = 0; i < 10; i++) {
        HashMap_put(&s, &i, NULL);
    }
    u64 cap_before = HashMap_capacity(&s);
    HashMap_clear(&s);

    EXPECT_EQ(HashMap_size(&s), 0u);
    EXPECT_TRUE(HashMap_empty(&s));
    EXPECT_EQ(HashMap_capacity(&s), cap_before); // capacity unchanged
    for (int i = 0; i < 10; i++) {
        EXPECT_FALSE(HashMap_has(&s, &i));
    }
    HashMap_destroy(&s);
}

UTEST(set, clear_then_reuse)
{
    HashMap s = int_set();
    for (int i = 0; i < 10; i++) {
        HashMap_put(&s, &i, NULL);
    }
    HashMap_clear(&s);

    for (int i = 100; i < 110; i++) {
        HashMap_put(&s, &i, NULL);
    }
    EXPECT_EQ(HashMap_size(&s), 10u);
    for (int i = 100; i < 110; i++) {
        EXPECT_TRUE(HashMap_has(&s, &i));
    }
    HashMap_destroy(&s);
}

UTEST(set, clear_leaves_store_valid)
{
    // Clearing a view-keyed set does not free the store that owns the bytes.
    StringStore ss;
    StringStore_create(&ss, WC_LIBC, nKB(4), true);
    HashMap s = str_set();
    for (int i = 0; i < 5; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "str%d", i);
        StrView v = StringStore_cstr(&ss, buf);
        HashMap_put(&s, &v, NULL);
    }
    HashMap_clear(&s);
    EXPECT_EQ(HashMap_size(&s), 0u);

    StrView v = StringStore_cstr(&ss, "after_clear");
    HashMap_put(&s, &v, NULL);
    EXPECT_EQ(HashMap_size(&s), 1u);
    EXPECT_TRUE(SET_HAS_CSTR(&s, "after_clear"));
    HashMap_destroy(&s);
    StringStore_destroy(&ss);
}

UTEST(set, clear_empty_set)
{
    // clear on an already-empty set must be a safe no-op
    HashMap s = int_set();
    HashMap_clear(&s);
    EXPECT_EQ(HashMap_size(&s), 0u);
    HashMap_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * Set copy
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(set, copy_int_set)
{
    HashMap src = int_set();
    for (int i = 0; i < 10; i++) {
        HashMap_put(&src, &i, NULL);
    }

    HashMap dest = HashMap_copy(WC_LIBC, &src);
    EXPECT_EQ(HashMap_size(&dest), HashMap_size(&src));

    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(HashMap_has(&dest, &i));
    }

    HashMap_destroy(&src);
    HashMap_destroy(&dest);
}

UTEST(set, copy_independence)
{
    // Inserting into dest must not affect src
    HashMap src = int_set();
    int     x   = 1;
    HashMap_put(&src, &x, NULL);

    HashMap dest = HashMap_copy(WC_LIBC, &src);

    int y = 99;
    HashMap_put(&dest, &y, NULL);

    EXPECT_FALSE(HashMap_has(&src, &y)); // src unaffected
    EXPECT_TRUE(HashMap_has(&dest, &x)); // dest has original
    EXPECT_TRUE(HashMap_has(&dest, &y)); // dest has new

    HashMap_destroy(&src);
    HashMap_destroy(&dest);
}

UTEST(set, copy_str_set)
{
    // HashMap_copy copies the 16-byte views. Literal key bytes outlive both
    // sets, so destroying src leaves dest intact.
    HashMap     src     = str_set();
    const char* words[] = {"alpha", "beta", "gamma"};
    for (int i = 0; i < 3; i++) {
        StrView sv = StrView_over_cstr(words[i]);
        HashMap_put(&src, &sv, NULL);
    }

    HashMap dest = HashMap_copy(WC_LIBC, &src);
    EXPECT_EQ(HashMap_size(&dest), 3u);

    HashMap_destroy(&src); // src gone — dest still points at the literals

    EXPECT_TRUE(SET_HAS_CSTR(&dest, "beta"));

    HashMap_destroy(&dest);
}

UTEST(set, copy_empty_set)
{
    HashMap src = int_set();

    HashMap dest = HashMap_copy(WC_LIBC, &src);
    EXPECT_EQ(HashMap_size(&dest), 0u);
    EXPECT_EQ(HashMap_capacity(&dest), HashMap_capacity(&src));

    HashMap_destroy(&src);
    HashMap_destroy(&dest);
}

UTEST(set, copy_then_remove_src_elm)
{
    // Removing from src after copy must not affect dest
    HashMap src = int_set();
    int     x   = 5;
    HashMap_put(&src, &x, NULL);

    HashMap dest = HashMap_copy(WC_LIBC, &src);

    HashMap_del(&src, &x, NULL);
    EXPECT_FALSE(HashMap_has(&src, &x));
    EXPECT_TRUE(HashMap_has(&dest, &x));

    HashMap_destroy(&src);
    HashMap_destroy(&dest);
}


/* ════════════════════════════════════════════════════════════════════════════
 * StrView set  (view keys; bytes live elsewhere)
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(set, str_insert_view)
{
    HashMap s = str_set();
    StrView v = StrView_over_cstr("hello");
    EXPECT_FALSE(HashMap_put(&s, &v, NULL));
    EXPECT_TRUE(SET_HAS_CSTR(&s, "hello"));
    EXPECT_EQ(HashMap_size(&s), 1u);
    HashMap_destroy(&s);
}

UTEST(set, str_has_miss)
{
    HashMap s = str_set();
    EXPECT_FALSE(SET_HAS_CSTR(&s, "missing"));
    HashMap_destroy(&s);
}

UTEST(set, str_no_duplicates)
{
    HashMap s      = str_set();
    StrView sv     = StrView_over_cstr("dup");
    bool    first  = HashMap_put(&s, &sv, NULL);
    bool    second = HashMap_put(&s, &sv, NULL);
    EXPECT_FALSE(first);
    EXPECT_TRUE(second);
    EXPECT_EQ(HashMap_size(&s), 1u);
    HashMap_destroy(&s);
}

UTEST(set, str_same_content_different_views)
{
    // Content, not the pointer, determines membership.
    HashMap s   = str_set();
    char    a[] = "dup";
    char    b[] = "dup";
    StrView va  = {.data = a, .len = 3};
    StrView vb  = {.data = b, .len = 3};
    EXPECT_FALSE(HashMap_put(&s, &va, NULL));
    EXPECT_TRUE(HashMap_put(&s, &vb, NULL));
    EXPECT_EQ(HashMap_size(&s), 1u);
    HashMap_destroy(&s);
}

UTEST(set, str_remove)
{
    HashMap s = str_set();
    StrView v = StrView_over_cstr("remove_me");
    HashMap_put(&s, &v, NULL);

    EXPECT_TRUE(HashMap_del(&s, &v, NULL));
    EXPECT_FALSE(SET_HAS_CSTR(&s, "remove_me"));
    EXPECT_EQ(HashMap_size(&s), 0u);
    HashMap_destroy(&s);
}

UTEST(set, str_resize_preserves_membership)
{
    // Dynamic keys must be interned: the set stores only a view.
    StringStore ss;
    StringStore_create(&ss, WC_LIBC, nKB(4), true);
    HashMap s = str_set();
    char    buf[16];
    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "word%d", i);
        StrView sv = StringStore_cstr(&ss, buf);
        HashMap_put(&s, &sv, NULL);
    }
    EXPECT_EQ(HashMap_size(&s), 40u);

    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "word%d", i);
        StrView probe = StringStore_cstr(&ss, buf);
        EXPECT_TRUE(HashMap_has(&s, &probe));
    }
    HashMap_destroy(&s);
    StringStore_destroy(&ss);
}

UTEST(set, str_clear_then_reuse)
{
    StringStore ss;
    StringStore_create(&ss, WC_LIBC, nKB(4), true);
    HashMap s = str_set();
    for (int i = 0; i < 8; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "item%d", i);
        StrView sv = StringStore_cstr(&ss, buf);
        HashMap_put(&s, &sv, NULL);
    }
    HashMap_clear(&s);
    EXPECT_EQ(HashMap_size(&s), 0u);

    // Usable after clear; the store still owns the bytes.
    StrView v = StringStore_cstr(&ss, "fresh");
    HashMap_put(&s, &v, NULL);
    EXPECT_EQ(HashMap_size(&s), 1u);
    EXPECT_TRUE(SET_HAS_CSTR(&s, "fresh"));
    HashMap_destroy(&s);
    StringStore_destroy(&ss);
}

UTEST(set, clear_empty_set_noop)
{
    HashMap s = HashMap_create(WC_LIBC, sizeof(int), 0, NULL, NULL, NULL, NULL);
    HashMap_clear(&s);
    EXPECT_EQ(HashMap_size(&s), 0u);
    HashMap_destroy(&s);
}


/* ── SET_FOREACH macro ───────────────────────────────────────────────────── */

UTEST(set, set_foreach_visits_all)
{
    HashMap s = HashMap_create(WC_LIBC, sizeof(int), 0, NULL, NULL, NULL, NULL);
    for (int i = 0; i < 8; i++) {
        HashMap_put(&s, &i, NULL);
    }

    int count = 0, sum = 0;
    SET_FOREACH (&s, int, el) {
        count++;
        sum += *el;
    }
    EXPECT_EQ(count, 8);
    EXPECT_EQ(sum, 0 + 1 + 2 + 3 + 4 + 5 + 6 + 7);
    HashMap_destroy(&s);
}

UTEST(set, set_foreach_empty)
{
    HashMap s     = HashMap_create(WC_LIBC, sizeof(int), 0, NULL, NULL, NULL, NULL);
    int     count = 0;
    SET_FOREACH (&s, int, el) {
        count++;
        (void)el;
    }
    EXPECT_EQ(count, 0);
    HashMap_destroy(&s);
}

UTEST(set, set_foreach_after_remove)
{
    HashMap s = HashMap_create(WC_LIBC, sizeof(int), 0, NULL, NULL, NULL, NULL);
    for (int i = 0; i < 8; i++) {
        HashMap_put(&s, &i, NULL);
    }
    for (int i = 0; i < 4; i++) {
        HashMap_del(&s, &i, NULL);
    }

    int count = 0;
    SET_FOREACH (&s, int, el) {
        EXPECT_TRUE(*el >= 4);
        count++;
    }
    EXPECT_EQ(count, 4);
    HashMap_destroy(&s);
}


/* ════════════════════════════════════════════════════════════════════════════
 * Phase 5 New Tests: resizes, cross-allocator copy, move, zero-state
 * ════════════════════════════════════════════════════════════════════════════ */

UTEST(set, fill_past_several_resizes)
{
    HashMap   s     = int_set();
    const int count = 2000;
    for (int i = 0; i < count; i++) {
        HashMap_put(&s, &i, NULL);
    }
    EXPECT_EQ(HashMap_size(&s), (u64)count);

    for (int i = 0; i < count; i++) {
        EXPECT_TRUE(HashMap_has(&s, &i));
    }
    HashMap_destroy(&s);
}

UTEST(set, cross_alloc_copy)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(64));
    const wc_allocator* al = Arena_allocator(&a);

    HashMap src = HashMap_create(al, sizeof(int), 0, NULL, NULL, NULL, NULL);
    for (int i = 0; i < 20; i++) {
        HashMap_put(&src, &i, NULL);
    }

    // Copy from Arena to libc
    HashMap dest = HashMap_copy(WC_LIBC, &src);
    EXPECT_EQ(HashMap_size(&dest), 20u);

    // Destroy src and arena, dest must remain valid in libc
    HashMap_destroy(&src);
    Arena_destroy(&a);

    for (int i = 0; i < 20; i++) {
        EXPECT_TRUE(HashMap_has(&dest, &i));
    }
    HashMap_destroy(&dest);
}

UTEST(set, move)
{
    HashMap src = int_set();
    int     x   = 42;
    HashMap_put(&src, &x, NULL);

    HashMap dest = {0};
    HashMap_move(&dest, &src);

    EXPECT_EQ(src.capacity, 0u);
    EXPECT_EQ(HashMap_size(&dest), 1u);
    EXPECT_TRUE(HashMap_has(&dest, &x));

    HashMap_destroy(&src); // safe on zeroed
    HashMap_destroy(&dest);
}
