#include "wc_test.h"
#include "String.h"


/* ── Construction ────────────────────────────────────────────────────────── */

static void test_create_empty(void)
{
    String* s = string_create();
    WC_ASSERT_NOT_NULL(s);
    WC_ASSERT_EQ_U64(string_len(s), 0);
    WC_ASSERT_TRUE(string_empty(s));
    string_destroy(s);
}

static void test_create_stk(void)
{
    String s;
    string_create_stk(&s, "hello");
    WC_ASSERT_EQ_U64(string_len(&s), 5);
    WC_ASSERT(string_equals_cstr(&s, "hello"));
    string_destroy_stk(&s);
}

static void test_from_cstr(void)
{
    String* s = string_from_cstr("world");
    WC_ASSERT_NOT_NULL(s);
    WC_ASSERT_EQ_U64(string_len(s), 5);
    WC_ASSERT(string_equals_cstr(s, "world"));
    string_destroy(s);
}

static void test_from_cstr_empty(void)
{
    String* s = string_from_cstr("");
    WC_ASSERT_NOT_NULL(s);
    WC_ASSERT_EQ_U64(string_len(s), 0);
    WC_ASSERT_TRUE(string_empty(s));
    string_destroy(s);
}

static void test_from_string(void)
{
    String* a = string_from_cstr("copy me");
    String* b = string_from_string(a);
    WC_ASSERT_NOT_NULL(b);
    WC_ASSERT_TRUE(string_equals(a, b));
    /* must be independent */
    string_append_cstr(a, "!!!");
    WC_ASSERT_FALSE(string_equals(a, b));
    string_destroy(a);
    string_destroy(b);
}


/* ── Append ──────────────────────────────────────────────────────────────── */

static void test_append_cstr(void)
{
    String* s = string_from_cstr("hello");
    string_append_cstr(s, " world");
    WC_ASSERT_EQ_U64(string_len(s), 11);
    WC_ASSERT(string_equals_cstr(s, "hello world"));
    string_destroy(s);
}

static void test_append_char(void)
{
    String* s = string_from_cstr("ab");
    string_append_char(s, 'c');
    WC_ASSERT_EQ_U64(string_len(s), 3);
    WC_ASSERT(string_equals_cstr(s, "abc"));
    string_destroy(s);
}

static void test_append_string(void)
{
    String* a = string_from_cstr("foo");
    String* b = string_from_cstr("bar");
    string_append_string(a, b);
    WC_ASSERT(string_equals_cstr(a, "foobar"));
    /* b must be unchanged */
    WC_ASSERT(string_equals_cstr(b, "bar"));
    string_destroy(a);
    string_destroy(b);
}

/* append to empty */
static void test_append_to_empty(void)
{
    String* s = string_create();
    string_append_cstr(s, "first");
    WC_ASSERT_EQ_U64(string_len(s), 5);
    WC_ASSERT(string_equals_cstr(s, "first"));
    string_destroy(s);
}


/* ── Insert / Remove ─────────────────────────────────────────────────────── */

static void test_insert_char_front(void)
{
    String* s = string_from_cstr("bc");
    string_insert_char(s, 0, 'a');
    WC_ASSERT(string_equals_cstr(s, "abc"));
    string_destroy(s);
}

static void test_insert_char_mid(void)
{
    String* s = string_from_cstr("ac");
    string_insert_char(s, 1, 'b');
    WC_ASSERT(string_equals_cstr(s, "abc"));
    string_destroy(s);
}

static void test_insert_cstr(void)
{
    String* s = string_from_cstr("helo");
    string_insert_cstr(s, 3, "l");
    WC_ASSERT(string_equals_cstr(s, "hello"));
    string_destroy(s);
}

static void test_remove_char(void)
{
    String* s = string_from_cstr("aXb");
    string_remove_char(s, 1);
    WC_ASSERT_EQ_U64(string_len(s), 2);
    WC_ASSERT(string_equals_cstr(s, "ab"));
    string_destroy(s);
}

static void test_remove_range(void)
{
    String* s = string_from_cstr("aXXXb");
    string_remove_range(s, 1, 3);
    WC_ASSERT_EQ_U64(string_len(s), 2);
    WC_ASSERT(string_equals_cstr(s, "ab"));
    string_destroy(s);
}

static void test_pop_char(void)
{
    String* s = string_from_cstr("abc");
    char c = string_pop_char(s);
    WC_ASSERT_EQ_INT(c, 'c');
    WC_ASSERT_EQ_U64(string_len(s), 2);
    WC_ASSERT(string_equals_cstr(s, "ab"));
    string_destroy(s);
}


/* ── Access ──────────────────────────────────────────────────────────────── */

static void test_char_at(void)
{
    String* s = string_from_cstr("xyz");
    WC_ASSERT_EQ_INT(string_char_at(s, 0), 'x');
    WC_ASSERT_EQ_INT(string_char_at(s, 1), 'y');
    WC_ASSERT_EQ_INT(string_char_at(s, 2), 'z');
    string_destroy(s);
}

static void test_set_char(void)
{
    String* s = string_from_cstr("aXc");
    string_set_char(s, 1, 'b');
    WC_ASSERT(string_equals_cstr(s, "abc"));
    string_destroy(s);
}


/* ── Compare / Search ────────────────────────────────────────────────────── */

static void test_equals(void)
{
    String* a = string_from_cstr("same");
    String* b = string_from_cstr("same");
    String* c = string_from_cstr("different");
    WC_ASSERT_TRUE(string_equals(a, b));
    WC_ASSERT_FALSE(string_equals(a, c));
    string_destroy(a);
    string_destroy(b);
    string_destroy(c);
}

static void test_compare_ordering(void)
{
    String* a = string_from_cstr("abc");
    String* b = string_from_cstr("abd");
    WC_ASSERT_TRUE(string_compare(a, b) < 0);
    WC_ASSERT_TRUE(string_compare(b, a) > 0);
    WC_ASSERT_EQ_INT(string_compare(a, a), 0);
    string_destroy(a);
    string_destroy(b);
}

static void test_find_char(void)
{
    String* s = string_from_cstr("hello");
    WC_ASSERT_EQ_U64(string_find_char(s, 'e'), 1);
    WC_ASSERT_EQ_U64(string_find_char(s, 'z'), (u64)-1);
    string_destroy(s);
}

static void test_find_cstr(void)
{
    String* s = string_from_cstr("hello world");
    WC_ASSERT_EQ_U64(string_find_cstr(s, "world"), 6);
    WC_ASSERT_EQ_U64(string_find_cstr(s, "xyz"),   (u64)-1);
    WC_ASSERT_EQ_U64(string_find_cstr(s, ""),       0);
    string_destroy(s);
}

static void test_substr(void)
{
    String* s   = string_from_cstr("hello world");
    String* sub = string_substr(s, 6, 5);
    WC_ASSERT_NOT_NULL(sub);
    WC_ASSERT(string_equals_cstr(sub, "world"));
    string_destroy(s);
    string_destroy(sub);
}


/* ── Copy / Move ─────────────────────────────────────────────────────────── */

static void test_copy_independence(void)
{
    String* a = string_from_cstr("original");
    String  b;
    string_copy(&b, a);
    string_append_cstr(a, "_modified");
    WC_ASSERT_FALSE(string_equals(a, &b));
    WC_ASSERT(string_equals_cstr(&b, "original"));
    string_destroy(a);
    string_destroy_stk(&b);
}

static void test_move_nulls_src(void)
{
    String* src  = string_from_cstr("move me");
    String  dest;
    string_create_stk(&dest, "");
    string_move(&dest, &src);
    WC_ASSERT_NULL(src);
    WC_ASSERT(string_equals_cstr(&dest, "move me"));
    string_destroy_stk(&dest);
}


/* ── Misc ────────────────────────────────────────────────────────────────── */

static void test_clear(void)
{
    String* s = string_from_cstr("data");
    string_clear(s);
    WC_ASSERT_EQ_U64(string_len(s), 0);
    WC_ASSERT_TRUE(string_empty(s));
    string_destroy(s);
}

static void test_reserve_char(void)
{
    String* s = string_create();
    string_reserve_char(s, 5, 'x');
    WC_ASSERT_EQ_U64(string_len(s), 5);
    WC_ASSERT(string_equals_cstr(s, "xxxxx"));
    string_destroy(s);
}

static void test_to_cstr(void)
{
    String* s    = string_from_cstr("hello");
    char*   cstr = string_to_cstr(s);
    WC_ASSERT_NOT_NULL(cstr);
    WC_ASSERT_EQ_STR(cstr, "hello");
    free(cstr);
    string_destroy(s);
}

/* stress: many appends trigger multiple growths */
static void test_growth(void)
{
    String* s = string_create();
    for (int i = 0; i < 200; i++) {
        string_append_char(s, 'a');
    }
    WC_ASSERT_EQ_U64(string_len(s), 200);
    string_destroy(s);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void string_suite(void)
{
    WC_SUITE("String");

    /* construction */
    WC_RUN(test_create_empty);
    WC_RUN(test_create_stk);
    WC_RUN(test_from_cstr);
    WC_RUN(test_from_cstr_empty);
    WC_RUN(test_from_string);

    /* append */
    WC_RUN(test_append_cstr);
    WC_RUN(test_append_char);
    WC_RUN(test_append_string);
    WC_RUN(test_append_to_empty);

    /* insert / remove */
    WC_RUN(test_insert_char_front);
    WC_RUN(test_insert_char_mid);
    WC_RUN(test_insert_cstr);
    WC_RUN(test_remove_char);
    WC_RUN(test_remove_range);
    WC_RUN(test_pop_char);

    /* access */
    WC_RUN(test_char_at);
    WC_RUN(test_set_char);

    /* compare / search */
    WC_RUN(test_equals);
    WC_RUN(test_compare_ordering);
    WC_RUN(test_find_char);
    WC_RUN(test_find_cstr);
    WC_RUN(test_substr);

    /* copy / move */
    WC_RUN(test_copy_independence);
    WC_RUN(test_move_nulls_src);

    /* misc */
    WC_RUN(test_clear);
    WC_RUN(test_reserve_char);
    WC_RUN(test_to_cstr);
    WC_RUN(test_growth);
}
