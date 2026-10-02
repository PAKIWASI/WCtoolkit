# Allocators

Every WCtoolkit container takes a `wc_allocator` when it is created, stores it, and uses it for every allocation, reallocation and free it makes. There is no global allocator.

```c
typedef struct {
    const wc_alloc_vtable* vt;  // NULL means libc
    void*                  ctx; // Arena*, ChainArena*, test allocator, ...
} wc_allocator;                 // 16 bytes, passed and stored by value
```

## The allocators

| Allocator | How to get it | Frees | Notes |
|---|---|---|---|
| libc | `WC_LIBC` (a zeroed `wc_allocator`) | yes | Default for the non-`_IN` macros. |
| `Arena` | `Arena_allocator(&arena)` | last block only | Fixed capacity. Returns NULL when full. |
| `ChainArena` | `ChainArena_allocator(&ca)` | last block only | Grows by adding nodes. Oversized requests get their own node. |
| borrowed | `wc_borrowed` | never | Wraps memory you own. Allocation always fails. Used by `GenVec_create_buf` and `matrix_create_buf`. |
| test | `wc_test_alloc_allocator(&ta)` | yes | Tests only. Checks leaks, double frees, and free sizes. |

Functions take the allocator first: `wc_alloc(a, n, align)`, `wc_realloc(a, p, old_n, n, align)`, `wc_free(a, p, n, align)`. `free` must receive the exact size and alignment of the allocation. Arenas ignore it, so a wrong size only shows up when you switch to libc or run the test allocator.

Allocation failure is fatal in every build (`FATAL_IF`). There is no error-return path.

## Lifetimes

A container's memory lives as long as its allocator.

- **libc:** until you call `X_destroy`.
- **Arena / ChainArena:** until the arena is reset or destroyed, whether or not you call `X_destroy`. Calling it is still useful: freeing the most recent block rewinds the arena.
- **borrowed:** until the buffer you passed goes out of scope.

A container built on an arena must not outlive the arena. To keep a result, copy it into a longer-lived allocator first (see [Copy vs move](#copy-vs-move)).

### Scoped arenas

`ARENA_SCOPE` creates a libc-backed arena, exposes it as a `wc_allocator` inside the block, and destroys it when the block exits. That includes `break`, `return` and `goto`.

```c
ARENA_SCOPE(tmp, nKB(64)) {
    GenVec v = VEC_OF_IN(tmp, String, 16);
    VEC_PUSH_CSTR(&v, "scratch");
    // no destroy: everything dies with the block
}
```

`ARENA_SCRATCH(&arena)` is the in-place version: allocations made inside the block are rolled back on exit, and the arena itself survives.

Inside a scratch block, never grow a container that was created before the block. If it grew in place, the rollback would cut it short. If it were copied, the new block would be scratch memory and dangle after the block ends. Debug builds stop with an error when this happens.

## Pinned arenas

`Arena_allocator(&arena)` hands out the arena's address as the allocator context. **An `Arena` or `ChainArena` must never be moved or copied after it is created.** That is why they are initialized in place (`Arena_create(&arena, backing, cap)`), while every container is returned by value.

Debug builds catch a moved or copied arena: each arena stores its own address and checks it on every call.

Arenas can sit on other allocators. `Arena_create(&a, backing, cap)` takes its region from `backing`. `Arena_create_buf(&a, buf, size)` uses memory you own, such as a stack array.

## Copy vs move

| Operation | Shape | Allocator of the result | Source after |
|---|---|---|---|
| Copy | `X dst = X_copy(a, &src)` | `a` (never inherited from `src`) | unchanged |
| Move | `X_move(&dst, &src)` | `src`'s | zeroed |

- **Copy is deep.** Nested elements are copied into the destination allocator through their `copy_fn`. Copying an arena-backed `GenVec<GenVec<String>>` into `WC_LIBC` produces a tree with no pointers back into the arena.
- **Move is a memcpy plus a zero.** The destination must be raw or already destroyed, because move overwrites it without freeing it.
- **Element moves take a pointer to the source element:** `GenVec_push_move(&v, (u8*)&s)` or `VEC_PUSH_MOVE(&v, s)`. `s` is zeroed afterwards.

### Zero state is dead

A zeroed container, whether moved-from or destroyed, may only be destroyed or created again. Any change to it is fatal in every build. This stops a moved-from container from quietly allocating through `WC_LIBC`, which is its zeroed allocator, instead of the arena it came from. `destroy` is always safe on a zeroed container and can be called twice.

## Nested containers and boxing

Convenience macros use the **container's** allocator for the elements they build (`VEC_PUSH_CSTR`, `MAP_PUT_STR_*`, `SET_INSERT_CSTR`, `QUEUE_PUSH_CSTR`). Strings therefore follow their container automatically.

Elements are stored inline (by value) by default. When you need stable addresses, store pointers instead (`VEC_OF_STR_PTR`, `wc_str_ptr_ops`, `wc_vec_ptr_ops`). Pointer shells are created only through `WC_BOX_IN`:

```c
String* s = WC_BOX_IN(alloc, String, String_from_cstr, "hello");
// == String_from_cstr(alloc, "hello"), moved into a shell allocated from alloc
```

**Invariant:** a boxed child's shell always comes from the allocator the child stores. When the container frees the slot, it destroys the child and then frees the shell with that same allocator. Never allocate a shell any other way.

## Hash map keys

`MAP_OF(K, V)` / `MAP_OF_IN(A, K, V)` choose the ops, the hash and the compare function from the types. `String` and `String*` keys hash their contents. POD keys hash their raw bytes. For any other owning key type, call `HashMap_create` with an explicit hash and compare function. Hashing a struct that holds pointers makes every lookup miss.

## Enforcement

Raw `malloc`, `calloc`, `realloc`, `free` and `aligned_alloc` are banned in library code. There are two layers:

1. `src/wc_poison.h` is included last in every `src/*.c` file and applies `#pragma GCC poison`. System headers must be included before it.
2. The `no_raw_alloc` ctest greps `src/` and `include/`. This catches code in headers and macros, which poison cannot see. The only exception is the libc backend in `include/wc_allocator.h`.

Inside library sources, the vtable field names `realloc` and `free` are poisoned too, so vtables use positional initializers.

See `examples/allocators.c` for all of this end to end.
