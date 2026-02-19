# WCtoolkit — TODO

---

## Immediate

Minimal test suite, with seperate test exe

im getting waring on using gnu extention


---

## Testing

Currently I only have a single test header for each component, in which
I have multiple functions that test the various implementations
These functions are called by main

I want to explore how testing is done professionally

---

## Library

How to package this as a library ?
I have sinle header version but as a whole?
make a binary?

Need to add CONTRIBUTING.md

---

## SIMD

How much code is vectorizable using 'restrict' keyword ?

---

## Single Header Files

The generated files look correct
Need to test them

---

## Bug Fixes

- **`ARENA_PUSH_ARRAY` macro** (`arena.h`): `(T)* _dst` is a syntax error, should be `T* _dst`

- **`prev_prime` underflow** (`map_setup.h`): the loop iterates `u64 i = PRIMES_COUNT - 1; i >= 0`
  which never exits because `u64` wraps around. Change loop index to `i32` or iterate differently.

- **`find_slot` sentinel** (`hashmap.c`, `hashset.c`): returns `0` as a fallback when the table
  is entirely tombstones and no EMPTY bucket is found. `0` is a valid bucket index. Should return
  `(u64)-1` and callers should check for it.

- **`str_setup.h` multiple-definition** (`str_setup.h`): `murmurhash3_str` is a non-`static`
  function defined in a header. Including it in more than one translation unit causes linker errors.
  Mark it `static` or move it to a `.c` file.

- **`common.c` bitwise OR** (`common.c`): `if (ptr == NULL | size == 0 | bytes_per_line == 0)`
  uses `|` instead of `||`, evaluating all three operands even when `ptr` is null.

---

## gen_vector

- Add iterator / cursor support — right now the only way to loop is manual index arithmetic
- `genVec_swap(vec, i, j)` — swap two elements in place
- `genVec_find(vec, elm, cmp_fn)` — linear search, returns index or `(u64)-1`
- `genVec_reverse(vec)` — in-place reversal
- `genVec_filter(vec, predicate)` — in-place removal of elements where predicate returns false
- `genVec_extend(vec, val, count)` — append N copies of a value
- `genVec_view` — a non-owning slice into a vector (pointer + length, no copy)
- Guard against push-past-capacity in `genVec_init_arr` mode — currently crashes silently.
  Add an `ASSERT_FATAL` at the push site when the internal data pointer is stack-owned.

---

## String

- `string_to_cstr_buf(str, buf, buf_size)` — write into caller-provided buffer, no malloc
- `string_split(str, delim, &out_count)` — split by delimiter, returns `String**`
- `string_join(strings, count, sep)` — join array of strings with separator
- `string_trim(str)` — remove leading and trailing whitespace
- `string_trim_left(str)`, `string_trim_right(str)`
- `string_to_upper(str)`, `string_to_lower(str)`
- `string_replace(str, old, new)` — replace all occurrences, returns count
- `string_format(str, fmt, ...)` / `string_format_new(fmt, ...)` — sprintf-style building
- `string_reverse(str)`
- `string_starts_with(str, prefix)`, `string_ends_with(str, suffix)`
- `string_contains(str, substr)`
- `string_count_char(str, c)` — count occurrences of a character
- `string_repeat(str, times)` — returns a new repeated string
- **String view** — zero-copy slice, no ownership:
  ```c
  typedef struct { const char* data; u64 len; } StringView;
  StringView string_view(const String* str, u64 start, u64 len);
  StringView string_view_cstr(const char* cstr);
  ```
---

## HashMap

- `hashmap_clear(map)` — remove all elements, keep current capacity
- `hashmap_reset(map)` — remove all elements, reset to initial capacity
- `hashmap_update(map, key, val)` — update value only if key exists, return false if not found
- `hashmap_keys(map)` — returns a `genVec*` of all keys
- `hashmap_values(map)` — returns a `genVec*` of all values
- Iterator / `hashmap_for_each(map, fn)` — currently the only traversal is `hashmap_print`

---

## HashSet

- Iterator / `hashset_for_each(set, fn)` — same gap as HashMap
- Set operations: `hashset_union`, `hashset_intersect`, `hashset_difference`

---

## Matrix (float)

- `matrix_iden(n)` — create an n×n identity matrix
- `matrix_adj(out, mat)` — adjugate (adjoint) matrix — **currently stub, not implemented**
- `matrix_inv(out, mat)` — matrix inverse — **currently stub, not implemented**
- `matrix_xply_vec(mat, vec)` — matrix × vector product; current signature is unclear
- `matrix_trace(mat)` — sum of main diagonal
- `matrix_rank(mat)` — rank via row reduction
- `matrix_pow(out, mat, n)` — integer matrix power via repeated squaring
- Eigenvalues / eigenvectors (QR algorithm or power iteration)
- **Pivoting in LU decomposition** — current Doolittle implementation has no partial pivoting,
  making it numerically unstable for matrices with small diagonal entries.
  Add `matrix_LU_Decomp_pivot` with a permutation vector output.
- **Condition number check** before inversion / solving
- SIMD optimization — vectorized `matrix_add`, `matrix_sub`, `matrix_xply` using SSE/AVX/NEON
- User-adjustable `BLOCK_SIZE` macro for transpose and multiply tiling
- **Matrix views** — zero-copy slicing and strided access:
  ```c
  typedef struct {
      float* data;
      u64    m, n;
      u64    stride;
      u64    offset;
  } MatrixView;
  ```

---

## Matrix Generic (`matrix_generic.h`)

- `MATRIX_INV(T)` — inverse via LU
- `MATRIX_ADJ(T)` — adjugate
- `MATRIX_IDEN(T)` — identity matrix constructor
- `MATRIX_TRACE(T)`
- Arena allocation variants (`matrix_arena_alloc_T`, `matrix_arena_arr_alloc_T`)

---

## Random

- `pcg32_rand_gaussian` uses a `static` spare value — this state is invisible to callers
  and breaks if multiple RNG instances or sequences are needed.
  Move `gaussian_spare` and `has_spare` into `pcg32_random_t` so it's per-instance.
- Expose the `_r` variants (`pcg32_rand_r`, `pcg32_rand_bounded_r`, etc.) publicly
  so callers can manage independent RNG instances
- `pcg32_rand_seed_time_hp` — high-precision seed using `clock_gettime(CLOCK_MONOTONIC)`
  instead of `time(NULL)` (second precision) to allow multiple seeds within the same second
- Uniform integer in `[min, max]` convenience function

---

## Fast Math

- `fast_exp` range-reduction variant — use `e^x = e^(integer) × e^(fraction)` with repeated
  squaring for the integer part (this is actually the current implementation — document it better
  and add a note about where precision falls off vs `expf`)
- Higher-precision versions with more Taylor series terms, opt-in via macros
- `fast_atan2(y, x)`
- `fast_pow(base, exp)`

---

## Arena

- Thread-local arena support — `ARENA_THREAD_LOCAL` variant for per-thread scratch allocation
- Multiple arena pools — a pool allocator that hands out fixed-size arenas from a slab

---

## BitVector

- `bitVec_count_set(bv)` — population count (popcount) of all set bits
- `bitVec_and(out, a, b)`, `bitVec_or(out, a, b)`, `bitVec_xor(out, a, b)`, `bitVec_not(out, a)` — bitwise operations between two vectors
- `bitVec_find_first_set(bv)` — index of first 1 bit, or `(u64)-1`
- `bitVec_find_first_clear(bv)` — index of first 0 bit
- `bitVec_print_all(bv)` — print all bytes, not just one at a time

---

## Infrastructure / DX

- **Formal test framework** — replace ad-hoc `test_N()` functions with a lightweight harness
  that tracks pass/fail counts and reports results. Something minimal:
  ```c
  WC_TEST(name) { ... }
  WC_ASSERT_EQ(a, b)
  WC_ASSERT_TRUE(cond)
  WC_RUN_ALL()
  ```
- **Ergonomic macros** (candidates for `helpers.h`):
  ```c
  VEC_GET(vec, T, i)          // typed element access
  VEC_BACK(vec, T)            // typed last element
  VEC_FRONT(vec, T)           // typed first element
  VEC_FOR_EACH(vec, T, var, body)      // typed iteration loop
  VEC_FOR_EACH_PTR(vec, T, var, body)  // for vec-of-pointers
  MAP_PUT(map, k, v)          // typed hashmap put
  MAP_GET(map, T, key)        // typed hashmap get
  MAP_HAS(map, key)
  MAP_DEL(map, key)
  STACK_PUSH(stk, T, val)
  ENQUEUE(q, T, val)
  MATRIX_ARENA(arena, m, n)
  MATRIX_ARENA_ARR(arena, m, n, arr)
  ```
- **`string_printf(fmt, ...)`** — build a String using printf-style formatting
- `hashmap_for_each` / `hashset_for_each` — typed iteration callbacks
- Thread-safe container variants (lock-based or lock-free, opt-in)
- CMake install target + pkg-config file for using as a system library
