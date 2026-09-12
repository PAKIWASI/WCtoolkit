# WCtoolkit Macro/Helper Layer — Standardization & Completeness Plan

Companion to `wctoolkit-api-standardization-plan.md`, scoped to `wc_helpers.h`
and `wc_macros.h` specifically. The core library plan makes the underlying
functions consistent; this plan makes the convenience layer on top of them
consistent, as compile-time-checked as C11+GNU allows, and covering every
module instead of just `genVec` and half of `hashmap`.

Two things make this layer higher-priority than it looks: it's what most
callers actually type, and it currently has **zero test coverage of its
own** — which is how two macros that don't compile shipped undetected.
Everything here assumes Rule 0: **no macro merges without a test that
instantiates it.**

---

## Part A — Bugs to fix immediately (no design decisions needed)

1. `SET_INSERT_MOVE`: `(vec)` → `(set)`. Currently references an
   out-of-scope identifier; fails to compile at every call site, or worse,
   silently binds to an unrelated `vec` variable if one happens to exist in
   the caller's scope.
2. `DEQUEUE`: `_tmp` → `__tmp`. Same class of bug, same result.
3. Both are uncovered by `tests/`. Add `tests/macros_test.c` before or
   alongside the fix, so this class of bug can't regress silently again —
   see Part F.

## Part B — Naming grammar (standardize across every module)

**Decision: one grammar, `<MODULE>_<VERB>[_<QUALIFIER>]`, applied to macros
exactly like Rule 2 applies to functions in the core plan.** Today `VEC_*`
and `MAP_*` mostly follow this; `SET_*` follows it inconsistently;
`ENQUEUE`/`DEQUEUE` don't follow it at all; `Stack` has no macro layer to
be inconsistent *in*.

| Concept | Sequence (`VEC`/`STACK`/`QUEUE`) | Map (`MAP`) | Set (`SET`) |
|---|---|---|---|
| create (POD) | `_CREATE(T, cap)` | `_CREATE(K, V, cap)` | `_CREATE(T, cap)` |
| create (owned, explicit ops) | `_CREATE_CX(T, cap, ops)` | `_CREATE_CX(K, V, cap, kops, vops)` | `_CREATE_CX(T, cap, ops)` |
| create (owned, auto ops via `WC_OPS`, see Part C) | `_CREATE_OF(T, cap)` | `_CREATE_OF(K, V, cap)` | `_CREATE_OF(T, cap)` |
| create on stack | `_STK(T, cap, dst)` / `_CX_STK(...)` | same suffix pattern | same suffix pattern |
| insert (copy) | `_PUSH(c, val)` | `_PUT(m, k, v)` | `_INSERT(s, val)` |
| insert (move) | `_PUSH_MOVE(c, ptr)` | `_PUT_MOVE / _PUT_KEY_MOVE / _PUT_VAL_MOVE` | `_INSERT_MOVE(s, ptr)` |
| insert (cstr→String convenience) | `_PUSH_CSTR(c, cstr)` | `_PUT_CSTR(m, k, cstr)` / `_PUT_CSTR_CSTR(m, kcstr, vcstr)` | `_INSERT_CSTR(s, cstr)` |
| read by key/index (copy out, safe-on-miss, see Part E) | `_AT(c, T, i)` (bounds-checked, fatal on OOB — index access is a programmer contract) | `_TRY_GET(m, V, k, &out)` → `b8` | `_TRY_GET(s, T, elm, &out)` → `b8` |
| read by key (assert-exists, for callers who've already checked `_HAS`) | n/a | `_GET(m, V, k)` | n/a |
| read pointer (const / mut) | `_AT_PTR` / `_AT_PTR_MUT` | `_GET_PTR` / `_GET_PTR_MUT` | `_GET_PTR` / `_GET_PTR_MUT` |
| remove | `_POP(c, T)` / `_REMOVE(c, i)` | `_DEL(m, k)` | `_REMOVE(s, elm)` |
| iterate | `_FOREACH(c, T, name)` (mutable) | `_FOREACH_KEY` (const) / `_FOREACH_VAL` (mutable) | `_FOREACH` (const) |

Apply this to `Stack`/`Queue` for the first time (`STACK_PUSH`,
`STACK_POP`, `STACK_AT`, `QUEUE_PUSH`, `QUEUE_POP`, `QUEUE_AT` — since both
are backed by `genVec`, these can literally be `#define`s of the `VEC_*`
macro with the right accessor swapped in, near-zero new code). This also
retires the bare `ENQUEUE`/`DEQUEUE` — keep them as
`WCTOOLKIT_LEGACY_NAMES`-gated aliases per the core plan's Phase 4.

**Delete the false distinction.** `VEC_PUSH`/`VEC_PUSH_COPY` and
`SET_INSERT`/`SET_INSERT_COPY` are identical today. Keep one name per
concept (`_PUSH`, `_INSERT`) and delete the `_COPY`-suffixed duplicate
entirely rather than aliasing it — an alias would imply the distinction is
real and might come back later; deleting it is honest about the fact that
"copy" is the only thing `_PUSH`/`_INSERT` without `_MOVE` ever meant.

## Part C — Type safety: what C11+GNU actually lets us check

C has no generics and no constructors, but this codebase already leans on
two GNU extensions (`typeof`, statement expressions) plus C11 `_Generic`.
Push all three further than the current layer does:

**C.1 — Auto-derive `ops` from the element type with `_Generic`, instead of
requiring the caller to remember and pass `&wc_str_ops` by hand.**

```c
// in wc_helpers.h, alongside the ops instances
#define WC_OPS(T) _Generic((T*)0,          \
    String*:  &wc_str_ops,                 \
    String**: &wc_str_ptr_ops,             \
    genVec*:  &wc_vec_ops,                 \
    genVec**: &wc_vec_ptr_ops,             \
    default:  (const container_ops*)NULL   \
)

#define VEC_CREATE_OF(T, cap) genVec_init((cap), sizeof(T), WC_OPS(T))
```
This is the single highest-leverage change in this plan: it turns "did I
remember to pass the right ops struct for this element type" — currently a
silent, undetectable-until-it-corrupts-memory mistake if you get it wrong —
into a **compile-time-resolved lookup**. Getting it wrong either resolves
to `NULL` (safe: falls back to POD `memcpy` semantics, which is wrong for
an owned type but at least doesn't crash) or, if you extend the `_Generic`
table for your own type, resolves to exactly the ops you registered for
that type and nothing else.

Known C limitation, stated up front rather than discovered later: `_Generic`
tables are closed — a user's own type can't extend `WC_OPS` without editing
`wc_helpers.h` (or the user defining their own `MY_OPS(T)` macro alongside
it, which is the documented escape hatch). Keep the explicit-`ops`-pointer
macros (`_CREATE_CX`) permanently as the fallback for exactly this reason —
`_CREATE_OF` is sugar on top of `_CREATE_CX`, never a replacement for it.

**C.2 — Compile-time-checkable element-size assertions on typed accessors.**

`VEC_AT(vec, T, i)` currently trusts the caller's `T` blindly — nothing
stops `VEC_AT(int_vec, String, 0)` from reinterpreting four bytes of `int`
as a 40-byte `String` struct and reading off the end. Add a debug-mode
runtime check (can't be fully compile-time since `data_size` is a runtime
field, but make it zero-cost in release):

```c
#define WC_ASSERT_ELEM_SIZE(c, T) \
    CHECK_FATAL(sizeof(T) != (c)->data_size, \
                "element type size mismatch: sizeof(" #T ") != container data_size")

#define VEC_AT(vec, T, i) \
    (WC_ASSERT_ELEM_SIZE((vec), T), *(T*)genVec_get_ptr((vec), (i)))
```
Apply the same wrapper to every typed accessor macro across every module
(`_AT`, `_AT_MUT`, `_FRONT`, `_BACK`, `_POP`, `_TRY_GET`, `_GET`,
`FOREACH`'s `name` binding). This is the closest C gets to "the compiler
catches a type mismatch" — it's a runtime check, but it fires at the first
call with a specific, actionable message instead of manifesting later as
memory corruption three functions away.

**C.3 — `static_assert` where the mismatch genuinely is compile-time
knowable.** Anywhere a macro takes two type tokens that must agree (e.g. a
future `MAP_CREATE_OF(K, V, cap)` where `K`'s and `V`'s sizes get baked into
the call), add `_Static_assert(sizeof(K) <= UINT32_MAX, ...)`-style guards
for the things C *can* verify at compile time (size fits the `u32`
key_size/val_size fields, alignment is sane) even though "is this the
right type" fundamentally can't be.

**C.4 — Never reach into private struct fields from a macro.** This is a
type-safety issue as much as an encapsulation one: `SET_FOREACH`,
`MAP_FOREACH_KEY`, `MAP_FOREACH_VAL`, and `SET_FROM_VEC` currently compute
raw pointer arithmetic against `->elms`/`->keys`/`->vals`/`->psls`/
`->data_size`/`->ops` directly. Every one of those must be rewritten to go
through a public accessor, same as `VEC_FOREACH` already does via
`genVec_get_ptr_mut`. Where no accessor exists yet, add one to the core
API rather than let the macro layer route around the boundary — concretely:
  - `hashmap`/`hashset` need public bucket-iteration primitives:
    `hashmap_bucket_count(map)`, `hashmap_bucket_occupied(map, i)`,
    `hashmap_bucket_key_ptr(map, i)`, `hashmap_bucket_val_ptr(map, i)` (and
    the `hashset` equivalents). These are small, mechanical additions to
    `hashmap.c`/`hashset.c` — add them as part of this plan's Phase 1 (they
    belong to the core-API plan's Phase 3 coverage-gap work too; do them
    once, reference from both plans).
  - Rewrite the four macros to call these instead of touching fields. Now
    if the robin-hood layout changes (the `TODO` in `hashmap.h` about a
    sentinel-slot scheme is exactly such a change), only the four accessor
    *functions* need updating — no macro call site anywhere in user code
    breaks.

## Part D — Safe error signaling for "get" (Part E of the core plan applies
here directly)

`MAP_GET` currently discards `hashmap_get`'s found/not-found result and
returns uninitialized memory on a miss — strictly worse than the function
it wraps. Replace the single ambiguous macro with two unambiguous ones,
named per the Part B grammar:

```c
// Returns b8; writes *out only if found. Caller MUST check the return.
#define MAP_TRY_GET(map, V, key, out_ptr)                          \
    ({                                                             \
        typeof(key) _mk = (key);                                  \
        hashmap_get((map), (const u8*)&_mk, (u8*)(out_ptr));      \
    })

// Asserts the key exists (CHECK_FATAL on miss) — for call sites that
// already checked hashmap_has, or where a miss is a programmer error.
#define MAP_GET(map, V, key)                                       \
    ({                                                             \
        V _out;                                                    \
        typeof(key) _mk = (key);                                  \
        CHECK_FATAL(!hashmap_get((map), (const u8*)&_mk, (u8*)&_out), \
                    "MAP_GET: key not found");                     \
        _out;                                                      \
    })
```
`MAP_GET` becomes loud-fail instead of silent-garbage — consistent with
`CHECK_FATAL`'s role as the "this is a programmer error" tier from the core
plan's Rule 4. `MAP_TRY_GET` is the safe default for anything that expects
misses. Apply the identical `_TRY_GET`/assert-variant split to the `SET`
equivalent (`hashset` doesn't currently have a value-bearing get at all,
only `_has` — add `SET_TRY_GET` only if/when `hashset_get_ptr` lands per
the core plan's Rule 6, otherwise skip it there, it doesn't apply to a
value-less set).

## Part E — Completeness matrix (target state, every module)

| | Vec (`genVec`) | Stack | Queue | Map (`hashmap`) | Set (`hashset`) |
|---|---|---|---|---|---|
| `_CREATE` / `_CREATE_OF` / `_CREATE_CX` | ✅ has all three | ❌ add (thin alias over Vec's) | ❌ add | ⚠️ has `_CX` only, add `_OF` | ⚠️ has `_CX` only, add `_OF` |
| `_STK` / `_CX_STK` | ✅ | ❌ add | ✅ (`queue_create_stk` exists, macro missing) | ❌ add (blocked on core plan Rule 5 exposing `hashmap_create_stk`) | ❌ add (same, `hashset_create_stk`) |
| insert (copy/move/cstr) | ✅ | ❌ add (alias to Vec's) | ⚠️ has bare `ENQUEUE` only, needs `_MOVE`/`_CSTR` too | ✅ has `_PUT`/`_PUT_MOVE`/`_PUT_KEY_MOVE`/`_PUT_VAL_MOVE`, add `_PUT_CSTR` general form | ⚠️ has `_INSERT`/`_INSERT_MOVE`/`_INSERT_CSTR`, fix the `_MOVE` bug |
| typed read (`_AT`/`_TRY_GET`/ptr variants) | ✅ | ❌ add | ❌ add | ⚠️ split `_GET` per Part D | ❌ add `_TRY_GET`/`_GET_PTR` (needs core-plan Rule 6 accessors first) |
| remove/pop | ✅ | ❌ add | ⚠️ has bare `DEQUEUE` only (buggy), rename+fix | ✅ has `_DEL` implicitly via raw call, add macro form for consistency | add `_REMOVE` macro form |
| `_FOREACH` | ✅ (mutable, via accessor) | ❌ add (alias to Vec's) | ❌ add | ⚠️ exists but breaches abstraction (Part C.4) | ⚠️ same |
| typed creation shorthand (`_OF_INT`, `_OF_STR`, ...) | ✅ | n/a (inherits Vec's) | n/a | ⚠️ two hardcoded combos only, generalize via `WC_OPS`-based `_CREATE_OF` (Part C.1) instead of hand-listing pairs | ⚠️ same generalization applies |

Legend: ✅ done · ⚠️ partial/buggy, fix · ❌ missing, add.

The right-hand read of this table is the actual backlog: **Stack and Queue
get a macro layer for the first time** (mechanical — both wrap `genVec`,
so most of it is one-line aliases to the `VEC_*` macros with the right
underlying function swapped in), **Map/Set's typed-creation shorthands get
generalized** via `WC_OPS` instead of hand-maintained pairs like
`MAP_PUT_INT_STR` (which doesn't scale — every new K/V combo currently
needs its own bespoke macro; `_CREATE_OF(K, V, cap)` plus generic
`_PUT`/`_TRY_GET` replaces the whole hardcoded-combo pattern), and every
`FOREACH`/`FROM_VEC` macro gets rebuilt on accessors per Part C.4.

## Part F — Test coverage requirement (this is what was missing)

Add `tests/macros_test.c` to `CMakeLists.txt`'s `tests` target. One test
per macro, minimum assertions:
1. It compiles (the two broken macros would have failed at this step
   alone).
2. It has the documented effect (push then read back; insert then has;
   move nulls the source pointer; `_TRY_GET` returns 0 and leaves `out`
   provably untouched on a miss, per Part D's contract).
3. For `_CREATE_OF`/`WC_OPS`-based macros: a POD type (`int`) and an owned
   type (`String`) both get their own test, confirming the `_Generic`
   dispatch actually resolved to `NULL` vs. `&wc_str_ops` respectively —
   this is the test that would catch a future edit to `WC_OPS` silently
   breaking dispatch for one type while fixing another.

This file is the actual deliverable that prevents this plan from
regressing the way the original macro layer did — every other phase below
is gated on it existing first.

## Part G — Phased execution

**Phase 0 — `tests/macros_test.c` skeleton + register in `CMakeLists.txt`.**
Empty but building and running under `ctest`, so every subsequent phase
adds tests to a suite that's already wired into the sanitizer gate from the
core plan.

**Phase 1 — Bug fixes + core-API accessor additions.**
Fix `SET_INSERT_MOVE`/`DEQUEUE` (Part A). Add the `hashmap`/`hashset`
bucket-accessor functions needed by Part C.4 (shared work item with the
core plan). Land tests for both before anything else touches this file.

**Phase 2 — Delete the false distinctions, apply the naming grammar
(Part B).** `VEC_PUSH_COPY`/`SET_INSERT_COPY` removed. `Stack`/`Queue` get
their macro layers as thin aliases over `genVec`'s. `ENQUEUE`/`DEQUEUE`
become `QUEUE_PUSH`/`QUEUE_POP` (legacy-gated per core plan Phase 4).

**Phase 3 — Type-safety mechanisms (Part C).** Introduce `WC_OPS`,
`WC_ASSERT_ELEM_SIZE`, wrap every typed accessor macro across every module.
Rewrite `FOREACH`/`FROM_VEC` to use the new bucket accessors instead of
private fields.

**Phase 4 — Safe get split (Part D).** `MAP_TRY_GET` added,
`MAP_GET` redefined to assert-on-miss instead of returning garbage. Audit
every existing test/example that calls `MAP_GET` expecting silent-miss
behavior (there shouldn't be any real ones, since the old behavior was
undefined-in-practice, but check `tests/hashmap_test.c` and
`tests/complex_test.c` explicitly since they're the two files that already
touch macros).

**Phase 5 — Completeness pass (Part E).** Work the matrix top to bottom;
every ❌/⚠️ cell gets a macro + a Part F test in the same commit.

**Phase 6 — Re-derive the matrix from the actual header, confirm every
cell is ✅, ship alongside core-plan 0.8.0.**
