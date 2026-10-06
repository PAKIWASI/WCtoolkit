# Memory rules

[Back to README](../README.md)

Every rule the toolkit follows about allocation, copying, moving and lifetimes, in one place. Code comments cite these IDs (for example `// B7`). `tests/test_memory_rules.c` proves the allocation-sensitive ones by counting allocator calls.

## A. Allocators

| ID | Rule | Enforced by |
|---|---|---|
| A1 | No global allocator. Every container receives one at creation, stores it, and uses it for every alloc, realloc and free. | API shape |
| A2 | Allocators are passed and stored as `const wc_allocator*`, never by value. | Types |
| A3 | An allocator pointer is never `NULL`. libc is `WC_LIBC`, nothing else. `wc_libc` is the only allocator with `vt == NULL`. | `nonnull` attributes |
| A4 | The allocator object outlives every container that stores it. `WC_LIBC` and `WC_BORROWED` are static. `Arena`, `ChainArena` and `wc_test_alloc` embed their own `wc_allocator`, and their `*_allocator()` returns a pointer into them. | Embedded `self` |
| A5 | `free` and `realloc` receive the exact size and align of the block's last alloc/realloc. | `WC_FREE`, `wc_test_alloc` |
| A6 | Container allocation failure aborts in every build. Raw `Arena_alloc` returns `NULL` and sets `wc_errno`. | `FATAL_IF` |
| A7 | `alloc(0)` returns `NULL`. `realloc(NULL, ..)` acts as alloc. `realloc(p, 0)` frees. | `wc_alloc`, `wc_realloc` |
| A8 | `wc_mul` saturates, so an oversized request fails instead of wrapping. | `wc_mul` |
| A9 | Every element buffer is aligned for its element: `wc_align_for_size(elm_size)`. | All containers |
| A10 | Typed helpers: `WC_NEW`, `WC_NEW_N` take a type; `WC_FREE`, `WC_FREE_N`, `WC_REALLOC_N` take the pointer and derive size and align from it. | `wc_allocator.h` |
| A11 | `Arena` and `ChainArena` are pinned: never moved or copied after create. | Debug: `self.ctx == arena` on every call |
| A12 | An arena reallocs and frees only its last block, in place. A block created before a scratch scope never grows inside it. | Arena floor, Debug assert |
| A13 | A container on an arena must not outlive the arena. To escape, deep-copy into a longer-lived allocator. | Documentation |
| A14 | A copy never inherits the source's allocator. The destination allocator is always an explicit argument. | `X_copy(alloc, &src)` |
| A15 | Nested elements follow their parent: `copy_fn` receives the destination container's allocator. | `wc_copy_fn` signature |
| A16 | Elements built by macros (`*_CSTR`) use the container's allocator. | Macros |

## B. Element ownership

| ID | Rule | Enforced by |
|---|---|---|
| B1 | `ops == NULL` means plain data: memcpy, no delete. | All containers |
| B2 | An element that owns memory stores its own allocator; `del_fn` frees through it. | `String`, `GenVec`, ... |
| B3 | `copy_fn(dst, dest, src)`: `dest` is raw memory. Never read or free it. | Documentation |
| B5 | **Elements are trivially relocatable.** A container moves an element to another address with a raw memcpy (growth, insert/remove shifts, hash table shuffles, queue compaction, taking out) and never asks it first. A type that points to itself, or that something outside points into, must be stored by pointer. | All containers |
| B6 | There is no move callback. Under B5 every move is memcpy + zero the source. `wc_container_ops` is `{copy_fn, del_fn}`. | Type |
| B7 | **Taking out is a move.** `pop`, `remove`, `swap_pop`, `Queue_pop`, `Queue_pop_back`, `HashMap_del`, `PriorityQueue_pop` / `_remove` memcpy the element into `out` and forget the slot. With `out == NULL` they call `del_fn`. | Tests count zero allocator calls |
| B8 | **A copy happens only when both sides keep the value**: push/put/insert from an argument, get/peek into `out`, `X_copy`, `subarr`, `create_val`, `from_vec`. Internal operations (growth, compaction, rehash, take out) never call `copy_fn`. | Tests |
| B9 | A value taken out still uses the container's allocator. | Behavior of B7 |
| B10 | Move-inserting a duplicate destroys the incoming value. The source is zeroed either way. | `HashMap_put_move`, `HashMap_put_key_move` (sets) |
| B11 | Replace/put on an existing key destroys the old value, then copies or moves the new one in. | |
| B12 | Boxed shells come only from `WC_BOX_IN`, from the same allocator the element stores. `del_ptr` frees the shell with that allocator. | `wc_helpers.h` |
| B13 | Store by pointer when something outside keeps the element's address, or when the type is not trivially relocatable. | Documentation |
| B14 | A container frees what its elements own, never the slot. `destroy` never frees the container struct. | |

B4 was the old `move_fn` contract, removed by B6.

## C. Lifecycle and state

| ID | Rule |
|---|---|
| C1 | `X_create` returns by value and never returns `NULL`. Pinned types initialise in place. |
| C2 | `destroy` frees contents, then zeroes the struct. Safe on a zeroed struct, safe twice. |
| C3 | The zero state is dead: only destroy or re-create. Any mutation aborts in every build with a clear message (a zeroed allocator pointer is `NULL`). Reads see an empty container. |
| C4 | Container move is memcpy + zero source. `dest` must be raw or destroyed. |
| C5 | Hash tables allocate their table at create (one block). `GenVec` and `Queue` allocate the capacity you ask for; capacity 0 allocates nothing. |

## D. Pointer validity and aliasing

| ID | Rule |
|---|---|
| D1 | Element pointers are invalidated by: vector growth; any map/set insert or delete; queue push or pop; any change to a `String` (`cstr_view`). |
| D2 | A container must not grow inside its own FOREACH. |
| D3 | A `StrView` is valid while its memory lives. `StringStore` views stay valid until destroy. A `String_borrow` is valid while its bytes are. |
| D4 | The element passed to `push`, `insert` or `replace` must not live inside that container's own buffer. Debug builds assert it. |

## E. Macro layer

| ID | Rule |
|---|---|
| E1 | A macro never allocates or frees anything you don't see. `String`-keyed lookups by C string go through `String_borrow`: no allocation. |
| E2 | Value-in macros (`VEC_PUSH`, `VEC_SET`, `QUEUE_PUSH`, `SET_INSERT`, `MAP_PUT`, the by-value side of `MAP_PUT_*_MOVE`, `MAP_GET_PTR` keys) accept plain data only. Compile-time check. Owning elements use `_MOVE` or `_COPY`, which take an lvalue: no hidden temporary. |
| E3 | Value-out macros (`VEC_AT`, `VEC_FRONT`, `VEC_BACK`, `QUEUE_PEEK`, `MAP_GET`, `MAP_TRY_GET`) reject owning structs at compile time. Read those through pointers (`VEC_REF`, `MAP_GET_PTR`, `QUEUE_PEEK_REF`). Pointer elements are allowed: a returned pointer is a visible borrow. `VEC_POP` / `QUEUE_POP` work for every type (B7). |
| E4 | Typed macros check `sizeof(T)` against the slot in Debug. |

## F. Growth and capacity

| ID | Rule |
|---|---|
| F1 | `GenVec`, `String` and `Queue` grow by 1.5x (at least +1). Hash tables double at load 0.75. |
| F2 | Nothing shrinks on its own. Capacity only goes down through `shrink_to_fit` or `reset`. |
| F3 | Growth from capacity 0 jumps to the minimum (`GENVEC_MIN_CAPACITY`, `QUEUE_MIN_CAP`: 8). |
| F4 | Bulk inserts grow geometrically, never to the exact size. |
| F5 | A copy allocates for its contents (`max(size, MIN)`, or nothing when empty), not for the source's capacity. |
