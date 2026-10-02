# Macros

[Back to README](../README.md)

`wc_macros.h` is a typed layer over the `void*` C API. Where a macro takes the element type `T`, it checks `sizeof(T)` against the container's element size, so passing the wrong type aborts in Debug (`WC_ASSERT_ELEM_SIZE`). Macros never allocate or free anything you don't see: they build values the same way the function API does.

Include `wc_macros.h`. It pulls in `gen_vector.h`, `wc_string.h`, `map_setup.h` and the built-in ops from `wc_helpers.h`.

## The value-type rule

Macros that take a **value** (`VEC_PUSH`, `VEC_SET`, `STACK_PUSH`, `QUEUE_PUSH`, `SET_INSERT`, `MAP_PUT`, `MAP_GET`, `MAP_TRY_GET`, `MAP_PUT_KEY_MOVE`, `MAP_PUT_VAL_MOVE`) copy it through a temporary of the value's own type. **That type must be exactly the element type.** These macros don't check it.

```c
#include "wc_macros.h"

int main(void)
{
    GenVec d = VEC(double, 4);
    VEC_PUSH(&d, 1.0);          // correct: a double
    // VEC_PUSH(&d, 1);         // WRONG: an int; reads 8 bytes from a 4-byte temporary
    VEC_PUSH(&d, (double)2);    // correct: cast to the element type
    GenVec_destroy(&d);
    return 0;
}
```

Watch for integer literals in `double`, `u64` or `float` containers, and for `char` vs `int`.

## Creating

| Macro | Expands to |
|---|---|
| `VEC(T, cap)` / `VEC_IN(A, T, cap)` | Vector of plain data, no ops |
| `VEC_CX(T, cap, ops)` / `VEC_CX_IN(A, T, cap, ops)` | Vector with explicit ops |
| `VEC_OF(T, cap)` / `VEC_OF_IN(A, T, cap)` | Ops picked by `WC_OPS(T)`: `String`, `String*`, `GenVec`, `GenVec*`, else `NULL` |
| `VEC_EMPTY(T)`, `VEC_EMPTY_CX(T, ops)` | Capacity 0 |
| `VEC_LIKE(&v, T, cap)` | New vector on `v`'s allocator |
| `VEC_FROM_ARR(T, n, arr)` / `VEC_FROM_ARR_IN(A, T, n, arr)` | Copies `n` elements from an array |
| `VEC_OF_INT`, `VEC_OF_STR`, `VEC_OF_STR_PTR`, `VEC_OF_VEC`, `VEC_OF_VEC_PTR` | Shorthands, libc only |
| `MAP_OF(K, V)` / `MAP_OF_IN(A, K, V)` | Map with ops, hash and compare picked from `K` and `V` |
| `STACK_CREATE(T, cap)`, `STACK_CREATE_CX`, `STACK_CREATE_IN`, `STACK_CREATE_CX_IN` | Stacks |
| `QUEUE_CREATE(T, cap)`, `QUEUE_CREATE_CX`, `QUEUE_CREATE_IN`, `QUEUE_CREATE_CX_IN` | Queues |
| `VEC_MAKE_OPS(copy, move, del)` | A `wc_container_ops` compound literal |

`MAP_OF` chooses `wyhash_str` and `str_cmp` for `String` keys, and the `_ptr` versions for `String*` keys. Every other key type is hashed and compared as raw bytes, which is only correct for keys without pointers.

## Vectors

| Macro | Returns | Notes |
|---|---|---|
| `VEC_PUSH(&v, val)` | | Copies `val`. See the value-type rule. |
| `VEC_PUSH_MOVE(&v, lval)` | | Moves an lvalue in; `lval` is zeroed. Size-checked. |
| `VEC_PUSH_CSTR(&v, "text")` | | Builds a `String` in `v`'s allocator. Works for `String` and `String*` vectors. |
| `VEC_PUSH_VEC(&outer, inner)`, `VEC_PUSH_VEC_PTR(&outer, p)` | | Move a vector, or a boxed vector, into a vector of vectors |
| `VEC_AT(&v, T, i)` | `T` | A copy of element `i` |
| `VEC_AT_MUT(&v, T, i)` | `T*` | A pointer into the vector |
| `VEC_FRONT(&v, T)`, `VEC_BACK(&v, T)` | `T` | |
| `VEC_SET(&v, i, val)` | | Replaces element `i`; the old one is deleted |
| `VEC_POP(&v, T)` | `T` | Removes and returns the last element; you own it now |
| `VEC_FOREACH(&v, T, name) { ... }` | | `name` is a `T*`. **`break` does not stop the loop**; see below. |

```c
#include "wc_macros.h"

#include <stdio.h>

int main(void)
{
    GenVec v = VEC_FROM_ARR(int, 4, ((int[4]){3, 1, 4, 1}));
    VEC_FOREACH(&v, int, x) { *x *= 10; }
    int last = VEC_POP(&v, int);               // 10
    printf("%d %d\n", VEC_AT(&v, int, 0), last);
    GenVec_destroy(&v);
    return 0;
}
```

`VEC_FOREACH` does no element-size check of its own. Instead, the `T*` assignment gives a compile-time type check against what you wrote. The vector must not grow inside the loop.

### `break` inside a `*_FOREACH`

Every `*_FOREACH` macro (`VEC_`, `STACK_`, `MAP_FOREACH_KEY`, `MAP_FOREACH_VAL`, `SET_`) is two nested `for` loops. `break` leaves only the inner one, so **it acts like `continue`: iteration goes on with the next element.** `continue` works as expected.

To stop early, loop by index instead:

```c
#include "wc_macros.h"

int main(void)
{
    GenVec v = VEC_FROM_ARR(int, 4, ((int[4]){5, 7, 9, 11}));
    u64 found = WC_NOT_FOUND;
    for (u64 i = 0; i < GenVec_size(&v); i++) {
        if (VEC_AT(&v, int, i) == 9) { found = i; break; }   // a real break
    }
    GenVec_destroy(&v);
    return found == 2 ? 0 : 1;
}
```

## Maps

| Macro | Returns | Notes |
|---|---|---|
| `MAP_PUT(&m, key, val)` | `b8` | Copies both. `1` if the key existed. |
| `MAP_PUT_MOVE(&m, klval, vlval)` | `b8` | Moves both; both zeroed |
| `MAP_PUT_KEY_MOVE(&m, klval, val)`, `MAP_PUT_VAL_MOVE(&m, key, vlval)` | `b8` | Move one, copy the other |
| `MAP_PUT_STR_STR(&m, "k", "v")`, `MAP_PUT_STR_INT(&m, "k", 1)`, `MAP_PUT_INT_STR(&m, 1, "v")` | `b8` | Build the `String`s in the map's allocator |
| `MAP_GET(&m, V, key)` | `V` | A copy of the value. A missing key aborts in Debug and returns a zeroed `V` in Release. Use `MAP_TRY_GET` when absence is normal. |
| `MAP_TRY_GET(&m, V, key, &out)` | `b8` | `1` and writes `out` if found |
| `MAP_FOREACH_KEY(&m, K, k) { ... }` | | `k` is a `const K*` |
| `MAP_FOREACH_VAL(&m, V, v) { ... }` | | `v` is a `V*` |

```c
#include "hashmap.h"
#include "wc_macros.h"

#include <stdio.h>

int main(void)
{
    HashMap m = MAP_OF(int, int);
    for (int i = 1; i <= 3; i++) { MAP_PUT(&m, i, i * i); }

    int sq;
    if (MAP_TRY_GET(&m, int, 2, &sq)) { printf("2 -> %d\n", sq); }
    MAP_FOREACH_VAL(&m, int, v) { *v += 1; }

    HashMap_destroy(&m);
    return 0;
}
```

## Sets

| Macro | Notes |
|---|---|
| `SET_INSERT(&s, val)` | Copies `val`. Returns `b8`: `1` if it was already present. See the value-type rule. |
| `SET_INSERT_MOVE(&s, lval)` | Moves in, or destroys `lval` if already present. `lval` is zeroed either way. |
| `SET_INSERT_CSTR(&s, "text")` | Builds a `String` in the set's allocator |
| `SET_FROM_VEC(&v, hash, cmp)` | A new set of `v`'s elements, on `v`'s allocator |
| `SET_FOREACH(&s, T, x) { ... }` | `x` is a `const T*` |

## Stacks and queues

| Macro | Notes |
|---|---|
| `STACK_PUSH`, `STACK_PUSH_MOVE`, `STACK_POP(&s, T)`, `STACK_AT(&s, T, i)`, `STACK_FOREACH` | The `VEC_*` macros under stack names |
| `QUEUE_PUSH(&q, val)`, `QUEUE_PUSH_MOVE(&q, lval)`, `QUEUE_PUSH_CSTR(&q, "text")` | |
| `QUEUE_POP(&q, T)` | Returns the head |
| `QUEUE_PEEK(&q, T)` | The head, without removing it. On an empty queue `Queue_peek_ptr` returns `NULL`, so check `Queue_empty` first. |

## Every macro is an expression

All macros are expressions, never `do { } while (0)`. You can use them in comma expressions, in `?:` arms, inside `({ })`, and in an unbraced `if` / `else` without surprises. The iteration macros (`*_FOREACH`) are `for` loops and take a body.
