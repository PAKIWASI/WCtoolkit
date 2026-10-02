# Allocators

[Back to README](../README.md)

Every container takes a `wc_allocator` when it's created, stores it, and uses it for every allocation, reallocation and free it makes. There is no global allocator.

```c
typedef struct {
    const wc_alloc_vtable* vt;   // NULL means libc
    void*                  ctx;  // Arena*, ChainArena*, your own state
} wc_allocator;                  // 16 bytes, passed and stored by value
```

## Choosing one

| Allocator | Get it with | Frees | Use it when |
|---|---|---|---|
| libc | `WC_LIBC` | Yes | Default. Long-lived data with independent lifetimes. |
| `Arena` | `Arena_allocator(&arena)` | Last block only | Many allocations that die together, and you know the upper bound. |
| `ChainArena` | `ChainArena_allocator(&ca)` | Last block only | Same, but the total size is unknown. Grows by adding nodes. |
| `ARENA_SCOPE` | `ARENA_SCOPE(name, cap) { ... }` | When the block exits | Per-request or per-frame scratch work. |
| borrowed | `wc_borrowed` | Never | Wrapping memory you own: `GenVec_create_buf`, `matrix_create_buf`. |
| test | `wc_test_alloc_allocator(&ta)` | Yes | Tests. Checks leaks, double frees and free sizes. |

`WC_LIBC` is simply a zeroed `wc_allocator`. That's also why a zeroed (moved-from) container must never allocate: it would silently switch to libc. See [Conventions](conventions.md#zero-state-is-dead).

## The allocation API

```c
#include "wc_allocator.h"

int main(void)
{
    wc_allocator a = WC_LIBC;
    int* xs = WC_NEW_N(a, int, 16);          // wc_alloc(a, 16 * sizeof(int), alignof(int))
    xs      = WC_REALLOC_N(a, int, xs, 16, 32);
    WC_DELETE_N(a, int, xs, 32);             // the size must match the last alloc/realloc
    return 0;
}
```

| Function | Notes |
|---|---|
| `wc_alloc(a, n, align)` | `n == 0` returns `NULL` without calling the backend |
| `wc_realloc(a, p, old_n, n, align)` | `p == NULL` behaves like `wc_alloc`; `n == 0` frees and returns `NULL` |
| `wc_free(a, p, n, align)` | `n` and `align` must match the block's last `alloc` / `realloc` |
| `WC_NEW`, `WC_NEW_N`, `WC_DELETE`, `WC_DELETE_N`, `WC_REALLOC_N` | Typed wrappers using `sizeof(T)` and `alignof(T)` |
| `wc_mul(count, size)` | Saturates to `SIZE_MAX` on overflow, so oversized requests fail instead of wrapping |

Arenas ignore the size you pass to `free`. A wrong size therefore only shows up when you switch to libc or run the test allocator, which is why the tests run every container on it.

Containers that call these functions abort if the allocator returns `NULL`, in every build. There is no error-return path.

## Arena

A single fixed-capacity region with a bump pointer.

```c
#include "arena.h"
#include "wc_macros.h"

int main(void)
{
    Arena arena;
    Arena_create(&arena, WC_LIBC, nKB(64));      // region comes from libc
    wc_allocator a = Arena_allocator(&arena);

    GenVec v = VEC_IN(a, int, 8);
    for (int i = 0; i < 100; i++) { VEC_PUSH(&v, i); }   // grows in place when it's the last block

    Arena_reset(&arena);                          // forget everything, keep the region
    Arena_destroy(&arena);                        // give the region back to libc
    return 0;
}
```

| Function | Behavior |
|---|---|
| `Arena_create(&a, backing, cap)` | Takes a `cap`-byte region from `backing` |
| `Arena_create_buf(&a, buf, size)` | Uses memory you own, for example a stack array. Destroy frees nothing. |
| `ARENA_CREATE_BUF(&a, nbytes)` | Same, over an anonymous stack buffer that lives until the enclosing block ends |
| `Arena_alloc(&a, n)` / `Arena_alloc_aligned(&a, n, align)` | Returns `NULL` and sets `wc_errno = WC_ERR_FULL` when full. Alignment is by address. |
| `Arena_reset(&a)` | Forgets every allocation and keeps the region |
| `Arena_destroy(&a)` | Frees the region. Safe on a zeroed arena, safe to call twice. |
| `Arena_used(&a)` / `Arena_remaining(&a)` | Bytes used and bytes free |
| `ARENA_ALLOC(&a, T)`, `ARENA_ALLOC_N(&a, T, n)`, `ARENA_ALLOC_ZERO(_N)`, `ARENA_PUSH_ARRAY(&a, T, src, n)` | Typed helpers |

Through the allocator interface, an arena can `realloc` and `free` **only the most recent block**, and does it in place. Everything else is a no-op `free`, or an alloc-and-copy `realloc`. That makes a single growing vector on an arena waste nothing. Several vectors growing alternately leave dead blocks behind.

## ChainArena

The same model, but it never runs out: when a node fills, it allocates another from its backing allocator.

- Each node is `CHAIN_ARENA_NODE_SIZE` bytes (4 KB by default), header included.
- A request larger than a node gets a dedicated node sized to fit.
- `ChainArena_reset` frees every node except the first, which is emptied. `ChainArena_clear` empties every node but keeps them all for reuse.
- `CHAIN_ARENA_SCRATCH(&ca) { ... }` works like `ARENA_SCRATCH`, and also frees nodes appended inside the block.

## Lifetimes

| Allocator | Memory lives until |
|---|---|
| libc | You call `X_destroy` |
| `Arena` / `ChainArena` | The arena is reset or destroyed, whether or not you call `X_destroy` |
| `ARENA_SCOPE` | The block exits |
| borrowed | The buffer you passed goes out of scope |

A container built on an arena must not outlive the arena. To keep a result, deep-copy it into a longer-lived allocator first. Copies never inherit the source's allocator, and nested elements follow the copy:

```c
#include "arena.h"
#include "wc_macros.h"

static GenVec build_names(void)
{
    GenVec result = {0};
    ARENA_SCOPE(tmp, nKB(16)) {
        GenVec names = VEC_OF_IN(tmp, String, 8);
        VEC_PUSH_CSTR(&names, "kept");
        result = GenVec_copy(WC_LIBC, &names);   // every String is copied into libc
    }                                            // the arena and `names` die here
    return result;
}

int main(void)
{
    GenVec names = build_names();
    GenVec_destroy(&names);                      // the libc copy must be destroyed
    return 0;
}
```

## Scopes

`ARENA_SCOPE(name, cap)` creates a libc-backed arena, exposes it inside the block as `wc_allocator name`, and destroys it when the block exits by any route. `break` leaves the scope. Scopes nest.

`ARENA_SCRATCH(&arena)` rolls back everything allocated inside the block, and the arena survives:

```c
#include "arena.h"

int main(void)
{
    Arena arena;
    Arena_create(&arena, WC_LIBC, nKB(4));
    char* kept = ARENA_ALLOC_N(&arena, char, 64);

    ARENA_SCRATCH(&arena) {
        char* tmp = ARENA_ALLOC_N(&arena, char, 1024);   // rolled back on exit
        (void)tmp;
    }

    (void)kept;                                         // still valid
    Arena_destroy(&arena);
    return 0;
}
```

**Never grow a block created before a scratch scope from inside that scope.** Grown in place, the rollback would cut it short. Copied instead, the new block would be scratch memory and would dangle after the scope. Debug builds stop with an error when this happens. Shrinking or freeing a pre-scope block is safe.

The manual form is `ArenaScratch s = Arena_scratch_begin(&arena); ... Arena_scratch_end(s);`. Scopes must end in the reverse order they started.

## Pinned arenas

`Arena_allocator(&arena)` hands out the arena's own address. **Never move or copy an `Arena` or `ChainArena` after creating it.** That's why they're initialized in place while every container is returned by value.

Debug builds catch it: each arena stores its own address and checks it on every call, so a copied or destroyed arena is reported, not silently corrupted.

## Writing your own allocator

Provide a vtable and a context pointer:

```c
typedef struct {
    void* (*alloc)  (void* ctx, size_t size, size_t align);
    void* (*realloc)(void* ctx, void* p, size_t old_size, size_t new_size, size_t align); // may be NULL
    void  (*free)   (void* ctx, void* p, size_t size, size_t align);                       // may be NULL
} wc_alloc_vtable;
```

The `wc_*` wrappers guarantee your callbacks:

- are never called with `p == NULL` or `size == 0`;
- get an `align` that is a power of two, meaning the **address** alignment;
- get, in `free` and `realloc`, the exact size and alignment of the block's last allocation.

Your callbacks must:

- return `NULL` from a failed `realloc` and leave `p` valid;
- accept that a missing `realloc` is emulated with alloc + copy + free, and a missing `free` is a no-op.

```c
#include "wc_allocator.h"
#include "gen_vector.h"

#include <stdlib.h>

typedef struct { size_t live; } Counter;

static void* counting_alloc(void* ctx, size_t size, size_t align)
{
    (void)align;                       // malloc's alignment covers align <= 16
    ((Counter*)ctx)->live += size;
    return malloc(size);
}

static void counting_free(void* ctx, void* p, size_t size, size_t align)
{
    (void)align;
    ((Counter*)ctx)->live -= size;
    free(p);
}

static const wc_alloc_vtable counting_vt = { counting_alloc, NULL, counting_free };

int main(void)
{
    Counter      c = {0};
    wc_allocator a = { &counting_vt, &c };

    GenVec v = GenVec_create(a, 8, sizeof(int), NULL);
    GenVec_destroy(&v);
    return c.live == 0 ? 0 : 1;        // every byte came back
}
```

See [`examples/allocators.c`](../examples/allocators.c) for a stack-buffer arena, a scoped arena with nested containers, and an escape copy, end to end.
