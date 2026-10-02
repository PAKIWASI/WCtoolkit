# Ownership

[Back to README](../README.md)

Containers never guess how to copy, move or free your data. You describe it once, in a `wc_container_ops` table, and every container (vector, stack, queue, priority queue, map, set) uses the same table.

## The ops table

```c
typedef struct {
    wc_copy_fn   copy_fn;   // deep copy src into dest, allocating from `dst`
    wc_move_fn   move_fn;   // dest takes over src; src is left zeroed
    wc_delete_fn del_fn;    // free what the element owns, not the slot
} wc_container_ops;

typedef void (*wc_copy_fn)  (wc_allocator dst, void* dest, const void* src);
typedef void (*wc_move_fn)  (void* dest, void* src);
typedef void (*wc_delete_fn)(void* elm);
```

| Element type | Pass as `ops` |
|---|---|
| Plain data: `int`, `float`, structs without pointers | `NULL`. Containers use `memcpy` and skip deletion. |
| A type that owns memory | A `wc_container_ops*` |

Any single callback may be `NULL`:
- **No `copy_fn`:** copies are `memcpy`.
- **No `move_fn`:** moves are `memcpy`, then the source is zeroed.
- **No `del_fn`:** nothing is freed per element.

## Built-in ops

`wc_helpers.h` provides ops for the toolkit's own owning types:

| Ops | Element | Storage |
|---|---|---|
| `wc_str_ops` | `String` | By value |
| `wc_str_ptr_ops` | `String*` | By pointer |
| `wc_vec_ops` | `GenVec` | By value (vector of vectors) |
| `wc_vec_ptr_ops` | `GenVec*` | By pointer |

The `WC_OPS(T)` macro picks the right one from the type, and returns `NULL` for any other type. `VEC_OF`, `VEC_OF_IN`, `MAP_OF` and `MAP_OF_IN` use it, so for these types you rarely name the ops yourself.

## Writing your own

The rules for each callback:

- **`copy_fn(dst, dest, src)`**: `dest` is raw, uninitialized slot memory. Never read or free it. Allocate any owned resources from `dst`, which is the container's allocator, so the copy follows its container.
- **`move_fn(dest, src)`**: transfer everything and leave `src` zeroed. For most types this is `memcpy` + `memset`, which is the default when `move_fn` is `NULL`.
- **`del_fn(elm)`**: free what the element owns, using the element's **own stored allocator**. Never free `elm` itself: it's a slot inside the container.

A struct that owns a `String` and a `GenVec`:

```c
#include "wc_helpers.h"
#include "wc_macros.h"

#include <string.h>

typedef struct {
    String name;
    GenVec scores;   // of int
    int    id;
} Player;

static void player_copy(wc_allocator dst, void* dest, const void* src)
{
    const Player* s = src;
    Player*       d = dest;
    d->name   = String_copy(dst, &s->name);
    d->scores = GenVec_copy(dst, &s->scores);
    d->id     = s->id;
}

static void player_del(void* elm)
{
    Player* p = elm;
    String_destroy(&p->name);
    GenVec_destroy(&p->scores);
}

// No move_fn: memcpy + zero is correct for this struct.
static const wc_container_ops player_ops = { player_copy, NULL, player_del };

int main(void)
{
    GenVec team = VEC_CX(Player, 4, &player_ops);

    Player p = { String_from_cstr(WC_LIBC, "ada"), VEC(int, 4), 1 };
    VEC_PUSH(&p.scores, 90);
    VEC_PUSH_MOVE(&team, p);         // p is zeroed: the vector owns it now

    GenVec copy = GenVec_copy(WC_LIBC, &team);   // player_copy runs per element
    GenVec_destroy(&copy);
    GenVec_destroy(&team);                        // player_del runs per element
    return 0;
}
```

## By value or by pointer

| | By value (default) | By pointer |
|---|---|---|
| Slot holds | The whole struct, `sizeof(T)` bytes | A pointer, 8 bytes |
| Locality | Elements are contiguous | One extra dereference |
| Addresses | Change when the container grows | Stable |
| Ops | `wc_str_ops`, `wc_vec_ops`, yours | `wc_str_ptr_ops`, `wc_vec_ptr_ops` |

Use by-pointer storage only when something outside the container keeps the element's address.

## Boxing

By-pointer elements need a heap "shell". Create shells only with `WC_BOX_IN`:

```c
#include "wc_macros.h"

int main(void)
{
    GenVec v = VEC_OF(String*, 4);

    String* s = WC_BOX_IN(WC_LIBC, String, String_from_cstr, "hello");
    // a shell from WC_LIBC holding String_from_cstr(WC_LIBC, "hello")
    VEC_PUSH_MOVE(&v, s);            // the slot owns the pointer; s is NULL now

    GenVec_destroy(&v);              // destroys the String, then frees the shell
    return 0;
}
```

`WC_BOX_IN(A, T, init_fn, args...)` calls `init_fn(A, args...)`, so the allocator is passed once.

**Invariant:** a boxed element's shell comes from the same allocator the element stores. When a container frees the slot, it destroys the element and then frees the shell with that same allocator. A shell allocated any other way breaks this.

## Nested containers follow their parent

Copy callbacks receive the destination container's allocator. A deep copy of an arena-backed `GenVec<GenVec<String>>` into `WC_LIBC` therefore produces a tree with no pointers left into the arena. That's how a result escapes a scratch arena; see [Allocators](allocators.md#lifetimes).

Macros that build elements also use the container's allocator: `VEC_PUSH_CSTR`, `MAP_PUT_STR_*`, `SET_INSERT_CSTR` and `QUEUE_PUSH_CSTR`.
