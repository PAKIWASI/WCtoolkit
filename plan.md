# WCtoolkit: Allocator & Construction Refactor Plan


## 1. Goals

1. **One ownership story.** Every data structure receives a `wc_allocator` at construction, stores it, and uses it for every allocation, reallocation and free.
2. **No global allocator.** Remove the thread-local `wc_default_allocator` and `WC_SET_ALLOCATOR`.
3. **Smaller API surface.** Remove every constructor that returns a heap-allocated struct pointer. Every structure is initialised in place (or returned by value), and the `_stk` form becomes the only form.
4. **Explicit by default, ergonomic by macro.** Functions always take the allocator (first parameter). Macros supply libc for the common case.
5. **Nesting just works.** A container of containers (`GenVec<String>`, `GenVec<GenVec>`, `HashMap<String, GenVec>`) is consistent because every element knows its own allocator.
6. **No UX regression for libc users.** Nothing changes for someone who never thinks about allocators, at the macro level.

### Non-goals

- Thread-safe allocators, allocator hooks/OOM callbacks, or returning error codes instead of fatal on OOM (see D5).
- Single-allocation layouts for `HashMap`/`HashSet` (possible later optimisation).
- Changing container algorithms (Robin Hood hashing, circular queue, heap logic).
- Changing `fast_math`, `random` (they perform no allocation; confirmed by grep).
- Virtual-memory arena backing (`wc_vm`) is **not** required for the refactor. It is scheduled as optional Phase 8, after the Definition of Done is met.
- When to use explicit u8* and when to use void*

---

## 2. Current state (audit)

Only `GenVec` calls `wc_alloc`/`wc_realloc`/`wc_free` (about 10 sites, all on the thread-local global). Everything else uses raw libc. `ChainArena` stores its node list in a `GenVec`, so it indirectly depends on the global too. `main.c` sets the global to `Arena_allocator(a)` via `WC_SET_ALLOCATOR`.

Build and tests: `CMakeLists.txt` builds `main` and a `tests` binary from 13 files under `tests/` (in the repo, not in the shared zip). The Debug build already runs ASAN + UBSan; `WC_WARNINGS_AS_ERRORS` exists as an option.

| Structure | Allocation sites today | Size now |
|---|---|---|
| `GenVec` (`Stack` is `typedef GenVec`) | wired to global allocator; shell + data + grow/reserve/shrink/copy | 40 (asserted) |
| `Queue` | raw `malloc`/`free` shell; embeds `GenVec`; resize builds a temp `GenVec` | 64 |
| `PriorityQueue` | raw shell; embeds `Queue` | 72 |
| `BitVec` | raw shell + heap `GenVec*` (heap-only, no `_stk`) | 16 (+40 separate shell) |
| `HashMap` | shell; `keys` (malloc), `psls` (calloc), `vals` (malloc), `scratch` (malloc); again in resize and copy; `free(*key)` on moved-in duplicates | 88 |
| `HashSet` | shell; `elms` (calloc), `psls` (calloc), `scratch` (malloc); resize, copy; `free(*elm)` on duplicates | 72 (measured) |
| `String` | shells (`malloc`), heap buffer (`malloc`/`realloc`/`free`), `String_to_cstr` returns malloc'd buffer | 40 (asserted) |
| `wc_helpers.h` ops | `str_copy` mallocs; `str_move`/`vec_move` `free` the source shell; `vec_copy_ptr` mallocs a shell | n/a |
| `Matrixf` + `matrix_generic.h` macros | shell + data (`malloc`), `free` both; `matrix_create_stk` wraps a user array | 24 |
| `StringStore` / `StrView` | 1 KB nodes via `malloc`; overflow `heap` buffers; `StrView_from_cstr(..., Arena*)` | 24 |
| `Arena` | `base = malloc(cap)`; heap and `_stk` and `_arr_stk` forms | 24 |
| `ChainArena` | node `malloc`s, node list is a `GenVec` of `ArenaNode*`, `chain_del` calls raw `free` | 48 |

### Known defects to fix along the way

| # | Location | Defect |
|---|---|---|
| A1 | `Arena_alloc`, `Arena_alloc_aligned` | Capacity check `size - aligned_idx < req` underflows when alignment pushes `aligned_idx` past `size` (reproduced: 13-byte arena returned a pointer past the end). |
| A2 | `Arena_alloc_aligned`, `chain_Arena_alloc_aligned` | Alignment is applied to the *offset*, not the *address*. Reproduced: a 16-aligned request returned `addr % 16 == 4` for a base at `+4`. In `ChainArena` the block starts 8 bytes into a `malloc`'d node (after `u64 used`), so 16-aligned requests are misaligned by construction. |
| A3 | `ChainArena` | Allocations larger than one node (4088 bytes) are fatal, so a `GenVec` growing past 4 KB on a `ChainArena` aborts. |
| A4 | `Arena_allocator` | `realloc = NULL` means every `GenVec` growth abandons the old block. The audited dump showed ~50% of a 1 KB arena consumed by dead blocks. |
| A5 | `wc_allocator.h` | `WC_REALLOC_N(T, p, n)` passes 3 arguments to a 4-parameter function (does not compile if used). |
| A6 | `common.h` | `CHECK_FATAL` compiles to nothing under `NDEBUG`, so allocation-failure checks disappear in release builds. Harmless with libc in practice, dangerous once arenas can legitimately return NULL. |
| A7 | `GenVec_copy` | `memcpy(dest, src, sizeof(GenVec))` copies every field. Once the allocator is stored, `dest` would silently inherit `src`'s allocator. |
| A8 | `String_shrink_to_fit` | On an empty heap-mode string, the `size == 0` branch freed the buffer but left the SSO flag at `'\0'` (heap mode) with `heap == NULL`, `capacity == 23`. The next append wrote to NULL (reproduced under ASAN). **Hotfixed on current code** before Phase 0: the branch is removed and `size == 0` goes through `heap_to_stk`, which restores SSO mode. |
| A9 | `String` growth from zero | `new_cap = capacity * 1.5` is 0 when `capacity == 0`, so a zero-filled `String` (the planned moved-from state) would write out of bounds on append. Resolved by D4: mutation of a zero-state container is an unconditional fatal. |
| A10 | `MAP_GET` (`wc_macros.h`) | The `HashMap_get` call lived inside `CHECK_FATAL(...)`, which compiles to nothing under `NDEBUG`: in Release the lookup never ran and `MAP_GET` returned an uninitialised value (`test_map_get_and_try_get` failed in Release only). **Fixed in Phase 0**: lookup hoisted out of the check, output zero-initialised. A sweep of every `CHECK_FATAL` in `src/` and `include/` found no other side-effecting condition. Rule from now on: `CHECK_FATAL` conditions must be side-effect free. |

---

## 3. Target architecture

### 3.1 The allocator type

```c
typedef struct {
    void* (*alloc)  (void* ctx, size_t size, size_t align);
    void* (*realloc)(void* ctx, void* p, size_t old_size, size_t new_size, size_t align); // may be NULL
    void  (*free)   (void* ctx, void* p, size_t size, size_t align);                       // may be NULL
} wc_alloc_vtable;

typedef struct {
    const wc_alloc_vtable* vt;    // NULL => libc (zero-initialised allocator is libc)
    void*                  ctx;   // Arena*, ChainArena*, test allocator, ...
} wc_allocator;                   // 16 bytes, passed and stored BY VALUE

#define WC_LIBC ((wc_allocator){0})           // zero-initialised == libc
extern const wc_allocator wc_borrowed;        // non-owning: alloc -> NULL, free -> no-op
```

**Contract (wrappers enforce, backends may rely on it):**

- Callbacks are never called with `p == NULL` or `size == 0`.
- `align` is a power of two, >= 1. Backends must honour it as an **address** alignment.
- `free` receives the exact `size`/`align` of the block's last `alloc`/`realloc`. (This is what the test allocator verifies; arenas ignore it, so mismatches are invisible until an arena is swapped for libc.)
- `realloc` failure returns NULL and leaves `p` valid.
- Missing `realloc` is emulated (alloc + copy + free). Missing `free` is a no-op.
- `wc_alloc(a, 0, …)` returns NULL without calling the backend. `wc_realloc(a, NULL, 0, n, …)` is `wc_alloc`. `wc_realloc(a, p, old, 0, …)` frees and returns NULL.

**Function API (allocator is always the first parameter):**

```c
void* wc_alloc  (wc_allocator a, size_t n, size_t align);
void* wc_realloc(wc_allocator a, void* p, size_t old_n, size_t n, size_t align);
void  wc_free   (wc_allocator a, void* p, size_t n, size_t align);

#define WC_NEW(a, T)          ((T*)wc_alloc((a), sizeof(T), alignof(T)))
#define WC_NEW_N(a, T, n)     ((T*)wc_alloc((a), sizeof(T) * (n), alignof(T)))
#define WC_DELETE(a, T, p)    wc_free((a), (p), sizeof(T), alignof(T))
#define WC_DELETE_N(a, T, p, n) wc_free((a), (p), sizeof(T) * (n), alignof(T))
```

Everything from the old design that implied a global is removed: `wc_default_allocator`, `WC_SET_ALLOCATOR`, `WC_RESOLVE`, the global-based "simple API" (`wc_alloc(n)` etc.), and the old typed macros. The old `WC_NEW_IN`-style macros survive without the `_IN` suffix and with the allocator first (`WC_NEW(a, T)`). `WC_REALLOC_N` is rewritten with the correct arity (A5).

**Transitional names (D11).** The final names (`wc_alloc`, `wc_realloc`, `wc_free`, `WC_NEW`, ...) collide with the old global API, which `GenVec` and `ChainArena` still use until Phase 2. The two cannot coexist, so:

- Phase 1 adds the new API as `wc_allocator` (new type name; the old type is `wc_allocator_t`, so no clash) with transitional functions/macros `wc2_alloc`, `wc2_realloc`, `wc2_free`, `WC2_NEW`, `WC2_NEW_N`, `WC2_DELETE`, `WC2_DELETE_N`, `WC2_REALLOC_N`.
- The old global API, `wc_allocator_t` and `wc_libc_allocator` stay untouched until Phase 2 exit.
- At Phase 2 exit, the old API is deleted and `wc2_`/`WC2_` is renamed to `wc_`/`WC_` in one mechanical commit (`sed` + build). Old call sites then fail on arity, which is the intended migration signal.

**Alignment policy (D7):** containers pass the smallest *safe* alignment instead of always `WC_MAX_ALIGN`. Safe lower bound for an element of `data_size` bytes is the largest power of two dividing `data_size`, capped at `WC_MAX_ALIGN` (valid because `sizeof(T)` is always a multiple of `alignof(T)`). This removes most of the 4- and 12-byte padding gaps seen in the arena dump. The libc backend still uses `malloc` when `align <= WC_MAX_ALIGN`.

### 3.2 Backends

**`Arena` (single block, fixed capacity)**

```c
typedef struct {
    wc_allocator backing;     // where `base` came from (WC_LIBC by default)
    u8*          base;
    u64          size, idx;
    u64          floor;       // in-place realloc/free never touch blocks below this (D10)
    const Arena* self;        // == this arena while live, NULL after destroy (debug check, see below)
    b8           owns_base;   // 0 for Arena_create_buf
} Arena;

void        Arena_create    (Arena* a, wc_allocator backing, u64 capacity);
void        Arena_create_buf(Arena* a, u8* buf, u64 size);         // caller-owned memory, e.g. stack array
void        Arena_destroy  (Arena* a);                            // frees base via backing iff owns_base
void        Arena_reset   (Arena* a);
wc_allocator Arena_allocator(Arena* a);                          // {&arena_vt, a}
```

Vtable semantics:

- `alloc`: compute `aligned = ALIGN_UP((uintptr_t)base + idx, align) - (uintptr_t)base` (fixes A2); reject if `aligned > size || size - aligned < req` (fixes A1); return `base + aligned`.
- `realloc`: if `p + old_size == base + idx` (last block) **and** `p >= base + floor`, grow or shrink in place when capacity allows; otherwise alloc + copy (fixes A4).
- `free`: if last block **and** `p >= base + floor`, rewind `idx = p - base`; otherwise no-op.

**Scratch and the floor (D10).** The existing scratch API (`Arena_scratch_begin/end`, `ARENA_SCRATCH`) is kept. Without a floor, in-place realloc corrupts memory: a `GenVec` that is the last block grows in place inside a scratch scope, past the mark; `scratch_end` rewinds `idx` to the mark and the next allocation overwrites the vector's tail. Rules:

- `ArenaScratch` stores `{arena, mark, prev_floor}`.
- `scratch_begin`: `mark = idx`, `prev_floor = floor`, `floor = idx`.
- `scratch_end`: `idx = mark`, `floor = prev_floor`.
- **Correction (found while implementing Phase 1):** the floor does not make it safe to *grow* a block that predates the scope. Grown in place, `scratch_end` truncates it; copied, the new block is scratch memory and dangles at scope end. Either way it is a lifetime bug in the caller. What the floor guarantees is the arena's own invariant: nothing below the mark moves, grows or rewinds in place during the scope, so `scratch_end` never truncates a live pre-scope block. Growing a pre-scope block inside a scope is therefore a debug `CHECK_FATAL` ("it would dangle at scope end"); shrinking or freeing one is harmless (kept in place / no-op). Rule for users: containers that must outlive a scope are never grown inside it.
- Nesting works because each level restores its own `prev_floor`. Scopes must end in LIFO order (already true with `ARENA_SCRATCH`).
- `Arena_reset` sets `idx = 0` and `floor = 0`.

**Pinned check.** Instead of a magic number, `Arena` and `ChainArena` store `self`. Every entry point checks `self == this` in debug builds, which catches use after destroy **and** use of a copied/moved arena (a copy's `self` still points at the original). `Arena` is exactly 64 bytes.

`Arena_create_buf` replaces `Arena_create_arr_stk` and doubles as the fixed-buffer allocator (a `GenVec` or `HashMap` living entirely on a stack array). The `Arena` struct is **pinned**: its address is the allocator's `ctx`, so it must not be moved or copied while containers use it.

**`ChainArena` (growing)**

- Takes a `backing` allocator; nodes are allocated/freed through it (no raw `malloc`/`free`).
- Node list becomes an intrusive singly-linked list instead of a `GenVec<ArenaNode*>`. This removes the dependency of the allocator layer on a container (which would otherwise need an allocator to create itself).
- Alignment by address (A2): node header layout adjusted, or alignment computed from the real address.
- Requests larger than a node get a dedicated node (A3), sized to the request.
- In-place `realloc` and last-block `free` as for `Arena`, guarded by a floor (D10): the floor is `(floor_node, floor_used)`. An in-place op is allowed only if the block lives in the current tail node and, when that node is `floor_node`, starts at or after `floor_used`. Blocks in earlier nodes are always below the floor.
- Scratch API (`CHAIN_ARENA_SCRATCH`) preserved: save `(node, node_used, total_used, prev_floor)`, set the floor to the current position, and on end restore position and floor and free nodes appended after the mark through the backing allocator.

**`wc_borrowed`** (non-owning allocator): `alloc` returns NULL, `free` is a no-op. A container built with it wraps existing memory, never grows (growth aborts, matching today's "crashes when full"), and `destroy` frees nothing. This replaces `GenVec_create_arr`, `GenVec_create_stk_arr` and the borrowed-buffer form of `matrix_create_stk`.

**`wc_test_allocator`** (in `tests/`, not the library): wraps another allocator and records every live block with its size and align. Features: allocation/free counters, live and peak bytes, fail-on-Nth-allocation, fill on alloc (0xBE) and poison on free (0xDD), immediate failure on double free, foreign pointer, or `size`/`align` mismatch at free, and a leak report at `destroy`. This is the main safety net for the whole refactor.

**`wc_vm`** (virtual-memory backend): design is in Phase 8. Not part of the core refactor.


### 3.3 Cross-cutting API conventions

**Construction and destruction**

- Every container is a value, and its `create` function **returns the struct by value** (D1): `GenVec v = GenVec_create(A, 8, sizeof(int), NULL);`. Safe because none of the containers are self-referential (String's SSO buffer is inline, not pointed into; `HashMap`/`HashSet` `scratch` is a separate allocation, not a pointer into the struct; verified).
- **Pinned exceptions:** `Arena` and `ChainArena` hand out their own address as the allocator's `ctx`, so they are initialised in place through an out-parameter: `Arena_create(Arena* a, wc_allocator backing, u64 cap)`. The call shape itself signals "do not move this".
- Names are unchanged (D3): `X_create` / `X_destroy`. Only the signatures change (allocator first, struct returned by value, no heap variants). `_stk`, `_val`, `_arr` variants are removed.
- `destroy` never frees the struct itself. It reads the allocator into a local before freeing anything and leaves the struct zeroed. **Every `destroy` must be safe on a zeroed struct** (needed for moved-from objects, D4).
- **Zero state is dead, not empty (D4).** A zeroed container may only be destroyed or re-created. Every mutating operation checks the zero state and calls unconditional `FATAL` (not `CHECK_FATAL`), in all builds. Check per type: `GenVec`/`Stack`/`Queue`/`PriorityQueue`/`BitVec`: `data_size == 0`; `String`: `capacity == 0`; `HashMap`/`HashSet`: `capacity == 0`; `Matrixf`: `data == NULL && m * n != 0` is impossible, so check `m == 0`. This also stops a moved-from container from silently allocating through `WC_LIBC` (its zeroed allocator) instead of its original arena. Read-only ops (`size`, `empty`, iteration) are allowed and see an empty container.
- The allocator is stored by value inside each structure (D2). Size impact in section 6.
- **Migration safety.** Because names are kept, rely on signature changes to surface stale call sites: a stale `GenVec* v = GenVec_create(n, sz, ops)` fails on arity and type, and every `_stk` name disappears. Build with `-Werror=incompatible-pointer-types -Werror=implicit-function-declaration -Werror=int-conversion` for the whole migration so that leftover `u8**` move calls (now `u8*`) and mismatched callback pointers are errors, not warnings.

**Ownership: move, copy, delete**

```c
typedef void (*wc_copy_fn)  (wc_allocator dst, u8* dest, const u8* src); // deep copy INTO allocator `dst`
typedef void (*wc_move_fn)  (u8* dest, u8* src);                        // transfer; src left zeroed
typedef void (*wc_delete_fn)(u8* elm);                                  // uses the element's own stored allocator
```

- The `u8**` move protocol is removed everywhere (about 30 occurrences: `gen_vector.h`, `hashmap.h`, `hashset.h`, `queue.h`, `stack.h`, `wc_helpers.h`, `wc_macros.h`). It existed only to free heap shells.
- Move = `memcpy` then zero the source. `VEC_PUSH_MOVE(v, x)` takes an lvalue struct.
- `copy_fn` gets the destination allocator (fixes A7 and lets you copy an arena-backed container into libc). All `X_copy` functions take an allocator explicitly and never inherit `src->alloc` by `memcpy`.
- `del_fn` needs no allocator argument because each element stores its own.

**By-pointer storage (D6)** stays supported (address stability; sections 2 and 4 of `wc_helpers.h`, `wc_str_ptr_ops`, `wc_vec_ptr_ops`, `VEC_OF_STR_PTR`). Heap shells are created only through one helper so the shell's allocator equals the child's stored allocator:

```c
#define WC_BOX_IN(A, T, init_fn, ...)                                   \
    ({ wc_allocator _a = (A);                                           \
       T* _p = (T*)wc_alloc(_a, sizeof(T), alignof(T));                 \
       FATAL_IF(!_p);                                                   \
       *_p = init_fn(_a, __VA_ARGS__);                                  \
       _p; })
```

`del_ptr` reads the child's allocator, calls the child's `destroy`, then frees the shell with that same allocator. Invariant: *a boxed child's shell is always allocated from the allocator the child stores.*

**Errors (D5):** logic errors (bad index, size 0, element-size mismatch) keep `CHECK_FATAL` (debug-only). Resource failures (`alloc`/`realloc` returned NULL) use an unconditional `FATAL`, so release builds never dereference NULL from an exhausted arena (fixes A6).

**Macros**

| Pair | Meaning |
|---|---|
| `VEC(T, cap)` / `VEC_IN(A, T, cap)` | POD vector; first form is `VEC_IN(WC_LIBC, ...)` |
| `VEC_CX(T, cap, ops)` / `VEC_CX_IN(A, T, cap, ops)` | vector with ops |
| `VEC_OF(T, cap)` / `VEC_OF_IN(A, T, cap)` | ops chosen by `WC_OPS(T)` (replaces `VEC_CREATE_OF`) |
| `MAP_OF(K, V)` / `MAP_OF_IN(A, K, V)` | hashmap with ops from `WC_OPS` |
| `VEC_LIKE(v, T, cap)` | new vector using `(v)->alloc` |
| `ARENA_SCOPE(name, cap)` | creates an arena, exposes `name` as a `wc_allocator`, `destroy`s on scope exit |

Removed: `VEC_STK`, `VEC_CX_STK`, `STACK_STK` (everything is in-place now). Convenience macros that construct strings or nested containers (`VEC_PUSH_CSTR`, `MAP_PUT_STR_*`, `QUEUE_PUSH_CSTR`, `SET_INSERT_CSTR`, `SET_FROM_VEC`) use the **container's** allocator so strings follow their container automatically.

---

## 4. Decisions

| ID | Decision | Recommended default | Status |
|---|---|---|---|
| D1 | Constructors return by value vs. out-parameter | **By value** for containers; out-param only for pinned `Arena`/`ChainArena` | DECIDED |
| D2 | Store allocator by value vs. pointer | **By value**, `{vt*, ctx*}` (16 B) in every structure | DECIDED |
| D3 | Constructor/destructor names | **Keep `create`/`destroy`** | DECIDED |
| D4 | Moved-from state | Zero-filled; every `destroy` safe on zeroed; any mutation of a zeroed container is an unconditional `FATAL` in all builds (section 3.3) | DECIDED |
| D5 | Allocation-failure policy | Unconditional fatal; error-return API deferred | DECIDED |
| D6 | Keep by-pointer storage | Yes, via `WC_BOX_IN` | DECIDED |
| D7 | Alignment passed by containers | Size-derived, capped at `WC_MAX_ALIGN` | DECIDED |
| D8 | Keep `Stack` as a `GenVec` typedef with thin wrappers | Yes | DECIDED |
| D9 | `String` size | **64 bytes, SSO buffer grows from 24 to 32** | DECIDED |
| D10 | Scratch vs. in-place realloc/free | **Floor field** in `Arena`/`ChainArena`; in-place ops only above the floor; scratch saves/restores it; growing a pre-scope block inside a scope is a debug fatal (section 3.2) | DECIDED |
| D11 | Keeping every phase green while the global API is removed | New API under transitional `wc2_`/`WC2_` names in Phase 1; old globals deleted and names finalised at **Phase 2 exit** (section 3.1) | DECIDED |
| D12 | `wc_vm` scope | Optional **Phase 8** of this plan; selected **explicitly only** (no size threshold) | DECIDED |

**D9 detail:** with a by-value allocator, `String` would be 56 bytes at the old 24-byte SSO buffer. Instead the layout is fixed at 64 bytes (one cache line) by growing the inline buffer: `union { char* heap; char stk[32]; }` (32) + `size` (8) + `capacity` (8) + `alloc` (16) = 64. Inline capacity goes from 23 to 31 usable bytes, so more strings never touch the heap. The mode flag stays in the last byte of `stk[]` (`stk[STR_SSO_SIZE - 1]`), and all logic is already expressed through `STR_SSO_SIZE`, so the change is mechanical (section 5.5). `GenVec` is 56 bytes, so the `_Static_assert(sizeof(String) == sizeof(GenVec))` in `wc_helpers.h` must be deleted (each helper already uses its own `sizeof`).

---

## 5. Per-structure plan

### 5.1 `GenVec` (foundation, everything nests on it)

- Struct: add `wc_allocator alloc`; update `_Static_assert` (40 to 56).
- Replace constructors:

| Old | New |
|---|---|
| `GenVec* GenVec_create(n, data_size, ops)`, `GenVec_create_stk(&v, …)` | `GenVec GenVec_create(wc_allocator, n, data_size, ops)` |
| `GenVec_create_val`, `GenVec_create_val_stk` | `GenVec_create` + `GenVec_reserve_val` (thin helper allowed) |
| `GenVec_create_arr`, `GenVec_create_stk_arr` | `GenVec_create_buf(buf, n, data_size, ops)` (sets `wc_borrowed`) |
| `GenVec_destroy`, `GenVec_destroy_stk` | `GenVec_destroy(GenVec*)` (never frees the struct) |
| `GenVec_subarr` (returns `GenVec*`) | `GenVec GenVec_subarr(const GenVec*, wc_allocator, start, len)` |
| `GenVec_copy(dest, src)` | `GenVec GenVec_copy(wc_allocator, const GenVec* src)` |
| `GenVec_move(dest, GenVec** src)` | `void GenVec_move(GenVec* dest, GenVec* src)` |

- All `wc_alloc`/`wc_realloc`/`wc_free` calls become `wc_*(vec->alloc, …)`; sizes passed to `free` are `GET_SCALED(vec, vec->capacity)` (verify each site: reserve, shrink, grow, reset, destroy).
- `GenVec_push_move`, `insert_move`, `replace_move`, `insert_multi_move`: switch to `u8* src`; POD path unchanged.
- `GenVec_reset` frees the buffer and keeps the allocator.
- Growth: replace the `CHECK_FATAL(!new_data, …)` sites with unconditional fatal (A6).

### 5.2 `Stack`

- `typedef GenVec Stack;` stays. `Stack_create(wc_allocator, n, data_size, ops)`, `Stack_create_val` (if kept) and `Stack_destroy` become inline forwards to the `GenVec` functions, returning `Stack` by value.

### 5.3 `Queue` and `PriorityQueue`

- `Queue`: `GenVec arr` already embedded; the allocator lives in `arr.alloc` (no duplication). Remove the raw shell `malloc`/`free`. `Queue_create`, `Queue_create_val`, `Queue_copy(wc_allocator, src)`, `Queue_move(dest, src)`.
- `Queue` resize (`queue.c`, builds a temp `GenVec`): pass `q->arr.alloc`.
- `PriorityQueue`: embeds `Queue`; `PriorityQueue_create(alloc, n, data_size, ops, cmp_fn)`; drop shell handling.
- Size: `Queue` 64 to 80, `PriorityQueue` 72 to 88.

### 5.4 `BitVec`

- Change `GenVec* arr` to embedded `GenVec arr`. `BitVec_create(wc_allocator)`, `BitVec_destroy`. Drop the raw shell.
- Remove the "heap-only, no `_stk`" note; there is one form now.

### 5.5 `String` and `wc_helpers.h`

- **Layout (D9):** `STR_SSO_SIZE` 24 to 32; `String` = `union { char* heap; char stk[32]; }` + `size` + `capacity` + `wc_allocator alloc` = **64 bytes**. Replace `_Static_assert(sizeof(String) == 40, …)` with `== 64`. Inline usable capacity becomes `STR_SSO_SIZE - 1` = 31 (was 23).
- **SSO mode flag:** the flag lives in `stk[STR_SSO_SIZE - 1]` (nonzero = inline, `'\0'` = heap). Every use already goes through `STR_SSO_SIZE` (`IS_SSO`, `String_is_sso`, `String_get`/`String_set`, `MAYBE_GROW_STR`, `String_reset`, `String_shrink_to_fit`, SSO-to-heap promotion), so the size change is mechanical. Verified: no hard-coded `22`/`23`/`24` in code; only the comments "0-22 bit is usable" and "last bit (23)" in `wc_string.c` need updating (Phase 4). In heap mode `stk[31]` must be `'\0'`: the promotion code already writes it explicitly, and with a 32-byte union it must also not be left holding stale bytes from the SSO text (the promotion path sets `stk[STR_SSO_SIZE - 1] = '\0'`, keep that).
- **Zeroed String:** a zero-filled `String` (the moved-from state, D4) has flag byte `'\0'`, so it reads as heap mode with `heap == NULL`, `size == 0`, `capacity == 0`. `String_destroy` must therefore tolerate `heap == NULL` (`wc_free` already ignores NULL). Every mutating entry point (`MAYBE_GROW_STR`, `ensure_capacity`, `String_set`, `insert`, `reserve`, `shrink_to_fit`) checks `capacity == 0` and `FATAL`s (D4, A9). Note that a live heap string always has `capacity > 23` and a live SSO string has `capacity == 31`, so `capacity == 0` identifies the zero state exactly.
- **A8 regression:** the hotfix removes the `size == 0` branch of `String_shrink_to_fit`; keep that when converting `heap_to_stk` to `wc_free(str->alloc, ...)`.
- `String_create`/`String_from_*` (heap `String*` returns) become value-returning `String_create*` with an allocator parameter (`String String_create(wc_allocator)`, `String_from_cstr(wc_allocator, const char*)`, …). The existing `String_create_stk` shape is the model.
- **Heap buffer:** `malloc`/`realloc`/`free` become `wc_*(str->alloc, …)`. The invariant "heap block size == `capacity`" holds everywhere (verified: `String_shrink_to_fit` reallocs to `size` and sets `capacity = size`; the `malloc(new_cap)` / `realloc(s->heap, new_cap)` promotion and growth paths set `capacity = new_cap`). Keep it, because `free` now needs the exact size.
- `String_to_cstr` (returns a malloc'd buffer): takes an allocator; the caller frees with it.
- `String_copy`, `String_move`: allocator parameter / `u8*` move as in section 3.3. `String_move` becomes memcpy + zero.
- `wc_helpers.h`:
  - Delete `_Static_assert(sizeof(String) == sizeof(GenVec), …)`; `String` is 64 and `GenVec` is 56.
  - `str_copy`: `wc_alloc(a, …)` instead of `malloc`.
  - `str_move`, `vec_move`: memcpy + zero source; delete the `free(*src)` and the TODO about it.
  - `*_ptr` variants: `copy_ptr` allocates the shell with `dst`; `del_ptr` reads the child's allocator, destroys the child, frees the shell; `move_ptr` transfers the pointer and nulls the source variable.
  - Update `wc_str_ops`, `wc_str_ptr_ops`, `wc_vec_ops`, `wc_vec_ptr_ops` to the new callback signatures.

### 5.6 `HashMap`

- Struct: add `wc_allocator alloc` (88 to 104).
- Four buffers per table: `keys` (`cap * key_size`), `psls` (`cap`), `vals` (`cap * val_size`), `scratch` (`2 * (ALIGN8(key_size) + val_size)`). Introduce one private helper per size formula and use it for alloc, free, resize and copy so `free` sizes cannot drift.
- `calloc` for `psls` becomes `wc_alloc` + `memset` (arena memory is not zero after `reset`).
- Sites: `HashMap_create*` (init), resize (~line 691), copy (~line 522), destroy (~line 120), `HashMap_clear`/`reset` if present.
- Duplicate-key path (lines ~215 and ~312): replace `k_del(*key); free(*key); *key = NULL;` with `k_del(key)` only (no shell exists anymore).
- Move APIs (`put_move`, `put_val_move`, `put_key_move`): `u8** ` becomes `u8*`.
- `HashMap_copy(wc_allocator, src)`; nested keys/values copied via `copy_fn(dst, …)`.

### 5.7 `HashSet`

- Same treatment as `HashMap`: `elms` (`cap * elm_size`), `psls`, `scratch` (`2 * elm_size`).
- `calloc` sites (init copy, resize) become `wc_alloc` + `memset`.
- Duplicate-insert path (~line 145): `e_del(elm)` only.
- `SET_FROM_VEC` uses the vector's allocator by default.

### 5.8 `Matrixf` and `matrix_generic.h`

- Add `wc_allocator alloc` (24 to 40). `matrix_create(alloc, m, n)`, `matrix_create_arr`, `matrix_destroy`, `matrix_copy(alloc, src)`, `matrix_move(dest, src)`.
- Borrowed data (today `matrix_create_stk(mat, m, n, data)`): `matrix_create_buf(m, n, float* data)` uses `wc_borrowed`; `destroy` frees nothing.
- The `MATRIX_*` macros in `matrix_generic.h` (`malloc` at lines ~39/43, `free` at ~71/72) take the allocator; remove the separate `MATRIX_ARENA` path, since `MATRIX_IN(arena_allocator, …)` covers it.

### 5.9 `StringStore` and `StrView`

- `StringStore_create(ss, wc_allocator)`; nodes and overflow `heap` buffers via the allocator; `StringStore_destroy`. Size 24 to 40.
- `StrView_from_cstr(cstr, clen, Arena*)` splits into two functions: `StrView_from_cstr(cstr, clen)` (no allocation) and `StrView_copy_cstr(alloc, cstr, clen)` (allocates). Returned views over allocated memory borrow from the allocator's lifetime; document this.

### 5.10 `Arena` and `ChainArena`

See section 3.2 (including the scratch floor, D10). Signature changes: `Arena_create`/`Arena_create_stk` collapse into one out-param `Arena_create(Arena*, backing, cap)`; `Arena_create_arr_stk` becomes `Arena_create_buf`; `chain_Arena_create` (returns a pointer) becomes out-param `ChainArena_create(ChainArena*, backing)` with a matching `destroy`. Typed macros (`ARENA_ALLOC`, `ARENA_ALLOC_N`, `ARENA_ALLOC_ZERO*`, `CHAIN_ARENA_*`) stay.

---

## 6. Memory footprint

| Structure | Now | After (by-value allocator) |
|---|---|---|
| `GenVec` / `Stack` | 40 | 56 |
| `String` | 40 | **64** (SSO buffer 24 to 32) |
| `Queue` | 64 | 80 |
| `PriorityQueue` | 72 | 88 |
| `BitVec` | 16 + 40 heap shell + malloc header | 64 (one embedded `GenVec` + `size`) |
| `HashMap` | 88 | 104 |
| `HashSet` | 72 | 88 |
| `Matrixf` | 24 | 40 |
| `StringStore` | 24 | 40 |

`String` grows by 24 bytes but keeps a full cache line and stores 31-byte strings inline instead of 23. `GenVec<String>` slots are 64 bytes. Net effect elsewhere: no heap shells (one fewer allocation and free per container, no allocator-header overhead per shell), and arena waste drops through in-place realloc and size-derived alignment.

---

## 7. Phases

Effort: S (hours), M (a day), L (multi-day). Each phase ends with a green build, all tests passing under ASAN + UBSan, and a leak-free test allocator report.

### Phase 0: Safety net (S): DONE

- [x] Run the existing `tests` binary green in the Debug build (ASAN + UBSan). One stale test fixed: `test_overflow_by_one_goes_to_heap` asserted the head node's flag byte was 1, but the overflow node is pushed at the head and owns a heap buffer, so the flag is 0. Test now asserts 0 and that the view points into `head->heap`.
- [x] Land the A8 hotfix with its regression test (`test_shrink_empty_heap_string_returns_to_sso`). Verified: fails with an ASAN NULL-store on pre-hotfix code, passes after.
- [x] Write `wc_test_allocator` (`tests/wc_test_allocator.{h,c}`). Targets today's `wc_allocator_t`; its three callbacks already match the planned vtable signatures, so Phase 1 only adds a vtable. Self-tested (counts, leaks, double free, foreign pointer, size/align mismatch, zero size, fail-Nth, table churn, arena backing) and already run against `GenVec` through the global allocator: lifecycle, copy/move/reset and nested `GenVec<GenVec>` are leak-free with correct `free` sizes today.
- [x] Golden scenarios (`tests/regression_test.c`): `main.c` 70-push on a 1 KB arena (contents 0..69, **arena used = 964 bytes baseline**), the same on libc (capacity 73), and a fixed-seed queue + hashmap workload pinned by checksums.
- [x] Expected-fail regression tests: A1 (two variants), A2 (`Arena` and `ChainArena`) via the new `WC_RUN_XFAIL` (XPASS counts as a failure, so a fix forces the switch to `WC_RUN`). A5 as a ctest compile check (`a5_realloc_n_compiles`, `WILL_FAIL`).
- [x] Release-only defect found and fixed: A10 (`MAP_GET`).
- Exit met: 419/419 tests pass in Debug (ASAN + UBSan) and Release; ctest 2/2; A1/A2 report XFAIL, A5 compile check fails as expected.
- Toolchain note: build with gcc if the local clang has no ASAN runtime (`-DCMAKE_C_COMPILER=gcc`).

### Phase 1: Allocator core and backends (M–L): DONE

- [x] New allocator core in `wc_allocator.h`/`.c` alongside the old one (D11): `wc_alloc_vtable`, `wc_allocator` (16 B, asserted), `WC_LIBC`, `wc_borrowed`, `wc2_alloc/realloc/free`, `WC2_NEW/NEW_N/DELETE/DELETE_N/REALLOC_N`. Also `wc2_mul` (saturating to `SIZE_MAX`, so an overflowing count fails instead of wrapping, and never turns a realloc into a free), `wc2_align_for_size` (D7, ready for Phase 2), `wc2_is_libc`, `wc2_same`. Requests above `PTRDIFF_MAX` fail in the wrapper. Old globals untouched.
- [x] `Arena`: out-param `Arena_create(a, backing, cap)`, `Arena_create_buf`, `Arena_destroy` (zero-safe, idempotent), `Arena_reset`; address-aligned, underflow-safe alloc (A1, A2); in-place realloc and last-block free guarded by `floor` (D10, A4); shrinking a non-top block keeps it in place; scratch saves/restores the floor; `self` liveness check. `ARENA_ALLOC*` now use `alignof(T)` and `wc2_mul`; `ARENA_ALLOC_ZERO*` no longer `memset` a NULL on a full arena; `ARENA_PUSH_ARRAY` compiles (it had a `(T)*` typo). `ARENA_CREATE_STK_ARR` became `ARENA_CREATE_BUF(a, nbytes)`.
- [x] `Arena_allocator` returns the new `wc_allocator`; `Arena_allocator_legacy` returns the old `wc_allocator_t` over the same callbacks. `main.c` and the golden scenario use it.
- [x] `ChainArena` rewritten: `ChainArena_create/destroy/reset/clear/alloc/alloc_aligned/allocator/scratch_begin/scratch_end` (all `chain_Arena_*` names are gone), backing allocator, intrusive doubly linked node list (no `GenVec`), oversize requests get a dedicated node (A3), address alignment (A2), floor-guarded in-place realloc and last-block free, `clear` keeps nodes and reuses them with zero backing allocations.
- [x] Tests: `tests/allocator_core_test.c` (11), `tests/arena_test.c` rewritten (31), `tests/chain_arena_test.c` (13), including alignment 1 to 64 on every base offset 0 to 15, exhaustion, in-place grow/shrink, non-top copy, failed realloc keeps the block, last-block free, arena on arena, region freed with exact size through the test allocator.
- [x] Scratch-floor tests for `Arena` and `ChainArena`: growing a pre-scope block dies in debug (verified with a control that the legal shape does not die), shrink/free of a pre-scope block does not rewind below the mark, nested scopes restore the floor in LIFO order, in-place growth above the mark still works, a scratch spanning several nodes releases every appended node.
- [x] A1/A2 flipped to `WC_RUN`; new A3 and A4 regression tests; `tests/compile/a5_realloc_n.c` rewritten against `WC2_REALLOC_N` and compiled with `-Werror`, `WILL_FAIL` removed.
- [x] `wc_test_allocator`: `wc_test_alloc_init2` (new-API backing) and `wc_test_alloc_allocator2` (vtable view).
- [x] `tests/wc_test_fatal.h`: `WC_ASSERT_DIES(fn)` runs `fn` in a forked child and asserts it terminates. This is the "test hook for `FATAL`" section 8 needs; Phase 2 uses it for zero-state and fail-Nth tests.
- Exit met: 462/462 tests in Debug (ASAN + UBSan) and Release, ctest 2/2, no new warnings. `ChainArena` no longer depends on `GenVec`. Old global API present and unchanged.
- **Golden:** the `main.c` 70-push scenario now uses **340 bytes** of the 1 KB arena (was 964) with identical contents, through the legacy adapter alone. `GOLDEN_MAIN_ARENA_USED_BASELINE` lowered to 340.

### Phase 2: `GenVec` (L): DONE

- [x] `GenVec` is 56 bytes (asserted) and stores `wc_allocator alloc`. `GenVec_create(alloc, n, size, ops)`, `GenVec_create_val(alloc, ...)` and `GenVec_create_buf(buf, n, size, ops)` (over `wc_borrowed`) return by value. `GenVec_destroy` never frees the struct, is zero-safe and leaves the struct zeroed. `_stk`, `_val_stk`, `_arr`, `_stk_arr`, `destroy_stk` are gone.
- [x] Every allocation, reallocation and free goes through `vec->alloc` with size-derived alignment (D7). `free` sizes are `capacity * data_size` everywhere, verified by the test allocator. Resource failures are unconditional `FATAL` (D5).
- [x] New callback signatures in `common.h`: `wc_copy_fn(wc_allocator dst, u8* dest, const u8* src)`, `wc_move_fn(u8* dest, u8* src)` (source left zeroed). `GenVec_copy(alloc, src)` never inherits `src->alloc` (A7). `GenVec_move(dest, src)` is memcpy + zero. `GenVec_subarr(vec, alloc, start, len)`.
- [x] `WC_BOX_IN(A, T, init_fn, ...)` in `common.h`; `wc_vec_ptr_ops` allocates shells through it and frees them with the child's own allocator (D6).
- [x] Macros: `VEC`/`VEC_IN`, `VEC_CX`/`VEC_CX_IN`, `VEC_OF`/`VEC_OF_IN` (replaces `VEC_CREATE_OF`), `VEC_LIKE`, `VEC_FROM_ARR`/`VEC_FROM_ARR_IN`, `VEC_PUSH_MOVE(v, lvalue)`. `VEC_PUSH_CSTR` works for by-value and by-pointer String vectors. `VEC_STK`, `VEC_CX_STK`, `STACK_STK` removed.
- [x] Zero-state guard (D4): growth, reserve and create with `data_size == 0` are unconditional `FATAL`. Found while testing: a zeroed vector has `is_pod == 0` and `ops == NULL`, so `clear`/`reset` dereferenced NULL. Every ops access now goes through the NULL-safe `VEC_*_FN` accessors.
- [x] **Pulled forward from Phases 3 and 5 (forced):** the callback type is shared by every container, so the `u8**` move protocol was removed everywhere in this phase. `GenVec`, `Stack`, `Queue`, `HashMap` (`put_move`, `put_key_move`, `put_val_move`) and `HashSet_insert_move` take `u8*`, leave the source zeroed, and fall back to memcpy + zero when `move_fn` is NULL (it was mandatory before). Duplicate paths destroy the incoming element instead of `free(*key)`. Map/set/queue/stack macros follow. `HashMap`/`HashSet` copies pass `WC_LIBC` as the destination allocator until Phase 5.
- [x] Dependents ported internally only (their public APIs are Phase 3/5): `Stack_create*` box a libc `GenVec`; `Queue` uses value `GenVec` and its compaction reuses `q->arr.alloc`; `PriorityQueue_from_vec` copies with `WC_LIBC`; `BitVec` holds a `WC_BOX_IN(WC_LIBC, ...)` shell. String callbacks ignore `dst` until Phase 4 (String has no allocator yet).
- [x] **Global removal (D11):** `wc_allocator_t`, `wc_default_allocator`, `WC_SET_ALLOCATOR`, `WC_RESOLVE`, `wc_libc_allocator`, the simple API, the old typed macros and `Arena_allocator_legacy` are deleted; `wc2_`/`WC2_` renamed to `wc_`/`WC_`. `wc_test_alloc` has one view (`wc_test_alloc_init(ta, backing)`, `wc_test_alloc_allocator(ta)`). `main.c` builds its vector with `VEC_OF_IN(Arena_allocator(&a), int, 5)`.
- [x] CMake now always builds with `-Werror=incompatible-pointer-types -Werror=implicit-function-declaration -Werror=int-conversion` (section 3.3). This caught stale `(u8**)` call sites in the map/set tests that plain warnings would have let through with wrong behaviour.
- [x] Tests: every existing GenVec/complex/macros/speed/map/set test ported. New `tests/gen_vector_alloc_test.c` (17): the same workload on libc, test allocator, `Arena`, arena over a stack buffer, `ChainArena`; borrowed buffer fills in place and dies on growth; copy arena to libc; nested and boxed children follow the destination allocator; subarr and `VEC_LIKE`; D7 alignment per element size and tight packing; zero-state deaths and safe reads; fail-Nth at every allocation attempt of a scenario covering create, grow, reserve, shrink, copy, subarr and nested element copy (each one must abort cleanly).
- Exit met: 479/479 tests in Debug (ASAN + UBSan) and Release, ctest 2/2, no new warnings. `rg 'wc2_|WC2_|wc_allocator_t|WC_SET_ALLOCATOR|u8\*\*'` over `src/`, `include/`, `tests/` returns nothing.
- **Golden:** `main.c` scenario on the 1 KB arena now uses **292 bytes** (was 340, originally 964): exactly 73 x 4 bytes, zero waste. Libc capacity unchanged (73). `GOLDEN_MAIN_ARENA_USED_BASELINE` lowered to 292.

### Phase 3: `GenVec` dependents (M)

- [ ] `Stack` forwards; `Queue` (init, copy, move, resize with stored allocator); `PriorityQueue`; `BitVec` (embedded `GenVec`).
- [ ] Macros: `QUEUE_*`, `STACK_*` (move macros already on `u8*` since Phase 2; creation macros still wrap the heap-returning Stack/Queue API).
- [x] `u8*` move protocol for `Stack_push_move` / `Queue_push_move` (done in Phase 2).
- Exit: tests for each, including queue wrap-around resize on an arena.

### Phase 4: `String` and ops helpers (M)

- [ ] Audit `wc_string.c`/`wc_string.h` fully: every heap-returning function, `String_to_cstr`. (Heap size == capacity invariant already verified on current code: `stk_to_heap`, `String_grow`, `ensure_capacity` and `shrink_to_fit` all keep the block size equal to `capacity`; `String_to_cstr` is a separate `size + 1` block.)
- [ ] Zero-state guard (D4, A9) on every mutating entry point (`capacity == 0`).
- [ ] Update the stale literal comments in `wc_string.c` ("0-22 bit is usable", "last bit (23)") to refer to `STR_SSO_SIZE - 1`.
- [ ] Convert `String` to create/destroy, stored allocator, by-value returns, 64-byte layout.
- [ ] Rewrite `wc_helpers.h` ops for the new signatures and `WC_BOX_IN`; retire the `sizeof(String) == sizeof(GenVec)` assert.
- [ ] Convert `VEC_PUSH_CSTR` and friends to use the container's allocator.
- Exit: `GenVec<String>` and `GenVec<String*>` tests leak-free on libc and arena; copy across allocators works.

### Phase 5: `HashMap` and `HashSet` (L)

- [ ] Size helpers for all buffers; convert create/resize/copy/destroy/clear.
- [x] Replace duplicate-path `free(*key)`/`free(*elm)`; convert move APIs to `u8*` (done in Phase 2).
- [ ] Replace `MAP_ELEM_ALLOC` (`WC_LIBC`, transitional) with the map/set's stored allocator in every `copy_fn` call.
- [ ] Macros: `MAP_*`, `SET_*`.
- [ ] Tests: fill past several resizes, remove/backward-shift, duplicate puts with owning keys (String), copy into a different allocator, fail-Nth in resize.
- Exit: leak-free with test allocator; behaviour matches the pre-refactor golden results on the same inputs.

### Phase 6: `Matrixf`, `StringStore`/`StrView` (M)

- [ ] `Matrixf` and `matrix_generic.h` macros; `matrix_create_buf` via `wc_borrowed`.
- [ ] `StringStore` and the `StrView_*` split.
- Exit: matrix and string-store tests green; no `Arena*` parameters remain outside the arena module.

### Phase 7: Enforcement and cleanup (S)

- [ ] Delete every legacy heap-returning constructor, `*_stk` / `*_val` / `*_arr` variant, `u8**` move signature, and global-allocator remnant.
- [ ] Add `#pragma GCC poison malloc calloc realloc free` (or a CI grep) for `src/` and `include/`, with exceptions only in the libc backend inside `wc_allocator.h`.
- [ ] Update all header docs/examples (`gen_vector.h` TLDR still shows `GenVec_create`), `main.c`, and add a short `docs/allocators.md` (lifetimes, pinned arenas, copy vs move, boxing).
- [ ] Add an `examples/` program using `ARENA_SCOPE`, nested containers, and `Arena_create_buf`.
- Exit: `rg 'malloc\(|calloc\(|realloc\(|free\('` in `src/` and `include/` returns only the libc backend and tests.

### Phase 8 (optional): `wc_vm` backend and reserve/commit arenas (M)

Starts only after the Definition of Done (section 10) is met.

- [ ] `wc_vm` allocator backend: `mmap`/`munmap` on POSIX, `VirtualAlloc`/`VirtualFree` on Windows, `malloc` fallback elsewhere. Rounds sizes up to the page size. Used as a backing allocator: `Arena_create(&a, wc_vm, cap)`. No API change, since `Arena` already takes a backing allocator.
- [ ] Optional reserve/commit mode for `Arena`: `reserved` and `committed` fields; the alloc slow path runs when `idx + req > committed` and commits the next chunk (default 256 KB). Growth never moves memory. `ChainArena` stays the fallback on platforms without virtual memory.
- [ ] **Selection is explicit only (D12).** `malloc` stays the default backing for every arena, including sanitizer builds. No size threshold.
- [ ] `wc_vm` is for arena backing only, never for containers directly: page granularity and a syscall per allocation would cost far more than `malloc` for container-sized blocks.
- [ ] Tests: reserve large, commit across chunk boundaries, scratch floor across a commit boundary, `Arena_reset` keeps commits, `destroy` releases the full reservation.
- Exit: arena tests pass on `wc_vm` backing under UBSan (ASAN does not track `mmap` regions; run the leak check through the test allocator wrapping `wc_vm`).

---

## 8. Test strategy

- **Sanitizers:** ASAN + UBSan are already on in the Debug build (the `0xBE` fill in your dump is ASAN's malloc fill); run every test on each backend (libc, `Arena`, `Arena_create_buf`, `ChainArena`, test allocator wrapping libc).
- **Contract tests via `wc_test_allocator`:** leaks, double free, wrong `size`/`align` at free, use of memory after free (poison check), peak/live byte counts.
- **Failure injection:** for each container operation that allocates, run it with fail-Nth for N = 1..k and assert it aborts cleanly (no partial state visible after a caught fatal, using a test hook for `FATAL`).
- **Differential tests:** run the same operation sequence on libc and on arena-backed containers and compare contents.
- **Cross-allocator copy tests:** deep-copy nested containers from arena into libc and back; verify every child pointer belongs to the destination allocator.
- **Golden test:** the 70-push scenario and a queue/hashmap scenario with fixed seeds.

---

## 9. Risks and mitigations

| Risk | Mitigation |
|---|---|
| `free` sizes drift from `alloc` sizes (invisible under arenas) | Size helper per buffer; test allocator verifies every free. |
| Boxed child's shell allocated from a different allocator than the child stores | Only create shells through `WC_BOX_IN`; document the invariant. |
| Moved-from objects double-freed | Zero on move; every `destroy` safe on zeroed; tests double-destroy. |
| Pinned `Arena` moved/copied while containers reference it | Debug magic check in callbacks; document; `Arena` marked non-copyable like `ChainArena`. |
| Big-bang breakage | Phases each leave a green build (D11: new API under `wc2_` names until the old globals are deleted at Phase 2 exit). Names stay `create`/`destroy` (D3); stale call sites fail on the **signature** changes (allocator parameter, by-value return, removed `_stk`), not on a rename. |
| In-place realloc/free corrupting scratch scopes | `floor` field (D10); dedicated scratch-floor tests in Phase 1 for `Arena` and `ChainArena`. |
| Moved-from container reused, silently allocating through `WC_LIBC` | Unconditional zero-state `FATAL` on mutation (D4); push-after-move tests per container. |
| `String` grows 40 to 64 bytes; heap-mode strings waste the unused inline bytes | Accepted (D9); larger SSO offsets it for short strings. Measure on a real workload. |
| SSO size change breaks hard-coded offsets | Grep for literals tied to 24/23/22; keep all logic on `STR_SSO_SIZE`; test strings of length 0, 30, 31, 32, 33 across the SSO-to-heap boundary and back via `shrink_to_fit`. |
| By-value struct return of `HashMap` (104 B) | No self-referential fields (verified); cost is one small memcpy at init. |
| Release builds hiding failures | Resource failures use unconditional `FATAL` (D5). |

---

## 10. Definition of done

1. No raw `malloc`/`calloc`/`realloc`/`free` outside the libc backend (enforced by poison or CI grep).
2. No global allocator; `WC_SET_ALLOCATOR` and `wc_default_allocator` are gone.
3. No function returns a heap-allocated container pointer; no `u8**` in the public API.
4. Every container stores its allocator; every `copy` takes a destination allocator; every `destroy` is zero-safe; every mutation of a zeroed container is an unconditional fatal.
5. Test suite passes on all backends under ASAN + UBSan with zero leaks and full failure-injection coverage.
6. Defects A1–A10 have regression tests, and in-place realloc/free never crosses a scratch floor.
7. Headers and docs describe only the new API.
