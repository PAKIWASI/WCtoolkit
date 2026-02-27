# WCtoolkit

A C toolkit built around explicit ownership, value semantics, and zero hidden costs.

WCtoolkit is designed for C programmers who care about lifetime correctness more than convenience.
Containers do not guess how to copy or destroy your data — you must define it.
This makes ownership explicit, prevents accidental deep copies, and scales well as programs grow.

No dependencies beyond the C standard library.
Targets C11 with GNU extensions (Clang/GCC). But can degrade to C99 by sacrificing some ergonomics.

```c
// A taste of the library
genVec* vec = VEC_CX(String, 8, &wc_str_ops);

VEC_PUSH_CSTR(vec, "PAKI");     // create Strings and move into vec
VEC_PUSH_CSTR(vec, "WASI");     // zero copies, only one move

// append in place
VEC_FOREACH(vec, String, s) { string_append_char(s, '!'); }

String* s = string_create();    // create new string as buffer
genVec_pop(vec, castptr(s));    // get element out of vec, s owns it now

string_print(s);
genVec_print(vec, str_print);

string_destroy(s);              // free resources
genVec_destroy(vec);
```

---

## Comparison with Similar C Libraries

| Feature / Library | WCtoolkit | klib | stb_ds | GLib | ccontainers |
|------------------|----------|------|--------|------|-------------|
| Language | C11 (+ GNU ext) (removeable) | C89 | C89 | C89 | C99 |
| Ownership model | **Explicit (copy / move / del)** | Implicit | Implicit | Implicit (ref-counted) | Semi-explicit |
| Value semantics | **Yes** | Limited | No | No | Partial |
| Move semantics | **Yes (manual, explicit)** | No | No | No | No |
| Hidden allocations | **None** | Yes | Yes | Yes | Some |
| Stable element addresses | Optional (by-pointer strategy) | No | No | Yes | Yes |
| Type safety | Medium (macros + sizeof) | Low | Low | High | Medium |
| Runtime overhead | **Zero** | Low | Low | High | Medium |
| Single-header | **Yes (per utility)** | Yes | Yes | No | No |
| Primary goal | **Explicit ownership & control** | Convenience | Header-only convenience | Safety & portability | STL-like containers |

---

## Table of Contents

- [Design Philosophy](#design-philosophy)
- [Ownership Model](#ownership-model)
- [Error Handling](#error-handling)
- [Macro Layer and Ergonomics](#macro-layer-and-ergonomics)
- [Components](#components)
  - [Arena Allocator](#arena-allocator)
  - [Generic Vector](#generic-vector)
  - [String](#string)
  - [Stack](#stack)
  - [Queue](#queue)
  - [HashMap](#hashmap)
  - [HashSet](#hashset)
  - [BitVector](#bitvector)
  - [Matrix](#matrix)
  - [Random (PCG32)](#random-pcg32)
  - [Fast Math](#fast-math)
- [Helpers and Callbacks](#helpers-and-callbacks)
- [Building](#building)
- [Testing](#testing)
- [Single-Header Distribution](#single-header-distribution)
- [Known Issues](#known-issues)
- [Type and Diagnostic Reference](#type-and-diagnostic-reference)

---

## Design Philosophy

WCtoolkit makes one central trade-off: **you tell the library how your data behaves, and it does the rest**. This means more setup code upfront, but total control over memory, copying, and lifetimes — with no hidden allocations and no garbage collection.

### Value Semantics Over Pointer Chasing

Elements are stored **inline by value** inside containers. A `genVec` of `String` stores the actual `String` structs contiguous in memory, not pointers to heap-scattered objects. This keeps data cache-friendly and makes the ownership structure obvious — there is no shared ownership, no reference counting, no implicit heap allocation behind your back.

```
┌─────────────────────────────────────────────────────┐
│  genVec (by value)                                  │
│  ┌──────────┬──────────┬──────────┬──────────┐      │
│  │ String   │ String   │ String   │ String   │ ...  │
│  │ {data, …}│ {data, …}│ {data, …}│ {data, …}│      │
│  └──────────┴──────────┴──────────┴──────────┘      │
└─────────────────────────────────────────────────────┘
```

By-pointer storage is supported when you need stable addresses or very large elements, but by-value is the default and the preferred path.

### Callbacks Over Macros

Genericity is achieved through **function pointer callbacks** grouped into a vtable struct, not `void*`-cast macros or `_Generic`. Every container accepts an optional `container_ops*` that bundles the `copy_fn`, `move_fn`, and `delete_fn` callbacks describing how to deep-copy, transfer ownership of, and free an element. For plain-old-data types — `int`, `float`, any struct without heap pointers — pass `NULL` and the container uses `memcpy` directly.

```c
// Define once, reuse everywhere
static const container_ops string_ops = { str_copy, str_move, str_del };

genVec* vec = genVec_init(8, sizeof(String), &string_ops);
```

`wc_helpers.h` ships pre-built ops instances for `String` and `genVec` in both storage strategies so you rarely need to define your own.

This has a real cost: you write more setup code. The benefit is that the behavior is explicit, readable, debuggable, and correct across container operations like `push`, `insert`, `replace`, `pop`, `remove`, `copy`, and `move`.

### No Magic, No Hidden Cost

Every allocation is explicit. `arena_alloc` is a pointer bump. `genVec_push` sometimes calls `realloc` — you can see it in the source. There is no background thread, no lazy initialization, no defer queue. What you write is what runs.

Growth and shrink thresholds for `genVec` are compile-time configurable macros you set before including the header. The hash table load factors are constants in `map_setup.h`. Nothing is hidden in a global config struct.

### Stack Wherever Possible

Most types offer both heap-allocated and stack-allocated variants:

```c
// Heap: library manages memory
Arena*  arena = arena_create(nKB(4));
genVec* vec   = genVec_init(8, sizeof(int), NULL);
String* str   = string_from_cstr("hello");

// Stack: you provide the struct, often the data too
Arena arena;
ARENA_CREATE_STK_ARR(&arena, 4);   // 4KB on the stack

genVec vec;
genVec_init_stk(8, sizeof(int), NULL, &vec);

String str;
string_create_stk(&str, "hello");
```

Stack variants reduce heap pressure and are useful in tight loops, embedded contexts, or anywhere you want lifetime tied to a scope.

### Separation of Concerns

The library is layered deliberately. `common.h` defines primitive types and macros. `gen_vector` is the single workhorse container. Everything else — `String`, `Stack`, `Queue`, `bitVec` — is built on top of it, reusing its growth, shrink, insert, remove, and copy logic without duplicating it. The hash containers share `map_setup.h` for hashing, prime selection, and load factor logic.

---

## Ownership Model

This is the most important concept in the library. Get this right and everything else follows naturally.

### The Three Callbacks

Any type that owns heap resources needs three callbacks:

```c
typedef void (*copy_fn)(u8* dest, const u8* src);    // deep copy src into dest
typedef void (*move_fn)(u8* dest, u8** src);          // transfer ownership, null *src
typedef void (*delete_fn)(u8* elm);                   // free resources owned by elm
```

Pass `NULL` for any you don't need. For `int`, `float`, and flat structs without pointers, all three are `NULL`.

The key contract:

- `copy_fn`: `dest` is **uninitialized raw memory** — never free it first. Write fresh allocations into it.
- `move_fn`: `*src` is a **heap pointer** to the source struct. Copy the struct fields into `dest`, then `free(*src)` and null it. Do **not** free the data buffer — it moves with the fields.
- `delete_fn`: `elm` is **a slot in the container** — do not `free(elm)` itself. Only free resources the element owns (e.g. a `data` buffer).

For `genVec` and the adapters built on it, callbacks are passed as a `container_ops` vtable:

```c
typedef struct {
    copy_fn   copy_fn;
    move_fn   move_fn;
    delete_fn del_fn;
} container_ops;
```

For `hashmap`, keys and values each get their own `container_ops` (same three fields, different type name). `hashset` still takes the three callbacks directly.

### By Value vs By Pointer

**Strategy A — By Value**: The slot holds the entire struct (`sizeof(String)` bytes). Better cache locality. Pointers into the container are invalidated by any realloc.

```c
genVec* vec = genVec_init(8, sizeof(String), &wc_str_ops);
String s;
string_create_stk(&s, "hello");
genVec_push(vec, (u8*)&s);     // str_copy is called, vec owns its own copy
string_destroy_stk(&s);        // s's buffer is freed, vec is unaffected
```

**Strategy B — By Pointer**: The slot holds `String*` (8 bytes). The pointed-to struct lives elsewhere. Addresses inside the container are stable across reallocations.

```c
genVec* vec = genVec_init(8, sizeof(String*), &wc_str_ptr_ops);
String* s = string_from_cstr("hello");
genVec_push_move(vec, (u8**)&s);    // str_move_ptr is called, s is now NULL
// vec now owns the String* and the String it points to
```

The only visible difference at call sites is the type you pass (`String` vs `String*`) and which ops instance you register. Everything else — push, pop, iterate, destroy — works identically.

### Copy vs Move at the Call Site

```c
String* s = string_from_cstr("hello");

// Copy: str_copy is called, s is still valid and still owned by you
genVec_push(vec, (u8*)s);
string_destroy(s);    // you must still free s

// Move: str_move is called, s is nulled, vec owns it
String* s2 = string_from_cstr("world");
genVec_push_move(vec, (u8**)&s2);
// s2 == NULL — do not free it
```

Move is preferred when the source is no longer needed. It avoids the deep copy entirely.

### Destruction

```c
genVec_destroy(vec);       // calls del_fn on each element, frees data, frees struct
genVec_destroy_stk(&vec);  // calls del_fn on each element, frees data — NOT the struct
```

The `_stk` variant is for vectors whose struct lives on your stack. The data buffer is still heap-allocated by default (unless you used `genVec_init_arr`), so it still needs to be freed.

---

## Error Handling

WCtoolkit distinguishes between two kinds of failures:

**Programmer errors** — null pointer passed to a function that requires one, index out of bounds, zero-size allocation — are caught by `CHECK_FATAL` and abort with a descriptive message. These are bugs, not conditions to handle at runtime.

**Expected conditions** — popping an empty container, allocating from a full arena — are reported through `wc_errno`, a thread-local variable modeled after `errno(3)`. Functions return `NULL`, `0`, or `void` on these conditions and set `wc_errno` to a code explaining why.

```c
#include "wc_errno.h"

// Check a single call
wc_errno = WC_OK;
u8* ptr = arena_alloc(arena, size);
if (!ptr && wc_errno == WC_ERR_FULL) {
    // arena is exhausted — handle it
}

// Check a batch — wc_errno stays set if any call failed
wc_errno = WC_OK;
float* a = (float*)arena_alloc(arena, 256);
float* b = (float*)arena_alloc(arena, 256);
if (wc_errno) { wc_perror("alloc"); }
```

**Rules:**
- Successful calls do **not** clear `wc_errno` — clear it yourself before a batch you want to check.
- Check the return value first; `wc_errno` tells you *why*, not *whether*.
- `wc_errno` is thread-local — each thread has an independent copy.

**Error codes:**

| Code | Meaning | Set by |
|---|---|---|
| `WC_OK` | No error | — |
| `WC_ERR_FULL` | Arena exhausted | `arena_alloc`, `arena_alloc_aligned` |
| `WC_ERR_EMPTY` | Container is empty | `genVec_pop`, `genVec_front`, `genVec_back`, `dequeue`, `queue_peek`, `stack_pop`, `stack_peek` |

---

## Macro Layer and Ergonomics

Raw `genVec` calls require casts and address-taking. `wc_macros.h` provides a typed macro layer using GNU statement expressions (`({ })`) and `__typeof__` that reads more naturally, requires no explicit casts, and works correctly for both storage strategies.

Include `wc_macros.h` and `wc_helpers.h` together to get the full ergonomic API.

### Creating Vectors

```c
// Generic
genVec* v = VEC(int, 16);                     // POD, no callbacks
genVec* v = VEC_CX(String, 8, &wc_str_ops);  // with callbacks
genVec* v = VEC_EMPTY(float);                 // capacity 0

// Common shorthands
genVec* v = VEC_OF_INT(16);
genVec* v = VEC_OF_STR(8);          // String by value
genVec* v = VEC_OF_STR_PTR(8);      // String* by pointer
genVec* v = VEC_OF_VEC(4);          // genVec by value (nested vectors)
genVec* v = VEC_OF_VEC_PTR(4);      // genVec* by pointer

// From a literal array
genVec* v = VEC_FROM_ARR(int, 4, ((int[4]){1, 2, 3, 4}));
```

### Pushing

```c
VEC_PUSH(v, 42);                    // any POD value — typeof deduces the type
VEC_PUSH(v, 3.14f);
VEC_PUSH(v, my_struct);

VEC_PUSH_COPY(v, my_string);        // explicit deep copy
VEC_PUSH_MOVE(v, my_string_ptr);    // transfer ownership, my_string_ptr → NULL
VEC_PUSH_CSTR(v, "literal");        // allocate String, move into vec
```

### Accessing

```c
// By value (Strategy A):
String  s  = VEC_AT(v, String, i);      // value copy
String* sp = VEC_AT_MUT(v, String, i);  // pointer into slot (invalidated by realloc)

// By pointer (Strategy B):
String*  s  = VEC_AT(v, String*, i);    // the stored pointer
String** sp = VEC_AT_MUT(v, String*, i);

String front = VEC_FRONT(v, String);
String back  = VEC_BACK(v, String);
```

### Iterating

```c
// Strategy A (by value): name is String* — pointer into the slot
VEC_FOREACH(v, String, s) {
    string_print(s);
}

// Strategy B (by pointer): name is String** — *name is the stored String*
VEC_FOREACH(v, String*, sp) {
    string_print(*sp);
}
```

### Popping

```c
int    x = VEC_POP(v, int);      // pops and returns by value
String s = VEC_POP(v, String);   // you own s, must destroy it
```

### Mutating

```c
VEC_SET(v, i, new_val);          // replace element at i (del_fn on old, copy_fn on new)
```

### HashMap Shorthands

```c
MAP_PUT(m, key, val);                   // copy both key and value
MAP_PUT_MOVE(m, key_ptr, val_ptr);      // move both, both → NULL
MAP_PUT_VAL_MOVE(m, key, val_ptr);      // copy key, move value
MAP_PUT_KEY_MOVE(m, key_ptr, val);      // move key, copy value
MAP_PUT_STR_STR(m, "key", "val");       // allocate two Strings and insert

int result = MAP_GET(m, int, key);      // copy value out
```

---

## Components

### Arena Allocator

A bump-pointer allocator. Allocations are O(1) pointer arithmetic. Cleanup is a single `free()`.

```c
// Heap-backed
Arena* arena = arena_create(nKB(4));   // pass 0 to use ARENA_DEFAULT_SIZE
arena_release(arena);                   // frees everything at once

// Stack-backed (no heap allocation at all)
Arena stack_arena;
ARENA_CREATE_STK_ARR(&stack_arena, 4); // 4KB on the stack — no release needed
```

**Allocating:**

```c
// Untyped
u8* raw     = arena_alloc(arena, 256);
u8* aligned = arena_alloc_aligned(arena, 64, 16);  // align to 16 bytes

// Typed macros (preferred)
int*      nums = ARENA_ALLOC_N(arena, int, 100);
MyStruct* s    = ARENA_ALLOC_ZERO(arena, MyStruct);    // zero-initialized
char*     buf  = ARENA_ALLOC_N(arena, char, 256);
```

Default alignment is `sizeof(u64)` (8 bytes), configurable via `#define ARENA_DEFAULT_ALIGNMENT`.

**Marks — partial rollback:**

```c
u64 mark = arena_get_mark(arena);      // save current position
// ... allocate freely ...
arena_clear_mark(arena, mark);         // roll back to saved position
```

**Scratch arenas — scoped temporary allocations:**

```c
// Automatic cleanup using a for-loop scope trick
ARENA_SCRATCH(temp, arena) {
    float* scratch = ARENA_ALLOC_N(arena, float, 1024);
    // ... work with scratch ...
}   // scratch memory is reclaimed here automatically

// Manual variant
arena_scratch sc = arena_scratch_begin(arena);
u8* tmp = arena_alloc(arena, nKB(1));
// ... work ...
arena_scratch_end(&sc);
```

**Querying:**

```c
u64 used      = arena_used(arena);
u64 remaining = arena_remaining(arena);
arena_clear(arena);    // reset idx to 0, memory is reusable (no free)
arena_release(arena);  // free everything
```

**Arena-allocated matrices** (no `malloc`/`free` in a hot loop):

```c
Arena* arena = arena_create(nKB(2));
Matrixf* result = matrix_arena_alloc(arena, 4, 4);

ARENA_SCRATCH(sc, arena) {
    Matrixf* a = matrix_arena_arr_alloc(arena, 4, 4, data_a);
    Matrixf* b = matrix_arena_arr_alloc(arena, 4, 4, data_b);
    matrix_xply(result, a, b);
}   // a and b are reclaimed; result survives outside the scratch

arena_release(arena);  // single cleanup for everything
```

---

### Generic Vector

The foundation of the library. `String`, `Stack`, `Queue`, and `bitVec` are all built on top of it.

**Creating:**

```c
// Heap-allocated vector struct, heap-allocated data
genVec* v = genVec_init(capacity, sizeof(T), &ops);     // with callbacks
genVec* v = genVec_init(capacity, sizeof(T), NULL);     // POD types

// Stack-allocated vector struct, heap-allocated data
genVec v;
genVec_init_stk(capacity, sizeof(T), &ops, &v);

// Fully stack-allocated (data array provided by you — cannot grow)
int buf[8];
genVec v;
genVec_init_arr(8, (u8*)buf, sizeof(int), NULL, &v);

// Pre-filled with a value
genVec* v = genVec_init_val(n, (u8*)&val, sizeof(T), &ops);

// From a literal array (heap vector, macro layer)
genVec* v = VEC_FROM_ARR(int, 4, ((int[4]){1, 2, 3, 4}));
```

The `ops` pointer is a `const container_ops*`. Define one statically and share it across all containers of the same type:

```c
static const container_ops string_ops = { str_copy, str_move, str_del };
genVec* v = genVec_init(8, sizeof(String), &string_ops);
```

`wc_helpers.h` provides `wc_str_ops`, `wc_str_ptr_ops`, `wc_vec_ops`, and `wc_vec_ptr_ops` ready to use — see [Helpers and Callbacks](#helpers-and-callbacks).

**Growth behavior** — configurable before the include:

```c
#define GENVEC_GROWTH    1.5F   // capacity multiplier on grow (default)
#define GENVEC_SHRINK_AT 0.25F  // shrink when < 25% full
#define GENVEC_SHRINK_BY 0.5F   // halve capacity on shrink
#include "gen_vector.h"
```

**Push and pop:**

```c
genVec_push(v, (u8*)&val);             // copy (calls copy_fn or memcpy)
genVec_push_move(v, (u8**)&ptr);       // move (calls move_fn or memcpy + null)

genVec_pop(v, (u8*)&out);             // copy last element out, then remove
genVec_pop(v, NULL);                   // just remove last element
```

**Access:**

```c
genVec_get(v, i, (u8*)&out);           // copy element at i into out
const u8* p  = genVec_get_ptr(v, i);   // const pointer into slot i
u8*       pm = genVec_get_ptr_mut(v, i); // mutable pointer into slot i

const u8* first = genVec_front(v);
const u8* last  = genVec_back(v);
// Both set wc_errno = WC_ERR_EMPTY if the vector is empty
```

**Insert and remove:**

```c
genVec_insert(v, i, (u8*)&val);              // shift right, insert copy at i
genVec_insert_move(v, i, (u8**)&ptr);        // shift right, insert move at i
genVec_insert_multi(v, i, (u8*)arr, n);      // insert n elements at i

genVec_remove(v, i, (u8*)&out);             // copy element out, shift left
genVec_remove(v, i, NULL);                   // just remove at i
genVec_remove_range(v, l, r);               // remove [l, r] inclusive

genVec_replace(v, i, (u8*)&val);            // overwrite i (del_fn then copy_fn)
genVec_replace_move(v, i, (u8**)&ptr);      // overwrite i with ownership transfer
```

**Capacity and state:**

```c
u64 n   = genVec_size(v);
u64 cap = genVec_capacity(v);
b8  emp = genVec_empty(v);

genVec_reserve(v, new_cap);     // ensure capacity >= new_cap (never shrinks)
genVec_reserve_val(v, n, val);  // grow to n, fill new slots with val
genVec_shrink_to_fit(v);        // reallocate to exact size
```

**Lifecycle:**

```c
genVec_clear(v);         // call del_fn on every element, size → 0, keep capacity
genVec_reset(v);         // call del_fn on every element, free data, capacity → 0
genVec_copy(dest, src);  // deep copy src into dest (cleans up dest first)
genVec_move(dest, &src); // transfer ownership from heap src to dest, src → NULL

genVec_destroy(v);       // for heap-allocated vectors
genVec_destroy_stk(&v);  // for stack-allocated vectors
```

**Print:**

```c
genVec_print(v, my_print_fn);   // applies print_fn to each element
```

---

### String

A dynamic, length-based string. No null terminator is stored internally. Implemented as `typedef genVec String`, so all `genVec` internals apply — growth, shrink, inline data, the works.

**Creating:**

```c
String* s = string_create();                 // empty, heap
String* s = string_from_cstr("hello");       // from C literal, heap
String* s = string_from_string(other);       // deep copy, heap

String s;
string_create_stk(&s, "hello");              // stack struct, heap data
string_create_stk(&s, NULL);                // empty, stack struct

string_reserve(s, 64);                       // pre-allocate capacity
string_reserve_char(s, 5, 'x');             // pre-allocate and fill with 'x'
```

**Appending:**

```c
string_append_cstr(s, " world");
string_append_char(s, '!');
string_append_string(s, other);
string_append_string_move(s, &other);   // consume and nulls other
```

**Inserting and removing:**

```c
string_insert_char(s, i, 'c');
string_insert_cstr(s, i, "sub");
string_insert_string(s, i, other);

char c = string_pop_char(s);         // remove and return last char
string_remove_char(s, i);
string_remove_range(s, l, r);        // remove [l, r] inclusive
string_clear(s);                     // size → 0, keep capacity
```

**Access:**

```c
char  c   = string_char_at(s, i);
void      string_set_char(s, i, 'x');

char* ptr = string_data_ptr(s);      // raw pointer, NO null terminator
char* cs  = string_to_cstr(s);       // malloced, null-terminated copy — you free it

u64 len = string_len(s);
u64 cap = string_capacity(s);
b8  emp = string_empty(s);
```

**Comparison and search:**

```c
int  cmp = string_compare(a, b);          // like strcmp: negative, 0, positive
b8   eq  = string_equals(a, b);
b8   eq  = string_equals_cstr(s, "hi");

u64 idx = string_find_char(s, 'e');       // returns (u64)-1 if not found
u64 idx = string_find_cstr(s, "sub");

String* sub = string_substr(s, start, length);  // heap-allocated substring
```

**Temporary null-terminated access without allocation:**

```c
// Appends '\0', runs your block, then removes it
TEMP_CSTR_READ(s) {
    printf("%s\n", string_data_ptr(s));   // safe to use as cstr here
}   // '\0' removed here
```

**Lifecycle:**

```c
string_copy(dest, src);      // deep copy into dest (frees dest's old data first)
string_move(dest, &src);     // transfer ownership, src → NULL

string_destroy(s);           // heap-allocated
string_destroy_stk(&s);      // stack-allocated struct
```

---

### Stack

A LIFO adapter over `genVec`. `typedef genVec Stack`. All callbacks and storage strategies from `genVec` apply.

```c
Stack* s = stack_create(capacity, sizeof(T), copy_fn, move_fn, del_fn);

stack_push(s, (u8*)&val);
stack_push_move(s, (u8**)&ptr);

int out;
stack_pop(s, (u8*)&out);          // WC_ERR_EMPTY if empty
stack_pop(s, NULL);

const u8* top = stack_peek_ptr(s);
stack_peek(s, (u8*)&out);

u64 n = stack_size(s);
b8  e = stack_empty(s);

stack_clear(s);
stack_reset(s);
stack_destroy(s);
stack_print(s, print_fn);
```

---

### Queue

A circular FIFO queue backed by `genVec`. Grows and shrinks automatically. Uses head/tail pointers to avoid shifting elements on every dequeue.

```c
Queue* q = queue_create(capacity, sizeof(T), copy_fn, move_fn, del_fn);

enqueue(q, (u8*)&val);
enqueue_move(q, (u8**)&ptr);

int out;
dequeue(q, (u8*)&out);            // WC_ERR_EMPTY if empty
dequeue(q, NULL);

const u8* front = queue_peek_ptr(q);
queue_peek(q, (u8*)&out);

u64 n = queue_size(q);
b8  e = queue_empty(q);
u64 c = queue_capacity(q);

queue_clear(q);
queue_reset(q);
queue_shrink_to_fit(q);
queue_destroy(q);
queue_print(q, print_fn);
```

When the buffer wraps around and more space is needed, `queue_compact` rebuilds it linearly into a new buffer — the FIFO order is always preserved across resizes.

---

### HashMap

Open-addressing hashmap with linear probing and tombstone deletion. Keys and values are heap-allocated separately per bucket. Supports independent copy/move/delete semantics for keys and values via `container_ops` vtables.

Load factors: grow at **0.70**, shrink at **0.20**. Capacity is always chosen from a prime number table.

**Creating:**

```c
hashmap* m = hashmap_create(
    sizeof(key_type), sizeof(val_type),
    hash_fn, cmp_fn,          // NULL = FNV-1a, memcmp
    key_ops, val_ops          // const container_ops*, NULL for POD
);
```

For `int` keys and `int` values with no ownership:

```c
hashmap* m = hashmap_create(sizeof(int), sizeof(int),
                             NULL, NULL, NULL, NULL);
```

For `String` keys and `String` values, use the pre-built ops from `wc_helpers.h`:

```c
hashmap* m = hashmap_create(
    sizeof(String), sizeof(String),
    murmurhash3_str, str_cmp,
    &wc_str_ops, &wc_str_ops
);
```

**Inserting:**

```c
// Copy both key and value
b8 was_update = hashmap_put(m, (u8*)&key, (u8*)&val);

// Move both (key and val are nulled after)
hashmap_put_move(m, (u8**)&key, (u8**)&val);

// Mixed: copy key, move value
hashmap_put_val_move(m, (u8*)&key, (u8**)&val);

// Mixed: move key, copy value
hashmap_put_key_move(m, (u8**)&key, (u8*)&val);

// Returns 1 if key existed and was updated, 0 if key was new
```

**Looking up:**

```c
b8 found = hashmap_get(m, (u8*)&key, (u8*)&out);   // copy value into out
u8* ptr  = hashmap_get_ptr(m, (u8*)&key);           // direct pointer to value
b8 has   = hashmap_has(m, (u8*)&key);
```

**Deleting:**

```c
b8 found = hashmap_del(m, (u8*)&key, (u8*)&out);   // copy value out before delete
b8 found = hashmap_del(m, (u8*)&key, NULL);          // delete without copying
```

**State and lifecycle:**

```c
u64 n = hashmap_size(m);
u64 c = hashmap_capacity(m);
b8  e = hashmap_empty(m);
hashmap_print(m, key_print_fn, val_print_fn);
hashmap_destroy(m);
```

**Convenience macros** (from `wc_macros.h`):

```c
MAP_PUT(m, key, val);
MAP_PUT_MOVE(m, key_ptr, val_ptr);
MAP_PUT_VAL_MOVE(m, key, val_ptr);
MAP_PUT_STR_STR(m, "name", "Alice");    // creates Strings, inserts by move
int v = MAP_GET(m, int, key);
```

---

### HashSet

Same architecture as HashMap — open addressing, linear probing, tombstones, prime capacities — but stores only elements with no associated value. Takes the three callbacks directly rather than as a vtable.

```c
hashset* s = hashset_create(
    sizeof(elm_type),
    hash_fn, cmp_fn,        // NULL = FNV-1a, memcmp
    copy_fn, move_fn, del_fn
);

b8 already_existed = hashset_insert(s, (u8*)&elm);
b8 already_existed = hashset_insert_move(s, (u8**)&elm);

b8 found   = hashset_has(s, (u8*)&elm);
b8 removed = hashset_remove(s, (u8*)&elm);

u64 n = hashset_size(s);
u64 c = hashset_capacity(s);
b8  e = hashset_empty(s);

hashset_clear(s);    // remove all elements, keep capacity
hashset_reset(s);    // remove all elements, reset to initial capacity
hashset_print(s, print_fn);
hashset_destroy(s);
```

---

### BitVector

A compact dynamic bit array backed by a `genVec<u8>`. Bits are indexed from 0. The backing byte array grows automatically as you set higher-index bits.

```c
bitVec* bv = bitVec_create();

bitVec_set(bv, 42);           // set bit 42 to 1
bitVec_clear(bv, 42);         // set bit 42 to 0
bitVec_toggle(bv, 42);        // flip bit 42
u8 b = bitVec_test(bv, 42);  // returns 0 or 1

bitVec_push(bv);              // append a set (1) bit at the end
bitVec_pop(bv);               // remove the last bit

u64 nbits  = bitVec_size_bits(bv);    // logical number of bits
u64 nbytes = bitVec_size_bytes(bv);   // backing byte count

bitVec_print(bv, byte_index);  // print one byte as binary digits
bitVec_destroy(bv);
```

`bitVec_set` will grow the backing array as needed — setting bit 255 on an empty vector allocates 32 bytes automatically. `bitVec_clear`, `bitVec_test`, and `bitVec_toggle` require the bit to already be within the allocated range.

---

### Matrix

Row-major 2D float matrix (`Matrixf`). Three allocation strategies: heap, stack, arena.

**Creating:**

```c
Matrixf* m = matrix_create(rows, cols);
Matrixf* m = matrix_create_arr(rows, cols, float_array);

// Stack: you provide the struct and the data array
float data[9];
Matrixf m;
matrix_create_stk(&m, 3, 3, data);

// Arena (no free needed — released with the arena)
Matrixf* m = matrix_arena_alloc(arena, 3, 3);
Matrixf* m = matrix_arena_arr_alloc(arena, 3, 3, float_array);

matrix_destroy(m);   // heap only — do NOT call on stack or arena matrices
```

**Setting values:**

```c
matrix_set_elm(m, value, row, col);

// From a flat C array (row-major)
matrix_set_val_arr(m, 9, (float*)(float[3][3]){
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9}
});
```

**Element access macros:**

```c
MATRIX_AT(m, i, j)     // m->data[i * m->n + j] — direct lvalue access
IDX(m, i, j)           // just the index, without dereferencing
MATRIX_TOTAL(m)        // m->m * m->n
```

**Operations:**

```c
matrix_add(out, a, b);         // out = a + b  (out may alias a or b)
matrix_sub(out, a, b);         // out = a - b  (out may alias a or b)
matrix_scale(m, scalar);       // m *= scalar (in place)
matrix_div(m, scalar);         // m /= scalar (in place)
matrix_copy(dest, src);        // element-wise copy

// Multiplication (out may NOT alias a or b)
matrix_xply(out, a, b);        // blocked ikj — good for small to medium matrices
matrix_xply_2(out, a, b);      // transposes b first — better for large matrices

matrix_T(out, m);              // transpose (out may NOT alias m)

// LU decomposition and determinant
matrix_LU_Decomp(L, U, m);    // Doolittle algorithm — m = L * U
float det = matrix_det(m);    // uses LU decomposition internally

matrix_print(m);               // formatted matrix to stdout
```

---

### Random (PCG32)

Permuted Congruential Generator. Fast, high-quality, reproducible. All output uses a single global RNG instance.

**Seeding:**

```c
pcg32_rand_seed(seed, sequence);   // explicit seed — same inputs always produce same sequence
pcg32_rand_seed_time();            // seed from time(NULL) — different each second
```

**Integer output:**

```c
u32 r    = pcg32_rand();                  // full 32-bit random
u32 dice = pcg32_rand_bounded(6);         // uniform in [0, 6), no modulo bias
```

**Floating point:**

```c
float  f = pcg32_rand_float();              // uniform in [0.0, 1.0)
double d = pcg32_rand_double();             // uniform in [0.0, 1.0), 53-bit precision

float  fv = pcg32_rand_float_range(-1.0f, 1.0f);
double dv = pcg32_rand_double_range(0.0,  100.0);
```

**Gaussian:**

```c
float g  = pcg32_rand_gaussian();                      // standard normal N(0, 1)
float gv = pcg32_rand_gaussian_custom(mean, stddev);   // N(mean, stddev²)
```

The Gaussian generator uses the Box-Muller transform. It internally caches one value per two calls — the second value from each transform pair is returned on the next call. This is efficient but means the internal `static` state breaks if you use multiple independent RNG instances (see Known Issues).

---

### Fast Math

Approximations of common math functions using numerical methods. Faster than `<math.h>` with reduced precision. Originally built to support the Gaussian RNG, but generally usable wherever exactness is not required.

```c
#include "fast_math.h"

float s = fast_sqrt(x);    // Newton-Raphson, 4 iterations, bit-hack initial guess
float l = fast_log(x);     // range reduction + Taylor series (5 terms)
float e = fast_exp(x);     // range reduction + repeated squaring for integer part
float s = fast_sin(x);     // Taylor series, input reduced to [-π, π]
float c = fast_cos(x);     // = fast_sin(x + π/2)
float v = fast_ceil(x);
```

Constants: `PI`, `TWO_PI`, `LN2` are defined in `fast_math.h`.

---

## Helpers and Callbacks

`wc_helpers.h` provides ready-to-use copy/move/delete/print/compare callbacks for `String` and `genVec` in both storage strategies. It also exports four pre-built `container_ops` instances you can pass directly to any container — no need to define your own vtable for the common cases.

Include it alongside `wc_macros.h` for the full ergonomic API.

### Pre-built ops instances

```c
// Pass these directly to genVec_init, VEC_CX, hashmap_create, etc.
&wc_str_ops       // String stored by value  — { str_copy,     str_move,     str_del     }
&wc_str_ptr_ops   // String* stored by ptr   — { str_copy_ptr, str_move_ptr, str_del_ptr }
&wc_vec_ops       // genVec stored by value  — { vec_copy,     vec_move,     vec_del     }
&wc_vec_ptr_ops   // genVec* stored by ptr   — { vec_copy_ptr, vec_move_ptr, vec_del_ptr }
```

Example:

```c
// Vector of Strings
genVec* v = genVec_init(8, sizeof(String), &wc_str_ops);

// Map from String keys to String values
hashmap* m = hashmap_create(sizeof(String), sizeof(String),
                             murmurhash3_str, str_cmp,
                             &wc_str_ops, &wc_str_ops);

// Vector of Strings via macro
genVec* v = VEC_CX(String, 8, &wc_str_ops);
```

### For String, stored by value

```c
str_copy    // copy_fn  — deep copies the String struct and its data buffer
str_move    // move_fn  — moves fields into slot, frees heap container, nulls src
str_del     // delete_fn — frees data buffer only (not the slot)
str_print   // print_fn
str_cmp     // compare_fn — wraps string_compare
```

### For String*, stored by pointer

```c
str_copy_ptr    // copy_fn  — allocs new String, deep copies
str_move_ptr    // move_fn  — transfers pointer into slot, nulls src
str_del_ptr     // delete_fn — calls string_destroy (frees both data and struct)
str_print_ptr   // print_fn
str_cmp_ptr     // compare_fn
```

### For genVec, stored by value (vec of vecs)

```c
vec_copy       // copy_fn
vec_move       // move_fn
vec_del        // delete_fn
vec_print_int  // print_fn for int elements
```

### For genVec*, stored by pointer

```c
vec_copy_ptr
vec_move_ptr
vec_del_ptr
vec_print_int_ptr
```

### Hash functions (from map_setup.h)

```c
fnv1a_hash          // default — fast 32-bit FNV-1a, works for any key type
murmurhash3_str     // MurmurHash3 for String keys (by value)
murmurhash3_str_ptr // MurmurHash3 for String* keys (by pointer)
```

### Print functions (from common.h)

```c
wc_print_int    wc_print_u32    wc_print_u64
wc_print_float  wc_print_char   wc_print_cstr
```

---

## Building

The project uses CMake with Clang. C extensions must be enabled (`set(CMAKE_C_EXTENSIONS ON)`) for GNU statement expressions used in `wc_macros.h`.

```bash
# Debug build: ASan + UBSan, -O0
cmake -B build -G Ninja
cmake --build build
./build/main

# Release build: -O3, no sanitizers
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

All source files in `src/` are compiled into both the `main` and `tests` targets. There is no separate library target — the sources are included directly.

---

## Testing

The test suite lives in `tests/`. Each component has its own test file. `test_main.c` runs them all.

```bash
# Build and run tests
cmake -B build -G Ninja
cmake --build build
./build/tests

# Via CTest
ctest --test-dir build --output-on-failure
```

Each suite exercises creation, operations, edge cases, ownership correctness, `wc_errno` signaling, growth/shrink behavior, and cross-component ownership patterns. The random number suite tests statistical properties: mean, standard deviation, 68% rule, and full range coverage.

The test framework (`wc_test.h`) is minimal and self-contained — zero dependencies. Assertions are non-aborting: all asserts in a test function run even after a failure, so you see every problem in one pass.

```
══ String ══
  test_create_empty                                 OK
  test_append_cstr                                  OK
  test_insert_char_mid                              FAIL (1 assertion(s))
    FAIL at: string_equals_cstr(s, "abc")
         at tests/string_test.c:78
```

---

## Single-Header Distribution

Any component (and its full transitive dependency chain) can be bundled into a self-contained single-header file — no separate compilation step required. Drop the file into your project and include it.

**Generating:**

```bash
cd scripts

# Specific components — dependencies pulled in automatically
python make_single_header.py arena gen_vector String

# Everything
python make_single_header.py --all

# Custom directory layout
python make_single_header.py --all \
    --include-dir path/to/include \
    --src-dir     path/to/src \
    --out-dir     path/to/output

# List all components and their dependency chains
python make_single_header.py --list
```

Output is written to `../single_header/<component>_single.h`.

**Using a generated header:**

In **exactly one** `.c` file in your project, define `WC_IMPLEMENTATION` before the include:

```c
// impl.c — only in this one file
#define WC_IMPLEMENTATION
#include "String_single.h"
```

In all other files, just include normally:

```c
// other.c
#include "String_single.h"
```

**Using multiple single-headers together:**

Each implementation block is guarded against duplicate emission, so combining several single-headers is safe:

```c
#define WC_IMPLEMENTATION
#include "String_single.h"     // inlines: common, gen_vector, String
#include "hashmap_single.h"    // inlines: common, map_setup, hashmap
                               // common is NOT duplicated — guard prevents it
```

**Dependency table:**

| Component | Depends on |
|---|---|
| `common` | — |
| `wc_errno` | — |
| `fast_math` | `common` |
| `arena` | `common`, `wc_errno` |
| `gen_vector` | `common`, `wc_errno` |
| `bit_vector` | `gen_vector` |
| `String` | `gen_vector` |
| `Stack` | `gen_vector` |
| `Queue` | `gen_vector` |
| `map_setup` | `String` |
| `random` | `fast_math` |
| `hashmap` | `map_setup` |
| `hashset` | `map_setup` |
| `matrix` | `arena` |
| `helpers` | `String` |

---

## Known Issues

These are documented bugs identified but not yet fixed.

**`ARENA_PUSH_ARRAY` macro** (`arena.h`): The type syntax `(T)* _dst` is a compiler error. Should be `T* _dst`. The macro is unusable as written.

**`prev_prime` underflow** (`map_setup.h`): The loop uses `u64 i = PRIMES_COUNT - 1; i >= 0` as its condition. Since `i` is unsigned, it wraps around on underflow and the loop never exits. Change the loop index to `int` or `i64`.

**`find_slot` sentinel** (`hashmap.c`, `hashset.c`): When the table is entirely tombstones and no `EMPTY` bucket is found, the function returns `0` as a fallback. `0` is a valid bucket index. Should return `(u64)-1` and callers should check for it.

**Gaussian spare state** (`random.c`): `pcg32_rand_gaussian` caches its second Box-Muller output in a `static` variable. This state is invisible to callers and breaks correctness if multiple independent RNG instances are needed. Move `gaussian_spare` and `has_spare` into `pcg32_random_t`.

---

## Type and Diagnostic Reference

### Primitive types (`common.h`)

```c
typedef uint8_t  u8;
typedef uint8_t  b8;    // boolean: false (0) / true (1)
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define false ((b8)0)
#define true  ((b8)1)
```

### Size helpers

```c
nKB(n)   // n * 1024
nMB(n)   // n * 1024 * 1024
```

### Casting helpers

```c
cast(x)     // (u8*)(&x)   — address of a local variable
castptr(x)  // (u8*)(x)    — reinterpret a pointer as u8*
```

### Diagnostics

```c
FATAL(fmt, ...)                      // print to stderr, exit
WARN(fmt, ...)                       // print to stdout, continue
LOG(fmt, ...)                        // info print

CHECK_FATAL(cond, fmt, ...)          // exit if cond is true (bugs)
CHECK_WARN(cond, fmt, ...)           // warn if cond is true
CHECK_WARN_RET(cond, ret, fmt, ...)  // warn and return ret if cond is true
```

All diagnostics print `[FATAL]`, `[WARN]`, or `[LOG]` in color, with file, line, and function name.

### Function pointer typedefs (`common.h`)

```c
typedef void (*copy_fn)(u8* dest, const u8* src);
typedef void (*move_fn)(u8* dest, u8** src);
typedef void (*delete_fn)(u8* elm);
typedef void (*print_fn)(const u8* elm);
typedef int  (*compare_fn)(const u8* a, const u8* b, u64 size);
typedef u64  (*custom_hash_fn)(const u8* key, u64 size);
```

---

## License

MIT — see `LICENSE`.
