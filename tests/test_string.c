#include "arena.h"
#include "common.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_string.h"
#include "wc_test_allocator.h"

#include <string.h>


// Construction

UTEST(string, create_empty)
{
    String s = String_create(WC_LIBC);
    EXPECT_EQ(String_len(&s), 0u);
    EXPECT_TRUE(String_empty(&s));
    String_destroy(&s);
}

UTEST(string, create_stk)
{
    String s = String_from_cstr(WC_LIBC, "hello");
    EXPECT_EQ(String_len(&s), 5u);
    EXPECT_TRUE(String_equals_cstr(&s, "hello"));
    String_destroy(&s);
}

UTEST(string, from_cstr)
{
    String s = String_from_cstr(WC_LIBC, "world");
    EXPECT_EQ(String_len(&s), 5u);
    EXPECT_TRUE(String_equals_cstr(&s, "world"));
    String_destroy(&s);
}

UTEST(string, from_cstr_empty)
{
    String s = String_from_cstr(WC_LIBC, "");
    EXPECT_EQ(String_len(&s), 0u);
    EXPECT_TRUE(String_empty(&s));
    String_destroy(&s);
}

UTEST(string, from_String)
{
    String a = String_from_cstr(WC_LIBC, "copy me");
    String b = String_from_String(WC_LIBC, &a);
    EXPECT_TRUE(String_equals(&a, &b));
    /* must be independent */
    String_append_cstr(&a, "!!!");
    EXPECT_FALSE(String_equals(&a, &b));
    String_destroy(&a);
    String_destroy(&b);
}


// Append

UTEST(string, append_cstr)
{
    String s = String_from_cstr(WC_LIBC, "hello");
    String_append_cstr(&s, " world");
    EXPECT_EQ(String_len(&s), 11u);
    EXPECT_TRUE(String_equals_cstr(&s, "hello world"));
    String_destroy(&s);
}

UTEST(string, append_char)
{
    String s = String_from_cstr(WC_LIBC, "ab");
    String_append_char(&s, 'c');
    EXPECT_EQ(String_len(&s), 3u);
    EXPECT_TRUE(String_equals_cstr(&s, "abc"));
    String_destroy(&s);
}

UTEST(string, append_String)
{
    String a = String_from_cstr(WC_LIBC, "foo");
    String b = String_from_cstr(WC_LIBC, "bar");
    String_append_String(&a, &b);
    EXPECT_TRUE(String_equals_cstr(&a, "foobar"));
    /* b must be unchanged */
    EXPECT_TRUE(String_equals_cstr(&b, "bar"));
    String_destroy(&a);
    String_destroy(&b);
}

/* append to empty */
UTEST(string, append_to_empty)
{
    String s = String_create(WC_LIBC);
    String_append_cstr(&s, "first");
    EXPECT_EQ(String_len(&s), 5u);
    EXPECT_TRUE(String_equals_cstr(&s, "first"));
    String_destroy(&s);
}


// Insert / Remove

UTEST(string, insert_char_front)
{
    String s = String_from_cstr(WC_LIBC, "bc");
    String_insert_char(&s, 0, 'a');
    EXPECT_TRUE(String_equals_cstr(&s, "abc"));
    String_destroy(&s);
}

UTEST(string, insert_char_mid)
{
    String s = String_from_cstr(WC_LIBC, "ac");
    String_insert_char(&s, 1, 'b');
    EXPECT_TRUE(String_equals_cstr(&s, "abc"));
    String_destroy(&s);
}

UTEST(string, insert_cstr)
{
    String s = String_from_cstr(WC_LIBC, "helo");
    String_insert_cstr(&s, 3, "l");
    EXPECT_TRUE(String_equals_cstr(&s, "hello"));
    String_destroy(&s);
}

UTEST(string, remove_char)
{
    String s = String_from_cstr(WC_LIBC, "aXb");
    String_remove_char(&s, 1);
    EXPECT_EQ(String_len(&s), 2u);
    EXPECT_TRUE(String_equals_cstr(&s, "ab"));
    String_destroy(&s);
}

UTEST(string, remove_range)
{
    String s = String_from_cstr(WC_LIBC, "aXXXb");
    String_remove_range(&s, 1, 3);
    EXPECT_EQ(String_len(&s), 2u);
    EXPECT_TRUE(String_equals_cstr(&s, "ab"));
    String_destroy(&s);
}

UTEST(string, pop_char)
{
    String s = String_from_cstr(WC_LIBC, "abc");
    char   c = String_pop_char(&s);
    EXPECT_EQ(c, 'c');
    EXPECT_EQ(String_len(&s), 2u);
    EXPECT_TRUE(String_equals_cstr(&s, "ab"));
    String_destroy(&s);
}


// Access

UTEST(string, char_at)
{
    String s = String_from_cstr(WC_LIBC, "xyz");
    EXPECT_EQ(String_char_at(&s, 0), 'x');
    EXPECT_EQ(String_char_at(&s, 1), 'y');
    EXPECT_EQ(String_char_at(&s, 2), 'z');
    String_destroy(&s);
}

UTEST(string, set_char)
{
    String s = String_from_cstr(WC_LIBC, "aXc");
    String_set_char(&s, 1, 'b');
    EXPECT_TRUE(String_equals_cstr(&s, "abc"));
    String_destroy(&s);
}


// Compare / Search

UTEST(string, equals)
{
    String a = String_from_cstr(WC_LIBC, "same");
    String b = String_from_cstr(WC_LIBC, "same");
    String c = String_from_cstr(WC_LIBC, "different");
    EXPECT_TRUE(String_equals(&a, &b));
    EXPECT_FALSE(String_equals(&a, &c));
    String_destroy(&a);
    String_destroy(&b);
    String_destroy(&c);
}

UTEST(string, compare_ordering)
{
    String a = String_from_cstr(WC_LIBC, "abc");
    String b = String_from_cstr(WC_LIBC, "abd");
    EXPECT_TRUE(String_compare(&a, &b) < 0);
    EXPECT_TRUE(String_compare(&b, &a) > 0);
    EXPECT_EQ(String_compare(&a, &a), 0);
    String_destroy(&a);
    String_destroy(&b);
}

UTEST(string, find_char)
{
    String s = String_from_cstr(WC_LIBC, "hello");
    EXPECT_EQ(String_find_char(&s, 'e'), 1u);
    EXPECT_EQ(String_find_char(&s, 'z'), WC_NOT_FOUND);
    String_destroy(&s);
}

UTEST(string, find_cstr)
{
    String s = String_from_cstr(WC_LIBC, "hello world");
    EXPECT_EQ(String_find_cstr(&s, "world"), 6u);
    EXPECT_EQ(String_find_cstr(&s, "xyz"), WC_NOT_FOUND);
    EXPECT_EQ(String_find_cstr(&s, ""), 0u);
    String_destroy(&s);
}

UTEST(string, substr)
{
    String s   = String_from_cstr(WC_LIBC, "hello world");
    String sub = String_substr(WC_LIBC, &s, 6, 5);
    EXPECT_TRUE(String_equals_cstr(&sub, "world"));
    String_destroy(&s);
    String_destroy(&sub);
}


// Copy / Move

UTEST(string, copy_independence)
{
    String a = String_from_cstr(WC_LIBC, "original");
    String b = String_copy(WC_LIBC, &a);
    String_append_cstr(&a, "_modified");
    EXPECT_FALSE(String_equals(&a, &b));
    EXPECT_TRUE(String_equals_cstr(&b, "original"));
    String_destroy(&a);
    String_destroy(&b);
}

UTEST(string, move_nulls_src)
{
    String src = String_from_cstr(WC_LIBC, "move me");
    String dest;
    String_move(&dest, &src);
    EXPECT_EQ(src.capacity, 0u); // moved-from String is zeroed
    EXPECT_TRUE(String_equals_cstr(&dest, "move me"));
    String_destroy(&dest);
    String_destroy(&src); // safe on zeroed
}


// Misc

UTEST(string, clear)
{
    String s = String_from_cstr(WC_LIBC, "data");
    String_clear(&s);
    EXPECT_EQ(String_len(&s), 0u);
    EXPECT_TRUE(String_empty(&s));
    String_destroy(&s);
}

UTEST(string, reserve_char)
{
    String s = String_create(WC_LIBC);
    String_reserve_char(&s, 5, 'x');
    EXPECT_EQ(String_len(&s), 5u);
    EXPECT_TRUE(String_equals_cstr(&s, "xxxxx"));
    String_destroy(&s);
}

UTEST(string, to_cstr)
{
    String s    = String_from_cstr(WC_LIBC, "hello");
    char*  cstr = String_to_cstr(WC_LIBC, &s);
    EXPECT_TRUE((cstr) != NULL);
    EXPECT_STREQ(cstr, "hello");
    wc_free(WC_LIBC, cstr, s.size + 1, 1);
    String_destroy(&s);
}

// stress: many appends trigger multiple growths
UTEST(string, growth)
{
    String s = String_create(WC_LIBC);
    for (int i = 0; i < 200; i++) {
        String_append_char(&s, 'a');
    }
    EXPECT_EQ(String_len(&s), 200u);
    String_destroy(&s);
}

// test manual shrinkage
UTEST(string, shrink)
{
    String s = String_create(WC_LIBC);

    // grow within sso (usable sso capacity = STR_SSO_SIZE - 1 = 31)
    for (int i = 0; i < 25; i++) {
        String_append_char(&s, 'a');
    }
    EXPECT_EQ(String_len(&s), 25u);
    EXPECT_EQ(String_capacity(&s), (u64)(STR_SSO_SIZE - 1));
    EXPECT_TRUE(String_is_sso(&s));

    // grow over sso (25 + 15 = 40 > 31)
    for (int i = 0; i < 15; i++) {
        String_append_char(&s, 'b');
    }
    EXPECT_EQ(String_len(&s), 40u);
    EXPECT_FALSE(String_is_sso(&s));

    // remove 15 (within sso range but no auto shrinkage)
    for (int i = 0; i < 15; i++) {
        String_pop_char(&s);
    }
    EXPECT_EQ(String_len(&s), 25u);
    EXPECT_FALSE(String_is_sso(&s));

    // do the manual shrink
    String_shrink_to_fit(&s);
    EXPECT_EQ(String_len(&s), 25u);
    EXPECT_EQ(String_capacity(&s), (u64)(STR_SSO_SIZE - 1));
    EXPECT_TRUE(String_is_sso(&s));

    String_destroy(&s);
}

// insert_String

UTEST(string, insert_String_front)
{
    String a = String_from_cstr(WC_LIBC, "world");
    String b = String_from_cstr(WC_LIBC, "hello ");
    String_insert_String(&a, 0, &b);
    EXPECT_TRUE(String_equals_cstr(&a, "hello world"));
    String_destroy(&a);
    String_destroy(&b);
}

UTEST(string, insert_String_end)
{
    String a = String_from_cstr(WC_LIBC, "hello");
    String b = String_from_cstr(WC_LIBC, " world");
    String_insert_String(&a, String_len(&a), &b);
    EXPECT_TRUE(String_equals_cstr(&a, "hello world"));
    String_destroy(&a);
    String_destroy(&b);
}

UTEST(string, insert_String_mid)
{
    String a = String_from_cstr(WC_LIBC, "ac");
    String b = String_from_cstr(WC_LIBC, "b");
    String_insert_String(&a, 1, &b);
    EXPECT_TRUE(String_equals_cstr(&a, "abc"));
    String_destroy(&a);
    String_destroy(&b);
}

UTEST(string, insert_empty_String_noop)
{
    String a = String_from_cstr(WC_LIBC, "hello");
    String b = String_from_cstr(WC_LIBC, "");
    String_insert_String(&a, 0, &b);
    EXPECT_TRUE(String_equals_cstr(&a, "hello"));
    EXPECT_EQ(String_len(&a), 5u);
    String_destroy(&a);
    String_destroy(&b);
}


// String_to_cstr_buf

UTEST(string, to_cstr_buf_basic)
{
    String s       = String_from_cstr(WC_LIBC, "hello");
    char   buf[16] = {0};
    String_to_cstr_buf(&s, buf, sizeof(buf));
    EXPECT_STREQ(buf, "hello");
    String_destroy(&s);
}

UTEST(string, to_cstr_buf_nul_terminated)
{
    String s = String_from_cstr(WC_LIBC, "abc");
    char   buf[8];
    memset(buf, 0xFF, sizeof(buf));
    String_to_cstr_buf(&s, buf, 8);
    EXPECT_EQ(buf[3], '\0');
    String_destroy(&s);
}

UTEST(string, to_cstr_buf_empty_String)
{
    String s = String_create(WC_LIBC);
    char   buf[8];
    memset(buf, 0xFF, sizeof(buf));
    String_to_cstr_buf(&s, buf, 8);
    EXPECT_EQ(buf[0], '\0');
    String_destroy(&s);
}


// String_data_ptr

UTEST(string, data_ptr_non_empty)
{
    String s = String_from_cstr(WC_LIBC, "hello");
    char*  p = String_data_ptr(&s);
    EXPECT_TRUE((p) != NULL);
    EXPECT_EQ(p[0], 'h');
    EXPECT_EQ(p[4], 'o');
    String_destroy(&s);
}

UTEST(string, data_ptr_empty_returns_null)
{
    String s = String_create(WC_LIBC);
    EXPECT_TRUE((String_data_ptr(&s)) == NULL);
    String_destroy(&s);
}

UTEST(string, data_ptr_mutation)
{
    String s = String_from_cstr(WC_LIBC, "hello");
    char*  p = String_data_ptr(&s);
    p[0]     = 'H';
    EXPECT_TRUE(String_equals_cstr(&s, "Hello"));
    String_destroy(&s);
}


// TEMP_CSTR_READ macro / String_ensure_null_term

UTEST(string, temp_cstr_read)
{
    String s            = String_from_cstr(WC_LIBC, "test");
    u64    len_before   = String_len(&s);
    char   captured[16] = {0};

    String_ensure_null_term(&s);
    EXPECT_EQ(String_len(&s), len_before);
    const char* ptr = String_cstr_view(&s);
    EXPECT_TRUE((ptr) != NULL);
    EXPECT_STREQ(ptr, "test");
    for (u64 i = 0; i < len_before; i++) {
        captured[i] = ptr[i];
    }
    captured[len_before] = '\0';

    EXPECT_EQ(String_len(&s), len_before);
    EXPECT_STREQ(captured, "test");
    String_destroy(&s);
}

UTEST(string, temp_cstr_read_empty)
{
    String s = String_create(WC_LIBC);

    String_ensure_null_term(&s);
    EXPECT_EQ(String_len(&s), 0u);
    const char* ptr = String_cstr_view(&s);
    EXPECT_TRUE((ptr) != NULL);
    EXPECT_STREQ(ptr, "");

    EXPECT_EQ(String_len(&s), 0u);
    String_destroy(&s);
}

UTEST(string, ensure_null_term_direct)
{
    String s          = String_from_cstr(WC_LIBC, "abc");
    u64    len_before = String_len(&s);
    u64    cap_before = String_capacity(&s);

    String_ensure_null_term(&s);

    EXPECT_EQ(String_len(&s), len_before);
    EXPECT_EQ(String_capacity(&s), cap_before);
    EXPECT_STREQ(String_cstr_view(&s), "abc");

    /* idempotent */
    String_ensure_null_term(&s);
    EXPECT_EQ(String_len(&s), len_before);
    EXPECT_STREQ(String_cstr_view(&s), "abc");

    String_destroy(&s);
}

UTEST(string, ensure_null_term_sso_boundary)
{
    /* Exactly STR_SSO_SIZE - 1 (31) chars: fills SSO capacity exactly */
    char buf[STR_SSO_SIZE];
    for (u64 i = 0; i < STR_SSO_SIZE - 1; i++) {
        buf[i] = 'x';
    }
    buf[STR_SSO_SIZE - 1] = '\0';

    String s = String_from_cstr(WC_LIBC, buf);
    EXPECT_EQ(String_len(&s), (u64)(STR_SSO_SIZE - 1));
    EXPECT_TRUE(String_is_sso(&s));

    String_ensure_null_term(&s);

    /* forced to convert to heap to get a safe byte past the last char */
    EXPECT_FALSE(String_is_sso(&s));
    EXPECT_EQ(String_len(&s), (u64)(STR_SSO_SIZE - 1));
    EXPECT_TRUE(String_capacity(&s) > STR_SSO_SIZE - 1);
    EXPECT_STREQ(String_cstr_view(&s), buf);

    String_destroy(&s);
}

UTEST(string, ensure_null_term_heap_boundary)
{
    String s = String_create(WC_LIBC);
    String_reserve(&s, STR_SSO_SIZE + 10);
    EXPECT_FALSE(String_is_sso(&s));

    u64 cap = String_capacity(&s);
    for (u64 i = 0; i < cap; i++) {
        String_append_char(&s, 'y');
    }
    EXPECT_EQ(String_len(&s), cap);

    String_ensure_null_term(&s);

    EXPECT_EQ(String_len(&s), cap);
    EXPECT_TRUE(String_capacity(&s) > cap);
    EXPECT_EQ(strlen(String_cstr_view(&s)), cap);

    String_destroy(&s);
}


// String_append_String_move

UTEST(string, append_String_move_nulls_src)
{
    String a = String_from_cstr(WC_LIBC, "hello");
    String b = String_from_cstr(WC_LIBC, " world");
    String_append_String_move(&a, &b);
    EXPECT_EQ(b.capacity, 0u); // zeroed
    EXPECT_TRUE(String_equals_cstr(&a, "hello world"));
    String_destroy(&a);
}


// String_copy

UTEST(string, copy_into_heap_String)
{
    String a = String_from_cstr(WC_LIBC, "source");
    String b = String_copy(WC_LIBC, &a);
    EXPECT_TRUE(String_equals(&a, &b));
    /* Independence */
    String_append_char(&a, '!');
    EXPECT_FALSE(String_equals(&a, &b));
    String_destroy(&a);
    String_destroy(&b);
}


// SSO boundary

UTEST(string, sso_stays_sso_up_to_limit)
{
    String s = String_create(WC_LIBC);
    for (int i = 0; i < STR_SSO_SIZE - 1; i++) {
        String_append_char(&s, 'a');
    }
    EXPECT_TRUE(String_is_sso(&s));
    EXPECT_EQ(String_len(&s), (u64)(STR_SSO_SIZE - 1));
    String_destroy(&s);
}

UTEST(string, sso_promotes_at_overflow)
{
    String s = String_create(WC_LIBC);
    for (int i = 0; i < STR_SSO_SIZE; i++) {
        String_append_char(&s, 'b');
    }
    EXPECT_FALSE(String_is_sso(&s));
    EXPECT_EQ(String_len(&s), (u64)STR_SSO_SIZE);
    String_destroy(&s);
}

// A8 regression: shrink_to_fit on an empty heap-mode string returns to SSO
UTEST(string, shrink_empty_heap_string_returns_to_sso)
{
    String s = String_create(WC_LIBC);
    for (int i = 0; i < 40; i++) {
        String_append_char(&s, 'a');
    }
    EXPECT_FALSE(String_is_sso(&s));

    String_clear(&s);
    String_shrink_to_fit(&s);

    EXPECT_TRUE(String_is_sso(&s));
    EXPECT_EQ(String_len(&s), 0u);
    EXPECT_EQ(s.capacity, (u64)(STR_SSO_SIZE - 1));

    String_append_char(&s, 'b');
    String_append_cstr(&s, "cd");
    EXPECT_EQ(String_len(&s), 3u);
    EXPECT_TRUE(String_equals_cstr(&s, "bcd"));

    String_destroy(&s);
}


// Allocator tests: Arena, test allocator, and cross-allocator copy

UTEST(string, arena_and_cross_alloc_copy)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 4096);

    const char* long_str = "a long string beyond sso to force arena heap allocation 12345678901234567890";
    String      s        = String_from_cstr(Arena_allocator(&a), long_str);
    EXPECT_FALSE(String_is_sso(&s));
    EXPECT_TRUE(String_equals_cstr(&s, long_str));

    /* Cross-allocator copy: Arena-backed string copied into WC_LIBC */
    String copy = String_copy(WC_LIBC, &s);
    EXPECT_TRUE(String_equals(&s, &copy));
    EXPECT_FALSE(String_is_sso(&copy));

    String_destroy(&copy);
    String_destroy(&s);
    Arena_destroy(&a);
}

UTEST(string, test_allocator_leak_free)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator alloc = wc_test_alloc_allocator(&ta);

    String s = String_create(alloc);
    for (int i = 0; i < 100; i++) {
        String_append_char(&s, 'x');
    }
    EXPECT_FALSE(String_is_sso(&s));
    EXPECT_TRUE(ta.live_bytes > 0);

    String_destroy(&s);
    EXPECT_EQ(ta.live_bytes, 0u);

    wc_test_alloc_destroy(&ta);
}
