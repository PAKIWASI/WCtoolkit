# WCtoolkit

A C11 toolkit built around explicit ownership, value semantics, and zero hidden costs.

WCtoolkit is designed for C programmers who care about lifetime correctness more than convenience.
Containers do not guess how to copy or destroy your data — you must define it.
This makes ownership explicit, prevents accidental deep copies, and scales well as programs grow.

No dependencies beyond the C standard library.
Targets C11 with GNU extensions (`typeof`, statement expressions, `__attribute__((cleanup))`) on Clang/GCC.

```c
// vector of complex 'String' SSO type
GenVec* vec = VEC_CX(String, 10, &wc_str_ops); // ops has copy/move/del fns for String

VEC_PUSH_CSTR(vec, "PAKI");     // create Strings and move into vec
VEC_PUSH_CSTR(vec, "WASI");     // zero copies, only one move

// append in place
VEC_FOREACH(vec, String, s) { String_append_char(s, '!'); }

String* s = String_create();    // create new String as buffer
GenVec_pop(vec, castptr(s));    // get element out of vec, s owns it now

String_print(s);                // do whatever
GenVec_print(vec, str_print);

String_destroy(s);              // free resources
GenVec_destroy(vec);
```

---

## Table of Contents

- [Design Philosophy](#design-philosophy)
- [Ownership Model](#ownership-model)
- [Error Handling](#error-handling)
- [Macro Layer](#macro-layer)
- [Components](#components)
- [Helpers and Callbacks](#helpers-and-callbacks)
- [Building](#building)
- [Testing](#testing)
- [Known Issues](#known-issues)

---

## Design Philosophy

WCtoolkit makes one central trade-off: **you tell the library how your data behaves, and it does the rest**. This means more setup code upfront, but total control over memory, copying, and lifetimes — with no hidden allocations and no garbage collection.

### Value Semantics Over Pointer Chasing

Elements are stored **inline by value** inside containers. A `GenVec` of `String` stores the actual `String` structs contiguous in memory, not pointers to heap-scattered objects. This keeps data cache-friendly and makes the ownership structure obvious — there is no shared ownership, no reference counting, no implicit heap allocation behind your back.

```
┌─────────────────────────────────────────────────────┐
│  GenVec (by value)                                  │
│  ┌──────────┬──────────┬──────────┬──────────┐      │
│  │ String   │ String   │ String   │ String   │ ...  │
│  │ {stk/…}  │ {stk/…}  │ {stk/…}  │ {stk/…}  │      │
│  └──────────┴──────────┴──────────┴──────────┘      │
└─────────────────────────────────────────────────────┘
```

By-pointer storage is supported when you need stable addresses or very large elements, but by-value is the default and the preferred path.

### Callbacks Over Macros

Genericity is achieved through **function pointer callbacks** grouped into a vtable struct, not `void*`-cast macros. Every container accepts an optional `container_ops*` that bundles the `copy_fn`, `move_fn`, and `del_fn` callbacks describing how to deep-copy, transfer ownership of, and free an element. For plain-old-data types — `int`, `float`, any struct without heap pointers — pass `NULL` and the container uses `memcpy` directly.

```c
// Define once, reuse everywhere
static const container_ops String_ops = { str_copy, str_move, str_del };

GenVec* vec = GenVec_create(8, sizeof(String), &String_ops);
```

`wc_helpers.h` ships pre-built ops instances (`wc_str_ops`, `wc_str_ptr_ops`) for `String` and `GenVec` in both storage strategies, so you rarely need to define your own.

This has a real cost: you write more setup code. The benefit is that the behavior is explicit, readable, debuggable, and correct across container operations like `push`, `insert`, `replace`, `pop`, `remove`, `copy`, and `move`.

### No Magic, No Hidden Cost

Every allocation is explicit. `Arena_alloc` is a pointer bump. `GenVec_push` sometimes calls `realloc` — you can see it in the source. There is no background thread, no lazy initialization, no defer Queue. What you write is what runs.

Growth thresholds for `GenVec` and `String` are compile-time configurable macros set before the include. The hash table load factor is a constant in `map_setup.h`. Nothing is hidden in a global config struct.

### Stack Wherever Possible

Most types offer both heap-allocated and Stack-allocated variants:

```c
// Heap: library manages memory
Arena*  Arena = Arena_create(nKB(4));
GenVec* vec   = GenVec_create(8, sizeof(int), NULL);
String* str   = String_from_cstr("hello");

// Stack: you provide the struct, often the data too
Arena Arena;
ARENA_CREATE_STK_ARR(&Arena, 4);        // 4KB on the Stack

GenVec vec;
GenVec_create_stk(8, sizeof(int), NULL, &vec);   // receiver-last

String str;
String_create_stk("hello", &str);

// cleanup destroys only the data buffer — NOT the struct itself
GenVec_destroy_stk(&vec);
String_destroy_stk(&str);
```

The `_stk` suffix always means: *you own the struct, the library only manages its contents*. `destroy` frees the container struct plus its data; `destroy_stk` frees only the data and leaves the struct in a valid, reusable empty state. All `_create_stk` functions take the receiver as the **last** parameter.

### API Conventions

- **Receiver first** for operations (`GenVec_push(vec, x)`), **dest first** for copies (`GenVec_copy(dest, src)`); `_create_stk` takes the receiver **last**.
- **Moves are `T**`**: a move transfers ownership and nulls the source — `String_move(dest, &src)`, `GenVec_push_move(vec, &ptr)`. After a move, `src == NULL`.
- **`_move` variants exist wherever ownership transfers**: `push_move`, `insert_move`, `replace_move`, `put_move`, `append_String_move`. The plain variants copy.
- **Optional out-params are last and nullable**: `GenVec_remove(vec, i, NULL)` discards the element; pass a buffer to receive it.
- **Not-found sentinels** use `WC_NOT_FOUND` (`u64`) from `common.h` — never `-1` on unsigned.
- **`copy` functions are SAFE ON raw destinations**: `GenVec_copy`, `String_copy`, `HashMap_copy` never read `dest` before overwriting it, so you can copy into an uninitialized struct.
- **Allocators warn on ignored results**: every heap constructor (`GenVec_create`, `String_from_cstr`, `HashMap_create`, `Arena_alloc`, …) is `__attribute__((warn_unused_result))` — a leaked allocation is a compile warning. Arena/chain-Arena allocators also carry `alloc_size(size_arg)` for `-Walloc-size` checking.

---

## Ownership Model

The toolkit follows a **strict creation–transfer–destruction** lifetime:

```
 create ──────────────────────────────────────────────────── destroy
    │                                                            ▲
    ▼                                                            │
 container owns data ──► transfer (move) ──► new owner ──────────┘
                              │
                              └──► source becomes empty/NULL
```

**Creation** — heap constructors return a fully initialized object, never a partially built one:

```c
GenVec* vec   = GenVec_create(8, sizeof(String), &String_ops);
String* str   = String_from_cstr("WASI");
HashMap* map  = HashMap_create(sizeof(u32), sizeof(String), &str_ops, NULL);
```

**Transfer** — moves are the only way ownership crosses a boundary. A move is always `void f(T* dest, T* src)` for whole objects and `void f(T** slot, T* val)` when storing into a container slot. The source is left empty and nulled:

```c
String dest;
String_move(&dest, &str);        // str is now empty, NULL

GenVec* slot = ...;
GenVec_push_move(vec, &ptr);     // vec takes ownership, ptr is NULL
```

**Destruction** — `destroy` frees the container plus its data; `destroy_stk` frees only the data:

```c
GenVec_destroy(vec);             // frees vec and all owned elements
GenVec_destroy_stk(&vec_stk);    // frees data, struct reusable

VEC_FOREACH(vec, String, s) {    // containers never free elements
    String_destroy(s);           // that your ops del_fn doesn't cover
}
```

**Borrowing** — passing a container to a function never transfers ownership. Functions that *read* take `const T*`; functions that *mutate in place* take `T*` but leave the lifetime with the caller. If a function needs to *keep* an element, it must move or copy it explicitly.

```c
void print_vec(const GenVec* vec);          // borrows
void mutate_String(String* s);              // borrows, mutates in place
void keep_String(GenVec* vec, u64 i) {      // wants ownership
    String tmp;
    GenVec_remove(vec, i, castptr(&tmp));   // move out
    ...
    String_destroy(&tmp);
}
```

**The rules in short**

- One owner at a time. If two containers reference the same data, one of them copied it — and both must be destroyed.
- Moves null the source. Never use a container after moving out of it.
- `destroy` is idempotent-safe only for `_stk` variants; for heap objects, `destroy` after `destroy` is undefined.
- Container elements are your responsibility unless the ops `del_fn` covers them (`push_del`-style helpers exist for the common cases).

---

## Error Handling

The toolkit uses **return codes + sentinels**, not exceptions and not `setjmp`:

```c
// Allocators abort on OOM (malloc failure) — documented per function
GenVec* vec = GenVec_create(8, sizeof(int), NULL);   // aborts if malloc fails

// Operations return status codes
bool ok = GenVec_push(vec, castptr(&x));

// Lookups return sentinels
u64 idx = GenVec_find(vec, castptr(&x));
if (idx == WC_NOT_FOUND) { ... }

// Range-checked accessors return optional out-params
String out;
if (GenVec_remove(vec, i, castptr(&out))) { ... }
```

**Conventions**

- Constructors never return `NULL` — they abort on allocation failure. This keeps every call site NULL-check-free.
- Mutating operations (`push`, `insert`, `remove`, `replace`) return `bool` — `true` on success.
- Lookups (`find`, `get`, `search`) return `u64` index or `WC_NOT_FOUND`.
- Nothing allocates inside an error path. If an operation fails midway (e.g., `insert` during `realloc`), the container is left unchanged.
- Arena and chain-Arena allocators abort on OOM — they are designed for program-lifetime allocations where recovery is meaningless.

---

## Macro Layer

A type-safe **convenience layer** lives on top of the C API. Macros never replace the C functions — they wrap them, adding compile-time type checking and receiver inference. Every macro expands to the plain C call, so you can always drop down to functions with zero behavior change.

```c
// Macro layer — type checked, concise
VEC_PUSH(vec, x);                 // infers sizeof(*vec) and element type
VEC_PUSH_MOVE(vec, &ptr);         // ownership transfer, ptr is NULL after
VEC_FOREACH(vec, String, s) { String_append_char(s, '!'); }

// C API — always available underneath
GenVec_push(vec, castptr(&x));
GenVec_push_move(vec, castptr(&ptr));
```

**Why a macro layer at all?** The C API is `void*`-based for genericity. The macro layer recovers type safety at the call site: `VEC_PUSH(vec, x)` checks that `typeof(x)` matches the vector's element type, so a mismatch is a compile error instead of silent memory corruption.

**Conventions**

- Macros never own resources. Nothing in the macro layer allocates or frees — that stays with the C API.
- `*_MOVE` variants transfer ownership (nulling the source); plain variants copy.
- `_FOREACH` macros borrow the container and expose the element by reference — never keep it past the loop.
- Where a macro needs a size, it infers it from the receiver (`sizeof(*vec)`); where it can't, it takes it as an explicit last parameter.
- `cast`/`castptr` helpers silence `-Wcast-qual` and `-Wvoid-pointer-to-int-cast` where the design is intentionally generic.

---

## Components

### Arena Allocator

A bump allocator for program-lifetime or frame-based allocations. Individual allocations are never freed — the Arena is reset or destroyed wholesale:

```c
// Heap Arena
Arena* Arena = Arena_create(nKB(4));

int*    xs = Arena_alloc(Arena, 100 * sizeof(int));   // pointer bump, O(1)
String* s  = Arena_alloc(Arena, sizeof(String));

Arena_reset(Arena);          // reuses memory, all previous data invalid

Arena_destroy(Arena);        // frees everything at once

// Stack Arena — 4KB on the Stack, nothing to destroy
Arena Arena;
ARENA_CREATE_STK_ARR(&Arena, 4);
```

**When to use an Arena**: parse trees, ASTs, render frames, per-request state — anything where objects die together. Never for objects with independent lifetimes.

### Chain Arena

A linked list of Arena blocks for **unbounded** allocation. Blocks are added as needed, so total size never has to be known upfront:

```c
ChainArena* ca = chain_Arena_create();         // block size via ARENA_NODE_INLINE_SIZE

void* p = chain_Arena_alloc(ca, size);         // bump within current block,
                                               // new block when full

chain_Arena_destroy(ca);       // frees all blocks
```

**Arena vs Chain Arena**: use a plain `Arena` when total size is bounded and known; use a `ChainArena` for program-lifetime allocations of unknown total size (symbol tables, caches, registries).
