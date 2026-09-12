# WCtoolkit — Task Tracker

> RECOVERY CHECKPOINT (see CHECKPOINT.md): build is GREEN on gcc under -Werror, 378/378
> tests pass. This was pre-Phase-5 crash recovery + gcc-clean work, not tracker items.
> Outstanding: ASan reports ~86 fixture leaks in hashset/hashmap tests (pre-existing).
> 9-6 now closeable on gcc; Clang unverified this session. Phase 5 not started.


## Phase 0 — Regression gate
- [x] 0-A: Add `wc_errno` empty-stack tests to `stack_queue_test.c`
- [x] 0-A: Add `wc_errno` empty-vec tests to `gen_vector_test.c`
- [x] 0-A: Add arena exhaustion test to `arena_test.c`
- [x] 0-B: Create `tests/macros_test.c` skeleton
- [x] 0-B: Wire `macros_test.c` into `CMakeLists.txt`

## Phase 1 — Live bug fixes
- [x] 1-A: Fix `stack_peek` / `stack_peek_ptr` underflow (`Stack.c`)
- [x] 1-B: Fix `SET_INSERT_MOVE` `(vec)` → `(set)` (`wc_macros.h`)
- [x] 1-C: Fix `DEQUEUE` `_tmp` → `__tmp` (`wc_macros.h`)
- [x] 1-D: Add `macros_test.c` tests for `SET_INSERT_MOVE` and `DEQUEUE`

## Phase 2 — Copy/move contract
- [x] 2-A: Rewrite `genVec_copy` (no `destroy_stk` on dest, build directly)
- [x] 2-A: Add `SAFE ON` comment above `genVec_copy` in `gen_vector.h`
- [x] 2-B: Collapse `vec_copy` / `vec_copy_ptr` in `wc_helpers.h` to delegate to `genVec_copy`
- [x] 2-C: Fix `str_copy` SSO detection → `string_is_sso()`
- [x] 2-D: Audit `hashmap_copy` / `hashset_copy` against Rule 3; fix if needed
- [x] 2-E: Add `SAFE ON` / `REQUIRES` tags to all copy-shaped declarations
- [x] 2-F: Add `genVec_copy` on raw-dest test to `gen_vector_test.c`

## Phase 3 — API coverage gaps
- [x] 3-A: Expose `hashset_destroy_stk` publicly
- [x] 3-A: Add `hashmap_create_stk` / `hashmap_destroy_stk`
- [x] 3-B: Add `hashmap_bucket_count/occupied/key_ptr/val_ptr`
- [x] 3-B: Add `hashset_bucket_count/occupied/elm_ptr`
- [x] 3-C: Add `hashmap_get_ptr` (const) / rename current to `hashmap_get_ptr_mut`
- [x] 3-C: Add `hashset_get_ptr` / `hashset_get_ptr_mut`
- [x] 3-D: Add `queue_copy` / `queue_move`
- [x] 3-E: Add `matrix_move`
- [x] 3-F: Audit `bitVec_pop` on empty; decide/fix behavior
- [x] 3-F: Audit `matrix_det` / `matrix_LU_Decomp` on bad input; decide/fix
- [x] 3-G: Add `NOT COPYABLE` comment to `chain_arena.h`
- [x] 3-G: Add `HEAP-ONLY` comment (or `_stk`) to `bit_vector.h`
- [x] 3-G: Add `NOT COPYABLE` comment to `views.h`
- [x] 3-H: Add `WC_NOT_FOUND` to `common.h`; replace `(u64)-1` literals

## Phase 4 — Macro layer
- [x] 4-A: Fix `MAP_GET` (assert-on-miss); add `MAP_TRY_GET`
- [x] 4-B: Delete `VEC_PUSH_COPY` and `SET_INSERT_COPY`
- [x] 4-C: Rename `ENQUEUE`/`DEQUEUE` macros → `QUEUE_PUSH`/`QUEUE_POP`; update call sites
- [x] 4-D: Rename `enqueue`/`dequeue` functions → `queue_push`/`queue_pop`; grep sweep
- [x] 4-E: Add `STACK_CREATE/PUSH/PUSH_MOVE/POP/AT/FOREACH` macro layer
- [x] 4-F: Add `QUEUE_CREATE/PUSH_MOVE/PUSH_CSTR/AT/FOREACH` macro layer
- [x] 4-G: Rewrite `MAP_FOREACH_KEY/VAL`, `SET_FOREACH`, `SET_FROM_VEC` to use bucket accessors
- [x] 4-H: Rebuild `ARENA_SCRATCH` with `__attribute__((cleanup))`
- [x] 4-H: Rebuild `TEMP_CSTR_READ` with `__attribute__((cleanup))`
- [x] 4-I: Add `macros_test.c` tests for all new/modified macros

if we are using __attribute__(nonnull), then drop the CHECK_FATAL validating non nulls

## Phase 5 — In-place renames

> Pulled ahead of the hardening phase: renames are mechanical churn with no behavioral risk, and
> doing them first lets the attribute / static-assert work in Phase 6 land once on final names.
> VEC_* / MAP_* / pcg32_* prefixes are untouched by this phase.

- [ ] 5-A: Rename genVec -> GenVec everywhere
- [ ] 5-B: Rename bitVec -> BitVec everywhere
- [ ] 5-C: Rename hashmap -> HashMap everywhere
- [ ] 5-D: Rename hashset -> HashSet everywhere
- [ ] 5-E: Rename strview -> StrView, string_store -> StringStore, pcg32_random_t -> Pcg32
- [ ] 5-F: Rename release verbs: arena_release -> arena_destroy, chain_arena_release -> chain_arena_destroy
- [ ] 5-G: Regenerate all *_single.h via tools/make_single_header.py; confirm no old names remain
- [ ] 5-H: Post-rename grep audit - no survivors in comments/strings/README/plan docs

## Phase 6 — Compile-time hardening + performance

> 6-A / 6-B already landed in the working tree (common.h). Remaining items run on the renamed API.

- [x] 6-A: CHECK_FATAL respects NDEBUG - DONE (include/common.h:68)
- [x] 6-B: FATAL -> wc_fatal_report with noreturn + format - DONE (include/common.h:39,51)
- [x] 6-C: nonnull sweep across all public headers (genVec/hashmap/String/Stack/Queue/hashset/arena/chain_arena/bitVec/matrix/views = 200 attrs) — nullable ops/callbacks/optional-out audited & left unmarked; fast_math/random have no pointer params. VERIFY build
- [ ] 6-D: warn_unused_result on _create/_init/_alloc family - audit ignored returns
- [ ] 6-E: malloc/alloc_size on allocator family - verify alloc_size(N) arg indices
- [ ] 6-F: _Static_assert(sizeof(String)==sizeof(genVec)) + sizing audit - verify sizes first (assert is a target)
- [ ] 6-G: Add genVec_get_ptr_unsafe / genVec_get_ptr_mut_unsafe
- [ ] 6-H: Add WC_ASSERT_ELEM_SIZE; wrap VEC_AT / AT_MUT / FRONT / BACK / POP / FOREACH
- [ ] 6-I: Add WC_OPS via _Generic; add VEC_CREATE_OF / MAP_CREATE_OF
- [ ] 6-J: Deduplicate CHECK_FATAL across Stack/Queue -> genVec call chains (_impl variants)
- [ ] 6-K: Merge multi-condition CHECK_FATALs into one branch (genVec_get, hashmap_get_ptr, ...)
- [ ] 6-L: VEC_FOREACH hoist bounds check + use _unsafe internally (needs 6-G)
- [ ] 6-M: Cache is_pod on genVec - field exists (gen_vector.h:46) + _Static_assert(sizeof(genVec)==40) restored; wire at init + consume at IS_POD sites

## Phase 7 — Baseline + single-header gate + residual bugfixes

- [ ] 7-A: Commit/rebase the WIP baseline + sync tracker (tick 6-A/B, note 6-M) so the gate matches code
- [ ] 7-B: Single-header gate - regen all *_single.h, add a smoke test that includes + links them; wire to ctest
- [ ] 7-C: Fix string_store_cstr for strings >1024 (wire unused heap union, views.c:84-92) + test
- [ ] 7-D: Verify genVec_remove_range memmove bound (the // TODO: is this right, incl. regenerated singles)

## Phase 8 — New features

> Last. Every feature ships with tests + single-header regen + README update (no feature without its gate). In 8-E, matrix_adj / matrix_inv must be built on matrix_LU_Decomp_pivot, so do the pivoted LU first.
- [ ] 8-A: `genVec_reverse`, `genVec_filter`, push guard for stack-array vecs
- [ ] 8-B: `string_split`, `string_join`, `string_trim*`, `string_to_upper/lower`
- [ ] 8-B: `string_replace`, `string_format`, `string_reverse`, `string_starts/ends_with`
- [ ] 8-B: `string_count_char`, `string_repeat`
- [ ] 8-C: `hashmap_reset`, `hashmap_update`, `hashmap_keys`, `hashmap_values`
- [ ] 8-D: `hashset_union`, `hashset_intersect`, `hashset_difference`
- [ ] 8-E: `matrix_iden`, `matrix_adj`, `matrix_inv`, `matrix_trace`, `matrix_rank`, `matrix_pow`
- [ ] 8-E: `matrix_LU_Decomp_pivot` (partial pivoting)
- [ ] 8-F: Move `gaussian_spare`/`has_spare` into `Pcg32` struct
- [ ] 8-F: Expose `_r` variants; add `pcg32_rand_seed_time_hp`, `pcg32_rand_range`
- [ ] 8-G: `fast_atan2`, `fast_pow`, `fast_floor`, `fast_abs`
- [ ] 8-H: `bitVec_count_set`, `bitVec_and/or/xor/not`, `bitVec_find_first_set/clear`, `bitVec_print_all`

## Phase 9 — Convention consistency refactor

> Audit found conventions mostly consistent (dest-first copies, receiver-first ops, `T**` moves, `WC_NOT_FOUND`, optional-out last). Items 1–3 fixed; 4–6 from the audit pending re-derivation.

- [x] 9-1: Flip `_create_stk` to receiver-last — `queue_create_stk`, `string_create_stk`, `arena_create_stk`, `arena_create_arr_stk` (+ `ARENA_CREATE_STK_ARR` macro), `matrix_create_stk`; all call sites across include/src/tests/examples updated
- [x] 9-2: Rename `genVec_init*` → `genVec_create*` everywhere (include/src/tests/examples; 87 sites, 0 leftovers; `single_header/` excluded — pending 7-B)
- [x] 9-3: const-correctness — `bitVec_test/size_bits/size_bytes`, `stack_size/empty/capacity/peek_ptr`, `queue_size/empty/capacity/peek_ptr`, `matrix_get_elm`, `genVec_get_ptr` take const receivers; `genVec_get_ptr_mut` takes non-const receiver
- [ ] 9-4: Re-derive + fix audit items 4–6 (from the consistency review)
- [ ] 9-5: Regenerate `single_header/*.h` + smoke gate (feeds 7-B) — currently out of sync after 9-2
- [ ] 9-6: VERIFY build (GCC + Clang) after 9-1..9-3 — baseline was green pre-refactor; nothing compiled since the crash recovery
