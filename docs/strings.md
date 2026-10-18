# Strings

[Back to README](../README.md)

| Type | Header | Owns memory | Use it for |
|---|---|---|---|
| [`String`](#string) | `wc_string.h` | Yes | Building and editing text |
| [`StrView`](#strview) | `views.h` | No | Passing a slice of text around without copying |
| [`StringStore`](#stringstore) | `views.h` | Yes | Many immutable strings that live and die together (not implemented yet) |

## String

A growable byte string with small-string optimization.

- **48 bytes**: a 24-byte union of inline buffer or heap pointer, then `size`, `capacity` and the stored allocator pointer.
- **Up to 23 characters live inline**, with no allocation. The 24th byte is the mode flag. Longer strings move to the heap and grow by `STRING_GROWTH` (1.5×).
- **Not NUL-terminated by default.** `size` is the length. Ask for a terminator when you need one (see [C strings](#c-strings)).
- Every container that holds `String`s by value uses `wc_str_ops`, so strings are deep-copied and destroyed with their container.

```c
#include "wc_string.h"

int main(void)
{
    String s = String_from_cstr(WC_LIBC, "hello", sizeof("hello") - 1);
    String_append_cstr(&s, ", world", sizeof(", world") - 1);
    String_insert_char(&s, 0, '>');                  // ">hello, world"

    u64 at = String_find_cstr(&s, "world", sizeof("world") - 1); // 8, or WC_NOT_FOUND
    String w = String_substr(WC_LIBC, &s, at, 5);    // "world", its own allocation

    String_destroy(&w);
    String_destroy(&s);
    return 0;
}
```

| Group | Functions |
|---|---|
| Create | `String_create(a)`, `String_with_cap(a, cap)` (pre-reserved), `String_from_cstr(a, cstr, clen)` |
| Lifetime | `String_destroy`, `String_copy(a, &src)`, `String_move(&dst, &src)` |
| Borrow | `String_borrow(ptr, len)`: a read-only `String` over existing bytes, no allocation. For lookups in `String`-keyed maps and sets. Never store it; any mutation is fatal. |
| Capacity | `String_reserve(s, cap)`, `String_reserve_char(s, cap, c)` (grow and fill), `String_shrink_to_fit` (back to inline when it fits) |
| Append | `String_append_char`, `String_append_cstr(s, cstr, clen)`, `String_append_String`, `String_append_String_move` (appends, then destroys the source; into an empty string on the same allocator it takes over the source's buffer instead of copying) |
| Insert / remove | `String_insert_char`, `String_insert_cstr(s, i, cstr, clen)`, `String_insert_String`, `String_remove_char`, `String_remove_range(s, start, len)`, `String_pop_char`, `String_clear` |
| Access | `String_char_at(s, i)`, `String_set_char(s, i, c)`, `String_char_at_unsafe` |
| Compare | `String_compare` (like `strcmp`), `String_equals`, `String_equals_cstr(s, cstr, clen)` |
| Search | `String_find_char`, `String_find_cstr(s, cstr, clen)`: index or `WC_NOT_FOUND` |
| Slice | `String_substr(a, s, start, len)` |
| Size | `String_len`, `String_capacity`, `String_empty`, `String_is_sso` |
| Output | `String_print` |

Index and range errors are `WC_ASSERT`s: checked in Debug, unchecked in Release.

Every function that takes a C string also takes an explicit `u64 clen`. It operates on exactly `clen` bytes, so the input does not need to be NUL-terminated and the library never calls `strlen` on it.

### C strings

| Need | Use |
|---|---|
| A pointer to the bytes, length known | `String_data_ptr(s)` with `String_len(s)`. May be `NULL` for an empty string. |
| A NUL-terminated view, no copy | `String_ensure_null_term(s)`, then `String_cstr_view(s)`. Valid until the next change to `s`. |
| An independent copy | `String_to_cstr(a, s)`. Free it with `wc_free(a, p, String_len(s) + 1, 1)`. |
| A copy into your buffer | `String_to_cstr_buf(s, buf, n)`. `n` must be at least `len + 1`. |

```c
#include "wc_string.h"

#include <stdio.h>

int main(void)
{
    String s = String_from_cstr(WC_LIBC, "path/to/file", sizeof("path/to/file") - 1);
    String_ensure_null_term(&s);
    printf("%s\n", String_cstr_view(&s));    // no copy, no allocation
    String_destroy(&s);
    return 0;
}
```

Call `String_cstr_view` only after `String_ensure_null_term`: on a fresh empty string it reads uninitialized memory.

## StrView

A non-owning `{ const char* data; u64 len; }` slice. It never frees anything, and is only valid while the memory it points at is.

A `StrView` is valid when `.data != NULL`. Functions that can fail return `(StrView){0}`; `StrView_is_valid()` tells that error value apart from a successful empty view, which keeps a non-NULL pointer. Slicing out of range returns `{0}` instead of asserting.

| Group | Functions |
|---|---|
| Create | `StrView_over_String(&s)`, `StrView_over_String_ex(&s, off, len)` (sub-range), `StrView_over_cstr(cstr, len)` — all borrow, no allocation |
| Combine | `StrView_slice(sv, start, len)`, `StrView_prefix(sv, n)`, `StrView_suffix(sv, start)` — return `{0}` when the range is out of bounds |
| Trim | `StrView_trim(sv, set, len)`, `StrView_trim_left`, `StrView_trim_right`. The six ASCII whitespace chars are always trimmable; `set` adds trimmable bytes on top. `set == NULL` or `len == 0` leaves only the defaults. |
| Search | `StrView_find(sv, needle)`, `StrView_findr(sv, needle)`, `StrView_find_char(sv, c)`, `StrView_findr_char(sv, c)` — index or `WC_NOT_FOUND`; `StrView_starts_with`, `StrView_ends_with`, `StrView_contains` |
| Advance | A view is a value: read `StrView_prefix(sv, n)`, then move on with `sv = StrView_suffix(sv, n)`. Nothing is mutated in place. |
| Convert | `StrView_to_String(a, sv)` (one allocation), `StrView_to_cstr(a, sv)` (NUL-terminated; free with `wc_free(a, p, len + 1, WC_MAX_ALIGN)`), `StrView_to_buf(sv, buf, cap)` (plain copy, `false` when `cap < len`; not terminated) |
| Compare | `StrView_compare` (like `memcmp` on shared bytes, then length as the tiebreak), `StrView_equals`, `StrView_equals_cstr`, `StrView_hash` |
| Output | `StrView_print` |

```c
#include "arena.h"
#include "views.h"
#include "wc_string.h"

#include <stdio.h>

int main(void)
{
    String  s  = String_from_cstr(WC_LIBC, "  cat/eats.txt  ", sizeof("  cat/eats.txt  ") - 1);
    StrView sv = StrView_trim(StrView_over_String(&s), NULL, 0);        // "cat/eats.txt"

    u64     slash = StrView_find_char(sv, '/');                         // 3
    StrView file  = StrView_suffix(sv, slash + 1);                      // "eats.txt"

    Arena arena;
    Arena_create(&arena, WC_LIBC, nKB(1));
    char* cstr = StrView_to_cstr(Arena_allocator(&arena), file);        // NUL-terminated copy
    printf("%s\n", cstr);                                               // eats.txt
    Arena_destroy(&arena); // cstr dies here, and with it every view of it

    // advance the view: fields come out one by one, no copies
    StrView csv = StrView_over_cstr("1,22,333", 8);
    while (csv.len > 0) {
        u64     stop  = StrView_find_char(csv, ',');
        u64     n     = stop == WC_NOT_FOUND ? csv.len : stop + 1;
        StrView field = StrView_prefix(csv, n);
        StrView_print(field); // "1," then "22," then "333"
        putchar(' ');
        csv = StrView_suffix(csv, n);
    }

    String_destroy(&s);
    return 0;
}
```

## StringStore

Append-only immutable string storage on a `ChainArena`. You hand it bytes, it hands back a `StrView` that stays valid until the store is destroyed. Every stored string is NUL-terminated, so the view is directly usable as a C string.

- **Interning** (`intern = true` at creation): identical bytes return the same view — dedup by content, O(1) amortized.
- **Path storage**: build, join, and normalize paths over stored strings (see `WCPath_*`).

| Function | Notes |
|---|---|
| `StringStore_create(&ss, backing, block_cap, intern)` | |
| `StringStore_destroy(&ss)` | |
| `StringStore_str(&ss, &s)`, `StringStore_cstr(&ss, cstr, clen)`, `StringStore_sv(&ss, sv)` | Copy the bytes in, NUL-terminate, return a view. `clen` is authoritative. |
