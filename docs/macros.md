# Macros

[Back to README](../README.md)

`wc_macros.h` is a typed layer over the `void*` C API. Where a macro takes the element type `T`, it checks `sizeof(T)` against the container's element size, so passing the wrong type aborts in Debug (`WC_ASSERT_ELEM_SIZE`). Macros never allocate or free anything you don't see.

Include `wc_macros.h`. It pulls in every container header, `wc_string.h`, `map_setup.h` and the built-in ops from `wc_helpers.h`.

## Plain data vs owning elements

A macro that takes an element **by value** copies it into a hidden temporary. For plain data that's free. For an owning type it would be a trap: `VEC_PUSH(&v, String_from_cstr(..))` would deep-copy the temporary and then leak it. So the macros split by element kind, and the compiler enforces the split ([memory rules](memory-rules.md) E2, E3):

| You have | Push / put / insert with | Read with |
|---|---|---|
| Plain data (`int`, `double`, flat structs) | `VEC_PUSH`, `MAP_PUT`, `SET_INSERT`, `QUEUE_PUSH`, `VEC_SET` | `VEC_AT`, `VEC_FRONT`, `VEC_BACK`, `MAP_GET`, `MAP_TRY_GET`, `QUEUE_PEEK` |
| An owning element you hand over (`String`, `GenVec`, `String*`, ...) | `*_MOVE(lval)`: the container takes it, `lval` is zeroed | `VEC_REF`, `VEC_AT_MUT`, `MAP_GET_PTR`, `QUEUE_PEEK_REF`: a pointer, no copy |
| An owning element you keep | `*_COPY(lval)`: the container deep-copies it | same |

Passing an owning type to a plain-data macro is a compile error:

```
VEC_PUSH: owning element type, use the _MOVE or _COPY form
VEC_AT: owning struct by value would alias the container, use the pointer form
```

The check knows the toolkit's own types (`String`, `GenVec`, `HashMap`, `HashSet`, `Queue`, `PriorityQueue`, `BitVec`, `Matrixf`, `StringStore`) and pointers to `String` and `GenVec`. Your own owning types aren't known to it: use the `_MOVE` / `_COPY` forms for them by hand.

Read macros accept pointer elements (`VEC_AT(&v, String*, i)` returns the `String*`): a returned pointer is a visible borrow. Popping works for every type: `VEC_POP` / `QUEUE_POP` **move** the element out, so the value you get is yours.

## Value types are checked

Macros that take a **value** copy it through a temporary of the value's own type, then check that type's size against the slot it goes into. A mismatch aborts in Debug, with a message naming both sizes:
- `VEC_PUSH`, `VEC_SET`, `STACK_PUSH`, `QUEUE_PUSH`, `QUEUE_PUSH_MOVE`, and every `_COPY` / `_MOVE` form
- `SET_INSERT`, `SET_INSERT_MOVE`
- `MAP_PUT`, `MAP_PUT_MOVE`, `MAP_PUT_KEY_MOVE`, `MAP_PUT_VAL_MOVE`, `MAP_GET`, `MAP_TRY_GET`

```c
#include "wc_macros.h"

int main(void)
{
    GenVec d = VEC(double, 4);
    VEC_PUSH(&d, 1.0);          // a double: fine
    VEC_PUSH(&d, (double)2);    // cast to the element type: fine
    // VEC_PUSH(&d, 1);         // an int: aborts in Debug ("value is 4 bytes, slot is 8 bytes")
    GenVec_destroy(&d);
    return 0;
}
```

Like every `WC_ASSERT`, the check is gone in Release, so a mismatch there reads past the temporary. Run your tests in Debug. Integer literals in `double`, `u64` or `float` containers are the usual cause.

`VEC_FROM_ARR` checks at compile time instead: the array's element type must be the same size as `T`.

## Creating

| Macro | Expands to |
|---|---|
| `VEC(T, cap)` / `VEC_IN(A, T, cap)` | Vector of plain data, no ops |
| `VEC_CX(T, cap, ops)` / `VEC_CX_IN(A, T, cap, ops)` | Vector with explicit ops |
| `VEC_OF(T, cap)` / `VEC_OF_IN(A, T, cap)` | Ops picked by `WC_OPS(T)`: `String`, `String*`, `GenVec`, `GenVec*`, else `NULL` |
| `VEC_EMPTY(T)`, `VEC_EMPTY_CX(T, ops)` | Capacity 0 |
| `VEC_LIKE(&v, T, cap)` | New vector on `v`'s allocator |
| `VEC_FROM_ARR(T, n, arr)` / `VEC_FROM_ARR_IN(A, T, n, arr)` | Copies `n` elements from an array: one `memcpy` for plain data, `WC_OPS(T)`'s copy for owning types |
| `VEC_OF_INT`, `VEC_OF_STR`, `VEC_OF_STR_PTR`, `VEC_OF_VEC`, `VEC_OF_VEC_PTR` | Shorthands, libc only |
| `MAP_OF(K, V)` / `MAP_OF_IN(A, K, V)` | Map with ops, hash and compare picked from `K` and `V` |
| `STACK_CREATE(T, cap)`, `STACK_CREATE_CX`, `STACK_CREATE_IN`, `STACK_CREATE_CX_IN` | Stacks |
| `QUEUE_CREATE(T, cap)`, `QUEUE_CREATE_CX`, `QUEUE_CREATE_IN`, `QUEUE_CREATE_CX_IN` | Queues |
| `VEC_MAKE_OPS(copy, del)` | A `wc_container_ops` compound literal |

`MAP_OF` chooses `wyhash_str` and `str_cmp` for `String` keys, and the `_ptr` versions for `String*` keys. Every other key type is hashed and compared as raw bytes, which is only correct for keys without pointers.

## Vectors

| Macro | Returns | Notes |
|---|---|---|
| `VEC_PUSH(&v, val)` | | Plain data. Size-checked in Debug. |
| `VEC_PUSH_MOVE(&v, lval)` | | Moves an lvalue in; `lval` is zeroed. Size-checked. |
| `VEC_PUSH_COPY(&v, lval)` | | Deep-copies an lvalue in; you keep it |
| `VEC_PUSH_CSTR(&v, "text")` | | Builds a `String` in `v`'s allocator. Works for `String` and `String*` vectors. |
| `VEC_PUSH_VEC(&outer, inner)`, `VEC_PUSH_VEC_PTR(&outer, p)` | | Move a vector, or a boxed vector, into a vector of vectors |
| `VEC_AT(&v, T, i)` | `T` | Element `i` by value. Not for owning structs. |
| `VEC_REF(&v, T, i)` | `const T*` | A pointer into the vector, any `T` |
| `VEC_AT_MUT(&v, T, i)` | `T*` | A mutable pointer into the vector |
| `VEC_FRONT(&v, T)`, `VEC_BACK(&v, T)` | `T` | Not for owning structs |
| `VEC_SET(&v, i, val)` | | Replaces element `i` with plain data; the old one is deleted |
| `VEC_SET_MOVE(&v, i, lval)`, `VEC_SET_COPY(&v, i, lval)` | | Replace with a moved or copied lvalue |
| `VEC_POP(&v, T)` | `T` | Removes and returns the last element, moved out: you own it now |
| `VEC_FOREACH(&v, T, name) { ... }` | | `name` is a `T*`. `break` and `continue` work. |

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

### How the `*_FOREACH` macros work

Each `*_FOREACH` (`VEC_`, `STACK_`, `MAP_FOREACH_KEY`, `MAP_FOREACH_VAL`, `SET_`) is two nested `for` loops sharing one flag, so `break` ends the whole loop and `continue` moves to the next element, as in a plain `for`. The optimizer removes the flag. Loop variables have line-unique names, so FOREACHes nest without shadowing warnings.

```c
#include "wc_macros.h"

int main(void)
{
    GenVec v = VEC_FROM_ARR(int, 4, ((int[4]){5, 7, 9, 11}));
    int seen = 0;
    VEC_FOREACH(&v, int, x) {
        seen++;
        if (*x == 9) { break; }     // stops after 5, 7, 9
    }
    GenVec_destroy(&v);
    return seen == 3 ? 0 : 1;
}
```

## Maps

| Macro | Returns | Notes |
|---|---|---|
| `MAP_PUT(&m, key, val)` | `bool` | Plain-data key and value. `1` if the key existed. |
| `MAP_PUT_COPY(&m, klval, vlval)` | `bool` | Deep-copies both lvalues; you keep them |
| `MAP_PUT_MOVE(&m, klval, vlval)` | `bool` | Moves both; both zeroed |
| `MAP_PUT_KEY_MOVE(&m, klval, val)`, `MAP_PUT_VAL_MOVE(&m, key, vlval)` | `bool` | Move one, copy the other (the copied one is plain data) |
| `MAP_PUT_STR_STR(&m, "k", "v")`, `MAP_PUT_STR_INT(&m, "k", 1)`, `MAP_PUT_INT_STR(&m, 1, "v")` | `bool` | The key is looked up through a borrowed `String`: an existing key costs no allocation, a new one is copied once. Values are built in the map's allocator. |
| `MAP_GET(&m, V, key)` | `V` | Plain data. A missing key aborts in Debug and returns a zeroed `V` in Release. Use `MAP_TRY_GET` when absence is normal. |
| `MAP_GET_PTR(&m, V, key)` | `const V*` | Any `V`, or `NULL` |
| `MAP_TRY_GET(&m, V, key, &out)` | `bool` | Plain data: `1` and writes `out` if found |
| `MAP_GET_CSTR(&m, V, "k")`, `MAP_GET_PTR_CSTR(&m, V, "k")`, `MAP_HAS_CSTR(&m, "k")` | | `String`-keyed lookups by C string. No allocation. |
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
| `SET_INSERT(&s, val)` | Plain data. Returns `bool`: `1` if it was already present. Size-checked in Debug. |
| `SET_INSERT_COPY(&s, lval)` | Deep-copies an lvalue in; you keep it |
| `SET_INSERT_MOVE(&s, lval)` | Moves in, or destroys `lval` if already present. `lval` is zeroed either way. |
| `SET_INSERT_CSTR(&s, "text")` | Probes with a borrowed `String`: an existing string costs nothing, a new one is copied once into the set's allocator |
| `SET_HAS_CSTR(&s, "text")` | No allocation |
| `SET_FROM_VEC(&v, hash, cmp)` | A new set of deep copies of `v`'s elements, on `v`'s allocator. Reserved once up front. |
| `SET_FOREACH(&s, T, x) { ... }` | `x` is a `const T*` |

## Stacks and queues

| Macro | Notes |
|---|---|
| `STACK_PUSH`, `STACK_PUSH_MOVE`, `STACK_PUSH_COPY`, `STACK_POP(&s, T)`, `STACK_AT(&s, T, i)`, `STACK_REF(&s, T, i)`, `STACK_FOREACH` | The `VEC_*` macros under stack names |
| `QUEUE_PUSH(&q, val)` (plain data), `QUEUE_PUSH_MOVE(&q, lval)`, `QUEUE_PUSH_COPY(&q, lval)`, `QUEUE_PUSH_CSTR(&q, "text")` | |
| `QUEUE_POP(&q, T)` | The head, moved out |
| `QUEUE_PEEK(&q, T)` | The head by value, without removing it. Plain data. On an empty queue `Queue_peek_ptr` returns `NULL`, so check `Queue_empty` first. |
| `QUEUE_PEEK_REF(&q, T)` | `const T*` to the head, any `T` |

## Every macro is an expression

All macros are expressions, never `do { } while (0)`. You can use them in comma expressions, in `?:` arms, inside `({ })`, and in an unbraced `if` / `else` without surprises. The iteration macros (`*_FOREACH`) are `for` loops and take a body.
