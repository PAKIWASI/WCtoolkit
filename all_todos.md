Here's every decision/TODO from all three plans (the core API plan and C11/GCC-Clang plan are both in `plan.md`; the macro layer plan is `macro_plan.md`), in one numbered list ordered roughly by benefit-per-effort — real bugs and cheap safety nets first, pure churn and performance tweaks last. Just reply with the numbers you want.

**Live bugs — fix now, cheap, no design decisions needed**
1. `genVec_copy` reads `dest`'s old fields via `genVec_destroy_stk(dest)` before they're valid — rewrite to build directly into `dest` like `string_copy` does.
2. `stack_peek`/`stack_peek_ptr` underflow `genVec_size(stk)-1` on an empty stack and crash instead of setting `WC_ERR_EMPTY`.
3. `SET_INSERT_MOVE` macro references `(vec)` instead of `(set)` — fails to compile or silently binds to an unrelated variable.
4. `DEQUEUE` macro uses `_tmp` instead of `__tmp` — same class of bug as #3.
5. `ARENA_SCRATCH`'s cleanup runs in a for-loop increment clause, which is skipped on early `return` — silent scratch-leak, no warning comment at all today.
6. `TEMP_CSTR_READ` has the identical footgun as #5 but is at least documented ("do NOT break/return/goto inside the block").
7. `MAP_GET` discards `hashmap_get`'s found/not-found result and returns uninitialized memory on a miss.

**Safety net — do before further changes land on top of these files**
8. Add `tests/macros_test.c` skeleton, wire into `CMakeLists.txt`/`ctest` (currently the macro layer has zero test coverage — how #3/#4 shipped).
9. Add one test per row of the `wc_errno` doc table, asserting the documented function doesn't crash and does set the right code (would've caught #2 immediately).
10. Turn on `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-align -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wundef`, triage findings, then `-Werror`.
11. Stop hardcoding `CMAKE_C_COMPILER` to clang; add a GCC+Clang CI matrix so "supports both" is actually tested.

**Correctness / contract fixes**
12. Kill the duplicate SSO-detection in `wc_helpers.h`'s `str_copy` — call `string_is_sso()` instead of re-deriving it from `capacity == STR_SSO_SIZE - 1`.
13. Audit `hashmap_copy`/`hashset_copy` against the "never read dest before writing it" rule (same class as #1).
14. Decide whether hashmap/hashset "key not found" should set `wc_errno` or stay a bare `b8` return, then apply that decision identically to both.
15. Audit `bitVec_pop` on an empty vector — decide/fix its behavior.
16. Audit `matrix_det`/`matrix_LU_Decomp` on singular/non-square input — decide/fix its behavior.
17. Add `// SAFE ON: raw/uninitialized dest` / `// REQUIRES: live, initialized dest` tags to every copy-shaped function.

**Coverage gaps**
18. Add `hashmap_create_stk`/`_destroy_stk` and `hashset_create_stk`/`_destroy_stk` (the latter already exists internally — just expose it).
19. Add public bucket-iteration accessors (`hashmap_bucket_count`, `_bucket_occupied`, `_bucket_key_ptr`, `_bucket_val_ptr`, + hashset equivalents) — needed to fix #34 below too.
20. Add const `hashmap_get_ptr`/hashset `_get_ptr` + `_get_ptr_mut` pairs.
21. Add `Queue` copy/move.
22. Add `Matrixf` move.
23. Document `NOT COPYABLE`/`HEAP-ONLY` at the type definition for `ChainArena`/`bitVec`/`StringStore` wherever you choose not to fill the gap.
24. Add a single `#define WC_NOT_FOUND ((u64)-1)` in `common.h`, replace the five independent literals; confirm `genVec_find` uses the same sentinel as `string_find_*`.

**Compile-time hardening**
25. Convert the `String`/`genVec` slot-size comment to `_Static_assert(sizeof(String) == sizeof(genVec), ...)`; audit hashmap/hashset/matrix sizing comments for the same treatment.
26. `__attribute__((nonnull))` on every function whose first lines are `CHECK_FATAL(!x, ...)`.
27. `__attribute__((warn_unused_result))` on the `_create`/`_init` family.
28. `__attribute__((malloc, alloc_size(N)))` on the allocator family.
29. Convert `FATAL`'s macro body to a real `wc_fatal_report()` function with `__attribute__((noreturn, format(printf, 4, 5)))`.

**Macro layer naming & completeness**
30. Standardize macro naming grammar (`<MODULE>_<VERB>[_<QUALIFIER>]`) across `VEC`/`STACK`/`QUEUE`/`MAP`/`SET` per the Part B table.
31. Delete the false `_COPY`-suffixed duplicate macros (`VEC_PUSH_COPY`, `SET_INSERT_COPY`).
32. Add Stack/Queue macro layers for the first time (mostly one-line aliases over `VEC_*`).
33. Generalize Map/Set typed-creation shorthand via `WC_OPS`/`_CREATE_OF` instead of hand-listing combos like `MAP_PUT_INT_STR`.
34. Rewrite `SET_FOREACH`/`MAP_FOREACH_KEY`/`_VAL`/`SET_FROM_VEC` to use the accessors from #19 instead of reaching into private struct fields.

**Type-safety macro mechanisms**
35. `WC_OPS(T)` — `_Generic`-based auto-derivation of the right `ops` struct from the element type (highest-leverage single item in the macro plan).
36. `WC_ASSERT_ELEM_SIZE` — debug-mode size-mismatch check wrapped around every typed accessor macro.
37. `_Static_assert` guards anywhere a macro takes two type tokens that must agree in size/alignment.
38. Document the C11+GNU-only toolchain restriction explicitly (`CONTRIBUTING.md` or header note) so `typeof`/`_Generic` sites don't get "fixed" into more-portable-but-worse C99 later.

**Renames — pure churn, mechanical, do last**
39. `genVec`→`GenVec`, `bitVec`→`BitVec`, `hashmap`→`HashMap`, `hashset`→`HashSet`, `strview`→`StrView`, `string_store`→`StringStore`.
40. `pcg32_random_t`→`Pcg32`.
41. `arena_release`→`arena_destroy`, `chain_arena_release`→`chain_arena_destroy`.
42. `enqueue`/`dequeue`→`queue_push`/`queue_pop` (gate old names behind `WCTOOLKIT_LEGACY_NAMES` if you have external callers).

**RAII ergonomics** (not important, maybe later)
43. `AutoString`/`AutoVec`/`AutoMap`/`AutoSet`/`AutoQueue`/`AutoArena` — `__attribute__((cleanup(...)))` wrappers as an opt-in second API tier.

**Performance — MUST
48. Cache the resolved `copy_fn`/`IS_POD` dispatch on the `genVec` struct at construction instead of re-branching on every push/pop/get.
49. Dedupe redundant checks across call chains — e.g. `stack_peek_ptr` calling two functions that each null-check the same pointer. Free, no tradeoff.
50. Merge multi-condition `CHECK_FATAL` checks into one branch on the hot path, splitting out which check failed only on the cold path. Free, no tradeoff.
51. Hoist bounds checks out of loops — validate once, then use `_unsafe` accessors for the iterations (also the fix for `VEC_FOREACH`).
52. `__attribute__((hot))`/`((cold))` on fast paths vs. `CHECK_FATAL` failure paths.
53. Add explicit `_unsafe` sibling accessors (e.g. `genVec_get_ptr_unsafe`) so hot-path callers can opt out of checks per call site. Real safety tradeoff, but scoped.
54. Make `CHECK_FATAL` respect `-DNDEBUG` and strip in Release builds. Real safety tradeoff, and it's blanket (whole build), not per-call-site like #53.
55. `matrix_xply_2` cache-line-sized tile blocking + actually reading the `-Rpass-missed=loop-vectorize` output Release builds already generate.

(Not a TODO, just confirmed correct: `GENVEC_GROWTH`/`STRING_GROWTH` at 1.5× — leave as is.)
