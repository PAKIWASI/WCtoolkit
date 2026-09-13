#include "views.h"
#include "wc_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <String.h>


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

static void test_small_Strings_share_node(void)
{
    StringStore ss;
    StringStore_create(&ss);

    StrView a = StringStore_cstr(&ss, "hello", 5);
    StrView b = StringStore_cstr(&ss, "world", 5);

    WC_ASSERT_TRUE(sv_equals_cstr(a, "hello"));
    WC_ASSERT_TRUE(sv_equals_cstr(b, "world"));
    WC_ASSERT_EQ_U64(ss.num, 1); // both fit in the initial node
    WC_ASSERT_EQ_U64(ss.tail_off, 10);

    StringStore_destroy(&ss);
}


// A String exactly filling a node must not spill into a new node 

static void test_exact_fit_no_new_node(void)
{
    StringStore ss;
    StringStore_create(&ss);

    char big[StringStore_NODE_SIZE];
    memset(big, 'A', sizeof(big));

    StrView v = StringStore_cstr(&ss, big, StringStore_NODE_SIZE);

    WC_ASSERT_EQ_U64(v.len, StringStore_NODE_SIZE);
    WC_ASSERT(v.ptr[0] == 'A');
    WC_ASSERT_EQ_U64(ss.num, 1); // fits exactly, no extra node
    WC_ASSERT_EQ_U64(ss.tail_off, StringStore_NODE_SIZE);

    // next String must go to a fresh node, not overflow the full one
    StrView s = StringStore_cstr(&ss, "hi", 2);
    WC_ASSERT_TRUE(sv_equals_cstr(s, "hi"));
    WC_ASSERT(s.ptr != v.ptr);
    WC_ASSERT_EQ_U64(ss.num, 2);

    StringStore_destroy(&ss);
}


// One byte over the limit goes to a heap-owned overflow node 
// (pre-fix this silently overflowed the fixed-size node buffer — UB)

static void test_overflow_by_one_goes_to_heap(void)
{
    StringStore ss;
    StringStore_create(&ss);

    char big[StringStore_NODE_SIZE + 1];
    memset(big, 'B', sizeof(big));

    StrView v = StringStore_cstr(&ss, big, StringStore_NODE_SIZE + 1);

    WC_ASSERT_EQ_U64(v.len, StringStore_NODE_SIZE + 1);
    WC_ASSERT(v.ptr[0] == 'B');
    WC_ASSERT(v.ptr[StringStore_NODE_SIZE] == 'B');
    WC_ASSERT_EQ_U64(ss.num, 3); // initial node + overflow node + fresh tail
    WC_ASSERT(ss.head->owns_heap);

    StringStore_destroy(&ss); // must free the heap buffer too
}


// Way larger than a node 

static void test_overflow_way_past_node(void)
{
    StringStore ss;
    StringStore_create(&ss);

    u64   big_len = StringStore_NODE_SIZE * 3 + 7;
    char* big     = malloc(big_len);
    WC_ASSERT_NOT_NULL(big);
    memset(big, 'C', big_len);

    StrView v = StringStore_cstr(&ss, big, big_len);

    WC_ASSERT_EQ_U64(v.len, big_len);
    WC_ASSERT(v.ptr[0] == 'C');
    WC_ASSERT(v.ptr[big_len - 1] == 'C');
    WC_ASSERT_EQ_U64(ss.num, 3);
    WC_ASSERT_EQ_U64(ss.tail_off, 0);

    free(big);
    StringStore_destroy(&ss);
}


// Many overflows: views must stay intact and must not alias each other 

static void test_multiple_overflows_keep_content(void)
{
    StringStore ss;
    StringStore_create(&ss);

    enum { OVER_N = 4, OVER_LEN = StringStore_NODE_SIZE + 5 };
    char buf[OVER_LEN];
    memset(buf, 'D', sizeof(buf));

    StrView views[OVER_N];
    for (int i = 0; i < OVER_N; i++) {
        views[i] = StringStore_cstr(&ss, buf, OVER_LEN);
    }

    for (int i = 0; i < OVER_N; i++) {
        WC_ASSERT_EQ_U64(views[i].len, OVER_LEN);
        WC_ASSERT(views[i].ptr[0] == 'D');
        WC_ASSERT(views[i].ptr[OVER_LEN - 1] == 'D');
    }
    for (int i = 0; i < OVER_N; i++) {
        for (int j = i + 1; j < OVER_N; j++) {
            WC_ASSERT(views[i].ptr != views[j].ptr);
        }
    }

    // each overflow adds its own node + a fresh tail
    WC_ASSERT_EQ_U64(ss.num, 1 + 2 * OVER_N);

    StringStore_destroy(&ss);
}


// Small Strings after an overflow land in the fresh tail node 

static void test_small_after_overflow_appends_to_tail(void)
{
    StringStore ss;
    StringStore_create(&ss);

    char big[StringStore_NODE_SIZE + 3];
    memset(big, 'E', sizeof(big));
    (void)StringStore_cstr(&ss, big, sizeof(big));

    StrView small = StringStore_cstr(&ss, "tail", 4);
    WC_ASSERT_TRUE(sv_equals_cstr(small, "tail"));
    WC_ASSERT(small.ptr != ss.head->heap); // in a node buf, not the overflow heap buffer
    WC_ASSERT_EQ_U64(ss.tail_off, 4);

    StringStore_destroy(&ss);
}


// Destroy must tolerate overflow nodes anywhere in the chain 
// (ASan catches double-free / mismatched-free if the union is mishandled)

static void test_destroy_mixed_chain(void)
{
    StringStore ss;
    StringStore_create(&ss);

    (void)StringStore_cstr(&ss, "one", 3);

    char big[StringStore_NODE_SIZE + 9];
    memset(big, 'F', sizeof(big));
    (void)StringStore_cstr(&ss, big, sizeof(big));

    (void)StringStore_cstr(&ss, "two", 3);
    (void)StringStore_cstr(&ss, big, sizeof(big));
    (void)StringStore_cstr(&ss, "three", 5);

    WC_ASSERT_EQ_U64(ss.num, 5); // 1 + 2 per overflow

    StringStore_destroy(&ss);
}


// StrView_from_String sanity (same header) 

static void test_strview_from_String(void)
{
    String* s = String_from_cstr("viewme");
    StrView sv = StrView_from_String(s);
    WC_ASSERT_TRUE(sv_equals_cstr(sv, "viewme"));
    WC_ASSERT_EQ_U64(sv.len, 6);
    String_destroy(s);
}


// Suite 

void views_suite(void)
{
    WC_SUITE("Views");
    WC_RUN(test_small_Strings_share_node);
    WC_RUN(test_exact_fit_no_new_node);
    WC_RUN(test_overflow_by_one_goes_to_heap);
    WC_RUN(test_overflow_way_past_node);
    WC_RUN(test_multiple_overflows_keep_content);
    WC_RUN(test_small_after_overflow_appends_to_tail);
    WC_RUN(test_destroy_mixed_chain);
    WC_RUN(test_strview_from_String);
}
