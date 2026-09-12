# WCtoolkit API Standardization Plan

Goal: one consistent contract for every module (`String`, `genVec`, `hashmap`,
`hashset`, `Stack`, `Queue`, `Arena`, `ChainArena`, `bitVec`, `Matrixf`,
`views`/`string_store`) — same naming shape, same copy/move/init rules, same
error-handling tier, same allocation-mode coverage. No module gets to be a
special case unless the doc says so *explicitly*, at the point of definition.

This is organized as seven rule sets (the actual standard), followed by a
per-module compliance matrix (what breaks each rule today) and a phased
execution plan.

---

## Rule 1 — Type naming

**Decision: `PascalCase` for struct/typedef names, always.**

Today: `String`, `Stack`, `Queue`, `Arena`, `ChainArena`, `Matrixf`,
`ArenaScratch` already comply. `genVec` and `bitVec` don't (camelCase).
`hashmap`, `hashset`, `strview`, `string_store` are all-lowercase.

- Rename `genVec` → `GenVec`, `bitVec` → `BitVec`, `hashmap` → `HashMap`,
  `hashset` → `HashSet`, `strview` → `StrView`, `string_store` → `StringStore`.
- `pcg32_random_t` → `Pcg32` (drop the C-library-style `_t` suffix; nothing
  else in the codebase uses it).

Rationale for picking PascalCase over the alternative (lowercase everywhere,
which would mean renaming `String`→`string` and colliding with the
`string_*` function prefix): PascalCase types + lowercase function prefixes
is already the majority pattern (`String`/`string_`, `Stack`/`stack_`,
`Arena`/`arena_`), so it's less churn and reads better against C's lack of
namespaces — you can `grep 'Stack '` for type usages and `grep 'stack_'`
for calls without one being a substring of the other.

## Rule 2 — Function naming

**Decision: `<lowercase_module_prefix>_<verb>`, always. No bare verbs.**

`enqueue`/`dequeue` are the only violation. Rename to `queue_push` /
`queue_pop`. (Keep `#define enqueue queue_push` etc. behind a
`WCTOOLKIT_LEGACY_NAMES` compile flag for one release if you have existing
callers — see Phase 4.)

**Decision: the verb for "free everything, including the struct itself if
heap-allocated" is always `destroy`.**

`arena_release` and `chain_arena_release` are the only violations (everything
else — `string_destroy`, `genVec_destroy`, `hashmap_destroy`,
`hashset_destroy`, `stack_destroy`, `queue_destroy`, `bitVec_destroy`,
`matrix_destroy` — already uses `destroy`). Rename `arena_release` →
`arena_destroy`, `chain_arena_release` → `chain_arena_destroy`.

**Decision: the verb for "free only the internals, not the struct" (i.e. the
struct lives on the caller's stack or is embedded in a parent struct) is
always `destroy_stk`.** Already consistent where it exists
(`string_destroy_stk`, `genVec_destroy_stk`, `queue_destroy_stk`,
`hashset_destroy_stk` — the last one just needs to be exposed publicly, see
Rule 5).

## Rule 3 — The copy/move/init contract (the big one)

**Decision: every `copy_fn`-shaped function and every public `_copy`
function must be safe to call with `dest` pointing at raw, uninitialized
memory.** No function is allowed to read `dest`'s old field values before
it has written valid ones. This is the one rule every other language with
constructors gets for free and C doesn't — so the toolkit enforces it by
convention, and the convention has exactly one clause: **write to dest
before you read from it.**

Concretely, that means the pattern in `string_copy` and in `wc_helpers.h`'s
`str_copy`/`vec_copy`/`str_copy_ptr`/`vec_copy_ptr` becomes the *only*
allowed pattern:

```c
void T_copy(T* dest, const T* src)
{
    // 1. write every field dest needs, unconditionally, first
    // 2. only THEN branch on src's state / allocate / memcpy payloads
    // never call T_destroy(dest) or T_destroy_stk(dest) at the top
}
```

`genVec_copy` is the only function in the library that violates this (it
opens with `genVec_destroy_stk(dest)`, which reads `dest->data`,
`dest->ops`, `dest->size` before they're known-valid). Fix:

- Rewrite `genVec_copy` to build directly into `dest` the same way
  `string_copy` does — `memcpy` the header fields from `src`, `malloc` a
  fresh `data` buffer, then per-element copy or bulk `memcpy`. Never touch
  `dest`'s old contents.
- Once `genVec_copy` is raw-memory-safe, delete `vec_copy`/`vec_copy_ptr`'s
  duplicate logic in `wc_helpers.h` and have them call `genVec_copy`
  directly. One implementation of "how do you copy a genVec," not two.
- Add a one-line contract comment directly above *every* function with this
  signature (not just in the header prose, which people skim past):
  `// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.`
  Apply this same comment (or the negative version, `// REQUIRES: live,
  initialized dest`) to `hashmap_copy` and `hashset_copy` too — audit them
  under the same rule (see Phase 2 checklist).

**Decision: kill the duplicate SSO-detection.** `str_copy` in
`wc_helpers.h` currently re-derives "is this string SSO?" from
`s->capacity == STR_SSO_SIZE - 1` instead of calling the canonical
`string_is_sso()`. Two independent encodings of one fact is exactly the
"invariant maintained by convention, not by a single choke point" problem
that caused the original SSO bugs. Replace the inline check with a call to
`string_is_sso()` (or export an internal `IS_SSO` macro from `String.h` for
other TUs to use safely) so there is exactly one place that knows how SSO
is detected.

**Decision: every owned type gets `_copy` and `_move`, no exceptions
without a documented reason.** See the compliance matrix (Rule 3 column)
for what's currently missing: `Queue`, `BitVec`, `Matrixf` (has `_copy`,
missing `_move`), `ChainArena`, `StringStore`. For types where copy/move
genuinely doesn't make sense (e.g. `StringStore` is meant to be
append-only/immutable, `ChainArena` backs a bump allocator with external
pointers into it that a copy would invalidate) — don't silently omit the
function. Add a one-line `// NOT COPYABLE: <reason>` comment at the type
definition so it's a documented decision, not a gap someone has to
rediscover by grepping.

## Rule 4 — Error-handling tier coverage

The two-tier model (`CHECK_FATAL` for programmer errors, `wc_errno` for
expected runtime conditions like "empty") is good and should stay. The
problem is it's only wired up for `genVec`/`Queue`/`Stack`/`Arena`, and even
within that set `stack_peek`/`stack_peek_ptr` don't actually follow it (see
the bug from earlier — they crash via `CHECK_FATAL` on an empty stack
instead of setting `WC_ERR_EMPTY`).

**Decision:**
1. Fix `stack_peek`/`stack_peek_ptr` to check emptiness with `WC_SET_RET`
   *before* touching `genVec_get`/`genVec_get_ptr`, mirroring
   `queue_peek`'s already-correct implementation.
2. Every container gets an explicit, audited answer to "what happens on
   pop/peek/front/back of an empty container" — and it must be
   `WC_ERR_EMPTY`, not a bounds-check crash. Audit list: `hashmap_get`/
   `hashmap_del` on missing key (currently returns `b8 0` with no
   `wc_errno` set — decide: is "key not found" a `wc_errno` condition or is
   the `b8` return sufficient? Pick one and apply it to `hashset_has`/
   `hashset_remove` identically), `bitVec_pop` on empty, `matrix_det`/
   `matrix_LU_Decomp` on a singular/non-square matrix (currently
   presumably `CHECK_FATAL` or undefined — needs a real look).
3. Update the `wc_errno.h` doc table (the "WHAT SETS wc_errno" list) to be
   the single source of truth, and add a test per row that asserts the
   exact function does *not* crash and *does* set the documented code —
   this is what would have caught the `stack_peek` bug immediately.

## Rule 5 — Allocation-mode coverage (`_create` vs `_create_stk`)

**Decision: every container type gets both a heap-struct constructor
(`_create`) and a struct-on-stack/embeddable constructor (`_create_stk` +
`_destroy_stk`), unless it's structurally impossible.**

`String`, `genVec`, `Queue`, `Arena`, `Matrixf` already have both.
`hashmap`/`hashset` only have `_create` (always heap-allocates the struct
itself) even though `hashset.c` already has an internal, unexposed
`hashset_destroy_stk` it uses for `hashset_copy`. `bitVec` and `ChainArena`
only have `_create`.

- Add `hashmap_create_stk` / `hashmap_destroy_stk` and
  `hashset_create_stk` / `hashset_destroy_stk` (make the existing internal
  function public) — this directly unblocks embedding a hashmap/hashset as
  an inline field of another owned type, the exact composition scenario
  that's currently awkward.
- For `bitVec` and `ChainArena`, either add the `_stk` pair or add an
  explicit `// HEAP-ONLY: <reason>` comment at the type definition (Rule 3's
  "document, don't omit silently" principle applies here too).

## Rule 6 — Const-correctness and return-value conventions

- **`_ptr` accessors**: every container that returns a pointer into its
  internals should offer both a `const`-returning read accessor and a
  mutable one, named `_get_ptr` / `_get_ptr_mut` (the pattern `genVec`
  already uses). `hashmap_get_ptr` only has the mutable form. Add
  `hashmap_get_ptr_mut` as an alias-or-rename of the current one and a new
  `const u8* hashmap_get_ptr(const hashmap* map, const u8* key)` for the
  read-only case. Apply the same pair to `hashset` (`hashset_get_ptr` /
  `_mut`) — currently it has neither, only `_has`.
- **"found/not found" returns**: standardize on `b8` (1 = found/existed, 0 =
  not) across `hashmap_put`, `hashmap_has`, `hashmap_del`, `hashset_insert`,
  `hashset_has`, `hashset_remove` — this is already consistent, keep it.
  The internal `LOOKUP_RES` enum (`NOT_FOUND`/`FOUND`/`ROBINHOOD_EXIT`) in
  `map_setup.h` stays internal-only; don't leak it into the public API.
- **"not found" sentinels for indices**: `string_find_char`/
  `string_find_cstr` return `(u64)-1`. Confirm `genVec_find` does the same
  (audit item) and document the sentinel value once, in `common.h`, as
  `#define WC_NOT_FOUND ((u64)-1)`, then use that macro everywhere instead
  of five independent `(u64)-1` literals.

## Rule 7 — Documentation contract format

Every function whose behavior depends on one of the above rules gets a
one-line tag directly above its declaration, not just prose in the header
banner (banners get skimmed past; a tag on the function you're about to
call doesn't):

```
// SAFE ON: raw/uninitialized dest
// REQUIRES: live, initialized dest
// NOT COPYABLE: owns external pointers into the arena
// HEAP-ONLY: struct contains a scratch buffer sized at creation
```

---

## Compliance matrix (current state)

| Module | Type naming | destroy verb | copy | move | `_stk` create | wc_errno wired | notes |
|---|---|---|---|---|---|---|---|
| String | ✅ PascalCase | ✅ | ✅ safe-on-raw | ✅ | ✅ | n/a | reference implementation |
| genVec | ❌ camelCase | ✅ | ❌ needs live dest | ✅ | ✅ | ✅ | fix copy contract (Rule 3) |
| hashmap | ❌ lowercase | ✅ | ⚠️ audit | ❌ missing | ❌ missing | ❌ not found = no errno | |
| hashset | ❌ lowercase | ✅ | ⚠️ audit | ❌ missing | ⚠️ internal only | ❌ | expose `_stk` |
| Stack | ✅ | ✅ | (inherits genVec) | (inherits genVec) | (inherits genVec) | ⚠️ peek broken | fix `stack_peek` bug |
| Queue | ✅ | ✅ | ❌ missing | ❌ missing | ✅ | ✅ | add copy/move |
| Arena | ✅ | ❌ `_release` | n/a | n/a | ✅ | ✅ | rename to `_destroy` |
| ChainArena | ✅ | ❌ `_release` | ❌ | ❌ | ❌ | ❌ | rename + document NOT COPYABLE |
| bitVec | ❌ camelCase | ✅ | ❌ missing | ❌ missing | ❌ missing | ⚠️ audit pop | |
| Matrixf | ✅ | ✅ | ✅ | ❌ missing | ✅ | ⚠️ audit LU/det | add move |
| StrView/StringStore | ❌ lowercase | ✅ | n/a (view) | n/a | n/a | n/a | document immutability |
| naming: `enqueue`/`dequeue` | — | — | — | — | — | — | rename to `queue_push`/`queue_pop` |

---

## Execution plan

**Phase 0 — Lock the regression gate.**
The 369-test ASan/UBSan suite already passes cleanly; that's the safety net
for everything below. Before touching anything, add one test per row of the
`wc_errno` audit (Rule 4.3) so the `stack_peek` class of bug is caught by
CI, not by someone hitting it in the field. Every subsequent phase must
keep `ctest`/the raw test binary green under sanitizers.

**Phase 1 — Fix the one real bug.**
`stack_peek`/`stack_peek_ptr` (Rule 4.1). This is a live crash, ship it
alone if you want a fast win before the larger rename lands.

**Phase 2 — Copy/move contract audit and fix (Rule 3).**
Rewrite `genVec_copy` to be raw-memory-safe; collapse `wc_helpers.h`'s
`vec_copy`/`vec_copy_ptr` to call it; remove the duplicate SSO check in
`str_copy`; audit `hashmap_copy`/`hashset_copy` against the same rule and
fix or document. Add the `SAFE ON` / `REQUIRES` tags everywhere (Rule 7).
This phase has the highest bug-prevention value and should land before the
naming pass, since it changes behavior, not just spelling.

**Phase 3 — Fill the coverage gaps (Rules 3, 5, 6).**
Add `Queue` copy/move, `Matrixf` move, `hashmap`/`hashset` `_stk` variants,
`hashmap`/`hashset` const `_get_ptr` pairs. Decide and document
`NOT COPYABLE`/`HEAP-ONLY` for `ChainArena`, `bitVec`, `StringStore` if you
choose not to fill them in.

**Phase 4 — Renames (Rules 1, 2).**
This is pure churn with no behavior change, so do it last and do it as a
single mechanical pass with `sed`/an AST tool, not by hand:
`genVec`→`GenVec`, `bitVec`→`BitVec`, `hashmap`→`HashMap`,
`hashset`→`HashSet`, `strview`→`StrView`, `string_store`→`StringStore`,
`arena_release`→`arena_destroy`, `chain_arena_release`→
`chain_arena_destroy`, `enqueue`/`dequeue`→`queue_push`/`queue_pop`. Grep
the whole tree (`src/`, `include/`, `tests/`) for each old identifier after
the rename to make sure nothing survives in a comment or string literal
that would mislead a reader. If external callers exist, gate the old names
behind `#ifdef WCTOOLKIT_LEGACY_NAMES` macro aliases for one deprecation
cycle instead of a hard break.

**Phase 5 — Enforcement, so it doesn't drift back.**
Write a small `check_conventions.sh` (grep-based is fine — this doesn't
need to be clever) that runs in CI and fails on: a public function name not
matching `^[a-z_]+_[a-z_]+$` for a known module prefix; a `copy_fn`-shaped
function with no `SAFE ON`/`REQUIRES` tag on the line above it; a type
definition with no `_destroy`-suffixed free function findable in the same
header. This is what turns "we fixed it once" into "it stays fixed" — the
exact gap that let the SSO invariant and the `genVec_copy` contract drift
in the first place.

**Phase 6 — Re-run the full audit.**
Once all phases land, re-derive the compliance matrix above from the
actual code (not from memory) and confirm every cell is ✅ or has an
explicit documented exception. Ship 0.8.0.



# WCtoolkit — C11 + GCC/Clang-Only Improvement Plan

Companion to `wctoolkit-api-standardization-plan.md` (core API) and
`wctoolkit-macro-layer-standardization-plan.md` (`wc_helpers`/`wc_macros`).
Those two plans make the existing surface consistent; this plan is about
what becomes *available* once the toolchain is locked to C11 with GCC and
Clang as the only supported compilers — no MSVC, no strict-ISO-C11-only
builds. The project already depends on GNU extensions informally
(`typeof`, statement expressions, `__uint128_t` in `wyhash`,
`__builtin_expect` in `common.h`); this plan is about doing that on
purpose, completely, instead of by accretion.

**Relationship to the other two plans:** items 3 and 4 below apply
`__attribute__`/`_Static_assert` annotations to functions and macros that
those plans are also touching (`CHECK_FATAL`, `string_copy`/`genVec_copy`,
the `_create`/`_init` family, the new `WC_OPS`/`_CREATE_OF` macros). Do
this plan's Phase 1 (item 3's `nonnull`/`warn_unused_result` pass) *after*
the core plan's Phase 2 (copy/move contract fixes) lands, so the attributes
get attached to the corrected function bodies, not the ones with the
`genVec_copy` raw-memory bug. Item 5 (`_Generic` as the documented
approach) should be written into the macro-layer plan's Part C directly
rather than duplicated — see the note at the end of this doc.

---

## Item 1 — `__attribute__((cleanup))`: opt-in RAII, and it retires two
existing footguns

Both compilers implement `cleanup` identically. Add a second, parallel API
tier — not a replacement for `_destroy`/`_destroy_stk`, which stay as the
manual/explicit primitives — of scope-bound owning pointers:

```c
// wc_raii.h (new file)

static inline void wc_autofree_string(String** s)  { if (*s) string_destroy(*s); }
static inline void wc_autofree_vec(genVec** v)      { if (*v) genVec_destroy(v); }
static inline void wc_autofree_map(hashmap** m)     { if (*m) hashmap_destroy(*m); }
static inline void wc_autofree_set(hashset** s)     { if (*s) hashset_destroy(*s); }
static inline void wc_autofree_queue(Queue** q)     { if (*q) queue_destroy(*q); }
static inline void wc_autofree_arena(Arena** a)     { if (*a) arena_destroy(*a); }   // post core-plan rename

#define AutoString __attribute__((cleanup(wc_autofree_string))) String*
#define AutoVec    __attribute__((cleanup(wc_autofree_vec)))    genVec*
#define AutoMap    __attribute__((cleanup(wc_autofree_map)))    hashmap*
#define AutoSet    __attribute__((cleanup(wc_autofree_set)))    hashset*
#define AutoQueue  __attribute__((cleanup(wc_autofree_queue)))  Queue*
#define AutoArena  __attribute__((cleanup(wc_autofree_arena)))  Arena*
```

Usage:
```c
void foo(void) {
    AutoString s = string_from_cstr("hi");
    if (some_error) return;    // string_destroy runs automatically
    ...
}                                // and again here, on any exit path
```
`Stack` needs no separate `wc_autofree_stack` — it's `genVec` underneath,
`AutoVec` already covers it. `ChainArena`/`bitVec`/`Matrixf` get the same
treatment once the core plan's Phase 3 coverage-gap work confirms which of
them are actually heap-owning (Matrixf's heap variant, `matrix_create`,
qualifies; `matrix_create_stk` doesn't need one, same as `String`'s
`_stk` variants don't).

**This isn't just a nice-to-have — it fixes two real, currently
undocumented-or-under-documented bugs:**

- `TEMP_CSTR_READ` (`String.h`) is a for-loop trick that runs its cleanup
  (`string_pop_char`) in the loop's increment clause, which is *skipped* on
  `return`/`break`/`goto` out of the block — the header says so explicitly:
  *"Note: Do NOT break/return/goto inside the block."* That's a footgun
  documented into permanence rather than fixed. Rewritten on `cleanup`, the
  temporary NUL-termination is undone on every exit path, no exceptions,
  and the warning comment can be deleted because the failure mode it
  described no longer exists.
- `ARENA_SCRATCH` (`arena.h`) is the *identical* trick — cleanup runs in
  the for-loop increment — used for `arena_scratch_end()`. It has **no
  warning comment at all**, meaning `return` inside an `ARENA_SCRATCH`
  block today silently leaks the scratch mark with zero indication to the
  caller that anything went wrong. This is strictly worse than
  `TEMP_CSTR_READ`'s situation. Rebuilding it on `cleanup` fixes a live,
  silent bug, not a hypothetical one. Do this one first.

Ship `AutoArena`/`ARENA_SCRATCH`'s replacement in the very first commit of
this plan, ahead of the rest of item 1 — it's the highest-value single
change here.

## Item 2 — Turn on warnings the project isn't using yet

`CMakeLists.txt` currently sets sanitizer + optimization flags in
`CMAKE_C_FLAGS_DEBUG`/`_RELEASE` but no `-Wall`/`-Wextra`/`-Wpedantic`
anywhere. Also worth noting while in this file: `CMAKE_C_COMPILER` is
hardcoded to `clang` — meaning "GCC and Clang" is currently aspirational,
the build has never actually been exercised under GCC. Fix both:

```cmake
set(WC_WARN_FLAGS
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wcast-align -Wnull-dereference -Wdouble-promotion -Wformat=2
    -Wimplicit-fallthrough -Wundef -Werror
)
target_compile_options(main  PRIVATE ${WC_WARN_FLAGS})
target_compile_options(tests PRIVATE ${WC_WARN_FLAGS})
```
Stop hardcoding the compiler; make it a cache variable defaulting to
whichever is found, and add a CI matrix that runs *both*:
```cmake
if(NOT CMAKE_C_COMPILER)
  find_program(CMAKE_C_COMPILER NAMES clang gcc)
endif()
```
Expect this pass to surface real findings in `hashmap.c`'s bucket-index
arithmetic and `matrix.c`'s loop indices — anywhere a `u64` capacity gets
narrowed toward a `u32`-sized field or a `float` gets implicitly promoted —
exactly where `-Wconversion`/`-Wsign-conversion`/`-Wdouble-promotion` earn
their keep. Budget a full pass to triage and either fix or
`(void)`-cast-with-comment every warning `-Werror` turns up; don't turn on
`-Werror` until that triage is done, or the first CI run blocks on
unrelated pre-existing warnings instead of catching new ones.

Add compiler-specific static analysis as separate, non-blocking CI lanes,
since the compiler set is now known and fixed:
- **GCC** `-fanalyzer` (GCC 10+): its interprocedural malloc/free and
  use-after-free dataflow analysis is a good second opinion alongside ASan
  — it can flag some things statically (across all paths) that ASan only
  catches on the specific path a test happens to exercise.
- **Clang** `scan-build` / `clang-tidy` with `clang-analyzer-*` and
  `bugprone-*` checks enabled.

## Item 3 — Attribute-annotate the contracts already enforced at runtime

The library already enforces these contracts via `CHECK_FATAL` at the top
of nearly every function. Both compilers let you also state them as
compiler-visible attributes, which (a) gets you a compile-time diagnostic
at obviously-wrong call sites and (b) gives the analyzers from Item 2
sharper information, reducing false positives in exactly the places
they'd otherwise get noisy.

- **`__attribute__((nonnull))`** on every function whose first lines are
  `CHECK_FATAL(!x, "x is null")` — `string_copy`, `genVec_push`,
  `hashmap_put`, etc. (essentially the whole public API). This is a
  mechanical, header-wide pass: for each function, look at which
  `CHECK_FATAL(!param, ...)` checks exist, mark those parameter positions.
- **`__attribute__((warn_unused_result))`** on every `_create`/`_init`
  family function (`string_create`, `genVec_init`, `hashmap_create`,
  `arena_alloc`, `arena_alloc_aligned`, `chain_arena_alloc[_aligned]`) —
  catches "allocated it, dropped the pointer" at compile time for free.
- **`__attribute__((malloc, alloc_size(N)))`** on the same allocator
  family, plus `arena_alloc`/`arena_alloc_aligned` specifically — improves
  both compilers' aliasing assumptions and gives the static analyzers
  better information about freshly allocated, non-aliasing memory.
- **`__attribute__((format(printf, N, M)))`** on the underlying function
  behind `WARN`/`FATAL`/`LOG` in `common.h`. These are currently macros
  that inline a raw `fprintf`/`printf` call — meaning a mismatched format
  specifier against a `u64`/`float`/etc. argument at any of the hundreds of
  `CHECK_FATAL`/`CHECK_WARN` call sites is silently wrong today. Converting
  `FATAL`'s body to a real function —
  `void wc_fatal_report(const char* file, int line, const char* func, const char* fmt, ...) __attribute__((noreturn, format(printf, 4, 5)));`
  — with the macro reduced to `wc_fatal_report(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)`
  — gets every existing call site compiler-checked with no call-site
  changes required.
- **`__attribute__((noreturn))`** on that same `wc_fatal_report` function.
  Beyond the code-size win (the `exit()` call and message-printing logic
  stop being duplicated inline at every `CHECK_FATAL` site), this lets both
  compilers' flow analysis know that any code path past a `CHECK_FATAL`
  guard can assume the checked condition false — e.g. after
  `CHECK_FATAL(!ptr, ...)`, `ptr` is known non-null for the rest of the
  function. That's exactly the kind of fact `-fanalyzer`/Clang's analyzer
  use to both suppress false positives and sharpen true ones.

Sequencing note: do this pass *after* the core plan's copy/move-contract
fixes (Phase 2) land, so `nonnull`/`warn_unused_result` get attached to the
corrected `genVec_copy` etc., not the pre-fix version — attributes on a
buggy function just make the compiler more confident about wrong behavior.

## Item 4 — `_Static_assert` the invariants currently living only in
comments

`String.h` has a load-bearing fact stated as a comment, not a check:
```c
// 24 8 8 = 40 bytes (same as genVec)
```
This almost certainly matters for slot-size compatibility somewhere in the
by-value helper functions (`wc_helpers.h`'s `vec_copy` treating a `genVec`
slot generically). Nothing currently stops it from silently going stale the
next time a field is added to either struct. Convert it:
```c
_Static_assert(sizeof(String) == sizeof(genVec),
               "String/genVec slot-size invariant broken — check wc_helpers.h by-value copy paths");
```
Audit for others in the same category — the hashmap/hashset scratch-buffer
sizing comments (`key_size + val_size bytes + alignment`, `2 * elm_size`)
and the matrix arena-allocation macros are the other likely spots — and
convert every one found from "a human has to remember to check this" into
"the build fails if this breaks." This is a single, mechanical grep-and-fix
pass; do it as part of the same PR as Item 3's attribute pass since both
are "make an implicit contract explicit and compiler-checked" work.

## Item 5 — `_Generic`/`typeof`: stop treating them as a workaround

The macro-layer plan already specifies `WC_OPS(T)` built on `_Generic` as
its centerpiece type-safety mechanism (Part C.1 of that plan). With the
compiler restriction now explicit and locked in, the framing changes: this
isn't "using a GNU/C11 extension because we have no better option," it's
"the documented, intended way to write this API, because we've committed
to a toolchain that supports it well." Concretely:
- Add a short note to the top of `wc_macros.h` and `wc_helpers.h` (or a new
  `CONTRIBUTING.md`) stating the toolchain restriction explicitly, so a
  future contributor doesn't "fix" a `typeof`/`_Generic`/statement-expression
  site into more-portable-but-more-verbose C99, reintroducing the
  boilerplate those constructs exist to remove.
- No separate implementation work here beyond what the macro-layer plan
  already specifies — this item exists to record the toolchain-restriction
  rationale in one place rather than have it implicit in file-by-file usage.

## Item 6 — Sanitizer/fuzz matrix, now that the toolchain is fixed and known

Currently: Debug builds run ASan+UBSan, but only under whichever compiler
`CMakeLists.txt` happens to hardcode (see Item 2 — presently always
`clang`, GCC untested). Once both are officially and permanently the
supported set:

- **CI matrix**: GCC-Debug-ASan/UBSan and Clang-Debug-ASan/UBSan as two
  separate, both-required jobs. This is what actually delivers on "we
  support GCC and Clang" instead of asserting it.
- **libFuzzer harnesses (Clang-only feature — this plan is what makes it
  reasonable to add)**: `-fsanitize=fuzzer` targets for the API surfaces
  that take the most adversarial-shaped input:
  - `hashmap`/`hashset` insert+lookup under adversarial key byte patterns
    (robin-hood hashing has known worst-case-clustering adversarial inputs
    — good fuzz target for both correctness and the DoS-shaped performance
    cliff).
  - `arena_alloc_aligned` under adversarial size/alignment pairs.
  - `string_insert_range`/`string_remove_range`/`genVec_insert_multi` etc.
    under adversarial index/length pairs — these are exactly the "off by
    one in bounds math" functions where the SSO-flag and `add_node()`
    bugs from earlier in this project's history originated.
  Wire these as a third, nightly (not per-PR — fuzzing is slow) CI lane.

---

## Sequencing against the other two plans

1. Core plan Phase 0 (regression-gate lock-in) — unchanged, first.
2. Core plan Phase 1 (`stack_peek` bug fix) — unchanged.
3. Core plan Phase 2 (copy/move contract fixes).
4. **This plan, Item 1 first half** (`AutoArena`/`ARENA_SCRATCH` rebuild —
   fixes the silent scratch-leak bug; independent of the copy/move work,
   can run in parallel with step 3).
5. **This plan, Item 2** (warnings + compiler matrix) — do this early
   enough that its findings inform, rather than fight with, the core plan's
   remaining phases; a `-Wconversion` finding inside `hashmap.c` might be
   easier to fix at the same time as that file's other Phase-3 coverage
   work.
6. Core plan Phase 3 (coverage gaps) + macro-layer plan Phases 1–2, then
   **this plan Item 3** (attribute pass) once those land — attaching
   `nonnull`/`warn_unused_result` to finished, not in-flight, signatures.
7. **This plan Item 4** (`_Static_assert` pass) alongside Item 3 — same
   commit category.
8. Macro-layer plan Phase 3 (the `WC_OPS`/`_Generic` work) folds in
   **this plan's Item 5** note directly.
9. **This plan, Item 1 remainder** (`AutoString`/`AutoVec`/etc. for every
   owning type) — additive, can land any time after the types it wraps are
   stable, no hard dependency on anything else.
10. **This plan Item 6** (CI matrix + fuzz harnesses) last — it's the
    thing that *verifies* everything above, so it should be watching the
    most-complete version of the codebase, not gating early phases.



Smaller, real wins
Resolve the dispatch strategy once per container, not once per call. IS_POD(vec) and ops->copy_fn ? … : … get re-checked on every single push/pop/get, even though vec->ops never changes after genVec_init. Cache a resolved function pointer (or a small enum) on the genVec struct at construction time instead of re-branching every call — turns a data-dependent branch (mispredictable if you interleave POD and CX vectors in a loop) into a single indirect call resolved once. Small, but free.
__attribute__((hot)) / __attribute__((cold)) on the push/pop/get fast paths vs. the CHECK_FATAL failure paths respectively — ties into the earlier GCC/Clang-only plan's attribute work (item 3), and helps the compiler's code layout keep the hot path branch-predictor- and icache-friendly.
Growth factors are already good — don't touch them. GENVEC_GROWTH/STRING_GROWTH are both 1.5F, which is the well-established better-than-2x choice (leaves more reusable freed memory behind on realloc, matches folly/Facebook's finding). No action item here, just confirming it's not a regression risk if someone "optimizes" it back to 2x later.
Robin Hood hashmap swap loop — the actual byte-shuffling (memcpy into scratch) is already about as good as it gets for POD; the CX slowdown is the same indirect-copy_fn story as genVec, so it inherits whatever fix you pick above rather than needing a separate one.
matrix_xply_2's transpose-then-multiply is already a reasonable cache-locality strategy; if you want to push further, explicit cache-line-sized tile blocking (parameterize the block size off L1_CACHE_LINE/measured L1 size rather than the current fixed blocking) plus checking the -Rpass-missed=loop-vectorize output you're already generating in Release builds (it's in the flags but nothing currently reads the output) would tell you exactly which loops the compiler is failing to vectorize and why — that diagnostic is already being produced, just not acted on.



Same principle as before — you already have the right pattern in one place and just haven't applied it everywhere. `String.h` has `string_char_at` (checked) and `string_char_at_unsafe` (not) sitting right next to each other. That's the model: **checked-by-default, unsafe-opt-in for hot paths** — not "remove the checks," which trades a controlled crash for silent memory corruption on the very inputs the checks exist to catch.

## 1. Make `CHECK_FATAL` actually respect the `-DNDEBUG` you're already passing

I checked — `CMakeLists.txt`'s Release flags include `-DNDEBUG`, but `common.h`'s `CHECK_FATAL` never looks at that macro:
```c
#define CHECK_FATAL(cond, fmt, ...)                           \
    do {                                                      \
        if (__builtin_expect(!!(cond), 0)) {                  \
            FATAL(...);                                       \
        }                                                     \
    } while (0)
```
So `-DNDEBUG` currently does nothing for you — every `CHECK_FATAL` fires at full runtime cost in Release, exactly the same as Debug. Standard C's own `assert()` strips itself under `NDEBUG` for precisely this reason: preconditions that "should never fire in a correct program" are a debugging aid, not a runtime feature, and the convention exists so you can compile them out once you trust the caller.

This maps cleanly onto the two-tier model `wc_errno.h` already describes: `CHECK_FATAL` is explicitly the *programmer-error* tier (null pointer, out-of-bounds — "these are bugs, not conditions"), while `wc_errno` is the *expected-condition* tier (pop on empty). Only the first tier is a candidate for stripping — `wc_errno` checks are real control flow and must stay unconditional. Concretely:
```c
#ifdef NDEBUG
    #define CHECK_FATAL(cond, fmt, ...) ((void)0)
#else
    #define CHECK_FATAL(cond, fmt, ...) /* existing body */
#endif
```
This alone removes every null-check and bounds-check branch from Release builds, for free, with a flag you're already setting.

## 2. Give the hot-path functions an explicit `_unsafe` sibling instead of relying only on the global flag

Blanket-stripping via `NDEBUG` is all-or-nothing per build — fine for a "trust me, ship it" Release build, but it means you can't selectively skip checks in one hot loop while keeping them everywhere else in a Debug build where you're actively hunting a different bug. The `_unsafe` pattern gives you that per-call-site control:
```c
static inline const u8* genVec_get_ptr_unsafe(const genVec* vec, u64 i)
{
    return GET_PTR(vec, i);
}
```
Apply this next to every function whose checked version is currently the *only* version — `genVec_get`/`_get_ptr`/`_get_ptr_mut`, `hashmap_get_ptr`, `string_char_at` already has it. The caller decides, per call site, whether they've already proven the bounds are good (see #4 below) and can afford to skip re-checking.

## 3. Deduplicate checks across call chains — this is free, no tradeoff at all

This one's pure waste, not a safety/speed tradeoff. `stack_peek_ptr` calls:
```c
genVec_get_ptr(stk, genVec_size(stk) - 1)
```
`genVec_size(stk)` does `CHECK_FATAL(!vec, "vec is null")`. Then `genVec_get_ptr` immediately does `CHECK_FATAL(!vec, "vec is null")` again — the exact same pointer, checked twice, in one logical operation. That's two branches for one fact. Grep the codebase for this shape (a public function calling another public function that re-validates a pointer/argument the caller already validated) — anywhere `Stack`/`Queue`'s thin wrappers call into `genVec`, or the `wc_macros.h` typed macros call into functions that then call *other* functions, is a candidate. Fix: add `_impl`/internal variants that skip the check, and have the redundant outer call sites use those instead of the public checked entry points.

## 4. Hoist checks out of loops — validate once, iterate unsafely

If you're doing `for (i = 0; i < n; i++) genVec_get(vec, i, &out)` in a loop, that's `n` bounds checks for a bound that's constant for the whole loop. Validate `i < n <= vec->size` once before the loop, then use `genVec_get_ptr_unsafe` (per #2) for every iteration inside it. This is strictly better than either extreme — you keep exactly one real check instead of zero, but pay for it once instead of N times. This is also the natural fix for the `VEC_FOREACH` macro from the macro-layer plan: it can validate the vector's bounds once at loop setup and use the unsafe accessor internally, since the loop's own index variable is already provably in range by construction.

## 5. Merge multi-condition checks into one branch on the hot path

Functions like `genVec_get` currently do three separate `CHECK_FATAL`s:
```c
CHECK_FATAL(!vec, "vec is null");
CHECK_FATAL(!out, "out is null");
CHECK_FATAL(i >= vec->size, "index out of bounds");
```
That's three branches (each already `__builtin_expect`-hinted, so individually cheap — but not free) in the common valid-input case. Since these are all "unrecoverable, we're about to abort anyway" checks, you can combine them into a single branch and only figure out *which* one failed on the cold, about-to-crash-anyway path where the extra work costs nothing:
```c
if (__builtin_expect(!vec || !out || i >= (vec ? vec->size : 0), 0)) {
    // cold path: re-check individually just to produce the right message
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!out, "out is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
}
```
One branch instead of three on every call, for identical behavior on failure.

## The tradeoff, stated plainly

Items 1 and 2 genuinely remove safety, not just overhead — a bounds check catches the exact class of out-of-bounds write that turns into a CVE. The reason the checked/unsafe split (rather than a blanket "turn checks off") is the right default: it puts the decision to skip validation at the one call site where a human has actually reasoned about why it's safe, instead of at the build-flag level where it silently applies to every call in the program, including ones nobody audited. Items 3–5 have no such tradeoff — they're pure waste, safe to fix unconditionally, and worth doing first since they cost nothing.




