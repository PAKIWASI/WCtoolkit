#include "arena.h"
#include "common.h"
#include "test_support.h"
#include "utest.h"
#include "views.h"
#include "wc_allocator.h"
#include "wc_string.h"
#include "wc_test_allocator.h"

#include <stdlib.h>
#include <string.h>


// Helpers

// Content equality of a StrView against a cstr
static int sv_equals_cstr(StrView sv, const char* cstr)
{
    u64 clen = strlen(cstr);
    if (sv.len != clen) {
        return 0;
    }
    return memcmp(sv.ptr, cstr, clen) == 0;
}


// Small Strings share one node until it's full

UTEST(views, small_Strings_share_node)
{
    StringStore ss = StringStore_create(WC_LIBC);

    StrView a = StringStore_cstr(&ss, "hello", 5);
    StrView b = StringStore_cstr(&ss, "world", 5);

    EXPECT_TRUE(sv_equals_cstr(a, "hello"));
    EXPECT_TRUE(sv_equals_cstr(b, "world"));
    EXPECT_EQ(ss.num, 1u); // both fit in the initial node
    EXPECT_EQ(ss.tail_off, 10u);

    StringStore_destroy(&ss);
}


// A String exactly filling a node must not spill into a new node

UTEST(views, exact_fit_no_new_node)
{
    StringStore ss = StringStore_create(WC_LIBC);

    char big[StringStore_NODE_SIZE];
    memset(big, 'A', sizeof(big));

    StrView v = StringStore_cstr(&ss, big, StringStore_NODE_SIZE);

    EXPECT_EQ(v.len, (u64)(StringStore_NODE_SIZE));
    EXPECT_TRUE(v.ptr[0] == 'A');
    EXPECT_EQ(ss.num, 1u); // fits exactly, no extra node
    EXPECT_EQ(ss.tail_off, (u64)(StringStore_NODE_SIZE));

    // next String must go to a fresh node, not overflow the full one
    StrView s = StringStore_cstr(&ss, "hi", 2);
    EXPECT_TRUE(sv_equals_cstr(s, "hi"));
    EXPECT_TRUE(s.ptr != v.ptr);
    EXPECT_EQ(ss.num, 2u);

    StringStore_destroy(&ss);
}


// One byte over the limit goes to a heap-owned overflow node
// (pre-fix this silently overflowed the fixed-size node buffer — UB)

UTEST(views, overflow_by_one_goes_to_heap)
{
    StringStore ss = StringStore_create(WC_LIBC);

    char big[StringStore_NODE_SIZE + 1];
    memset(big, 'B', sizeof(big));

    StrView v = StringStore_cstr(&ss, big, StringStore_NODE_SIZE + 1);

    EXPECT_EQ(v.len, (u64)(StringStore_NODE_SIZE + 1));
    EXPECT_TRUE(v.ptr[0] == 'B');
    EXPECT_TRUE(v.ptr[StringStore_NODE_SIZE] == 'B');
    EXPECT_EQ(ss.num, 3u); // initial node + overflow node + fresh tail
    // The overflow node is pushed at the head and owns a heap buffer,
    // so its flag byte is 0 (heap active) and the view points into it.
    EXPECT_TRUE(ss.head->buf[StringStore_NODE_SIZE] == 0);
    EXPECT_TRUE(v.ptr == ss.head->heap);

    StringStore_destroy(&ss); // must free the heap buffer too
}


// Way larger than a node

UTEST(views, overflow_way_past_node)
{
    StringStore ss = StringStore_create(WC_LIBC);

    u64   big_len = (StringStore_NODE_SIZE * 3) + 7;
    char* big     = malloc(big_len);
    EXPECT_TRUE((big) != NULL);
    memset(big, 'C', big_len);

    StrView v = StringStore_cstr(&ss, big, big_len);

    EXPECT_EQ(v.len, big_len);
    EXPECT_TRUE(v.ptr[0] == 'C');
    EXPECT_TRUE(v.ptr[big_len - 1] == 'C');
    EXPECT_EQ(ss.num, 3u);
    EXPECT_EQ(ss.tail_off, 0u);

    free(big);
    StringStore_destroy(&ss);
}


// Many overflows: views must stay intact and must not alias each other

UTEST(views, multiple_overflows_keep_content)
{
    StringStore ss = StringStore_create(WC_LIBC);

    enum { OVER_N = 4, OVER_LEN = StringStore_NODE_SIZE + 5 };
    char buf[OVER_LEN];
    memset(buf, 'D', sizeof(buf));

    StrView views[OVER_N];
    for (int i = 0; i < OVER_N; i++) {
        views[i] = StringStore_cstr(&ss, buf, OVER_LEN);
    }

    for (int i = 0; i < OVER_N; i++) {
        EXPECT_EQ(views[i].len, (u64)(OVER_LEN));
        EXPECT_TRUE(views[i].ptr[0] == 'D');
        EXPECT_TRUE(views[i].ptr[OVER_LEN - 1] == 'D');
    }
    for (int i = 0; i < OVER_N; i++) {
        for (int j = i + 1; j < OVER_N; j++) {
            EXPECT_TRUE(views[i].ptr != views[j].ptr);
        }
    }

    // each overflow adds its own node + a fresh tail
    EXPECT_EQ(ss.num, (u64)(1 + (2 * OVER_N)));

    StringStore_destroy(&ss);
}


// Small Strings after an overflow land in the fresh tail node

UTEST(views, small_after_overflow_appends_to_tail)
{
    StringStore ss = StringStore_create(WC_LIBC);

    char big[StringStore_NODE_SIZE + 3];
    memset(big, 'E', sizeof(big));
    (void)StringStore_cstr(&ss, big, sizeof(big));

    StrView small = StringStore_cstr(&ss, "tail", 4);
    EXPECT_TRUE(sv_equals_cstr(small, "tail"));
    EXPECT_TRUE(small.ptr != ss.head->heap); // in a node buf, not the overflow heap buffer
    EXPECT_EQ(ss.tail_off, 4u);

    StringStore_destroy(&ss);
}


// Destroy must tolerate overflow nodes anywhere in the chain
// (ASan catches double-free / mismatched-free if the union is mishandled)

UTEST(views, destroy_mixed_chain)
{
    StringStore ss = StringStore_create(WC_LIBC);

    (void)StringStore_cstr(&ss, "one", 3);

    char big[StringStore_NODE_SIZE + 9];
    memset(big, 'F', sizeof(big));
    (void)StringStore_cstr(&ss, big, sizeof(big));

    (void)StringStore_cstr(&ss, "two", 3);
    (void)StringStore_cstr(&ss, big, sizeof(big));
    (void)StringStore_cstr(&ss, "three", 5);

    EXPECT_EQ(ss.num, 5u); // 1 + 2 per overflow

    StringStore_destroy(&ss);
}


// StrView_from_String sanity (same header)

UTEST(views, strview_from_String)
{
    String  s  = String_from_cstr(WC_LIBC, "viewme");
    StrView sv = StrView_from_String(&s);
    EXPECT_TRUE(sv_equals_cstr(sv, "viewme"));
    EXPECT_EQ(sv.len, 6u);
    String_destroy(&s);
}


// StrView_from_cstr never allocates; StrView_copy_cstr owns a terminated copy

UTEST(views, strview_from_cstr_borrows)
{
    const char* text = "borrowed";
    StrView     sv   = StrView_from_cstr(text, 3);
    EXPECT_TRUE(sv.ptr == text);
    EXPECT_TRUE(sv_equals_cstr(sv, "bor"));
}

UTEST(views, strview_copy_cstr_terminates_and_frees)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator al = wc_test_alloc_allocator(&ta);

    // clen shorter than the string: the old Arena path copied clen + 1 bytes
    // and left cstr[clen] ('l') where the terminator should be
    StrView sv = StrView_copy_cstr(al, "hello", 3);
    EXPECT_TRUE(sv_equals_cstr(sv, "hel"));
    EXPECT_TRUE(sv.ptr[3] == '\0');
    EXPECT_EQ(ta.live_blocks, 1u);

    StrView_free_copy(al, sv);
    EXPECT_EQ(ta.n_errors, 0u); // size/align matched the alloc
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, strview_copy_cstr_on_arena)
{
    Arena arena;
    Arena_create(&arena, WC_LIBC, 256);
    StrView sv = StrView_copy_cstr(Arena_allocator(&arena), "arena", 5);
    EXPECT_TRUE((const u8*)sv.ptr >= arena.base && (const u8*)sv.ptr < arena.base + arena.size);
    EXPECT_TRUE(sv_equals_cstr(sv, "arena"));
    Arena_destroy(&arena); // view's lifetime ends here
}


// StringStore through the allocator

UTEST(views, StringStore_test_allocator_leak_free)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator al = wc_test_alloc_allocator(&ta);

    StringStore ss = StringStore_create(al);
    (void)StringStore_cstr(&ss, "small", 5);
    char big[StringStore_NODE_SIZE + 40];
    memset(big, 'G', sizeof(big));
    StrView v = StringStore_cstr(&ss, big, sizeof(big)); // overflow: node + heap buffer
    EXPECT_TRUE(wc_test_alloc_owns(&ta, v.ptr));
    // 3 nodes (initial, overflow, fresh tail) + the overflow buffer
    EXPECT_EQ(ta.live_blocks, 4u);

    StringStore_destroy(&ss);
    EXPECT_TRUE((ss.head) == NULL);
    EXPECT_EQ(ta.n_errors, 0u); // heap_len recorded correctly for the free
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, StringStore_on_arena)
{
    Arena arena;
    Arena_create(&arena, WC_LIBC, nKB(8));
    StringStore ss = StringStore_create(Arena_allocator(&arena));
    StrView     a  = StringStore_cstr(&ss, "inside", 6);
    EXPECT_TRUE((const u8*)a.ptr >= arena.base && (const u8*)a.ptr < arena.base + arena.size);
    EXPECT_TRUE(sv_equals_cstr(a, "inside"));
    StringStore_destroy(&ss);
    Arena_destroy(&arena);
}

UTEST(views, StringStore_destroy_zeroed_is_safe)
{
    StringStore z;
    memset(&z, 0, sizeof(z));
    StringStore_destroy(&z);
    StringStore_destroy(&z);
    EXPECT_TRUE((z.head) == NULL);
}

static void cstr_after_destroy(void)
{
    StringStore ss = StringStore_create(WC_LIBC);
    StringStore_destroy(&ss);
    (void)StringStore_cstr(&ss, "dead", 4);
}

static void create_on_exhausted_arena(void)
{
    Arena arena;
    Arena_create(&arena, WC_LIBC, 64); // smaller than one node
    StringStore ss = StringStore_create(Arena_allocator(&arena));
    (void)ss;
}

UTEST(views, StringStore_zero_state_and_oom_die)
{
    EXPECT_DIES(cstr_after_destroy);
    EXPECT_DIES(create_on_exhausted_arena);
}


// Suite


UTEST(views, StringStore_append_joins_views)
{
    StringStore ss    = StringStore_create(WC_LIBC);
    StrView     path  = StringStore_cstr(&ss, "usr/", 4);
    StrView     full  = StringStore_append_cstr(&ss, path, "lib", 3);
    StrView     twice = StringStore_append(&ss, full, full); // both views point into the store

    EXPECT_TRUE(sv_equals_cstr(full, "usr/lib"));
    EXPECT_TRUE(sv_equals_cstr(twice, "usr/libusr/lib"));
    EXPECT_TRUE(sv_equals_cstr(path, "usr/")); // earlier views are untouched
    StringStore_destroy(&ss);
}

UTEST(views, StringStore_append_overflow_node)
{
    StringStore ss = StringStore_create(WC_LIBC);
    char        big[StringStore_NODE_SIZE];
    memset(big, 'z', sizeof(big));
    StrView a = StringStore_cstr(&ss, big, sizeof(big));
    StrView j = StringStore_append_cstr(&ss, a, "!", 1); // longer than a node: overflow path

    EXPECT_EQ(j.len, (u64)StringStore_NODE_SIZE + 1);
    EXPECT_EQ(j.ptr[0], 'z');
    EXPECT_EQ(j.ptr[StringStore_NODE_SIZE], '!');
    StringStore_destroy(&ss);
}
