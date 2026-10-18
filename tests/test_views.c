#include "arena.h"
#include "common.h"
#include "utest.h"
#include "views.h"
#include "wc_allocator.h"
#include "wc_string.h"
#include "wc_test_allocator.h"

#include <string.h>


// Helpers

// Content equality of a StrView against a cstr
static int sv_equals_cstr(StrView sv, const char* cstr)
{
    u64 clen = strlen(cstr);
    if (sv.len != clen) {
        return 0;
    }
    return (sv.len == 0 || memcmp(sv.data, cstr, clen) == 0) != 0;
}

// Compile-time view over a string literal. Keeps embedded NULs (sizeof, not strlen).
#define SV_LIT(s) StrView_over_cstr((s), sizeof(s) - 1)

static StrView sv_cstr(const char* s)
{
    return StrView_over_cstr(s, strlen(s));
}

// out == NULL expects the {0} error sentinel; out == "" expects a valid empty view.
static void expect_view(int* utest_result, StrView v, const char* out)
{
    if (out == NULL) {
        EXPECT_FALSE(StrView_is_valid(v));
        return;
    }
    EXPECT_TRUE(StrView_is_valid(v));
    EXPECT_TRUE(sv_equals_cstr(v, out));
}

typedef struct {
    const char* in;
    const char* out;
} PathCase;

typedef struct {
    const char* a;
    const char* b;
    const char* out;
} JoinCase;

static void expect_join_str(int* utest_result, StrView a, StrView b, const char* out)
{
    String s = WCPath_join_str(WC_LIBC, a, b);
    EXPECT_TRUE(String_equals_cstr(&s, out, strlen(out)));
    String_destroy(&s);
}


// Creation

// A view over a cstr borrows the memory, never copies

UTEST(views, over_cstr_borrows)
{
    const char* text = "borrowed";
    StrView     sv   = StrView_over_cstr(text, 3);

    EXPECT_EQ(sv.len, 3u);
    EXPECT_TRUE(sv.data == text);
    EXPECT_TRUE(sv_equals_cstr(sv, "bor"));
    EXPECT_TRUE(StrView_equals_cstr(sv, "bor", strlen("bor")));
}

UTEST(views, over_String)
{
    String  s  = String_from_cstr(WC_LIBC, "viewme", strlen("viewme"));
    StrView sv = StrView_over_String(&s);

    EXPECT_EQ(sv.len, String_len(&s));
    EXPECT_TRUE(sv.data == String_data_ptr(&s));
    EXPECT_TRUE(sv_equals_cstr(sv, "viewme"));

    String_destroy(&s);
}

// An empty String has a NULL data pointer internally; the view over it must
// still be valid, not the {0} error sentinel.

UTEST(views, over_empty_String_is_valid)
{
    String  s  = String_create(WC_LIBC);
    StrView sv = StrView_over_String(&s);

    EXPECT_EQ(sv.len, 0u);
    EXPECT_TRUE(StrView_is_valid(sv));
    EXPECT_TRUE(sv_equals_cstr(sv, ""));

    String_destroy(&s);
}

// A window over the middle of a String; offset == len gives an empty view

UTEST(views, over_String_ex_window)
{
    String s = String_from_cstr(WC_LIBC, "hello world", strlen("hello world"));

    StrView mid = StrView_over_String_ex(&s, 6, 5);
    EXPECT_TRUE(sv_equals_cstr(mid, "world"));

    StrView empty = StrView_over_String_ex(&s, String_len(&s), 0);
    EXPECT_EQ(empty.len, 0u);
    EXPECT_TRUE(StrView_is_valid(empty)); // empty, not an error

    // past the end, or a range that runs over the end: (StrView){0}
    EXPECT_FALSE(StrView_is_valid(StrView_over_String_ex(&s, 12, 0)));
    EXPECT_FALSE(StrView_is_valid(StrView_over_String_ex(&s, 6, 6)));

    String_destroy(&s);
}


// Slicing

UTEST(views, slice_prefix_suffix)
{
    StrView sv = StrView_over_cstr("abcdef", 6);

    StrView mid = StrView_slice(sv, 1, 3);
    EXPECT_TRUE(sv_equals_cstr(mid, "bcd"));
    EXPECT_TRUE(mid.data == sv.data + 1);

    StrView pfx = StrView_prefix(sv, 2);
    EXPECT_TRUE(sv_equals_cstr(pfx, "ab"));
    EXPECT_TRUE(pfx.data == sv.data);

    StrView sfx = StrView_suffix(sv, 3);
    EXPECT_TRUE(sv_equals_cstr(sfx, "def"));
    EXPECT_TRUE(sfx.data == sv.data + 3);
}

// Degenerate windows still work: whole view and empty view

UTEST(views, slice_degenerate_bounds)
{
    StrView sv = StrView_over_cstr("abcdef", 6);

    EXPECT_TRUE(sv_equals_cstr(StrView_slice(sv, 0, 6), "abcdef"));
    EXPECT_TRUE(sv_equals_cstr(StrView_prefix(sv, 6), "abcdef"));
    EXPECT_EQ(StrView_prefix(sv, 6).len, 6u);
    EXPECT_EQ(StrView_prefix(sv, 0).len, 0u);
    EXPECT_EQ(StrView_suffix(sv, 6).len, 0u);
    EXPECT_EQ(StrView_slice(sv, 6, 0).len, 0u);
}


// Trim: whitespace is always active; `set` only adds extra trimmable chars

UTEST(views, trim_default_whitespace)
{
    EXPECT_TRUE(sv_equals_cstr(StrView_trim(StrView_over_cstr(" \t hi \n ", 8), NULL, 0), "hi"));
    EXPECT_TRUE(sv_equals_cstr(StrView_trim(StrView_over_cstr("  hi there  ", 12), NULL, 0), "hi there"));
    EXPECT_TRUE(sv_equals_cstr(StrView_trim(StrView_over_cstr("   ", 3), NULL, 0), ""));
    EXPECT_TRUE(sv_equals_cstr(StrView_trim(StrView_over_cstr("", 0), NULL, 0), ""));
    EXPECT_TRUE(sv_equals_cstr(StrView_trim(StrView_over_cstr("no-trim", 7), NULL, 0), "no-trim"));
    // all six ASCII whitespace chars: \t \n \v \f \r space
    EXPECT_TRUE(sv_equals_cstr(StrView_trim(StrView_over_cstr("\t\n\v\f\r ", 6), NULL, 0), ""));
}

// Left and right trim half of the job each

UTEST(views, trim_left_right_split)
{
    StrView sv = StrView_over_cstr("  ab  ", 6);

    EXPECT_TRUE(sv_equals_cstr(StrView_trim_left(sv, NULL, 0), "ab  "));
    EXPECT_TRUE(sv_equals_cstr(StrView_trim_right(sv, NULL, 0), "  ab"));
}

UTEST(views, trim_custom_set)
{
    StrView path = StrView_over_cstr("///about.html///", 16);

    EXPECT_TRUE(sv_equals_cstr(StrView_trim(path, "/", 1), "about.html"));
    EXPECT_TRUE(sv_equals_cstr(StrView_trim_left(path, "/", 1), "about.html///"));
    EXPECT_TRUE(sv_equals_cstr(StrView_trim_right(path, "/", 1), "///about.html"));

    // defaults still active together with the set
    EXPECT_TRUE(sv_equals_cstr(StrView_trim(StrView_over_cstr(" \t/\tab/ \t", 9), "/", 1), "ab"));

    // set non-null but len 0: only the defaults are active
    EXPECT_TRUE(sv_equals_cstr(StrView_trim(StrView_over_cstr(" a ", 3), "x", 0), "a"));

    // set chars that never match: only the always-active whitespace is trimmed
    EXPECT_TRUE(sv_equals_cstr(StrView_trim(StrView_over_cstr("  keep  ", 8), "yz", 2), "keep"));
}


// Search

UTEST(views, find)
{
    StrView sv = StrView_over_cstr("banana", 6);

    EXPECT_EQ(StrView_find(sv, StrView_over_cstr("na", 2)), 2u);
    EXPECT_EQ(StrView_find(sv, StrView_over_cstr("ban", 3)), 0u);
    EXPECT_EQ(StrView_find(sv, StrView_over_cstr("banana", 6)), 0u);
    EXPECT_EQ(StrView_find(sv, StrView_over_cstr("xyz", 3)), WC_NOT_FOUND);
    EXPECT_EQ(StrView_find(sv, StrView_over_cstr("ana", 3)), 1u); // first of two matches (also at 3)
    EXPECT_EQ(StrView_find(sv, StrView_over_cstr("banana_", 7)), WC_NOT_FOUND); // longer than the view
    EXPECT_EQ(StrView_find(sv, StrView_over_cstr("", 0)), 0u); // empty needle matches at 0
}

UTEST(views, find_char)
{
    StrView sv = StrView_over_cstr("banana", 6);

    EXPECT_EQ(StrView_find_char(sv, 'a'), 1u); // first occurrence
    EXPECT_EQ(StrView_find_char(sv, 'n'), 2u);
    EXPECT_EQ(StrView_find_char(sv, 'z'), WC_NOT_FOUND);
    EXPECT_EQ(StrView_find_char(StrView_over_cstr("", 0), 'a'), WC_NOT_FOUND);
}

UTEST(views, findr)
{
    StrView sv = StrView_over_cstr("banana", 6);

    EXPECT_EQ(StrView_findr(sv, StrView_over_cstr("ana", 3)), 3u); // last occurrence
    EXPECT_EQ(StrView_findr(sv, StrView_over_cstr("ban", 3)), 0u);
    EXPECT_EQ(StrView_findr(sv, StrView_over_cstr("xyz", 3)), WC_NOT_FOUND);
    EXPECT_EQ(StrView_findr(StrView_over_cstr("", 0), StrView_over_cstr("", 0)), 0u);
}

UTEST(views, findr_char)
{
    StrView sv = StrView_over_cstr("banana", 6);

    EXPECT_EQ(StrView_findr_char(sv, 'a'), 5u); // last occurrence
    EXPECT_EQ(StrView_findr_char(sv, 'b'), 0u);
    EXPECT_EQ(StrView_findr_char(sv, 'z'), WC_NOT_FOUND);
    EXPECT_EQ(StrView_findr_char(StrView_over_cstr("", 0), 'a'), WC_NOT_FOUND);
}

UTEST(views, starts_ends_contains)
{
    StrView sv = StrView_over_cstr("hello world", 11);

    EXPECT_TRUE(StrView_starts_with(sv, StrView_over_cstr("hell", 4)));
    EXPECT_TRUE(StrView_starts_with(sv, StrView_over_cstr("hello world", 11)));
    EXPECT_TRUE(StrView_starts_with(sv, StrView_over_cstr("", 0)));
    EXPECT_FALSE(StrView_starts_with(sv, StrView_over_cstr("hello world!", 12)));

    EXPECT_TRUE(StrView_ends_with(sv, StrView_over_cstr("rld", 3)));
    EXPECT_TRUE(StrView_ends_with(sv, StrView_over_cstr("hello world", 11)));
    EXPECT_FALSE(StrView_ends_with(sv, StrView_over_cstr("x", 1)));

    EXPECT_TRUE(StrView_contains(sv, StrView_over_cstr("lo wo", 5)));
    EXPECT_TRUE(StrView_contains(sv, StrView_over_cstr("", 0)));
    EXPECT_FALSE(StrView_contains(sv, StrView_over_cstr("xyz", 3)));
}

// Conversion

UTEST(views, to_String_copies)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StrView sv = StrView_over_cstr("owned copy", 10);
    String  s  = StrView_to_String(al, sv);
    EXPECT_TRUE(String_equals_cstr(&s, "owned copy", strlen("owned copy")));

    String_destroy(&s);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, to_String_empty)
{
    StrView empty = StrView_over_cstr("", 0);
    String  s     = StrView_to_String(WC_LIBC, empty);

    EXPECT_EQ(String_len(&s), 0u);
    EXPECT_TRUE(String_empty(&s));
    String_destroy(&s);
}

UTEST(views, to_cstr_terminates_and_frees)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StrView sv = StrView_over_cstr("cat eats", 5);
    char*   c  = StrView_to_cstr(al, sv);

    EXPECT_TRUE(wc_test_alloc_owns(&ta, c));
    EXPECT_TRUE(memcmp(c, "cat e", 5) == 0);
    EXPECT_TRUE(c[5] == '\0'); // terminated at the view's length, not the cstr's

    wc_free(al, c, sv.len + 1, WC_MAX_ALIGN);
    EXPECT_EQ(ta.n_errors, 0u); // size/align matched the alloc
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, to_cstr_on_arena)
{
    Arena arena;
    Arena_create(&arena, WC_LIBC, 256);

    StrView sv = StrView_over_cstr("arena", 5);
    char*   c  = StrView_to_cstr(Arena_allocator(&arena), sv);

    EXPECT_TRUE((u8*)c >= arena.base && (u8*)c < arena.base + arena.cap);
    EXPECT_TRUE(sv_equals_cstr(StrView_over_cstr(c, 5), "arena"));
    EXPECT_TRUE(c[5] == '\0');

    Arena_destroy(&arena); // the cstr's lifetime ends here
}

// to_buf: plain copy, no terminator; false when the buffer is too small

UTEST(views, to_buf_exact_fit)
{
    StrView sv = StrView_over_cstr("12345", 5);

    char buf[8];
    memset(buf, 0xAA, sizeof(buf));

    EXPECT_TRUE(StrView_to_buf(sv, buf, 5));
    EXPECT_TRUE(memcmp(buf, "12345", 5) == 0);
    EXPECT_TRUE((u8)buf[5] == 0xAA); // no terminator written
    EXPECT_TRUE((u8)buf[7] == 0xAA); // untouched past the view
}

UTEST(views, to_buf_too_small)
{
    StrView sv = StrView_over_cstr("12345", 5);

    char buf[4];
    memset(buf, 0xAA, sizeof(buf));

    EXPECT_FALSE(StrView_to_buf(sv, buf, 4));
    EXPECT_TRUE((u8)buf[0] == 0xAA); // nothing was written
    EXPECT_TRUE((u8)buf[3] == 0xAA);
}

// cap larger than the view: exactly sv.len bytes copied, the rest untouched

UTEST(views, to_buf_larger_cap)
{
    StrView sv = StrView_over_cstr("12345", 5);

    char buf[8];
    memset(buf, 0xAA, sizeof(buf));

    EXPECT_TRUE(StrView_to_buf(sv, buf, 8));
    EXPECT_TRUE(memcmp(buf, "12345", 5) == 0);
    EXPECT_TRUE((u8)buf[5] == 0xAA); // only sv.len bytes moved, no terminator
    EXPECT_TRUE((u8)buf[7] == 0xAA);
}


// Slicing errors: (StrView){0}; successful empty slices stay valid

UTEST(views, slice_error_is_invalid)
{
    StrView sv = StrView_over_cstr("abcd", 4);

    EXPECT_FALSE(StrView_is_valid(StrView_slice(sv, 5, 0)));
    EXPECT_FALSE(StrView_is_valid(StrView_slice(sv, 2, 3)));
    EXPECT_FALSE(StrView_is_valid(StrView_prefix(sv, 5)));
    EXPECT_FALSE(StrView_is_valid(StrView_suffix(sv, 5)));
    EXPECT_FALSE(StrView_is_valid(StrView_slice((StrView){0}, 0, 0)));

    // empty successes keep data != NULL, unlike the {0} error sentinel
    EXPECT_TRUE(StrView_is_valid(StrView_prefix(sv, 0)));
    EXPECT_TRUE(StrView_is_valid(StrView_suffix(sv, 4)));
    EXPECT_TRUE(StrView_is_valid(StrView_slice(sv, 4, 0)));
}

// Splitting is prefix + suffix on a value; nothing is mutated

UTEST(views, split_with_prefix_suffix)
{
    StrView rest = StrView_over_cstr("abcd", 4);

    StrView head = StrView_prefix(rest, 2);
    rest         = StrView_suffix(rest, 2);
    EXPECT_TRUE(sv_equals_cstr(head, "ab"));
    EXPECT_TRUE(sv_equals_cstr(rest, "cd"));

    StrView tail = StrView_suffix(rest, 1);
    EXPECT_TRUE(sv_equals_cstr(tail, "d"));
    EXPECT_TRUE(sv_equals_cstr(rest, "cd")); // the original value is untouched
}

// A small tokenizer loop over components separated by '/'

UTEST(views, suffix_tokenizer)
{
    StrView comps[8];
    u64     n = 0;

    StrView rest = StrView_over_cstr("usr/local/bin", 13);
    while (rest.len > 0) {
        u64 slash = StrView_find_char(rest, '/');
        u64 adv   = slash == WC_NOT_FOUND ? rest.len : slash + 1;
        comps[n++] = StrView_prefix(rest, adv);
        rest       = StrView_suffix(rest, adv);
    }

    EXPECT_EQ(n, 3u);
    EXPECT_TRUE(sv_equals_cstr(comps[0], "usr/"));
    EXPECT_TRUE(sv_equals_cstr(comps[1], "local/"));
    EXPECT_TRUE(sv_equals_cstr(comps[2], "bin"));
}

// Path iteration signals exhaustion with the same {0} sentinel

UTEST(views, path_iter_next)
{
    WCPathIter it = WCpath_components(StrView_over_cstr("/a//b/", 6));

    StrView c = WCpathIter_next(&it);
    EXPECT_TRUE(sv_equals_cstr(c, "a"));

    c = WCpathIter_next(&it);
    EXPECT_TRUE(sv_equals_cstr(c, "b"));

    EXPECT_FALSE(StrView_is_valid(WCpathIter_next(&it)));
}


// Hashing / comparison

UTEST(views, hash_and_equals)
{
    StrView a = StrView_over_cstr("hello", 5);
    StrView b = StrView_over_cstr("hello", 5); // same content, different object
    StrView c = StrView_over_cstr("hellp", 5);

    EXPECT_EQ(StrView_hash(a), StrView_hash(b));
    EXPECT_TRUE(StrView_equals(a, b));
    EXPECT_TRUE(!StrView_equals(a, c));

    StrView zero = StrView_over_cstr("", 0);
    EXPECT_TRUE(StrView_equals(zero, zero));
    EXPECT_TRUE(!StrView_equals(zero, a));

    EXPECT_TRUE(StrView_equals_cstr(a, "hello", strlen("hello")));
    EXPECT_TRUE(!StrView_equals_cstr(a, "hell", strlen("hell"))); // cstr is longer than the view
    EXPECT_TRUE(!StrView_equals_cstr(a, "hello world", strlen("hello world")));
}

// memcmp ordering on shared bytes, then length as the tiebreak

UTEST(views, compare)
{
    StrView abc  = StrView_over_cstr("abc", 3);
    StrView abd  = StrView_over_cstr("abd", 3);
    StrView abcd = StrView_over_cstr("abcd", 4);

    EXPECT_EQ(StrView_compare(abc, abc), 0);
    EXPECT_TRUE(StrView_compare(abc, abd) < 0);
    EXPECT_TRUE(StrView_compare(abd, abc) > 0);
    EXPECT_TRUE(StrView_compare(abc, abcd) < 0); // prefix is the shorter one
    EXPECT_TRUE(StrView_compare(abcd, abc) > 0);

    StrView empty = StrView_over_cstr("", 0);
    EXPECT_TRUE(StrView_compare(empty, abc) < 0);
    EXPECT_TRUE(StrView_compare(abc, empty) > 0);
    EXPECT_EQ(StrView_compare(empty, empty), 0);
}


// Filepaths

UTEST(views, path_valid)
{
    EXPECT_TRUE(WCpath_valid(SV_LIT("a")));
    EXPECT_TRUE(WCpath_valid(SV_LIT("/")));
    EXPECT_TRUE(WCpath_valid(SV_LIT("a/b")));
    EXPECT_TRUE(WCpath_valid(SV_LIT("a/b/")));
    EXPECT_TRUE(WCpath_valid(SV_LIT("../x")));
    EXPECT_TRUE(WCpath_valid(SV_LIT(".")));
    EXPECT_TRUE(WCpath_valid(SV_LIT("..")));

    EXPECT_FALSE(WCpath_valid(SV_LIT(""))); // valid-empty view, but not a valid path
    EXPECT_FALSE(WCpath_valid((StrView){0}));
    EXPECT_FALSE(WCpath_valid(SV_LIT("a\0b"))); // embedded NUL
    EXPECT_FALSE(WCpath_valid(SV_LIT("a//b")));
    EXPECT_FALSE(WCpath_valid(SV_LIT("//")));
    EXPECT_FALSE(WCpath_valid(SV_LIT("///a")));
}

UTEST(views, path_is_absolute)
{
    EXPECT_TRUE(WCpath_is_absolute(SV_LIT("/")));
    EXPECT_TRUE(WCpath_is_absolute(SV_LIT("/a")));
    EXPECT_FALSE(WCpath_is_absolute(SV_LIT("a")));
    EXPECT_FALSE(WCpath_is_absolute(SV_LIT(".")));
    EXPECT_FALSE(WCpath_is_absolute(SV_LIT("..")));
    EXPECT_FALSE(WCpath_is_absolute(SV_LIT("")));
    EXPECT_FALSE(WCpath_is_absolute((StrView){0}));
}

UTEST(views, path_basename)
{
    static const PathCase cases[] = {
        {"a", "a"},
        {"a/", "a"},
        {"a/b", "b"},
        {"a/b/", "b"},
        {"/a/b/c.txt", "c.txt"},
        {"/", "/"},
        {".", "."},
        {"..", ".."},
        {"", NULL},
    };

    for (u64 i = 0; i < (u64)(sizeof(cases) / sizeof(cases[0])); i++) {
        expect_view(utest_result, WCpath_basename(sv_cstr(cases[i].in)), cases[i].out);
    }
}

UTEST(views, path_dirname)
{
    static const PathCase cases[] = {
        {"/a/b/c.txt", "/a/b"},
        {"/a", "/"},
        {"/", "/"},
        {"a/b", "a"},
        {"a/b/", "a"},
        {"a/../b", "a/.."}, // dirname is lexical, it does not normalize
        {"a", ""},
        {"a/", ""},
        {".", ""},
        {"..", ""},
        {"../a", ".."},
        {"./a", "."},
        {"", NULL},
    };

    for (u64 i = 0; i < (u64)(sizeof(cases) / sizeof(cases[0])); i++) {
        expect_view(utest_result, WCpath_dirname(sv_cstr(cases[i].in)), cases[i].out);
    }
}

UTEST(views, path_extension)
{
    static const PathCase cases[] = {
        {"a.txt", "txt"},
        {"a.tar.gz", "gz"},
        {"a", NULL},
        {"Makefile", NULL},
        {".bashrc", NULL},
        {"a.", ""},
        {".", NULL},
        {"..", NULL},
        {"a..b", "b"},
        {"/x/y.c", "c"},
        {"/", NULL},
        {"", NULL},
    };

    for (u64 i = 0; i < (u64)(sizeof(cases) / sizeof(cases[0])); i++) {
        expect_view(utest_result, WCpath_extension(sv_cstr(cases[i].in)), cases[i].out);
    }
}

UTEST(views, path_stem)
{
    static const PathCase cases[] = {
        {"a.txt", "a"},
        {"a.tar.gz", "a.tar"},
        {"Makefile", "Makefile"},
        {".bashrc", ".bashrc"},
        {"..", ".."},
        {"a.", "a"},
        {".", "."},
        {"a..b", "a."},
        {"/", "/"},
        {"/a/b.c", "b"},
        {"a/b/", "b"},
        {"", NULL},
    };

    for (u64 i = 0; i < (u64)(sizeof(cases) / sizeof(cases[0])); i++) {
        expect_view(utest_result, WCpath_stem(sv_cstr(cases[i].in)), cases[i].out);
    }
}

UTEST(views, path_has_extension)
{
    EXPECT_TRUE(WCpath_has_extension(SV_LIT("a.txt"), SV_LIT("txt")));
    EXPECT_TRUE(WCpath_has_extension(SV_LIT("a.txt"), SV_LIT(".txt")));
    EXPECT_TRUE(WCpath_has_extension(SV_LIT("a.tar.gz"), SV_LIT("gz")));
    EXPECT_FALSE(WCpath_has_extension(SV_LIT("a.txt"), SV_LIT("TXT"))); // case-sensitive
    EXPECT_FALSE(WCpath_has_extension(SV_LIT("a.txt"), SV_LIT("md")));
    EXPECT_FALSE(WCpath_has_extension(SV_LIT("a.TXT"), SV_LIT("txt")));

    // empty extension means "has no extension"
    EXPECT_TRUE(WCpath_has_extension(SV_LIT("Makefile"), SV_LIT("")));
    EXPECT_TRUE(WCpath_has_extension(SV_LIT("a."), SV_LIT("")));
    EXPECT_TRUE(WCpath_has_extension(SV_LIT("a."), SV_LIT(".")));
    EXPECT_FALSE(WCpath_has_extension(SV_LIT("a.txt"), SV_LIT("")));
}

UTEST(views, path_components)
{
    WCPathIter it = WCpath_components(SV_LIT("/a/b/c"));
    expect_view(utest_result, WCpathIter_next(&it), "a");
    expect_view(utest_result, WCpathIter_next(&it), "b");
    expect_view(utest_result, WCpathIter_next(&it), "c");
    expect_view(utest_result, WCpathIter_next(&it), NULL);
    expect_view(utest_result, WCpathIter_next(&it), NULL); // stays exhausted

    it = WCpath_components(SV_LIT("/"));
    expect_view(utest_result, WCpathIter_next(&it), NULL);

    it = WCpath_components(SV_LIT(""));
    expect_view(utest_result, WCpathIter_next(&it), NULL);

    it = WCpath_components(SV_LIT("."));
    expect_view(utest_result, WCpathIter_next(&it), ".");
    expect_view(utest_result, WCpathIter_next(&it), NULL);

    it = WCpath_components(SV_LIT(".."));
    expect_view(utest_result, WCpathIter_next(&it), "..");
    expect_view(utest_result, WCpathIter_next(&it), NULL);

    it = WCpath_components(SV_LIT("a/b/"));
    expect_view(utest_result, WCpathIter_next(&it), "a");
    expect_view(utest_result, WCpathIter_next(&it), "b");
    expect_view(utest_result, WCpathIter_next(&it), NULL);

    // Runs of '/' are skipped by the iterator, even though such a path is not valid.
    it = WCpath_components(SV_LIT("a//b"));
    expect_view(utest_result, WCpathIter_next(&it), "a");
    expect_view(utest_result, WCpathIter_next(&it), "b");
    expect_view(utest_result, WCpathIter_next(&it), NULL);

    it = WCpath_components((StrView){0});
    expect_view(utest_result, WCpathIter_next(&it), NULL);
}


// Path joining

UTEST(views, path_join_str)
{
    expect_join_str(utest_result, SV_LIT("a"), SV_LIT("b"), "a/b");
    expect_join_str(utest_result, SV_LIT("a/"), SV_LIT("b"), "a/b");
    expect_join_str(utest_result, SV_LIT("a"), SV_LIT("/b"), "/b");
    expect_join_str(utest_result, SV_LIT("x/y"), SV_LIT("/z"), "/z");
    expect_join_str(utest_result, SV_LIT(""), SV_LIT("b"), "b");
    expect_join_str(utest_result, SV_LIT("a"), SV_LIT(""), "a");
    expect_join_str(utest_result, SV_LIT("/"), SV_LIT("a"), "/a");
    expect_join_str(utest_result, SV_LIT("/a"), SV_LIT(""), "/a");
    expect_join_str(utest_result, SV_LIT(""), SV_LIT(""), "");
    expect_join_str(utest_result, SV_LIT("a/b"), SV_LIT("c"), "a/b/c");
    expect_join_str(utest_result, SV_LIT("a/b/"), SV_LIT("c"), "a/b/c");
    expect_join_str(utest_result, SV_LIT("a"), SV_LIT("b/"), "a/b/");
    expect_join_str(utest_result, SV_LIT("/"), SV_LIT(""), "/");
    expect_join_str(utest_result, SV_LIT(""), SV_LIT("/a"), "/a");
}

UTEST(views, path_join_str_cleanup)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    String s = WCPath_join_str(al, SV_LIT("a"), SV_LIT("b"));
    EXPECT_TRUE(String_equals_cstr(&s, "a/b", strlen("a/b")));
    String_destroy(&s);

    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, path_join_sv)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StringStore ss;
    StringStore_create(&ss, al, 128, false);

    static const JoinCase cases[] = {
        {"a", "b", "a/b"},
        {"a/", "b", "a/b"},
        {"a", "/b", "/b"},
        {"x/y", "/z", "/z"},
        {"", "b", "b"},
        {"a", "", "a"},
        {"/", "a", "/a"},
        {"/a", "", "/a"},
        {"", "", ""},
        {"a/b", "c", "a/b/c"},
        {"a/b/", "c", "a/b/c"},
        {"a", "b/", "a/b/"},
        {"/", "", "/"},
        {"", "/a", "/a"},
    };

    for (u64 i = 0; i < (u64)(sizeof(cases) / sizeof(cases[0])); i++) {
        StrView v = WCPath_join_sv(&ss, sv_cstr(cases[i].a), sv_cstr(cases[i].b));
        expect_view(utest_result, v, cases[i].out);
        EXPECT_EQ(v.data[v.len], (char)0);
    }

    StringStore_destroy(&ss);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}


// Normalization

UTEST(views, path_normalize)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StringStore ss;
    StringStore_create(&ss, al, 64, false);

    static const PathCase cases[] = {
        {"a//b/", "a/b"},
        {"a//b", "a/b"},
        {"a/./b", "a/b"},
        {"a/././b", "a/b"},
        {"a/b/../c", "a/c"},
        {"/../a", "/a"},
        {"../../a", "../../a"},
        {"a/..", "."},
        {"/..", "/"},
        {"", "."},
        {".", "."},
        {"/", "/"},
        {"/a/b/../..", "/"},
        {"a/b/../../..", ".."},
        {"a/../..", ".."},
        {"../a", "../a"},
        {"a/b/", "a/b"},
        {"a/b/../../c", "c"},
    };

    for (u64 i = 0; i < (u64)(sizeof(cases) / sizeof(cases[0])); i++) {
        StrView v = WCPath_normalize(&ss, sv_cstr(cases[i].in));
        expect_view(utest_result, v, cases[i].out);
        EXPECT_EQ(v.data[v.len], (char)0);
    }

    StringStore_destroy(&ss);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}


// StringStore

UTEST(views, stringstore_create_destroy)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StringStore ss;
    StringStore_create(&ss, al, 64, false);
    StringStore_destroy(&ss);

    StringStore_create(&ss, al, 64, true);
    StringStore_destroy(&ss);

    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, stringstore_str)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StringStore ss;
    StringStore_create(&ss, al, 64, false);

    char longbuf[65];
    memset(longbuf, 'a', 64);
    longbuf[64] = 0;
    String heap = String_from_cstr(WC_LIBC, longbuf, strlen(longbuf));

    StrView h = StringStore_str(&ss, &heap);
    EXPECT_EQ(h.len, 64u);
    EXPECT_TRUE(memcmp(h.data, longbuf, 64) == 0);
    EXPECT_EQ(h.data[h.len], (char)0);
    String_destroy(&heap);
    EXPECT_TRUE(memcmp(h.data, longbuf, 64) == 0); // stored copy survives the source

    String sso = String_from_cstr(WC_LIBC, "xy", strlen("xy"));
    StrView s = StringStore_str(&ss, &sso);
    EXPECT_TRUE(sv_equals_cstr(s, "xy"));
    EXPECT_EQ(s.data[s.len], (char)0);
    String_destroy(&sso);

    // An empty String has a NULL data pointer; the store must still make a
    // valid, NUL-terminated empty view.
    String empty = String_create(WC_LIBC);
    StrView e = StringStore_str(&ss, &empty);
    EXPECT_TRUE(StrView_is_valid(e));
    EXPECT_EQ(e.len, 0u);
    EXPECT_EQ(e.data[0], (char)0);
    String_destroy(&empty);

    StringStore_destroy(&ss);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, stringstore_cstr)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StringStore ss;
    StringStore_create(&ss, al, 64, false);

    // `clen` is authoritative: copy exactly that many bytes and add the NUL.
    char buf[4] = {'a', 'b', 'c', 'X'};
    StrView v = StringStore_cstr(&ss, buf, 3);
    EXPECT_TRUE(sv_equals_cstr(v, "abc"));
    EXPECT_EQ(v.len, 3u);
    EXPECT_EQ(v.data[3], (char)0);
    EXPECT_NE(v.data[3], (char)'X'); // the byte after clen was not copied

    StrView w = StringStore_cstr(&ss, "hello", 5);
    EXPECT_TRUE(sv_equals_cstr(w, "hello"));
    EXPECT_EQ(w.data[5], (char)0);

    StrView e = StringStore_cstr(&ss, "", 0);
    EXPECT_TRUE(StrView_is_valid(e));
    EXPECT_EQ(e.len, 0u);
    EXPECT_EQ(e.data[0], (char)0);

    StringStore_destroy(&ss);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, stringstore_sv)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StringStore ss;
    StringStore_create(&ss, al, 64, false);

    StrView v = StringStore_sv(&ss, SV_LIT("abc"));
    EXPECT_TRUE(sv_equals_cstr(v, "abc"));
    EXPECT_EQ(v.data[v.len], (char)0);

    StrView e = StringStore_sv(&ss, SV_LIT(""));
    EXPECT_TRUE(StrView_is_valid(e));
    EXPECT_EQ(e.len, 0u);
    EXPECT_EQ(e.data[0], (char)0);

    StrView bad = StringStore_sv(&ss, (StrView){0});
    EXPECT_FALSE(StrView_is_valid(bad));

    StringStore_destroy(&ss);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, stringstore_no_intern)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StringStore ss;
    StringStore_create(&ss, al, 64, false);

    StrView a = StringStore_sv(&ss, SV_LIT("x"));
    StrView b = StringStore_sv(&ss, SV_LIT("x"));
    EXPECT_TRUE(sv_equals_cstr(a, "x"));
    EXPECT_TRUE(sv_equals_cstr(b, "x"));
    EXPECT_TRUE(a.data != b.data);

    StringStore_destroy(&ss);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, stringstore_intern)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StringStore ss;
    StringStore_create(&ss, al, 64, true);

    StrView a = StringStore_sv(&ss, SV_LIT("x"));
    StrView b = StringStore_sv(&ss, SV_LIT("x"));
    EXPECT_TRUE(a.data == b.data);
    EXPECT_EQ(a.len, b.len);

    StrView c = StringStore_sv(&ss, SV_LIT("y"));
    EXPECT_TRUE(a.data != c.data);

    // Interning is shared by all three entry points.
    StrView d = StringStore_cstr(&ss, "x", 1);
    EXPECT_TRUE(a.data == d.data);

    String s = String_from_cstr(WC_LIBC, "x", strlen("x"));
    StrView e = StringStore_str(&ss, &s);
    EXPECT_TRUE(a.data == e.data);
    String_destroy(&s);

    // Empty strings dedupe too, including the NULL-data String path.
    StrView e1 = StringStore_sv(&ss, SV_LIT(""));
    StrView e2 = StringStore_cstr(&ss, "", 0);
    String  empty = String_create(WC_LIBC);
    StrView e3 = StringStore_str(&ss, &empty);
    EXPECT_TRUE(e1.data == e2.data);
    EXPECT_TRUE(e1.data == e3.data);
    EXPECT_TRUE(StrView_is_valid(e1));
    EXPECT_EQ(e1.len, 0u);
    EXPECT_EQ(e1.data[0], (char)0);
    String_destroy(&empty);

    StringStore_destroy(&ss);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, stringstore_large)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StringStore ss;
    StringStore_create(&ss, al, 64, false); // force a multi-block allocation

    char big[201];
    memset(big, 'a', 200);
    big[200] = 0;

    StrView v = StringStore_cstr(&ss, big, 200);
    EXPECT_EQ(v.len, 200u);
    EXPECT_TRUE(memcmp(v.data, big, 200) == 0);
    EXPECT_EQ(v.data[v.len], (char)0);

    StringStore_destroy(&ss);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}


// Edge cases around the {0} sentinel and empty views

UTEST(views, invalid_propagation)
{
    StrView bad = {0};

    EXPECT_FALSE(StrView_is_valid(StrView_trim(bad, NULL, 0)));
    EXPECT_FALSE(StrView_is_valid(StrView_trim_left(bad, NULL, 0)));
    EXPECT_FALSE(StrView_is_valid(StrView_trim_right(bad, NULL, 0)));
    EXPECT_FALSE(StrView_is_valid(StrView_slice(bad, 0, 0)));
    EXPECT_FALSE(StrView_is_valid(StrView_prefix(bad, 0)));
    EXPECT_FALSE(StrView_is_valid(StrView_suffix(bad, 0)));

    // Trimming everything away is a valid empty view, not an error.
    StrView ws = StrView_trim(SV_LIT(" \t"), NULL, 0);
    EXPECT_TRUE(StrView_is_valid(ws));
    EXPECT_EQ(ws.len, 0u);

    // Equality, comparison and hashing are content-based, so an invalid empty
    // view compares equal to a valid empty view. Validity is checked separately.
    EXPECT_TRUE(StrView_equals(bad, SV_LIT("")));
    EXPECT_EQ(StrView_compare(bad, SV_LIT("")), 0);
    EXPECT_EQ(StrView_hash(bad), StrView_hash(SV_LIT("")));
}

UTEST(views, over_String_ex_overflow)
{
    String s = String_from_cstr(WC_LIBC, "abc", strlen("abc"));

    EXPECT_FALSE(StrView_is_valid(StrView_over_String_ex(&s, (u64)-1, 0)));
    EXPECT_FALSE(StrView_is_valid(StrView_over_String_ex(&s, 1, (u64)-1)));

    String_destroy(&s);
}

UTEST(views, slice_overflow)
{
    StrView sv = SV_LIT("abcd");

    EXPECT_FALSE(StrView_is_valid(StrView_slice(sv, (u64)-1, 0)));
    EXPECT_FALSE(StrView_is_valid(StrView_slice(sv, 2, (u64)-1)));
    EXPECT_FALSE(StrView_is_valid(StrView_prefix(sv, (u64)-1)));
    EXPECT_FALSE(StrView_is_valid(StrView_suffix(sv, (u64)-1)));
}

UTEST(views, to_String_invalid)
{
    String s = StrView_to_String(WC_LIBC, (StrView){0});

    EXPECT_EQ(String_len(&s), 0u);
    EXPECT_TRUE(String_empty(&s));

    String_destroy(&s);
}

UTEST(views, to_cstr_invalid_and_empty)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    char* c = StrView_to_cstr(al, (StrView){0});
    EXPECT_TRUE(wc_test_alloc_owns(&ta, c));
    EXPECT_EQ(c[0], (char)0);
    wc_free(al, c, 1, WC_MAX_ALIGN);

    char* e = StrView_to_cstr(al, SV_LIT(""));
    EXPECT_EQ(e[0], (char)0);
    wc_free(al, e, 1, WC_MAX_ALIGN);

    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(views, to_buf_zero_cap)
{
    char buf[1] = {(char)0x7f};

    EXPECT_TRUE(StrView_to_buf(SV_LIT(""), buf, 0));
    EXPECT_TRUE(StrView_to_buf((StrView){0}, buf, 0));
    EXPECT_EQ(buf[0], (char)0x7f); // nothing written

    EXPECT_FALSE(StrView_to_buf(SV_LIT("a"), buf, 0));
    EXPECT_EQ(buf[0], (char)0x7f);
}

UTEST(views, findr_empty_needle)
{
    EXPECT_EQ(StrView_find(SV_LIT("banana"), SV_LIT("")), 0u);
    EXPECT_EQ(StrView_findr(SV_LIT("banana"), SV_LIT("")), 6u);
    EXPECT_EQ(StrView_findr(SV_LIT(""), SV_LIT("")), 0u);
    EXPECT_EQ(StrView_findr(SV_LIT("banana"), SV_LIT("banana_")), WC_NOT_FOUND);
}

UTEST(views, starts_ends_empty)
{
    EXPECT_TRUE(StrView_starts_with(SV_LIT("abc"), SV_LIT("")));
    EXPECT_TRUE(StrView_ends_with(SV_LIT("abc"), SV_LIT("")));
    EXPECT_TRUE(StrView_contains(SV_LIT("abc"), SV_LIT("")));
    EXPECT_FALSE(StrView_starts_with(SV_LIT("abc"), SV_LIT("abcd")));
    EXPECT_FALSE(StrView_ends_with(SV_LIT("abc"), SV_LIT("abcd")));
}

UTEST(views, strview_cmp_callback)
{
    StrView a = SV_LIT("abc");
    StrView b = SV_LIT("abd");
    StrView c = SV_LIT("abc");

    EXPECT_EQ(wc_strview_cmp(&a, &c, (u64)sizeof(StrView)), 0);
    EXPECT_LT(wc_strview_cmp(&a, &b, (u64)sizeof(StrView)), 0);
    EXPECT_GT(wc_strview_cmp(&b, &a, (u64)sizeof(StrView)), 0);
}
