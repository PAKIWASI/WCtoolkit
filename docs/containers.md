# Containers

[Back to README](../README.md)

All containers share the rules in [Conventions](conventions.md): created by value with an allocator, `destroy` frees contents and zeroes the struct, copies take a destination allocator, moves zero the source. Element pointers are `void*`. Elements are stored inline and described by [`wc_container_ops`](ownership.md), or `NULL` for plain data.

| Container | Header | Structure | Use it for |
|---|---|---|---|
| [`GenVec`](#genvec) | `gen_vector.h` | Growable array | The default sequence |
| [`Stack`](#stack) | `stack.h` | `GenVec` | LIFO |
| [`Queue`](#queue) | `queue.h` | Circular buffer | FIFO, and pops from both ends |
| [`PriorityQueue`](#priorityqueue) | `priority_queue.h` | Binary heap | Always-take-the-best order |
| [`HashMap`](#hashmap) | `hashmap.h` | Open addressing, Robin Hood | Key to value lookup |
| [Sets](#sets) | `hashmap.h` | A `HashMap` with `val_size == 0` | Membership |
| [`BitVec`](#bitvec) | `bit_vector.h` | `GenVec` of bytes | Dense flags |

Most code uses these through the [macro layer](macros.md), which adds compile-time element-type checks.

## GenVec

A growable array. Capacity grows by `GENVEC_GROWTH` (1.5×).

```c
#include "gen_vector.h"

int main(void)
{
    GenVec v = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    for (int i = 0; i < 6; i++) { GenVec_push(&v, &i); }

    int out;
    GenVec_pop(&v, &out);                    // out = 5
    GenVec_remove(&v, 0, NULL);              // shifts left: 1 2 3 4
    int* p = GenVec_get_ptr_mut(&v, 0);      // *p = 1
    *p = 10;

    GenVec_destroy(&v);
    return 0;
}
```

| Group | Functions | Cost |
|---|---|---|
| Create | `GenVec_create(a, n, size, ops)`, `GenVec_create_val(a, n, &val, size, ops)` (n copies of `val`), `GenVec_create_buf(buf, n, size, ops)` (your memory, cannot grow) | O(n) |
| Destroy | `GenVec_destroy` (calls `del_fn` per element, frees storage, zeroes struct) | O(n) |
| Clear | `GenVec_clear` (delete elements, keep capacity), `GenVec_reset` (delete elements and free storage; allocator kept) | O(n) |
| Capacity | `GenVec_reserve(v, cap)` (never shrinks), `GenVec_reserve_val(v, cap, &val)` (grow and fill), `GenVec_shrink_to_fit` | O(n) |
| Push / pop | `GenVec_push` (copy), `GenVec_push_move`, `GenVec_pop(v, out_or_NULL)` (moves out) | Amortized O(1) |
| Read | `GenVec_get(v, i, out)` (deep copy), `GenVec_get_ptr`, `GenVec_get_ptr_mut`, `GenVec_front`, `GenVec_back` | O(1) |
| Read, no check | `GenVec_get_ptr_unsafe`, `GenVec_get_ptr_mut_unsafe` | O(1) |
| Write | `GenVec_replace`, `GenVec_replace_move` (old element deleted), `GenVec_swap` (never allocates) | O(1) |
| Insert | `GenVec_insert`, `GenVec_insert_move`, `GenVec_insert_multi`, `GenVec_insert_multi_move` | O(n) |
| Remove | `GenVec_remove(v, i, out_or_NULL)` (keeps order), `GenVec_swap_pop(v, i, out_or_NULL)` (moves the last element into `i`), `GenVec_remove_range(v, start, len)` | O(n), O(1), O(n) |
| Search | `GenVec_find(v, &x, cmp_or_NULL)` (`memcmp` if `NULL`); returns the index or `WC_NOT_FOUND` | O(n) |
| Slice | `GenVec_subarr(v, a, start, len)`: a new vector of deep copies | O(len) |
| Whole | `GenVec_copy(a, &src)` (capacity sized to the contents), `GenVec_move(&dst, &src)`, `GenVec_print(v, print_fn)` | |
| Size | `GenVec_size`, `GenVec_capacity`, `GenVec_empty` | O(1) |

Notes:
- On an empty vector, `GenVec_pop` does nothing and `GenVec_front` / `GenVec_back` return `NULL`. All three set `wc_errno = WC_ERR_EMPTY` instead of aborting.
- `pop`, `remove` and `swap_pop` **move** the element into `out`: no copy, no allocation. It keeps the vector's allocator. With `out == NULL` the element is deleted.
- Growth: capacity 0 jumps to `GENVEC_MIN_CAPACITY` (8), then 1.5×. Bulk inserts grow to at least the size they need.
- Pointers from `get_ptr` become invalid when the vector grows.
- The element passed to `push` / `insert` / `replace` must not point into the same vector (Debug checks it).

## Stack

`Stack` is a `typedef` of `GenVec`, with LIFO names. Any `GenVec_*` function works on it.

| Function | Notes |
|---|---|
| `Stack_create(a, n, size, ops)`, `Stack_create_val`, `Stack_destroy`, `Stack_copy`, `Stack_move` | As `GenVec` |
| `Stack_push`, `Stack_push_move`, `Stack_pop(s, out)` | Top is the end |
| `Stack_peek(s, out)` (deep copy), `Stack_peek_ptr(s)` | Empty: `wc_errno = WC_ERR_EMPTY` |
| `Stack_clear`, `Stack_reset`, `Stack_size`, `Stack_empty`, `Stack_capacity`, `Stack_print` | |

## Queue

A circular buffer over a `GenVec`'s storage. Push at the tail, pop from the head or the tail. Slots outside the live range are raw memory: push writes into them without deleting anything.

| Function | Notes |
|---|---|
| `Queue_create(a, n, size, ops)` | `n` may be 0 |
| `Queue_push` (copy), `Queue_push_move` | Amortized O(1). Grows by 1.5× (0 jumps to `QUEUE_MIN_CAP`, 8) with one `realloc`: elements are relocated, never copied. |
| `Queue_pop(q, out)`, `Queue_pop_back(q, out)` | O(1). The element is **moved** into `out`, or deleted when `out` is `NULL`. The queue never shrinks on its own. |
| `Queue_peek(q, out)` (deep copy), `Queue_peek_ptr(q)` | The head |
| `Queue_get(q, i)` | The `i`-th element from the head, wrap-around handled |
| `Queue_swap(q, i, j)` | Logical positions. Never allocates. |
| `Queue_shrink_to_fit` | The only way capacity goes down |
| `Queue_copy(a, &src)` | Compacted: head at 0, capacity sized to the contents |
| `Queue_clear`, `Queue_reset`, `Queue_move`, `Queue_size`, `Queue_empty`, `Queue_capacity`, `Queue_print` | |

Pop and peek on an empty queue set `wc_errno = WC_ERR_EMPTY`.

## PriorityQueue

A binary heap stored in a `GenVec`. The comparison function decides priority: **`cmp(a, b) < 0` means `a` comes out first.** With a plain ascending comparison you get a min-heap.

```c
#include "priority_queue.h"

static int by_value(const void* a, const void* b, u64 size)
{
    (void)size;
    int x = *(const int*)a, y = *(const int*)b;
    return (x > y) - (x < y);                    // ascending: smallest first
}

int main(void)
{
    PriorityQueue pq = PriorityQueue_create(WC_LIBC, 8, sizeof(int), NULL, by_value);
    int xs[] = {5, 1, 4};
    for (int i = 0; i < 3; i++) { PriorityQueue_push(&pq, &xs[i]); }

    int top;
    PriorityQueue_pop(&pq, &top);                 // top = 1
    PriorityQueue_destroy(&pq);
    return 0;
}
```

| Function | Cost |
|---|---|
| `PriorityQueue_create(a, n, size, ops, cmp)` | O(n) |
| `PriorityQueue_from_vec(a, &vec, cmp)`: heapifies a deep copy | O(n) |
| `PriorityQueue_from_vec_move(&vec, cmp)`: takes over the vector's buffer, no copy; `vec` is zeroed | O(n) |
| `PriorityQueue_push` (copy), `PriorityQueue_push_move` | O(log n) |
| `PriorityQueue_pop(pq, out_or_NULL)`, `PriorityQueue_remove(pq, i, out_or_NULL)`: moved out | O(log n) |
| `PriorityQueue_peek`, `PriorityQueue_get(pq, i)`, `PriorityQueue_size`, `PriorityQueue_empty` | O(1) |

Sifting uses the hole technique: the moving element waits in a spare slot and each level costs one element move, not a swap. The heap therefore keeps one slot of spare capacity.


## HashMap

Open addressing with Robin Hood probing and backward-shift deletion. Capacity starts at 16, is always a power of two, and doubles at 75% load. Keys, values, probe lengths and a small scratch area live in one allocation, each region aligned for what it holds: creating a map is 1 allocation, a resize is 1 allocation and 1 free.

```c
#include "hashmap.h"

int main(void)
{
    // int -> double, default hash (wc_hash over the key bytes) and byte equality
    HashMap m = HashMap_create(WC_LIBC, sizeof(int), sizeof(double), NULL, NULL, NULL, NULL);

    int    k = 7;
    double v = 3.5, out = 0;
    HashMap_put(&m, &k, &v);                 // returns 0: new key
    HashMap_get(&m, &k, &out);               // returns 1, out = 3.5

    double* slot = HashMap_get_ptr_mut(&m, &k);
    *slot += 1;                              // update in place

    HashMap_del(&m, &k, NULL);
    HashMap_destroy(&m);
    return 0;
}
```

`HashMap_create(a, key_size, val_size, hash, cmp, key_ops, val_ops)`. `hash` and `cmp` default to `wc_hash` (rapidhash) and byte equality when `NULL`. That's only correct for keys without pointers. For `String` keys pass `wc_hash_str` and `str_cmp`, or use `MAP_OF(String, V)`, which picks them for you.

The bucket comes from the top bits of the hash after a Fibonacci multiply, so a weak custom hash (even identity on integers) still spreads. A hash whose probe lengths pass 250 aborts instead of corrupting the table.

| Function | Returns |
|---|---|
| `HashMap_put(m, &k, &v)` | `1` if the key existed (value replaced), `0` if new |
| `HashMap_put_move`, `HashMap_put_key_move`, `HashMap_put_val_move` | Same; the moved arguments are zeroed. A duplicate moved-in key is destroyed. |
| `HashMap_get(m, &k, &out)` | `1` if found; deep-copies the value into `out`. For owning values prefer `get_ptr`. |
| `HashMap_get_ptr(m, &k)`, `HashMap_get_ptr_mut(m, &k)` | Pointer to the value, or `NULL` |
| `HashMap_get_key_ptr(m, &k)` | Pointer to the **stored** key, or `NULL`. A set's element, or a map's canonical key (interning). Never change what the hash reads through it. |
| `HashMap_has(m, &k)` | `1` if present |
| `HashMap_del(m, &k, out_or_NULL)` | `1` if removed. The value is **moved** into `out`, or deleted. |
| `HashMap_clear` | Removes everything, keeps capacity |
| `HashMap_copy`, `HashMap_move`, `HashMap_size`, `HashMap_capacity`, `HashMap_empty`, `HashMap_print(m, key_print, val_print_or_NULL)` | |

Iterate the buckets directly, or use `MAP_FOREACH_KEY` / `MAP_FOREACH_VAL`:

```c
#include "hashmap.h"

#include <stdio.h>

int main(void)
{
    HashMap m = HashMap_create(WC_LIBC, sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
    for (int i = 0; i < 5; i++) { int sq = i * i; HashMap_put(&m, &i, &sq); }

    for (u64 i = 0; i < HashMap_bucket_count(&m); i++) {
        if (HashMap_bucket_occupied(&m, i)) {
            const int* k = HashMap_bucket_key_ptr(&m, i);
            int*       v = HashMap_bucket_val_ptr(&m, i);
            printf("%d -> %d\n", *k, *v);
        }
    }
    HashMap_destroy(&m);
    return 0;
}
```

Iteration order is unspecified. Pointers into the map become invalid on any insert or delete.

## Sets

A set is a `HashMap` with `val_size == 0`: the same table, with a values region of 0 bytes. There is no separate set type.

```c
#include "wc_macros.h"

int main(void)
{
    HashMap seen = SET_OF(int);              // HashMap_create(WC_LIBC, sizeof(int), 0, NULL, NULL, NULL, NULL)
    SET_INSERT(&seen, 7);                    // returns 0: new
    SET_INSERT(&seen, 7);                    // returns 1: already present, no change

    int k = 7;
    HashMap_has(&seen, &k);                  // 1
    HashMap_del(&seen, &k, NULL);            // 1: removed

    HashMap words = SET_OF(String);          // String elements, wc_str_ops
    SET_INSERT_CSTR(&words, "ada");
    SET_FOREACH(&words, String, w) { String_print(w); }

    HashMap_destroy(&words);
    HashMap_destroy(&seen);
    return 0;
}
```

Rules for a set:

- `val` arguments may be `NULL`: `HashMap_put(&s, &x, NULL)`, `HashMap_put_key_move(&s, &x, NULL)`.
- `val_ops` must be `NULL`. Passing ops with `val_size == 0` is a FATAL.
- `HashMap_get_key_ptr(&s, &x)` returns the stored element. `HashMap_get_ptr` points at a 0-byte value and is not useful on a set.
- Iterate with `SET_FOREACH`, or `HashMap_bucket_key_ptr`.

## BitVec

A growable bit array, 8 bits per byte, over an embedded `GenVec`.

| Function | Behavior |
|---|---|
| `BitVec_create(a)`, `BitVec_destroy` | |
| `BitVec_set(b, i)` | Sets bit `i`, **growing** the vector if `i` is past the end |
| `BitVec_clear(b, i)`, `BitVec_toggle(b, i)`, `BitVec_test(b, i)` | `i` must be in range |
| `BitVec_push(b)` | Appends a **1** bit |
| `BitVec_pop(b)` | Removes the last bit; empty sets `wc_errno = WC_ERR_EMPTY` |
| `BitVec_size_bits`, `BitVec_size_bytes`, `BitVec_print(b, byte_index)` | |
