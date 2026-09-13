# WCtoolkit — Task Tracker

> RECOVERY CHECKPOINT (see CHECKPOINT.md): build is GREEN on gcc under -Werror, 378/378
> tests pass. This was pre-Phase-5 crash recovery + gcc-clean work, not tracker items.
> Outstanding: ASan reports ~86 fixture leaks in HashSet/HashMap tests (pre-existing).
> Phase 5 (5-A..5-H) complete: all in-place renames landed across include/src/tests/examples
> and docs (README/plan/TODO). Phase 6 (6-A..6-M) and 7-C complete — see items below.
> 5-G (single-header regen) was dropped from this tracker (regen needs Python);
> single_header/ remains out of date. New views tests (+8) and Phase-6 macro tests
> (+8: CREATE_OF/WC_OPS, WC_ASSERT_ELEM_SIZE, VEC_FOREACH placement, _unsafe getters)
> are not counted in the 378 above.


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
- [x] 2-A: Rewrite `GenVec_copy` (no `destroy_stk` on dest, build directly)
- [x] 2-A: Add `SAFE ON` comment above `GenVec_copy` in `gen_vector.h`
- [x] 2-B: Collapse `vec_copy` / `vec_copy_ptr` in `wc_helpers.h` to delegate to `GenVec_copy`
- [x] 2-C: Fix `str_copy` SSO detection → `string_is_sso()`
- [x] 2-D: Audit `HashMap_copy` / `HashSet_copy` against Rule 3; fix if needed
- [x] 2-E: Add `SAFE ON` / `REQUIRES` tags to all copy-shaped declarations
- [x] 2-F: Add `GenVec_copy` on raw-dest test to `gen_vector_test.c`

## Phase 3 — API coverage gaps
- [x] 3-A: Expose `HashSet_destroy_stk` publicly
- [x] 3-A: Add `HashMap_create_stk` / `HashMap_destroy_stk`
- [x] 3-B: Add `HashMap_bucket_count/occupied/key_ptr/val_ptr`
- [x] 3-B: Add `HashSet_bucket_count/occupied/elm_ptr`
- [x] 3-C: Add `HashMap_get_ptr` (const) / rename current to `HashMap_get_ptr_mut`
- [x] 3-C: Add `HashSet_get_ptr` / `HashSet_get_ptr_mut`
- [x] 3-D: Add `queue_copy` / `queue_move`
- [x] 3-E: Add `matrix_move`
- [x] 3-F: Audit `BitVec_pop` on empty; decide/fix behavior
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

## Phase 5 — In-place renames

> Pulled ahead of the hardening phase: renames are mechanical churn with no behavioral risk, and
> doing them first lets the attribute / static-assert work in Phase 6 land once on final names.
> VEC_* / MAP_* / pcg32_* prefixes are untouched by this phase.

- [x] 5-A: Rename `genVec` -> `GenVec` everywhere
- [x] 5-B: Rename `bitVec` -> `BitVec` everywhere
- [x] 5-C: Rename `hashmap` -> `HashMap` everywhere
- [x] 5-D: Rename `hashset` -> `HashSet` everywhere
- [x] 5-E: Rename `strview` -> `StrView`, `string_store` -> `StringStore`, `pcg32_random_t` -> `Pcg32` (struct typedef; `pcg32_rand*` fns preserved)
- [x] 5-F: Rename release verbs: `arena_release` -> `arena_destroy`, `chain_arena_release` -> `chain_arena_destroy` (doc-only; C source already used _destroy)
- [x] 5-H: Post-rename grep audit — no survivors in comments/strings/README/plan docs

## Phase 6 — Compile-time hardening + performance

> 6-A / 6-B already landed in the working tree (common.h). Remaining items run on the renamed API.

- [x] 6-A: CHECK_FATAL respects NDEBUG - DONE (include/common.h:68)
- [x] 6-B: FATAL -> wc_fatal_report with noreturn + format - DONE (include/common.h:39,51)
- [x] 6-C: nonnull sweep across all public headers (GenVec/HashMap/String/Stack/Queue/HashSet/arena/chain_arena/BitVec/matrix/views = 200 attrs) — nullable ops/callbacks/optional-out audited & left unmarked; fast_math/random have no pointer params. VERIFY build
- [x] 6-D: warn_unused_result on _create/_init/_alloc family (19 decls: GenVec_create/_val/_arr/subarr, string_create/from_cstr/from_string/to_cstr/substr, HashMap_create, HashSet_create, arena_create, chain_arena_create, BitVec_create, matrix_create/_arr, queue_create/_val, stack_create/_val) — grep found 0 ignored call sites in src/tests/examples
- [x] 6-E: alloc_size(2) on arena_alloc, arena_alloc_aligned, chain_arena_alloc_aligned, chain_arena_alloc (inline) — all single-size-arg allocators
- [x] 6-F: _Static_assert(sizeof(String)==sizeof(GenVec)) — already present in wc_helpers.h:62 (asserts AND enforces the value-storage ops design)
- [x] 6-G: Add GenVec_get_ptr_unsafe / GenVec_get_ptr_mut_unsafe (gen_vector.c; bounds CHECK_FATAL elided, nonnull kept)
- [x] 6-H: Add WC_ASSERT_ELEM_SIZE; wraps VEC_AT / AT_MUT / FRONT / BACK / POP (wc_macros.h) — FOREACH intentionally unasserted (leading stmt breaks `if (x) FOREACH(...)` placement)
- [x] 6-I: Add WC_OPS via _Generic (String/String*/GenVec/GenVec* -> ops, default NULL = POD); add VEC_CREATE_OF / MAP_CREATE_OF
- [x] 6-J: Deduplicate CHECK_FATAL across Stack/Queue -> GenVec call chains — invariant q->size == GenVec_size(arr) - head proven; 15 wrapper re-checks dropped (queue_size/empty/capacity inlines + queue_pop/peek/peek_ptr/push_move) and the satisfied "drop CHECK_FATAL for nonnulls" floating note removed
- [x] 6-K: Merge multi-condition CHECK_FATALs — GenVec_create_stk_arr, GenVec_replace_move, GenVec_insert_move, GenVec_insert_multi, GenVec_insert_multi_move, queue_create/_val/_stk (n|data_size); HashMap/HashSet get_ptr & bucket accessors already single-branch
- [x] 6-L: VEC_FOREACH hoist bounds check + use _unsafe internally (needs 6-G) — bounds in _wvf_n, fetch via GenVec_get_ptr_mut_unsafe
- [x] 6-M: Cache is_pod on GenVec — IS_POD macro now reads the cached field; all 3 init sites (create/create_stk/create_stk_arr) wire via new CALC_POD(ops); copy path verified to memcpy the cached field

## Phase 7 — Residual bugfixes

- [x] 7-C: Fix StringStore_cstr for strings >1024 (wire unused heap union, views.c:84-92) + test — overflow nodes own their buffer via node->heap, flagged with new node->owns_heap; StringStore_destroy_node exported; tests/test_views.c added (8 tests incl. 3 overflow cases), wired into CMake + test_main
