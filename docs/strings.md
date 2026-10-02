# Strings

[Back to README](../README.md)

| Type | Header | Owns memory | Use it for |
|---|---|---|---|
| [`String`](#string) | `wc_string.h` | Yes | Building and editing text |
| [`StrView`](#strview) | `views.h` | No | Passing a slice of text around without copying |
| [`StringStore`](#stringstore) | `views.h` | Yes | Many immutable strings that live and die together |

## String

A growable byte string with small-string optimization.

- **64 bytes**, one cache line: a 32-byte union of inline buffer or heap pointer, then `size`, `capacity` and the stored allocator.
- **Up to 31 characters live inline**, with no allocation. The 32nd byte is the mode flag. Longer strings move to the heap and grow by `STRING_GROWTH` (1.5×).
- **Not NUL-terminated by default.** `size` is the length. Ask for a terminator when you need one (see [C strings](#c-strings)).
- Every container that holds `String`s by value uses `wc_str_ops`, so strings are deep-copied and destroyed with their container.

```c
#include "wc_string.h"

int main(void)
{
    String s = String_from_cstr(WC_LIBC, "hello");
    String_append_cstr(&s, ", world");
    String_insert_char(&s, 0, '>');                  // ">hello, world"

    u64 at = String_find_cstr(&s, "world");          // 8, or WC_NOT_FOUND
    String w = String_substr(WC_LIBC, &s, at, 5);    // "world", its own allocation

    String_destroy(&w);
    String_destroy(&s);
    return 0;
}
```

| Group | Functions |
|---|---|
| Create | `String_create(a)`, `String_from_cstr(a, cstr)`, `String_from_String(a, &other)` |
| Lifetime | `String_destroy`, `String_copy(a, &src)`, `String_move(&dst, &src)` |
| Capacity | `String_reserve(s, cap)`, `String_reserve_char(s, cap, c)` (grow and fill), `String_shrink_to_fit` (back to inline when it fits) |
| Append | `String_append_char`, `String_append_cstr`, `String_append_String`, `String_append_String_move` (appends, then destroys the source) |
| Insert / remove | `String_insert_char`, `String_insert_cstr`, `String_insert_String`, `String_remove_char`, `String_remove_range(s, start, len)`, `String_pop_char`, `String_clear` |
| Access | `String_char_at(s, i)`, `String_set_char(s, i, c)`, `String_char_at_unsafe` |
| Compare | `String_compare` (like `strcmp`), `String_equals`, `String_equals_cstr` |
| Search | `String_find_char`, `String_find_cstr`: index or `WC_NOT_FOUND` |
| Slice | `String_substr(a, s, start, len)` |
| Size | `String_len`, `String_capacity`, `String_empty`, `String_is_sso` |
| Output | `String_print` |

Index and range errors are `WC_ASSERT`s: checked in Debug, unchecked in Release.

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
    String s = String_from_cstr(WC_LIBC, "path/to/file");
    String_ensure_null_term(&s);
    printf("%s\n", String_cstr_view(&s));    // no copy, no allocation
    String_destroy(&s);
    return 0;
}
```

Call `String_cstr_view` only after `String_ensure_null_term`: on a fresh empty string it reads uninitialized memory.

## StrView

A non-owning `{ const char* ptr; u64 len; }` slice. It never frees anything, and is only valid while the memory it points at is.

| Function | Allocates | Notes |
|---|---|---|
| `StrView_from_String(&s)` | No | Invalid once `s` changes or is destroyed |
| `StrView_from_String_ex(&s, off, len)` | No | Sub-range; `off + len` must be within `s` |
| `StrView_from_cstr(cstr, len)` | No | `cstr` must outlive the view |
| `StrView_copy_cstr(a, cstr, len)` | Yes | Copies `len` bytes plus a NUL from `a`. The view lives as long as that allocation. |
| `StrView_free_copy(a, sv)` | | Frees a `StrView_copy_cstr` result. No-op on arenas. |
| `StrView_print(sv)` | | |

```c
#include "arena.h"
#include "views.h"

int main(void)
{
    // On an arena, copied views need no individual free
    ARENA_SCOPE(tmp, 1024) {
        StrView a = StrView_copy_cstr(tmp, "interned", 8);
        StrView_print(a);
    }

    // On libc, free each copy with the same allocator
    StrView b = StrView_copy_cstr(WC_LIBC, "owned", 5);
    StrView_free_copy(WC_LIBC, b);
    return 0;
}
```

## StringStore

An append-only store: you hand it bytes, it hands back a `StrView` that stays valid until the store is destroyed. All strings die together, like an arena dedicated to text.

- Strings are packed into 1 KB nodes (1015 usable bytes each). A new node is added when the current one can't fit the next string.
- A string longer than a node gets its own overflow allocation.
- Views are **not NUL-terminated**: use `ptr` and `len`.
- Nodes and overflow buffers come from the store's allocator.

```c
#include "views.h"

int main(void)
{
    StringStore ss = StringStore_create(WC_LIBC);
    StrView     a  = StringStore_cstr(&ss, "alpha", 5);
    StrView     b  = StringStore_cstr(&ss, "beta", 4);   // packed right after "alpha"
    StrView_print(a);
    StrView_print(b);
    StringStore_destroy(&ss);                             // every view dies here
    return 0;
}
```

| Function | Notes |
|---|---|
| `StringStore_create(a)` | Allocates the first node |
| `StringStore_cstr(&ss, cstr, len)` | Copies `len` bytes in, returns a view |
| `StringStore_destroy(&ss)` | Frees every node; safe on a zeroed store |

`StringStore_append`, `StringStore_append_cstr`, `StringStore_path_join` and `StringStore_path_join_cstr` are declared in `views.h` but **not implemented yet**. Don't call them.
