#include "arena.h"
#include "common.h"
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
    return (sv.len == 0 || memcmp(sv.data, cstr, clen) == 0) != 0;
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
    EXPECT_TRUE(StrView_equals_cstr(sv, "bor"));
}

UTEST(views, over_String)
{
    String  s  = String_from_cstr(WC_LIBC, "viewme");
    StrView sv = StrView_over_String(&s);

    EXPECT_EQ(sv.len, String_len(&s));
    EXPECT_TRUE(sv.data == String_data_ptr(&s));
    EXPECT_TRUE(sv_equals_cstr(sv, "viewme"));

    String_destroy(&s);
}

// A window over the middle of a String; offset == len gives an empty view

UTEST(views, over_String_ex_window)
{
    String s = String_from_cstr(WC_LIBC, "hello world");

    StrView mid = StrView_over_String_ex(&s, 6, 5);
    EXPECT_TRUE(sv_equals_cstr(mid, "world"));

    StrView empty = StrView_over_String_ex(&s, String_len(&s), 0);
    EXPECT_EQ(empty.len, 0u);

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


// Parse: whole view, digits only, target-type range

UTEST(views, parse_u8_u16)
{
    u8 v8 = 0;
    EXPECT_TRUE(StrView_parse_u8(StrView_over_cstr("255", 3), &v8));
    EXPECT_EQ(v8, 255u);
    EXPECT_TRUE(StrView_parse_u8(StrView_over_cstr("0", 1), &v8));
    EXPECT_EQ(v8, 0u);
    EXPECT_TRUE(StrView_parse_u8(StrView_over_cstr("007", 3), &v8)); // leading zeros allowed
    EXPECT_EQ(v8, 7u);
    EXPECT_FALSE(StrView_parse_u8(StrView_over_cstr("256", 3), &v8)); // overflow

    u16 v16 = 0;
    EXPECT_TRUE(StrView_parse_u16(StrView_over_cstr("65535", 5), &v16));
    EXPECT_EQ(v16, 65535u);
    EXPECT_FALSE(StrView_parse_u16(StrView_over_cstr("65536", 5), &v16));
}

UTEST(views, parse_u32_u64)
{
    u32 v32 = 0;
    EXPECT_TRUE(StrView_parse_u32(StrView_over_cstr("4294967295", 10), &v32));
    EXPECT_EQ(v32, 4294967295u);
    EXPECT_FALSE(StrView_parse_u32(StrView_over_cstr("4294967296", 10), &v32));

    u64 v64 = 0;
    EXPECT_TRUE(StrView_parse_u64(StrView_over_cstr("18446744073709551615", 20), &v64));
    EXPECT_EQ(v64, UINT64_MAX); // no wraparound at the top
    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr("18446744073709551616", 20), &v64));
}

UTEST(views, parse_rejects_junk_keeps_out)
{
    u64 v = 0xABCDEF;

    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr("", 0), &v)); // empty view
    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr(" ", 1), &v));     // whitespace is not trimmed
    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr(" 12", 3), &v));   // leading space
    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr("12 ", 3), &v));   // trailing space
    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr("+12", 3), &v));   // no sign
    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr("-1", 2), &v));    // no sign
    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr("12a", 3), &v));   // trailing junk
    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr("a12", 3), &v));   // leading junk
    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr("1.5", 3), &v));   // no float
    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr("0x10", 4), &v));  // no base prefix
    EXPECT_FALSE(StrView_parse_u64(StrView_over_cstr("1_2", 3), &v));   // separators
    EXPECT_EQ(v, 0xABCDEFu); // *out untouched on every failure

    // an invalid view (zero state) parses as false without dereferencing
    StrView invalid = {0};
    EXPECT_FALSE(StrView_parse_u64(invalid, &v));
    EXPECT_EQ(v, 0xABCDEFu);
}


// Conversion

UTEST(views, to_String_copies)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    StrView sv = StrView_over_cstr("owned copy", 10);
    String  s  = StrView_to_String(al, sv);
    EXPECT_TRUE(String_equals_cstr(&s, "owned copy"));

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


// Consumption: take_left / take_right shorten the source view

UTEST(views, take_left)
{
    u8      buf[] = "abcd";
    StrView sv    = StrView_over_cstr((char*)buf, 4);

    StrView head = StrView_take_left(&sv, 2);
    EXPECT_TRUE(sv_equals_cstr(head, "ab"));
    EXPECT_TRUE(sv_equals_cstr(sv, "cd")); // source moved forward

    StrView none = StrView_take_left(&sv, 0);
    EXPECT_EQ(none.len, 0u);
    EXPECT_EQ(sv.len, 2u); // unchanged

    StrView all = StrView_take_left(&sv, 2);
    EXPECT_TRUE(sv_equals_cstr(all, "cd"));
    EXPECT_EQ(sv.len, 0u);
}

UTEST(views, take_right)
{
    u8      buf[] = "abcd";
    StrView sv    = StrView_over_cstr((char*)buf, 4);

    StrView tail = StrView_take_right(&sv, 2);
    EXPECT_TRUE(sv_equals_cstr(tail, "cd"));
    EXPECT_TRUE(sv_equals_cstr(sv, "ab")); // source shrank

    StrView none = StrView_take_right(&sv, 0);
    EXPECT_EQ(none.len, 0u);
    EXPECT_EQ(sv.len, 2u); // unchanged
}

// Asking for more than there is: invalid view back, source untouched

UTEST(views, take_failure_keeps_source)
{
    StrView sv = StrView_over_cstr("abcd", 4);

    StrView bad_left = StrView_take_left(&sv, 5);
    EXPECT_FALSE(StrView_is_valid(bad_left));
    EXPECT_TRUE(sv_equals_cstr(sv, "abcd")); // still intact

    StrView bad_right = StrView_take_right(&sv, 5);
    EXPECT_FALSE(StrView_is_valid(bad_right));
    EXPECT_TRUE(sv_equals_cstr(sv, "abcd"));

    // after a failure the view still works
    StrView ok = StrView_take_left(&sv, 1);
    EXPECT_TRUE(sv_equals_cstr(ok, "a"));
}

// A small tokenizer loop over components separated by '/'

UTEST(views, take_left_components)
{
    StrView comps[8];
    u64     n = 0;

    StrView rest = StrView_over_cstr("usr/local/bin", 13);
    while (rest.len > 0) {
        u64 slash = StrView_find_char(rest, '/');
        u64 adv   = slash == WC_NOT_FOUND ? rest.len : slash + 1;
        comps[n++] = StrView_take_left(&rest, adv);
    }

    EXPECT_EQ(n, 3u);
    EXPECT_TRUE(sv_equals_cstr(comps[0], "usr/"));
    EXPECT_TRUE(sv_equals_cstr(comps[1], "local/"));
    EXPECT_TRUE(sv_equals_cstr(comps[2], "bin"));
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

    EXPECT_TRUE(StrView_equals_cstr(a, "hello"));
    EXPECT_TRUE(!StrView_equals_cstr(a, "hell")); // cstr is longer than the view
    EXPECT_TRUE(!StrView_equals_cstr(a, "hello world"));
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
