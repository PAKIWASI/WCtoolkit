# Conventions

[Back to README](../README.md)

Every type in the toolkit follows the same rules. Learn them once and the whole API is predictable.

## Naming

- Functions are `Type_verb`: `GenVec_push`, `HashMap_get`, `String_append_cstr`. Matrix functions are lowercase: `matrix_xply`.
- Variants use suffixes:

| Suffix | Meaning | Example |
|---|---|---|
| `_move` | Takes ownership of the argument and zeroes it | `GenVec_push_move` |
| `_mut` | Returns a mutable pointer (the plain form returns `const`) | `GenVec_get_ptr_mut` |
| `_unsafe` | No bounds check, not even in Debug. For hot loops where the index is already proven. | `GenVec_get_ptr_unsafe` |
| `_val` | Fills with copies of one value | `GenVec_create_val` |
| `_buf` | Wraps memory you own; never frees it | `GenVec_create_buf` |
| `_IN` (macros) | Takes an allocator; the plain form uses libc | `VEC_OF_IN` |

## Argument order

The allocator (a `const wc_allocator*`, never `NULL`) comes first when a function allocates something new. The instance being acted on comes first everywhere else.

```c
GenVec v = GenVec_create(WC_LIBC, 8, sizeof(int), NULL);   // allocator first
GenVec c = GenVec_copy(WC_LIBC, &v);                       // allocator first
int    x = 42;
GenVec_push(&v, &x);                                       // instance first
GenVec_destroy(&c);
GenVec_destroy(&v);
```

## Create and destroy

- `X_create(alloc, ...)` **returns the container by value**. It never returns `NULL`: allocation failure aborts.
- `X_destroy(&x)` frees what the container owns through its stored allocator, then zeroes the struct. It never frees the struct itself, because you own it.
- `destroy` is safe on a zeroed struct and can be called twice.
- Constructors are `warn_unused_result`, so dropping a return value is a compiler warning.

**Exception: arenas are pinned.** `Arena` and `ChainArena` are initialized in place, `Arena_create(&arena, backing, cap)`, because containers hold the arena's address. Never move or copy one after creating it. See [Allocators](allocators.md#pinned-arenas).

## Copy and move

| Operation | Shape | Result's allocator | Source afterwards |
|---|---|---|---|
| Copy | `X dst = X_copy(alloc, &src)` | `alloc`, never inherited from `src` | Unchanged |
| Move | `X_move(&dst, &src)` | `src`'s | Zeroed |

- Copies are deep: nested elements are copied into the destination allocator.
- Move is a `memcpy` followed by zeroing the source. `dst` must be raw or already destroyed, because move overwrites it without freeing.
- Element-level moves take a pointer to your element and zero it: `GenVec_push_move(&v, &s)`.
- Taking an element out (`pop`, `remove`, `del`) moves it to you: no copy. A copy only happens when both sides keep the value. See [Ownership](ownership.md#copy-move-take-out) and [Memory rules](memory-rules.md).

## Zero state is dead

A zeroed container, whether moved-from or destroyed, may only be destroyed or created again. Any change to it aborts in **every** build (`FATAL_IF`). Reading is allowed and sees an empty container.

A zeroed container's allocator pointer is `NULL`, so allocating through it would crash with no context. The `FATAL_IF` turns that into a clear message, in Release too.

| Type | Zero-state check |
|---|---|
| `GenVec`, `Stack`, `Queue`, `PriorityQueue`, `BitVec` | `data_size == 0` |
| `String`, `HashMap` (maps and sets) | `capacity == 0` |
| `Arena`, `ChainArena` | `self.ctx == NULL` |
| `Matrixf` | `m == 0` |
| `StringStore` | `tail == NULL` |

## `void*` at the boundary, `u8*` inside

| Pointer kind | Type | Examples |
|---|---|---|
| Points at one of your elements | `void*` / `const void*` | `GenVec_push(&v, &x)`, `HashMap_get(&m, &k, &out)` |
| Returns one of your elements | `void*` / `const void*` | `int* p = GenVec_get_ptr_mut(&v, i)` |
| Callbacks (copy, move, delete, print, hash, compare) | `void*` / `const void*` | `static void my_del(void* elm)` |
| Allocation results | `void*` | `Arena_alloc`, `wc_alloc` |
| Byte buffers the library indexes | `u8*` | `GenVec.data`, `HashMap.keys`, `Arena.base` |
| Byte-level helpers | `const u8*` | `print_hex`, hash internals |

You never need to cast at a call site. The type check lives in the [macro layer](macros.md), which compares `sizeof(T)` against the container's element size.

## Return values

- Lookups that fail return `WC_NOT_FOUND`: `GenVec_find`, `String_find_cstr`.
- `put` / `insert` return `bool`: `1` if the key already existed, `0` if it was new.
- `del` / `remove` return `bool`: `1` if something was removed.
- Expected runtime conditions (pop on empty, arena full) set `wc_errno` and return early. Programmer errors abort. See [Diagnostics](diagnostics.md).

## Types

`common.h` defines `u8`, `u16`, `u32`, `u64` and `bool` (a `u8` used as a boolean), plus `KB`, `MB`, `nKB(n)` and `nMB(n)` for sizes.
