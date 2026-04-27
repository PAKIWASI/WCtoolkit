# WCtoolkit

A C toolkit built around explicit ownership, value semantics, and zero hidden costs.

WCtoolkit is designed for C programmers who care about lifetime correctness more than convenience.
Containers do not guess how to copy or destroy your data — you must define it.
This makes ownership explicit, prevents accidental deep copies, and scales well as programs grow.

No dependencies beyond the C standard library.
Targets C11 with GNU extensions (Clang/GCC). Can degrade to C99 by sacrificing macro ergonomics. 

```c
// vector of complex 'String' type
genVec* vec = VEC_CX(String, 10, &wc_str_ops); // ops has copy/move/del fns for String

VEC_PUSH_CSTR(vec, "PAKI");     // create Strings and move into vec
VEC_PUSH_CSTR(vec, "WASI");     // zero copies, only one move

// append in place
VEC_FOREACH(vec, String, s) { string_append_char(s, '!'); }

String* s = string_create();    // create new string as buffer
genVec_pop(vec, castptr(s));    // get element out of vec, s owns it now

string_print(s);                // do whatever
genVec_print(vec, str_print);

string_destroy(s);              // free resources
genVec_destroy(vec);
```

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
  - [Matrix (float)](#matrix-float)
  - [Matrix (generic)](#matrix-generic)
  - [Random (PCG32)](#random-pcg32)
  - [Fast Math](#fast-math)
- [Helpers and Callbacks](#helpers-and-callbacks)
- [Building](#building)
- [Testing](#testing)
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
│  │ {stk/…}  │ {stk/…}  │ {stk/…}  │ {stk/…}  │      │
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

Growth thresholds for `genVec` and `String` are compile-time configurable macros set before the include. The hash table load factor is a constant in `map_setup.h`. Nothing is hidden in a global config struct.

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

The library is layered deliberately. `common.h` defines primitive types and macros. `gen_vector` is the single workhorse container. `Stack`, `Queue`, and `bitVec` are built on top of it, reusing its growth, insert, remove, and copy logic without duplicating it. `String` is its own independent implementation — it does not depend on `gen_vector`. The hash containers share `map_setup.h` for hashing and load factor logic.

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
- `delete_fn`: `elm` is **a slot in the container** — do not `free(elm)` itself. Only free resources the element owns (e.g. a heap data buffer).

These three are grouped into a shared vtable:

```c
typedef struct {
    copy_fn   copy_fn;
    move_fn   move_fn;
    delete_fn del_fn;
} container_ops;
```

Pass a `const container_ops*` to any container. For hashmap, keys and values each get their own `container_ops`. For POD types, pass `NULL` for the whole pointer.

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

The `_stk` variant is for vectors whose struct lives on your stack. The data buffer is still heap-allocated by default, so it still needs to be freed.

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
| `WC_ERR_EMPTY` | Container is empty | `genVec_pop`, `genVec_front`, `genVec_back`, `dequeue`, `queue_peek`, `stack_pop`, `stack_peek`, `string_pop_char` |
| `WC_ERR_INVALID_OP` | Precondition not met | reserved for future use |

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

### Popping and Mutating

```c
int    x = VEC_POP(v, int);      // pops and returns by value
String s = VEC_POP(v, String);   // you own s, must destroy it

VEC_SET(v, i, new_val);          // replace element at i (del_fn on old, copy_fn on new)
```

### HashMap Shorthands

```c
MAP_PUT(m, key, val);                   // copy both key and value
MAP_PUT_MOVE(m, key_ptr, val_ptr);      // move both, both → NULL
MAP_PUT_VAL_MOVE(m, key, val_ptr);      // copy key, move value
MAP_PUT_KEY_MOVE(m, key_ptr, val);      // move key, copy value
MAP_PUT_STR_STR(m, "key", "val");       // allocate two Strings and insert by move

int result = MAP_GET(m, int, key);      // copy value out

// Iteration (do not modify keys)
MAP_FOREACH_KEY(m, String, k) { string_print(k); }
MAP_FOREACH_VAL(m, int, v)    { printf("%d\n", *v); }
```

### HashSet Shorthands

```c
SET_INSERT(set, elm);            // copy insert
SET_INSERT_CSTR(set, "hello");   // allocate String, insert by move
SET_FROM_VEC(vec, hash_fn, cmp_fn);   // build a set from an existing vector

// Iteration (do not modify elements)
SET_FOREACH(set, int, e) { printf("%d\n", *e); }
```

---

## Components

### Arena Allocator

A bump-pointer allocator. Allocations are O(1) pointer arithmetic. Cleanup is a single `free()`.

```c
// Heap-backed
Arena* arena = arena_create(nKB(4));   // pass 0 to use ARENA_DEFAULT_SIZE (4KB)
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
MyStruct* arr  = ARENA_ALLOC_ZERO_N(arena, MyStruct, 8);
```

Default alignment is `sizeof(u64)` (8 bytes), configurable via `#define ARENA_DEFAULT_ALIGNMENT` before the include.

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
arena_release(arena);  // free everything — heap arenas only
```

---

### Generic Vector

The foundation of the library. `Stack`, `Queue`, and `bitVec` are all built on top of it.

**Creating:**

```c
// Heap-allocated struct + heap-allocated data
genVec* v = genVec_init(capacity, sizeof(T), &ops);   // with callbacks
genVec* v = genVec_init(capacity, sizeof(T), NULL);   // POD types

// Stack-allocated struct, heap-allocated data
genVec v;
genVec_init_stk(capacity, sizeof(T), &ops, &v);

// Fully stack-allocated (data array provided by you — cannot grow)
int buf[8];
genVec v;
genVec_init_arr(8, (u8*)buf, sizeof(int), NULL, &v);

// Pre-filled with a value
genVec* v = genVec_init_val(n, (u8*)&val, sizeof(T), &ops);

// From a literal array (macro layer)
genVec* v = VEC_FROM_ARR(int, 4, ((int[4]){1, 2, 3, 4}));
```

**Growth behavior** — configurable before the include:

```c
#define GENVEC_GROWTH 1.5F   // capacity multiplier on grow (default)
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
genVec_get(v, i, (u8*)&out);            // copy element at i into out
const u8* p  = genVec_get_ptr(v, i);    // const pointer into slot i
u8*       pm = genVec_get_ptr_mut(v, i); // mutable pointer into slot i

const u8* first = genVec_front(v);      // sets wc_errno = WC_ERR_EMPTY if empty
const u8* last  = genVec_back(v);
```

**Insert and remove:**

```c
genVec_insert(v, i, (u8*)&val);              // shift right, insert copy at i
genVec_insert_move(v, i, (u8**)&ptr);        // shift right, insert move at i
genVec_insert_multi(v, i, (u8*)arr, n);      // insert n elements at i

genVec_remove(v, i, (u8*)&out);              // copy element out, shift left
genVec_remove(v, i, NULL);                    // just remove at i
genVec_remove_range(v, start, len);           // remove [start, start+len)

genVec_replace(v, i, (u8*)&val);             // overwrite i (del_fn then copy_fn)
genVec_replace_move(v, i, (u8**)&ptr);       // overwrite i with ownership transfer
```

**Swap and O(1) delete:**

```c
genVec_swap(v, i, j);                // swap elements at i and j
genVec_swap_pop(v, i, (u8*)&out);   // swap i with last, pop last — O(1) unordered remove
genVec_swap_pop(v, i, NULL);         // same, discard the element
```

**Search and slice:**

```c
u64 idx = genVec_find(v, (u8*)&elm, cmp_fn);  // linear scan; cmp_fn=NULL uses memcmp
                                                // returns (u64)-1 if not found

genVec* sub = genVec_subarr(v, start, len);    // deep copy of [start, start+len)
```

**Capacity and state:**

```c
u64 n   = genVec_size(v);
u64 cap = genVec_capacity(v);
b8  emp = genVec_empty(v);

genVec_reserve(v, new_cap);          // ensure capacity >= new_cap (never shrinks)
genVec_reserve_val(v, n, (u8*)&val); // grow to n, fill new slots with val
genVec_shrink_to_fit(v);             // reallocate to exact size
```

**Lifecycle:**

```c
genVec_clear(v);          // del_fn on every element, size → 0, keep capacity
genVec_reset(v);          // del_fn on every element, free data, capacity → 0
genVec_copy(dest, src);   // deep copy src into dest (cleans up dest first)
genVec_move(dest, &src);  // transfer ownership from heap src to dest, src → NULL

genVec_destroy(v);        // for heap-allocated vectors
genVec_destroy_stk(&v);   // for stack-allocated vectors

genVec_print(v, print_fn);
```

---

### String

A dynamic, length-based string with **Small String Optimisation (SSO)**. No null terminator is stored internally. Short strings (up to 24 bytes) live entirely inside the struct with no heap allocation. Longer strings spill to a heap buffer transparently. `String` does **not** depend on `gen_vector`.

**Layout (40 bytes):**

```
┌─────────────────────────────────────┐
│  union { char* heap; char stk[24] } │  24 bytes — data in-place or heap ptr
│  u64 size                           │   8 bytes
│  u64 capacity                       │   8 bytes — equals 24 when in SSO mode
└─────────────────────────────────────┘
```

When `capacity == 24` (`STR_SSO_SIZE`), characters live in `stk[]`. When `capacity > 24`, `heap` is valid. The transition is automatic.

**Growth behavior** — configurable before the include:

```c
#define STRING_GROWTH 1.5F   // capacity multiplier on grow (default)
#include "String.h"
```

**Creating:**

```c
String* s = string_create();                 // empty, heap struct
String* s = string_from_cstr("hello");       // from C literal, heap struct
String* s = string_from_string(other);       // deep copy, heap struct

String s;
string_create_stk(&s, "hello");              // stack struct
string_create_stk(&s, NULL);                // empty, stack struct
```

**Capacity:**

```c
string_reserve(s, 64);               // pre-allocate (never shrinks)
string_reserve_char(s, 5, 'x');      // resize to 5, fill new slots with 'x'
string_shrink_to_fit(s);             // reallocate to exact size; demotes to SSO if size <= 24
```

**Appending:**

```c
string_append_char(s, '!');
string_append_cstr(s, " world");
string_append_string(s, other);
string_append_string_move(s, &other);   // consume other, nulls it
```

**Inserting and removing:**

```c
string_insert_char(s, i, 'c');
string_insert_cstr(s, i, "sub");
string_insert_string(s, i, other);

char c = string_pop_char(s);            // remove and return last char (WC_ERR_EMPTY if empty)
string_remove_char(s, i);
string_remove_range(s, start, len);     // remove [start, start+len)
string_clear(s);                        // size → 0, keep capacity
```

**Access and conversion:**

```c
char  c   = string_char_at(s, i);
void      string_set_char(s, i, 'x');

char* ptr = string_data_ptr(s);         // raw pointer, NO null terminator; NULL if empty
char* cs  = string_to_cstr(s);          // malloc'd, null-terminated — caller must free
void      string_to_cstr_buf(s, buf, n); // write into caller-provided buffer of size n

u64 len = string_len(s);
u64 cap = string_capacity(s);
b8  emp = string_empty(s);
b8  sso = string_sso(s);               // true when string is in SSO mode
```

**Comparison and search:**

```c
int  cmp = string_compare(a, b);          // negative, 0, positive
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
    printf("%s\n", string_data_ptr(s));
}   // '\0' removed automatically
// Note: do NOT break/return/goto inside this block
```

**Lifecycle:**

```c
string_copy(dest, src);      // deep copy into dest (frees dest's old data first)
string_move(dest, &src);     // transfer ownership, src → NULL

string_destroy(s);           // heap-allocated struct
string_destroy_stk(&s);      // stack-allocated struct — leaves it in valid empty SSO state
string_print(s);             // prints with surrounding quotes
```

---

### Stack

A LIFO adapter over `genVec`. `typedef genVec Stack`. All callbacks and storage strategies from `genVec` apply.

```c
Stack* s = stack_create(capacity, sizeof(T), &ops);   // ops = NULL for POD
Stack* s = stack_create_val(n, (u8*)&val, sizeof(T), &ops);

stack_push(s, (u8*)&val);
stack_push_move(s, (u8**)&ptr);

int out;
stack_pop(s, (u8*)&out);           // sets wc_errno = WC_ERR_EMPTY if empty
stack_pop(s, NULL);

const u8* top = stack_peek_ptr(s);
stack_peek(s, (u8*)&out);

u64 n = stack_size(s);
b8  e = stack_empty(s);
u64 c = stack_capacity(s);

stack_clear(s);
stack_reset(s);
stack_destroy(s);
stack_print(s, print_fn);
```

---

### Queue

A circular FIFO queue backed by `genVec`. Grows and shrinks automatically. Uses head/tail indices to avoid shifting elements on every dequeue.

```c
Queue* q = queue_create(capacity, sizeof(T), &ops);   // ops = NULL for POD
Queue* q = queue_create_val(n, (u8*)&val, sizeof(T), &ops);

enqueue(q, (u8*)&val);
enqueue_move(q, (u8**)&ptr);

int out;
dequeue(q, (u8*)&out);            // sets wc_errno = WC_ERR_EMPTY if empty
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

When the circular buffer needs to grow or shrink, `queue_compact` rebuilds it into a contiguous linear layout — FIFO order is always preserved across resizes.

---

### HashMap

Open-addressing hashmap using **Robin Hood hashing** with backward-shift deletion (no tombstones). Capacity is always a **power of two** — index calculation uses a bitmask instead of modulo. Keys, PSLs, and values are stored in three parallel arrays.

Robin Hood invariant: entries are kept sorted by their probe distance, so lookups terminate as soon as a slot's PSL drops below the expected distance — average probe length stays very short even at high load.

Load factor: **grows at 0.75**. Initial capacity: **16**.

**Creating:**

```c
hashmap* m = hashmap_create(
    sizeof(key_type), sizeof(val_type),
    hash_fn,  cmp_fn,     // NULL → wyhash, memcmp
    key_ops,  val_ops     // const container_ops*, NULL for POD
);
```

For `int` keys and `int` values:

```c
hashmap* m = hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
```

For `String` keys and `String` values using pre-built helpers:

```c
hashmap* m = hashmap_create(
    sizeof(String), sizeof(String),
    wyhash_str, NULL,           // wyhash_str hashes String by value; NULL → memcmp
    &wc_str_ops, &wc_str_ops
);
```

**Inserting:**

```c
// Returns 1 if key existed (updated), 0 if new
b8 updated = hashmap_put(m, (u8*)&key, (u8*)&val);          // copy both
b8 updated = hashmap_put_move(m, (u8**)&key, (u8**)&val);   // move both → NULL
b8 updated = hashmap_put_val_move(m, (u8*)&key, (u8**)&val); // copy key, move val
b8 updated = hashmap_put_key_move(m, (u8**)&key, (u8*)&val); // move key, copy val
```

**Looking up:**

```c
b8   found = hashmap_get(m, (u8*)&key, (u8*)&out);  // copy value into out
u8*  ptr   = hashmap_get_ptr(m, (u8*)&key);          // direct pointer to value slot
b8   has   = hashmap_has(m, (u8*)&key);
```

**Deleting:**

```c
b8 found = hashmap_del(m, (u8*)&key, (u8*)&out);   // copy value out before delete
b8 found = hashmap_del(m, (u8*)&key, NULL);          // delete without copying
```

**Lifecycle:**

```c
hashmap_clear(m);              // remove all elements, keep capacity
hashmap_copy(dest, src);       // deep copy src into dest (dest freed first)
hashmap_destroy(m);

u64 n = hashmap_size(m);
u64 c = hashmap_capacity(m);
b8  e = hashmap_empty(m);
hashmap_print(m, key_print_fn, val_print_fn);
```

**Convenience macros** (from `wc_macros.h`):

```c
MAP_PUT(m, key, val);
MAP_PUT_MOVE(m, key_ptr, val_ptr);
MAP_PUT_VAL_MOVE(m, key, val_ptr);
MAP_PUT_STR_STR(m, "name", "Alice");    // creates Strings, inserts by move
int v = MAP_GET(m, int, key);

MAP_FOREACH_KEY(m, String, k) { string_print(k); }  // do not modify keys
MAP_FOREACH_VAL(m, int, v)    { printf("%d\n", *v); }
```

---

### HashSet

Same architecture as HashMap — Robin Hood hashing, power-of-two capacity, backward-shift deletion — but stores only elements with no associated value.

```c
hashset* s = hashset_create(
    sizeof(elm_type),
    hash_fn, cmp_fn,        // NULL → wyhash, memcmp
    &ops                    // const container_ops*, NULL for POD
);

// Returns 1 if already existed, 0 if newly inserted
b8 dup = hashset_insert(s, (u8*)&elm);
b8 dup = hashset_insert_move(s, (u8**)&elm);

b8 found   = hashset_has(s, (u8*)&elm);
b8 removed = hashset_remove(s, (u8*)&elm);

hashset_clear(s);           // remove all elements, keep capacity
hashset_copy(dest, src);    // deep copy
hashset_destroy(s);
hashset_print(s, print_fn);

u64 n = hashset_size(s);
u64 c = hashset_capacity(s);
b8  e = hashset_empty(s);
```

---

### BitVector

A compact dynamic bit array backed by a `genVec<u8>`. Bits are indexed from 0. The backing byte array grows automatically as you set higher-index bits.

```c
bitVec* bv = bitVec_create();

bitVec_set(bv, 42);           // set bit 42 to 1 (grows backing array if needed)
bitVec_clear(bv, 42);         // set bit 42 to 0 (bit must be within allocated range)
bitVec_toggle(bv, 42);        // flip bit 42
u8 b = bitVec_test(bv, 42);  // returns 0 or 1

bitVec_push(bv);              // append a 1-bit at the end
bitVec_pop(bv);               // remove the last bit

u64 nbits  = bitVec_size_bits(bv);    // logical number of bits
u64 nbytes = bitVec_size_bytes(bv);   // backing byte count

bitVec_print(bv, byte_index);  // print one byte as binary digits LSB-first
bitVec_destroy(bv);
```

`bitVec_set` grows the backing array automatically. `bitVec_clear`, `bitVec_test`, and `bitVec_toggle` require the index to be within the currently allocated range.

---

### Matrix (float)

Row-major 2D float matrix (`Matrixf`). All multi-pointer operations are annotated with `restrict` so the compiler can auto-vectorize inner loops. Compile with `-O3 -march=native` to get AVX2/SSE4 on supported hardware.

**Three allocation strategies:**

```c
// Heap
Matrixf* m = matrix_create(rows, cols);
Matrixf* m = matrix_create_arr(rows, cols, float_array);
matrix_destroy(m);   // heap only — do NOT call on stack or arena matrices

// Stack (you provide the data array)
float data[9];
Matrixf m;
matrix_create_stk(&m, 3, 3, data);

// Arena (freed with the arena, no matrix_destroy)
Matrixf* m = matrix_arena_alloc(arena, 3, 3);
Matrixf* m = matrix_arena_arr_alloc(arena, 3, 3, float_array);
```

**Setting values:**

```c
matrix_set_elm(m, value, row, col);
float v = matrix_get_elm(m, row, col);

// From a flat C array (row-major)
matrix_set_val_arr(m, 9, (float*)(float[3][3]){
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9}
});
```

**Element access macros:**

```c
MATRIX_AT(m, i, j)     // m->data[i * m->n + j] — direct lvalue
IDX(m, i, j)           // the flat index only
MATRIX_TOTAL(m)        // m->m * m->n
```

**Operations:**

```c
// out must NOT alias a or b
matrix_add(out, a, b);
matrix_sub(out, a, b);

// in-place
matrix_scale(m, scalar);
matrix_div(m, scalar);
matrix_copy(dest, src);

// Multiplication — out must NOT alias a or b
matrix_xply(out, a, b);    // blocked ikj — good for small to medium matrices
matrix_xply_2(out, a, b);  // transposes b first — better for large matrices

// out must NOT alias mat
matrix_T(out, mat);

// LU decomposition and determinant (Doolittle, no pivoting — see Known Issues)
matrix_LU_Decomp(L, U, mat);
float det = matrix_det(mat);

matrix_print(mat);
```

**Using the arena with matrices in hot loops:**

```c
Arena* arena = arena_create(nKB(16));
Matrixf* result = matrix_arena_alloc(arena, 4, 4);

ARENA_SCRATCH(sc, arena) {
    Matrixf* a = matrix_arena_arr_alloc(arena, 4, 4, data_a);
    Matrixf* b = matrix_arena_arr_alloc(arena, 4, 4, data_b);
    matrix_xply(result, a, b);
}   // a and b reclaimed; result survives

arena_release(arena);
```

---

### Matrix (generic)

`matrix_generic.h` provides macro-based matrix functions parameterized over any numeric type. Expand the macros once per type to generate a full set of typed functions with no runtime overhead.

```c
#include "matrix_generic.h"

// Expand all functions for a type — do this once in a .c file
INSTANTIATE_MATRIX(double, "%f")
INSTANTIATE_MATRIX(int,    "%d")
```

This generates a `Matrix_double` and `Matrix_int` struct plus the following typed functions for each:

```c
Matrix_T* matrix_create_T(u64 m, u64 n);
Matrix_T* matrix_create_arr_T(u64 m, u64 n, const T* arr);
void      matrix_create_stk_T(Matrix_T* mat, u64 m, u64 n, T* data);
void      matrix_destroy_T(Matrix_T* mat);

void matrix_set_val_arr_T(Matrix_T* mat, u64 count, const T* arr);
void matrix_set_val_arr2_T(Matrix_T* mat, u64 m, u64 n, const T** arr2);
void matrix_set_elm_T(Matrix_T* mat, T elm, u64 i, u64 j);

void matrix_add_T(Matrix_T* out, const Matrix_T* a, const Matrix_T* b);
void matrix_sub_T(Matrix_T* out, const Matrix_T* a, const Matrix_T* b);
void matrix_scale_T(Matrix_T* mat, T val);
void matrix_div_T(Matrix_T* mat, T val);
void matrix_copy_T(Matrix_T* dest, const Matrix_T* src);
void matrix_T_T(Matrix_T* out, const Matrix_T* mat);    // transpose
void matrix_xply_T(Matrix_T* out, const Matrix_T* a, const Matrix_T* b);
void matrix_xply_2_T(Matrix_T* out, const Matrix_T* a, const Matrix_T* b);
void matrix_LU_Decomp_T(Matrix_T* L, Matrix_T* U, const Matrix_T* mat);
T    matrix_det_T(const Matrix_T* mat);
void matrix_print_T(const Matrix_T* mat);

Matrix_T* matrix_arena_alloc_T(Arena* arena, u64 m, u64 n);
```

Usage is identical to the float variants, with `_T` suffixes:

```c
Matrix_double* m = matrix_create_double(3, 3);
matrix_set_elm_double(m, 1.5, 0, 0);
matrix_destroy_double(m);
```

---

### Random (PCG32)

Permuted Congruential Generator. Fast, high-quality, reproducible. Uses a single global RNG instance; internal `_r` variants for per-instance use are defined statically in the source.

**Seeding:**

```c
pcg32_rand_seed(seed, sequence);   // same inputs always produce same sequence
pcg32_rand_seed_time();            // seed from time(NULL) — second precision
```

**Integer output:**

```c
u32 r    = pcg32_rand();                  // full 32-bit random
u32 dice = pcg32_rand_bounded(6);         // uniform in [0, 6), no modulo bias
```

**Floating point:**

```c
float  f = pcg32_rand_float();              // uniform [0.0, 1.0)
double d = pcg32_rand_double();             // uniform [0.0, 1.0), 53-bit precision

float  fv = pcg32_rand_float_range(-1.0f, 1.0f);
double dv = pcg32_rand_double_range(0.0,  100.0);
```

**Gaussian:**

```c
float g  = pcg32_rand_gaussian();                      // standard normal N(0, 1)
float gv = pcg32_rand_gaussian_custom(mean, stddev);   // N(mean, stddev²)
```

Uses the Box-Muller transform. Generates two values per transform and caches the second — see Known Issues regarding the static spare state.

---

### Fast Math

Approximations of common math functions using numerical methods. Faster than `<math.h>` with reduced precision. Originally built to support the Gaussian RNG.

```c
#include "fast_math.h"

float s = fast_sqrt(x);    // Newton-Raphson, 4 iterations, bit-hack initial guess
float l = fast_log(x);     // range reduction + 5-term Taylor series
float e = fast_exp(x);     // range reduction + repeated squaring (integer part)
float s = fast_sin(x);     // 4-term Taylor, input reduced to [-π, π]
float c = fast_cos(x);     // = fast_sin(x + π/2)
float v = fast_ceil(x);
```

Constants exported: `PI`, `TWO_PI`, `LN2`.

---

## Helpers and Callbacks

`wc_helpers.h` provides ready-to-use copy/move/delete/print/compare callbacks for `String` and `genVec` in both storage strategies, plus four pre-built `container_ops` instances. Include it alongside `wc_macros.h`.

### Pre-built ops instances

```c
&wc_str_ops       // String by value  — { str_copy,     str_move,     str_del     }
&wc_str_ptr_ops   // String* by ptr   — { str_copy_ptr, str_move_ptr, str_del_ptr }
&wc_vec_ops       // genVec by value  — { vec_copy,     vec_move,     vec_del     }
&wc_vec_ptr_ops   // genVec* by ptr   — { vec_copy_ptr, vec_move_ptr, vec_del_ptr }
```

### String callbacks (by value)

```c
str_copy     // copy_fn  — deep copies, handles SSO correctly
str_move     // move_fn  — transfers fields, frees heap container, nulls src
str_del      // del_fn   — frees heap data buffer only (not the slot itself)
str_print    // print_fn
str_cmp      // compare_fn — wraps string_compare
```

### String callbacks (by pointer)

```c
str_copy_ptr    // copy_fn  — allocs new String, deep copies
str_move_ptr    // move_fn  — transfers pointer, nulls src
str_del_ptr     // del_fn   — calls string_destroy (frees struct + data)
str_print_ptr   // print_fn
str_cmp_ptr     // compare_fn
```

### genVec callbacks (by value and by pointer)

```c
vec_copy / vec_copy_ptr
vec_move / vec_move_ptr
vec_del  / vec_del_ptr
vec_print_int / vec_print_int_ptr    // for vectors of int
```

### Hash functions (from `map_setup.h`)

```c
wyhash              // default — fast 64-bit, excellent avalanche, beats FNV1a on all key sizes
fnv1a_hash          // classic 64-bit FNV-1a, works for any key type
wyhash_str          // wyhash variant for String stored by value
wyhash_str_ptr      // wyhash variant for String* stored by pointer
```

### Print functions (from `common.h`)

```c
wc_print_int    wc_print_u32    wc_print_u64
wc_print_float  wc_print_char   wc_print_cstr
```

### Utility (from `common.h`)

```c
cast(x)         // (u8*)(&(x))  — take address and cast to u8*
castptr(x)      // (u8*)(x)     — cast pointer to u8*
nKB(n)          // (u64)(n * 1024)
nMB(n)          // (u64)(n * 1024 * 1024)
print_hex(ptr, size, bytes_per_line)   // dump raw bytes as hex
```

---

## Building

The project uses CMake with Clang. C extensions must be enabled (`CMAKE_C_EXTENSIONS ON`) for GNU statement expressions used in `wc_macros.h`.

```bash
# Debug build: ASan + UBSan, no optimisation
cmake -B build -G Ninja
cmake --build build
./build/main

# Release build: full optimisation + auto-vectorization
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Release flags:**

```cmake
set(CMAKE_C_FLAGS_RELEASE
    "-O3 -DNDEBUG -march=native -fassociative-math -fno-signed-zeros -fno-trapping-math")
```

`-march=native` unlocks AVX2/SSE4 on the build machine. The `-fassociative-math` group allows the compiler to reorder floating-point additions in the matrix multiply inner loop, enabling SIMD reductions. Without them the vectorizer emits scalar fallback code for reductions.

All source files in `src/` are compiled into both the `main` and `tests` targets. There is no separate library target.

---

## Testing

The test suite lives in `tests/`. Each component has its own test file. `test_main.c` runs them all.

```bash
cmake -B build -G Ninja
cmake --build build
./build/tests

# Via CTest
ctest --test-dir build --output-on-failure
```

The framework (`wc_test.h`) is minimal and self-contained. Assertions are non-aborting — all asserts in a test function run even after a failure, so you see every problem in one pass.

---

## Type and Diagnostic Reference

**Primitive types** (from `common.h`):

```c
u8   u16   u32   u64    // unsigned integers
b8                       // boolean (typedef u8)
```

**Callback typedefs:**

```c
typedef void (*copy_fn)(u8* dest, const u8* src);
typedef void (*move_fn)(u8* dest, u8** src);
typedef void (*delete_fn)(u8* key);
typedef void (*print_fn)(const u8* elm);
typedef int  (*compare_fn)(const u8* a, const u8* b, u64 size);
typedef u64  (*custom_hash_fn)(const u8* key, u64 size);
```

**Diagnostic macros** (from `common.h`):

```c
WARN(fmt, ...)               // print warning with file/line/func, continue
FATAL(fmt, ...)              // print error to stderr, exit(EXIT_FAILURE)
CHECK_WARN(cond, fmt, ...)   // WARN if cond is true
CHECK_WARN_RET(cond, ret, fmt, ...)  // WARN and return ret if cond is true
CHECK_FATAL(cond, fmt, ...)  // FATAL if cond is true
LOG(fmt, ...)                // print info message with func name
```

**`wc_errno` codes:**

```c
WC_OK             // 0 — no error
WC_ERR_FULL       // arena exhausted
WC_ERR_EMPTY      // pop or peek on empty container
WC_ERR_INVALID_OP // precondition not met (reserved)
```

## License

MIT — see `LICENSE`.
