#include "arena.h"
#include "common.h"
#include "wc_allocator.h"
#include "wc_string.h"
#include "wc_test.h"
#include "wc_test_allocator.h"
#include <stdlib.h>
#include <string.h>


// Construction

static void test_create_empty(void)
{
    String s = String_create(WC_LIBC);
    WC_ASSERT_EQ_U64(String_len(&s), 0);
    WC_ASSERT_TRUE(String_empty(&s));
    String_destroy(&s);
}

static void test_create_stk(void)
{
    String s = String_from_cstr(WC_LIBC, "hello");
    WC_ASSERT_EQ_U64(String_len(&s), 5);
    WC_ASSERT(String_equals_cstr(&s, "hello"));
    String_destroy(&s);
}

static void test_from_cstr(void)
{
    String s = String_from_cstr(WC_LIBC, "world");
    WC_ASSERT_EQ_U64(String_len(&s), 5);
    WC_ASSERT(String_equals_cstr(&s, "world"));
    String_destroy(&s);
}

static void test_from_cstr_empty(void)
{
    String s = String_from_cstr(WC_LIBC, "");
    WC_ASSERT_EQ_U64(String_len(&s), 0);
    WC_ASSERT_TRUE(String_empty(&s));
    String_destroy(&s);
}

static void test_from_String(void)
{
    String a = String_from_cstr(WC_LIBC, "copy me");
    String b = String_from_String(WC_LIBC, &a);
    WC_ASSERT_TRUE(String_equals(&a, &b));
    /* must be independent */
    String_append_cstr(&a, "!!!");
    WC_ASSERT_FALSE(String_equals(&a, &b));
    String_destroy(&a);
    String_destroy(&b);
}


// Append

static void test_append_cstr(void)
{
    String s = String_from_cstr(WC_LIBC, "hello");
    String_append_cstr(&s, " world");
    WC_ASSERT_EQ_U64(String_len(&s), 11);
    WC_ASSERT(String_equals_cstr(&s, "hello world"));
    String_destroy(&s);
}

static void test_append_char(void)
{
    String s = String_from_cstr(WC_LIBC, "ab");
    String_append_char(&s, 'c');
    WC_ASSERT_EQ_U64(String_len(&s), 3);
    WC_ASSERT(String_equals_cstr(&s, "abc"));
    String_destroy(&s);
}

static void test_append_String(void)
{
    String a = String_from_cstr(WC_LIBC, "foo");
    String b = String_from_cstr(WC_LIBC, "bar");
    String_append_String(&a, &b);
    WC_ASSERT(String_equals_cstr(&a, "foobar"));
    /* b must be unchanged */
    WC_ASSERT(String_equals_cstr(&b, "bar"));
    String_destroy(&a);
    String_destroy(&b);
}

/* append to empty */
static void test_append_to_empty(void)
{
    String s = String_create(WC_LIBC);
    String_append_cstr(&s, "first");
    WC_ASSERT_EQ_U64(String_len(&s), 5);
    WC_ASSERT(String_equals_cstr(&s, "first"));
    String_destroy(&s);
}


// Insert / Remove

static void test_insert_char_front(void)
{
    String s = String_from_cstr(WC_LIBC, "bc");
    String_insert_char(&s, 0, 'a');
    WC_ASSERT(String_equals_cstr(&s, "abc"));
    String_destroy(&s);
}

static void test_insert_char_mid(void)
{
    String s = String_from_cstr(WC_LIBC, "ac");
    String_insert_char(&s, 1, 'b');
    WC_ASSERT(String_equals_cstr(&s, "abc"));
    String_destroy(&s);
}

static void test_insert_cstr(void)
{
    String s = String_from_cstr(WC_LIBC, "helo");
    String_insert_cstr(&s, 3, "l");
    WC_ASSERT(String_equals_cstr(&s, "hello"));
    String_destroy(&s);
}

static void test_remove_char(void)
{
    String s = String_from_cstr(WC_LIBC, "aXb");
    String_remove_char(&s, 1);
    WC_ASSERT_EQ_U64(String_len(&s), 2);
    WC_ASSERT(String_equals_cstr(&s, "ab"));
    String_destroy(&s);
}

static void test_remove_range(void)
{
    String s = String_from_cstr(WC_LIBC, "aXXXb");
    String_remove_range(&s, 1, 3);
    WC_ASSERT_EQ_U64(String_len(&s), 2);
    WC_ASSERT(String_equals_cstr(&s, "ab"));
    String_destroy(&s);
}

static void test_pop_char(void)
{
    String s = String_from_cstr(WC_LIBC, "abc");
    char   c = String_pop_char(&s);
    WC_ASSERT_EQ_INT(c, 'c');
    WC_ASSERT_EQ_U64(String_len(&s), 2);
    WC_ASSERT(String_equals_cstr(&s, "ab"));
    String_destroy(&s);
}


// Access

static void test_char_at(void)
{
    String s = String_from_cstr(WC_LIBC, "xyz");
    WC_ASSERT_EQ_INT(String_char_at(&s, 0), 'x');
    WC_ASSERT_EQ_INT(String_char_at(&s, 1), 'y');
    WC_ASSERT_EQ_INT(String_char_at(&s, 2), 'z');
    String_destroy(&s);
}

static void test_set_char(void)
{
    String s = String_from_cstr(WC_LIBC, "aXc");
    String_set_char(&s, 1, 'b');
    WC_ASSERT(String_equals_cstr(&s, "abc"));
    String_destroy(&s);
}


// Compare / Search

static void test_equals(void)
{
    String a = String_from_cstr(WC_LIBC, "same");
    String b = String_from_cstr(WC_LIBC, "same");
    String c = String_from_cstr(WC_LIBC, "different");
    WC_ASSERT_TRUE(String_equals(&a, &b));
    WC_ASSERT_FALSE(String_equals(&a, &c));
    String_destroy(&a);
    String_destroy(&b);
    String_destroy(&c);
}

static void test_compare_ordering(void)
{
    String a = String_from_cstr(WC_LIBC, "abc");
    String b = String_from_cstr(WC_LIBC, "abd");
    WC_ASSERT_TRUE(String_compare(&a, &b) < 0);
    WC_ASSERT_TRUE(String_compare(&b, &a) > 0);
    WC_ASSERT_EQ_INT(String_compare(&a, &a), 0);
    String_destroy(&a);
    String_destroy(&b);
}

static void test_find_char(void)
{
    String s = String_from_cstr(WC_LIBC, "hello");
    WC_ASSERT_EQ_U64(String_find_char(&s, 'e'), 1);
    WC_ASSERT_EQ_U64(String_find_char(&s, 'z'), WC_NOT_FOUND);
    String_destroy(&s);
}

static void test_find_cstr(void)
{
    String s = String_from_cstr(WC_LIBC, "hello world");
    WC_ASSERT_EQ_U64(String_find_cstr(&s, "world"), 6);
    WC_ASSERT_EQ_U64(String_find_cstr(&s, "xyz"), WC_NOT_FOUND);
    WC_ASSERT_EQ_U64(String_find_cstr(&s, ""), 0);
    String_destroy(&s);
}

static void test_substr(void)
{
    String s   = String_from_cstr(WC_LIBC, "hello world");
    String sub = String_substr(WC_LIBC, &s, 6, 5);
    WC_ASSERT(String_equals_cstr(&sub, "world"));
    String_destroy(&s);
    String_destroy(&sub);
}


// Copy / Move

static void test_copy_independence(void)
{
    String a = String_from_cstr(WC_LIBC, "original");
    String b = String_copy(WC_LIBC, &a);
    String_append_cstr(&a, "_modified");
    WC_ASSERT_FALSE(String_equals(&a, &b));
    WC_ASSERT(String_equals_cstr(&b, "original"));
    String_destroy(&a);
    String_destroy(&b);
}

static void test_move_nulls_src(void)
{
    String src  = String_from_cstr(WC_LIBC, "move me");
    String dest;
    String_move(&dest, &src);
    WC_ASSERT_EQ_U64(src.capacity, 0); // moved-from String is zeroed
    WC_ASSERT(String_equals_cstr(&dest, "move me"));
    String_destroy(&dest);
    String_destroy(&src); // safe on zeroed
}


// Misc

static void test_clear(void)
{
    String s = String_from_cstr(WC_LIBC, "data");
    String_clear(&s);
    WC_ASSERT_EQ_U64(String_len(&s), 0);
    WC_ASSERT_TRUE(String_empty(&s));
    String_destroy(&s);
}

static void test_reserve_char(void)
{
    String s = String_create(WC_LIBC);
    String_reserve_char(&s, 5, 'x');
    WC_ASSERT_EQ_U64(String_len(&s), 5);
    WC_ASSERT(String_equals_cstr(&s, "xxxxx"));
    String_destroy(&s);
}

static void test_to_cstr(void)
{
    String s    = String_from_cstr(WC_LIBC, "hello");
    char*  cstr = String_to_cstr(WC_LIBC, &s);
    WC_ASSERT_NOT_NULL(cstr);
    WC_ASSERT_EQ_STR(cstr, "hello");
    wc_free(WC_LIBC, cstr, s.size + 1, 1);
    String_destroy(&s);
}

// stress: many appends trigger multiple growths
static void test_growth(void)
{
    String s = String_create(WC_LIBC);
    for (int i = 0; i < 200; i++) {
        String_append_char(&s, 'a');
    }
    WC_ASSERT_EQ_U64(String_len(&s), 200);
    String_destroy(&s);
}

// test manual shrinkage
static void test_shrink(void)
{
    String s = String_create(WC_LIBC);

    // grow within sso (usable sso capacity = STR_SSO_SIZE - 1 = 31)
    for (int i = 0; i < 25; i++) {
        String_append_char(&s, 'a');
    }
    WC_ASSERT_EQ_U64(String_len(&s), 25);
    WC_ASSERT_EQ_U64(String_capacity(&s), STR_SSO_SIZE - 1);
    WC_ASSERT_TRUE(String_is_sso(&s));

    // grow over sso (25 + 15 = 40 > 31)
    for (int i = 0; i < 15; i++) {
        String_append_char(&s, 'b');
    }
    WC_ASSERT_EQ_U64(String_len(&s), 40);
    WC_ASSERT_FALSE(String_is_sso(&s));

    // remove 15 (within sso range but no auto shrinkage)
    for (int i = 0; i < 15; i++) {
        String_pop_char(&s);
    }
    WC_ASSERT_EQ_U64(String_len(&s), 25);
    WC_ASSERT_FALSE(String_is_sso(&s));

    // do the manual shrink
    String_shrink_to_fit(&s);
    WC_ASSERT_EQ_U64(String_len(&s), 25);
    WC_ASSERT_EQ_U64(String_capacity(&s), STR_SSO_SIZE - 1);
    WC_ASSERT_TRUE(String_is_sso(&s));

    String_destroy(&s);
}

// insert_String

static void test_insert_String_front(void)
{
    String a = String_from_cstr(WC_LIBC, "world");
    String b = String_from_cstr(WC_LIBC, "hello ");
    String_insert_String(&a, 0, &b);
    WC_ASSERT(String_equals_cstr(&a, "hello world"));
    String_destroy(&a);
    String_destroy(&b);
}

static void test_insert_String_end(void)
{
    String a = String_from_cstr(WC_LIBC, "hello");
    String b = String_from_cstr(WC_LIBC, " world");
    String_insert_String(&a, String_len(&a), &b);
    WC_ASSERT(String_equals_cstr(&a, "hello world"));
    String_destroy(&a);
    String_destroy(&b);
}

static void test_insert_String_mid(void)
{
    String a = String_from_cstr(WC_LIBC, "ac");
    String b = String_from_cstr(WC_LIBC, "b");
    String_insert_String(&a, 1, &b);
    WC_ASSERT(String_equals_cstr(&a, "abc"));
    String_destroy(&a);
    String_destroy(&b);
}

static void test_insert_empty_String_noop(void)
{
    String a = String_from_cstr(WC_LIBC, "hello");
    String b = String_from_cstr(WC_LIBC, "");
    String_insert_String(&a, 0, &b);
    WC_ASSERT(String_equals_cstr(&a, "hello"));
    WC_ASSERT_EQ_U64(String_len(&a), 5);
    String_destroy(&a);
    String_destroy(&b);
}


// String_to_cstr_buf

static void test_to_cstr_buf_basic(void)
{
    String s       = String_from_cstr(WC_LIBC, "hello");
    char   buf[16] = {0};
    String_to_cstr_buf(&s, buf, sizeof(buf));
    WC_ASSERT_EQ_STR(buf, "hello");
    String_destroy(&s);
}

static void test_to_cstr_buf_nul_terminated(void)
{
    String s = String_from_cstr(WC_LIBC, "abc");
    char   buf[8];
    memset(buf, 0xFF, sizeof(buf));
    String_to_cstr_buf(&s, buf, 8);
    WC_ASSERT_EQ_INT(buf[3], '\0');
    String_destroy(&s);
}

static void test_to_cstr_buf_empty_String(void)
{
    String s = String_create(WC_LIBC);
    char   buf[8];
    memset(buf, 0xFF, sizeof(buf));
    String_to_cstr_buf(&s, buf, 8);
    WC_ASSERT_EQ_INT(buf[0], '\0');
    String_destroy(&s);
}


// String_data_ptr

static void test_data_ptr_non_empty(void)
{
    String s = String_from_cstr(WC_LIBC, "hello");
    char*  p = String_data_ptr(&s);
    WC_ASSERT_NOT_NULL(p);
    WC_ASSERT_EQ_INT(p[0], 'h');
    WC_ASSERT_EQ_INT(p[4], 'o');
    String_destroy(&s);
}

static void test_data_ptr_empty_returns_null(void)
{
    String s = String_create(WC_LIBC);
    WC_ASSERT_NULL(String_data_ptr(&s));
    String_destroy(&s);
}

static void test_data_ptr_mutation(void)
{
    String s = String_from_cstr(WC_LIBC, "hello");
    char*  p = String_data_ptr(&s);
    p[0]     = 'H';
    WC_ASSERT(String_equals_cstr(&s, "Hello"));
    String_destroy(&s);
}


// TEMP_CSTR_READ macro / String_ensure_null_term

static void test_temp_cstr_read(void)
{
    String s          = String_from_cstr(WC_LIBC, "test");
    u64    len_before = String_len(&s);
    char   captured[16] = {0};

    String_ensure_null_term(&s);
    WC_ASSERT_EQ_U64(String_len(&s), len_before);
    const char* ptr = String_cstr_view(&s);
    WC_ASSERT_NOT_NULL(ptr);
    WC_ASSERT_EQ_STR(ptr, "test");
    for (u64 i = 0; i < len_before; i++) {
        captured[i] = ptr[i];
    }
    captured[len_before] = '\0';

    WC_ASSERT_EQ_U64(String_len(&s), len_before);
    WC_ASSERT_EQ_STR(captured, "test");
    String_destroy(&s);
}

static void test_temp_cstr_read_empty(void)
{
    String s = String_create(WC_LIBC);

    String_ensure_null_term(&s);
    WC_ASSERT_EQ_U64(String_len(&s), 0);
    const char* ptr = String_cstr_view(&s);
    WC_ASSERT_NOT_NULL(ptr);
    WC_ASSERT_EQ_STR(ptr, "");

    WC_ASSERT_EQ_U64(String_len(&s), 0);
    String_destroy(&s);
}

static void test_ensure_null_term_direct(void)
{
    String s          = String_from_cstr(WC_LIBC, "abc");
    u64    len_before = String_len(&s);
    u64    cap_before = String_capacity(&s);

    String_ensure_null_term(&s);

    WC_ASSERT_EQ_U64(String_len(&s), len_before);
    WC_ASSERT_EQ_U64(String_capacity(&s), cap_before);
    WC_ASSERT_EQ_STR(String_cstr_view(&s), "abc");

    /* idempotent */
    String_ensure_null_term(&s);
    WC_ASSERT_EQ_U64(String_len(&s), len_before);
    WC_ASSERT_EQ_STR(String_cstr_view(&s), "abc");

    String_destroy(&s);
}

static void test_ensure_null_term_sso_boundary(void)
{
    /* Exactly STR_SSO_SIZE - 1 (31) chars: fills SSO capacity exactly */
    char buf[STR_SSO_SIZE];
    for (u64 i = 0; i < STR_SSO_SIZE - 1; i++) {
        buf[i] = 'x';
    }
    buf[STR_SSO_SIZE - 1] = '\0';

    String s = String_from_cstr(WC_LIBC, buf);
    WC_ASSERT_EQ_U64(String_len(&s), STR_SSO_SIZE - 1);
    WC_ASSERT_TRUE(String_is_sso(&s));

    String_ensure_null_term(&s);

    /* forced to convert to heap to get a safe byte past the last char */
    WC_ASSERT_FALSE(String_is_sso(&s));
    WC_ASSERT_EQ_U64(String_len(&s), STR_SSO_SIZE - 1);
    WC_ASSERT_TRUE(String_capacity(&s) > STR_SSO_SIZE - 1);
    WC_ASSERT_EQ_STR(String_cstr_view(&s), buf);

    String_destroy(&s);
}

static void test_ensure_null_term_heap_boundary(void)
{
    String s = String_create(WC_LIBC);
    String_reserve(&s, STR_SSO_SIZE + 10);
    WC_ASSERT_FALSE(String_is_sso(&s));

    u64 cap = String_capacity(&s);
    for (u64 i = 0; i < cap; i++) {
        String_append_char(&s, 'y');
    }
    WC_ASSERT_EQ_U64(String_len(&s), cap);

    String_ensure_null_term(&s);

    WC_ASSERT_EQ_U64(String_len(&s), cap);
    WC_ASSERT_TRUE(String_capacity(&s) > cap);
    WC_ASSERT_EQ_U64(strlen(String_cstr_view(&s)), cap);

    String_destroy(&s);
}


// String_append_String_move

static void test_append_String_move_nulls_src(void)
{
    String a = String_from_cstr(WC_LIBC, "hello");
    String b = String_from_cstr(WC_LIBC, " world");
    String_append_String_move(&a, &b);
    WC_ASSERT_EQ_U64(b.capacity, 0); // zeroed
    WC_ASSERT(String_equals_cstr(&a, "hello world"));
    String_destroy(&a);
}


// String_copy

static void test_copy_into_heap_String(void)
{
    String a = String_from_cstr(WC_LIBC, "source");
    String b = String_copy(WC_LIBC, &a);
    WC_ASSERT(String_equals(&a, &b));
    /* Independence */
    String_append_char(&a, '!');
    WC_ASSERT_FALSE(String_equals(&a, &b));
    String_destroy(&a);
    String_destroy(&b);
}


// SSO boundary

static void test_sso_stays_sso_up_to_limit(void)
{
    String s = String_create(WC_LIBC);
    for (int i = 0; i < STR_SSO_SIZE - 1; i++) {
        String_append_char(&s, 'a');
    }
    WC_ASSERT_TRUE(String_is_sso(&s));
    WC_ASSERT_EQ_U64(String_len(&s), (u64)(STR_SSO_SIZE - 1));
    String_destroy(&s);
}

static void test_sso_promotes_at_overflow(void)
{
    String s = String_create(WC_LIBC);
    for (int i = 0; i < STR_SSO_SIZE; i++) {
        String_append_char(&s, 'b');
    }
    WC_ASSERT_FALSE(String_is_sso(&s));
    WC_ASSERT_EQ_U64(String_len(&s), (u64)STR_SSO_SIZE);
    String_destroy(&s);
}

// A8 regression: shrink_to_fit on an empty heap-mode string returns to SSO
static void test_shrink_empty_heap_string_returns_to_sso(void)
{
    String s = String_create(WC_LIBC);
    for (int i = 0; i < 40; i++) {
        String_append_char(&s, 'a');
    }
    WC_ASSERT_FALSE(String_is_sso(&s));

    String_clear(&s);
    String_shrink_to_fit(&s);

    WC_ASSERT_TRUE(String_is_sso(&s));
    WC_ASSERT_EQ_U64(String_len(&s), 0);
    WC_ASSERT_EQ_U64(s.capacity, STR_SSO_SIZE - 1);

    String_append_char(&s, 'b');
    String_append_cstr(&s, "cd");
    WC_ASSERT_EQ_U64(String_len(&s), 3);
    WC_ASSERT(String_equals_cstr(&s, "bcd"));

    String_destroy(&s);
}


// Allocator tests: Arena, test allocator, and cross-allocator copy

static void test_string_arena_and_cross_alloc_copy(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, 4096);

    const char* long_str = "a long string beyond sso to force arena heap allocation 12345678901234567890";
    String s = String_from_cstr(Arena_allocator(&a), long_str);
    WC_ASSERT_FALSE(String_is_sso(&s));
    WC_ASSERT(String_equals_cstr(&s, long_str));

    /* Cross-allocator copy: Arena-backed string copied into WC_LIBC */
    String copy = String_copy(WC_LIBC, &s);
    WC_ASSERT(String_equals(&s, &copy));
    WC_ASSERT_FALSE(String_is_sso(&copy));

    String_destroy(&copy);
    String_destroy(&s);
    Arena_destroy(&a);
}

static void test_string_test_allocator_leak_free(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator alloc = wc_test_alloc_allocator(&ta);

    String s = String_create(alloc);
    for (int i = 0; i < 100; i++) {
        String_append_char(&s, 'x');
    }
    WC_ASSERT_FALSE(String_is_sso(&s));
    WC_ASSERT_TRUE(wc_test_alloc_active_bytes(&ta) > 0);

    String_destroy(&s);
    WC_ASSERT_EQ_U64(wc_test_alloc_active_bytes(&ta), 0);

    wc_test_alloc_destroy(&ta);
}


// Suite entry point

void String_suite(void)
{
    WC_SUITE("String");

    // construction
    WC_RUN(test_create_empty);
    WC_RUN(test_create_stk);
    WC_RUN(test_from_cstr);
    WC_RUN(test_from_cstr_empty);
    WC_RUN(test_from_String);

    // append
    WC_RUN(test_append_cstr);
    WC_RUN(test_append_char);
    WC_RUN(test_append_String);
    WC_RUN(test_append_to_empty);

    // insert / remove
    WC_RUN(test_insert_char_front);
    WC_RUN(test_insert_char_mid);
    WC_RUN(test_insert_cstr);
    WC_RUN(test_remove_char);
    WC_RUN(test_remove_range);
    WC_RUN(test_pop_char);

    // access
    WC_RUN(test_char_at);
    WC_RUN(test_set_char);

    // compare / search
    WC_RUN(test_equals);
    WC_RUN(test_compare_ordering);
    WC_RUN(test_find_char);
    WC_RUN(test_find_cstr);
    WC_RUN(test_substr);

    // copy / move
    WC_RUN(test_copy_independence);
    WC_RUN(test_move_nulls_src);

    // misc
    WC_RUN(test_clear);
    WC_RUN(test_reserve_char);
    WC_RUN(test_to_cstr);
    WC_RUN(test_growth);
    WC_RUN(test_shrink);
    WC_RUN(test_shrink_empty_heap_string_returns_to_sso);

    // new tests
    WC_RUN(test_insert_String_front);
    WC_RUN(test_insert_String_end);
    WC_RUN(test_insert_String_mid);
    WC_RUN(test_insert_empty_String_noop);

    WC_RUN(test_to_cstr_buf_basic);
    WC_RUN(test_to_cstr_buf_nul_terminated);
    WC_RUN(test_to_cstr_buf_empty_String);

    WC_RUN(test_data_ptr_non_empty);
    WC_RUN(test_data_ptr_empty_returns_null);
    WC_RUN(test_data_ptr_mutation);

    WC_RUN(test_temp_cstr_read);
    WC_RUN(test_temp_cstr_read_empty);
    WC_RUN(test_ensure_null_term_direct);
    WC_RUN(test_ensure_null_term_sso_boundary);
    WC_RUN(test_ensure_null_term_heap_boundary);
    WC_RUN(test_append_String_move_nulls_src);

    WC_RUN(test_copy_into_heap_String);

    WC_RUN(test_sso_stays_sso_up_to_limit);
    WC_RUN(test_sso_promotes_at_overflow);

    // allocator tests
    WC_RUN(test_string_arena_and_cross_alloc_copy);
    WC_RUN(test_string_test_allocator_leak_free);
}
