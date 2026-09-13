#include "common.h"
#include "wc_test.h"
#include "wc_string.h"
#include <stdlib.h>
#include <string.h>


// TODO: test SSO


// Construction

static void test_create_empty(void)
{
    String* s = String_create();
    WC_ASSERT_NOT_NULL(s);
    WC_ASSERT_EQ_U64(String_len(s), 0);
    WC_ASSERT_TRUE(String_empty(s));
    String_destroy(s);
}

static void test_create_stk(void)
{
    String s;
    String_create_stk("hello", &s);
    WC_ASSERT_EQ_U64(String_len(&s), 5);
    WC_ASSERT(String_equals_cstr(&s, "hello"));
    String_destroy_stk(&s);
}

static void test_from_cstr(void)
{
    String* s = String_from_cstr("world");
    WC_ASSERT_NOT_NULL(s);
    WC_ASSERT_EQ_U64(String_len(s), 5);
    WC_ASSERT(String_equals_cstr(s, "world"));
    String_destroy(s);
}

static void test_from_cstr_empty(void)
{
    String* s = String_from_cstr("");
    WC_ASSERT_NOT_NULL(s);
    WC_ASSERT_EQ_U64(String_len(s), 0);
    WC_ASSERT_TRUE(String_empty(s));
    String_destroy(s);
}

static void test_from_String(void)
{
    String* a = String_from_cstr("copy me");
    String* b = String_from_String(a);
    WC_ASSERT_NOT_NULL(b);
    WC_ASSERT_TRUE(String_equals(a, b));
    /* must be independent */
    String_append_cstr(a, "!!!");
    WC_ASSERT_FALSE(String_equals(a, b));
    String_destroy(a);
    String_destroy(b);
}


// Append

static void test_append_cstr(void)
{
    String* s = String_from_cstr("hello");
    String_append_cstr(s, " world");
    WC_ASSERT_EQ_U64(String_len(s), 11);
    WC_ASSERT(String_equals_cstr(s, "hello world"));
    String_destroy(s);
}

static void test_append_char(void)
{
    String* s = String_from_cstr("ab");
    String_append_char(s, 'c');
    WC_ASSERT_EQ_U64(String_len(s), 3);
    WC_ASSERT(String_equals_cstr(s, "abc"));
    String_destroy(s);
}

static void test_append_String(void)
{
    String* a = String_from_cstr("foo");
    String* b = String_from_cstr("bar");
    String_append_String(a, b);
    WC_ASSERT(String_equals_cstr(a, "foobar"));
    /* b must be unchanged */
    WC_ASSERT(String_equals_cstr(b, "bar"));
    String_destroy(a);
    String_destroy(b);
}

/* append to empty */
static void test_append_to_empty(void)
{
    String* s = String_create();
    String_append_cstr(s, "first");
    WC_ASSERT_EQ_U64(String_len(s), 5);
    WC_ASSERT(String_equals_cstr(s, "first"));
    String_destroy(s);
}


// Insert / Remove

static void test_insert_char_front(void)
{
    String* s = String_from_cstr("bc");
    String_insert_char(s, 0, 'a');
    WC_ASSERT(String_equals_cstr(s, "abc"));
    String_destroy(s);
}

static void test_insert_char_mid(void)
{
    String* s = String_from_cstr("ac");
    String_insert_char(s, 1, 'b');
    WC_ASSERT(String_equals_cstr(s, "abc"));
    String_destroy(s);
}

static void test_insert_cstr(void)
{
    String* s = String_from_cstr("helo");
    String_insert_cstr(s, 3, "l");
    WC_ASSERT(String_equals_cstr(s, "hello"));
    String_destroy(s);
}

static void test_remove_char(void)
{
    String* s = String_from_cstr("aXb");
    String_remove_char(s, 1);
    WC_ASSERT_EQ_U64(String_len(s), 2);
    WC_ASSERT(String_equals_cstr(s, "ab"));
    String_destroy(s);
}

static void test_remove_range(void)
{
    String* s = String_from_cstr("aXXXb");
    String_remove_range(s, 1, 3);
    WC_ASSERT_EQ_U64(String_len(s), 2);
    WC_ASSERT(String_equals_cstr(s, "ab"));
    String_destroy(s);
}

static void test_pop_char(void)
{
    String* s = String_from_cstr("abc");
    char c = String_pop_char(s);
    WC_ASSERT_EQ_INT(c, 'c');
    WC_ASSERT_EQ_U64(String_len(s), 2);
    WC_ASSERT(String_equals_cstr(s, "ab"));
    String_destroy(s);
}


// Access

static void test_char_at(void)
{
    String* s = String_from_cstr("xyz");
    WC_ASSERT_EQ_INT(String_char_at(s, 0), 'x');
    WC_ASSERT_EQ_INT(String_char_at(s, 1), 'y');
    WC_ASSERT_EQ_INT(String_char_at(s, 2), 'z');
    String_destroy(s);
}

static void test_set_char(void)
{
    String* s = String_from_cstr("aXc");
    String_set_char(s, 1, 'b');
    WC_ASSERT(String_equals_cstr(s, "abc"));
    String_destroy(s);
}


// Compare / Search

static void test_equals(void)
{
    String* a = String_from_cstr("same");
    String* b = String_from_cstr("same");
    String* c = String_from_cstr("different");
    WC_ASSERT_TRUE(String_equals(a, b));
    WC_ASSERT_FALSE(String_equals(a, c));
    String_destroy(a);
    String_destroy(b);
    String_destroy(c);
}

static void test_compare_ordering(void)
{
    String* a = String_from_cstr("abc");
    String* b = String_from_cstr("abd");
    WC_ASSERT_TRUE(String_compare(a, b) < 0);
    WC_ASSERT_TRUE(String_compare(b, a) > 0);
    WC_ASSERT_EQ_INT(String_compare(a, a), 0);
    String_destroy(a);
    String_destroy(b);
}

static void test_find_char(void)
{
    String* s = String_from_cstr("hello");
    WC_ASSERT_EQ_U64(String_find_char(s, 'e'), 1);
    WC_ASSERT_EQ_U64(String_find_char(s, 'z'), WC_NOT_FOUND);
    String_destroy(s);
}

static void test_find_cstr(void)
{
    String* s = String_from_cstr("hello world");
    WC_ASSERT_EQ_U64(String_find_cstr(s, "world"), 6);
    WC_ASSERT_EQ_U64(String_find_cstr(s, "xyz"),   WC_NOT_FOUND);
    WC_ASSERT_EQ_U64(String_find_cstr(s, ""),       0);
    String_destroy(s);
}

static void test_substr(void)
{
    String* s   = String_from_cstr("hello world");
    String* sub = String_substr(s, 6, 5);
    WC_ASSERT_NOT_NULL(sub);
    WC_ASSERT(String_equals_cstr(sub, "world"));
    String_destroy(s);
    String_destroy(sub);
}


// Copy / Move

static void test_copy_independence(void)
{
    String* a = String_from_cstr("original");
    String  b;
    String_copy(&b, a);
    String_append_cstr(a, "_modified");
    WC_ASSERT_FALSE(String_equals(a, &b));
    WC_ASSERT(String_equals_cstr(&b, "original"));
    String_destroy(a);
    String_destroy_stk(&b);
}

static void test_move_nulls_src(void)
{
    String* src  = String_from_cstr("move me");
    String  dest;
    String_create_stk("", &dest);
    String_move(&dest, &src);
    WC_ASSERT_NULL(src);
    WC_ASSERT(String_equals_cstr(&dest, "move me"));
    String_destroy_stk(&dest);
}


// Misc

static void test_clear(void)
{
    String* s = String_from_cstr("data");
    String_clear(s);
    WC_ASSERT_EQ_U64(String_len(s), 0);
    WC_ASSERT_TRUE(String_empty(s));
    String_destroy(s);
}

static void test_reserve_char(void)
{
    String* s = String_create();
    String_reserve_char(s, 5, 'x');
    WC_ASSERT_EQ_U64(String_len(s), 5);
    WC_ASSERT(String_equals_cstr(s, "xxxxx"));
    String_destroy(s);
}

static void test_to_cstr(void)
{
    String* s    = String_from_cstr("hello");
    char*   cstr = String_to_cstr(s);
    WC_ASSERT_NOT_NULL(cstr);
    WC_ASSERT_EQ_STR(cstr, "hello");
    free(cstr);
    String_destroy(s);
}

// stress: many appends trigger multiple growths
static void test_growth(void)
{
    String* s = String_create();
    for (int i = 0; i < 200; i++) {
        String_append_char(s, 'a');
    }
    WC_ASSERT_EQ_U64(String_len(s), 200);
    String_destroy(s);
}

// test manual strinkage
static void test_shrink(void)
{
    String* s = String_create();

    // grow within sso (usable sso capacity = STR_SSO_SIZE - 1 = 23)
    for (int i = 0; i < 20; i++) {
        String_append_char(s, 'a');
    }
    WC_ASSERT_EQ_U64(String_len(s), 20);
    WC_ASSERT_EQ_U64(String_capacity(s), STR_SSO_SIZE - 1);
    WC_ASSERT_FALSE(!String_is_sso(s));

    // grow over sso

    for (int i = 0; i < 10; i++) {
        String_append_char(s, 'b');
    }
    WC_ASSERT_EQ_U64(String_len(s), 30);
    WC_ASSERT_FALSE(String_is_sso(s));

    // remove 10 (within sso range but no auto shrinkage)
    for (int i = 0; i < 10; i++) {
        String_pop_char(s);
    }
    WC_ASSERT_EQ_U64(String_len(s), 20);
    WC_ASSERT_FALSE(String_is_sso(s));

    // do the manual shrink
    String_shrink_to_fit(s);
    WC_ASSERT_EQ_U64(String_len(s), 20);
    WC_ASSERT_EQ_U64(String_capacity(s), STR_SSO_SIZE - 1);
    WC_ASSERT_FALSE(!String_is_sso(s));


    String_destroy(s);
}

// insert_String

static void test_insert_String_front(void)
{
    String* a = String_from_cstr("world");
    String* b = String_from_cstr("hello ");
    String_insert_String(a, 0, b);
    WC_ASSERT(String_equals_cstr(a, "hello world"));
    String_destroy(a);
    String_destroy(b);
}

static void test_insert_String_end(void)
{
    String* a = String_from_cstr("hello");
    String* b = String_from_cstr(" world");
    String_insert_String(a, String_len(a), b);
    WC_ASSERT(String_equals_cstr(a, "hello world"));
    String_destroy(a);
    String_destroy(b);
}

static void test_insert_String_mid(void)
{
    String* a = String_from_cstr("ac");
    String* b = String_from_cstr("b");
    String_insert_String(a, 1, b);
    WC_ASSERT(String_equals_cstr(a, "abc"));
    String_destroy(a);
    String_destroy(b);
}

static void test_insert_empty_String_noop(void)
{
    String* a = String_from_cstr("hello");
    String* b = String_from_cstr("");
    String_insert_String(a, 0, b);
    WC_ASSERT(String_equals_cstr(a, "hello"));
    WC_ASSERT_EQ_U64(String_len(a), 5);
    String_destroy(a);
    String_destroy(b);
}


// String_to_cstr_buf

static void test_to_cstr_buf_basic(void)
{
    String* s = String_from_cstr("hello");
    char buf[16] = {0};
    String_to_cstr_buf(s, buf, sizeof(buf));
    WC_ASSERT_EQ_STR(buf, "hello");
    String_destroy(s);
}

static void test_to_cstr_buf_nul_terminated(void)
{
    String* s = String_from_cstr("abc");
    char buf[8]; memset(buf, 0xFF, sizeof(buf));
    String_to_cstr_buf(s, buf, 8);
    WC_ASSERT_EQ_INT(buf[3], '\0');
    String_destroy(s);
}

static void test_to_cstr_buf_empty_String(void)
{
    String* s = String_create();
    char buf[8]; memset(buf, 0xFF, sizeof(buf));
    String_to_cstr_buf(s, buf, 8);
    WC_ASSERT_EQ_INT(buf[0], '\0');
    String_destroy(s);
}


// String_data_ptr

static void test_data_ptr_non_empty(void)
{
    String* s = String_from_cstr("hello");
    char* p = String_data_ptr(s);
    WC_ASSERT_NOT_NULL(p);
    WC_ASSERT_EQ_INT(p[0], 'h');
    WC_ASSERT_EQ_INT(p[4], 'o');
    String_destroy(s);
}

static void test_data_ptr_empty_returns_null(void)
{
    String* s = String_create();
    WC_ASSERT_NULL(String_data_ptr(s));
    String_destroy(s);
}

static void test_data_ptr_mutation(void)
{
    String* s = String_from_cstr("hello");
    char* p = String_data_ptr(s);
    p[0] = 'H';
    WC_ASSERT(String_equals_cstr(s, "Hello"));
    String_destroy(s);
}


// TEMP_CSTR_READ macro

static void test_temp_cstr_read(void)
{
    String* s = String_from_cstr("test");
    u64 len_before = String_len(s);
    char captured[16] = {0};

    TEMP_CSTR_READ(s) {
        /* Inside the block, s has a trailing NUL appended */
        WC_ASSERT_EQ_U64(String_len(s), len_before + 1);
        const char* ptr = String_data_ptr(s);
        WC_ASSERT_NOT_NULL(ptr);
        for (u64 i = 0; i < len_before; i++) { captured[i] = ptr[i]; }
        captured[len_before] = '\0';
    }

    /* After block: NUL removed, length restored */
    WC_ASSERT_EQ_U64(String_len(s), len_before);
    WC_ASSERT_EQ_STR(captured, "test");
    String_destroy(s);
}


// String_append_String_move

static void test_append_String_move_nulls_src(void)
{
    String* a = String_from_cstr("hello");
    String* b = String_from_cstr(" world");
    String_append_String_move(a, &b);
    WC_ASSERT_NULL(b);
    WC_ASSERT(String_equals_cstr(a, "hello world"));
    String_destroy(a);
}


// String_copy (stk variant pattern)

static void test_copy_into_heap_String(void)
{
    String* a = String_from_cstr("source");
    String* b = String_create();
    String_copy(b, a);
    WC_ASSERT(String_equals(a, b));
    /* Independence */
    String_append_char(a, '!');
    WC_ASSERT_FALSE(String_equals(a, b));
    String_destroy(a);
    String_destroy(b);
}

static void test_copy_into_non_empty_String(void)
{
    // String_copy must destroy old content of dest before copying
    String* dest = String_from_cstr("old_long_content_here");
    String* src  = String_from_cstr("new");
    String_copy(dest, src);
    WC_ASSERT(String_equals_cstr(dest, "new"));
    String_destroy(dest);
    String_destroy(src);
}

static void test_copy_self_noop(void)
{
    String* s = String_from_cstr("same");
    String_copy(s, s);
    WC_ASSERT(String_equals_cstr(s, "same"));
    String_destroy(s);
}


// SSO boundary

static void test_sso_stays_sso_up_to_limit(void)
{
    String* s = String_create();
    // Usable SSO capacity is STR_SSO_SIZE - 1 (last byte reserved for the mode flag)
    for (int i = 0; i < STR_SSO_SIZE - 1; i++) { String_append_char(s, 'a'); }
    WC_ASSERT_TRUE(String_is_sso(s));
    WC_ASSERT_EQ_U64(String_len(s), (u64)(STR_SSO_SIZE - 1));
    String_destroy(s);
}

static void test_sso_promotes_at_overflow(void)
{
    String* s = String_create();
    for (int i = 0; i < STR_SSO_SIZE; i++) { String_append_char(s, 'b'); }
    WC_ASSERT_FALSE(String_is_sso(s));
    WC_ASSERT_EQ_U64(String_len(s), (u64)STR_SSO_SIZE);
    String_destroy(s);
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
    WC_RUN(test_append_String_move_nulls_src);

    WC_RUN(test_copy_into_heap_String);
    WC_RUN(test_copy_into_non_empty_String);
    WC_RUN(test_copy_self_noop);

    WC_RUN(test_sso_stays_sso_up_to_limit);
    WC_RUN(test_sso_promotes_at_overflow);
}
