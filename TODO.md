
# WCtoolkit — TODO


---

## Immediate

- Flag for faster read/writes?
- iterators
- multithreading

---

## Testing

- proper speed tests:
    seperate tests for pod types and complex types
    should be done after pod optimizations

---

## Ergonomics


---

## SIMD

- look at how many loops are not auto vectorised and
    manually vectorise them using intrinsics

---

## Single Header Files

The generated files look correct
Need to test them

---

## Bug Fixes


---

## Improvements

### gen_vector

- Add iterator / cursor support — right now the only way to loop is index loops and FOREACH macros
- `genVec_reverse(vec)` — in-place reversal
- `genVec_filter(vec, predicate)` — in-place removal of elements where predicate returns false
- `genVec_view` — a non-owning slice into a vector (pointer + length, no copy)
- Guard against push-past-capacity in `genVec_init_arr` mode — currently crashes silently.
    Add an `ASSERT_FATAL` at the push site when the internal data pointer is stack-owned.

### String

- `string_split(str, delim, &out_count)` — split by delimiter, returns `String**`
- `string_join(strings, count, sep)` — join array of strings with separator
- `string_trim(str)` — remove leading and trailing whitespace
- `string_trim_left(str)`, `string_trim_right(str)`
- `string_to_upper(str)`, `string_to_lower(str)`
- `string_replace(str, old, new)` — replace all occurrences, returns count
- `string_format(str, fmt, ...)` / `string_format_new(fmt, ...)` — sprintf-style building
- `string_reverse(str)`
- `string_starts_with(str, prefix)`, `string_ends_with(str, suffix)`
- `string_count_char(str, c)` — count occurrences of a character
- `string_repeat(str, times)` — returns a new repeated string
- **String view** — zero-copy slice, no ownership:
  ```c
  typedef struct { const char* data; u64 len; } StringView;
  StringView string_view(const String* str, u64 start, u64 len);
  StringView string_view_cstr(const char* cstr);
  ```

### HashMap

- `hashmap_clear(map)` — remove all elements, keep current capacity
- `hashmap_reset(map)` — remove all elements, reset to initial capacity
- `hashmap_update(map, key, val)` — update value only if key exists, return false if not found
- `hashmap_keys(map)` — returns a `genVec*` of all keys
- `hashmap_values(map)` — returns a `genVec*` of all values


### HashSet

- Set operations: `hashset_union`, `hashset_intersect`, `hashset_difference`
- Union-Find data structure ?


### Matrix (float)

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


### Matrix Generic (`matrix_generic.h`)

- `MATRIX_INV(T)` — inverse via LU
- `MATRIX_ADJ(T)` — adjugate
- `MATRIX_IDEN(T)` — identity matrix constructor
- `MATRIX_TRACE(T)`
- Arena allocation variants (`matrix_arena_alloc_T`, `matrix_arena_arr_alloc_T`)


### Random

- `pcg32_rand_gaussian` uses a `static` spare value — this state is invisible to callers
  and breaks if multiple RNG instances or sequences are needed.
  Move `gaussian_spare` and `has_spare` into `pcg32_random_t` so it's per-instance.
- Expose the `_r` variants (`pcg32_rand_r`, `pcg32_rand_bounded_r`, etc.) publicly
  so callers can manage independent RNG instances
- `pcg32_rand_seed_time_hp` — high-precision seed using `clock_gettime(CLOCK_MONOTONIC)`
  instead of `time(NULL)` (second precision) to allow multiple seeds within the same second
- Uniform integer in `[min, max]` convenience function


### Fast Math

- `fast_atan2(y, x)`
- `fast_pow(base, exp)`
- `fast_floor(x)`
- `fast_abs(x)`


### Arena

- Thread-local arena support — `ARENA_THREAD_LOCAL` variant for per-thread scratch allocation
- Multiple arena pools — a pool allocator that hands out fixed-size arenas from a slab


### BitVector

- `bitVec_count_set(bv)` — population count (popcount) of all set bits
- `bitVec_and(out, a, b)`, `bitVec_or(out, a, b)`, `bitVec_xor(out, a, b)`, `bitVec_not(out, a)` — bitwise operations between two vectors
- `bitVec_find_first_set(bv)` — index of first 1 bit, or `(u64)-1`
- `bitVec_find_first_clear(bv)` — index of first 0 bit
- `bitVec_print_all(bv)` — print all bytes, not just one at a time

---

## Speed Improvement

flag for pod types to skip the ops functions checks

### hashmap

### genvec
    - restrict and inlining for genVec POD. For tight numerical loops, adding __restrict__ to the data pointer 
        tells the compiler that data doesn't alias any other pointer, enabling auto-vectorisation. 

---

