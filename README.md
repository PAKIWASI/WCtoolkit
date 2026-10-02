# WCtoolkit

A C11 data-structures and utility toolkit built around explicit ownership, value semantics and explicit allocators. No dependencies beyond libc.

Containers don't guess how to copy, move, or free your data. You tell them once with a small `wc_container_ops` vtable, and every container (vector, map, set, stack, queue) reuses the same three callbacks. Plain-old-data types (`int`, `float`, flat structs) pass `NULL` and get raw `memcpy`. Containers also don't guess where memory comes from: each one is created with a `wc_allocator` and uses it for everything.

```c
GenVec names = VEC_OF(String, 8);       // libc; ops picked from the type

VEC_PUSH_CSTR(&names, "Wasi");           // String built in names' allocator, moved in
VEC_FOREACH(&names, String, s) { String_append_char(s, '!'); }

String out = VEC_POP(&names, String);    // take ownership of the last element
String_destroy(&out);
GenVec_destroy(&names);                  // destroys remaining Strings, then the buffer

ARENA_SCOPE(tmp, nKB(4)) {               // same code, arena-backed, no destroys
    GenVec words = VEC_OF_IN(tmp, String, 8);
    VEC_PUSH_CSTR(&words, "scratch");
}
```

## Design in three rules

1. **Value semantics, not pointer chasing.** Elements live inline inside containers (a `GenVec` of `String` stores actual `String` structs contiguously), not scattered behind pointers. By-pointer storage is supported when you need stable addresses, but by-value is the default.
2. **Every container stores its allocator.** `X_create(alloc, ...)` returns the container by value. `X_destroy(&x)` frees its contents through that allocator, never the struct itself, and leaves it zeroed. libc (`WC_LIBC`), `Arena`, `ChainArena`, a caller-owned buffer, or your own vtable all plug in the same way. There is no global allocator.
3. **No hidden cost.** No garbage collection, no background threads, no implicit allocation. Growth factors and hash table load are compile-time constants you can override before including a header.

## API conventions

- **Allocator first, receiver first.** Constructors and copies take the allocator first: `GenVec_create(alloc, n, size, ops)`, `GenVec_copy(alloc, &src)`. Everything else takes the instance first: `GenVec_push(&v, x)`.
- **Copies never inherit.** `X_copy(alloc, &src)` deep-copies into `alloc`, so an arena-backed container can be copied out to libc.
- **Moves zero the source.** `X_move(&dest, &src)` and element moves (`GenVec_push_move(&v, &s)`, `VEC_PUSH_MOVE(&v, s)`) leave the source zeroed.
- **Zero state is dead.** A moved-from or destroyed container may only be destroyed or created again. Changing it is a fatal error in every build. `destroy` is safe on zeroed structs and can be called twice.
- **Pinned arenas.** `Arena` and `ChainArena` are created in place (`Arena_create(&arena, backing, cap)`) and must never be moved or copied, because containers hold their address.
- **`void*` at the boundary, `u8*` inside.** Anything that points at one of your elements, or at untyped memory, is `void*` / `const void*`: element parameters and returns, callbacks, allocation results. No casts at call sites: `GenVec_push(&v, &x)`, `int* p = GenVec_get_ptr_mut(&v, i)`. `u8*` is only for byte buffers the library indexes (container storage, `Arena.base`) and byte-level helpers (`print_hex`, hash internals).
- Lookups return `WC_NOT_FOUND`.
- **Diagnostics, split by build.** Every macro is an expression. In every build: `FATAL_IF` for allocation failure and changes to a dead container, `WARN_IF` / `WARN_IF_RET` / `LOG_IF` to report and continue, plus unconditional `FATAL`, `WARN`, `LOG`. Debug only: `WC_ASSERT(invariant, msg)` for bounds and API misuse; under `NDEBUG` its condition is not evaluated. `wc_errno` covers expected conditions (pop on empty, arena full). Failure paths are `WC_UNLIKELY` and report through `cold` functions, so they sit outside hot code.
- Constructors are `warn_unused_result`: dropping a `GenVec_create()` return value is a compiler warning, not a silent leak.

A type-checked macro layer (`VEC_PUSH`, `VEC_FOREACH`, `MAP_PUT`, ...) sits on top of the `void*`-based C API and catches element-type mismatches at compile time. The plain forms use libc; the `_IN` forms take an allocator (`VEC_OF_IN(A, T, n)`, `MAP_OF_IN(A, K, V)`, `MATRIX_IN(A, m, n)`). Macros that build elements (`VEC_PUSH_CSTR`, `MAP_PUT_STR_*`, ...) use the container's allocator.

Allocator lifetimes, scratch scopes, copy vs move and boxing are covered in [docs/allocators.md](docs/allocators.md). A runnable tour is in [examples/allocators.c](examples/allocators.c).

## Components

| Component | Header | What it is |
|---|---|---|
| `GenVec` | `gen_vector.h` | Generic growable vector, the base every other container is built on |
| `String` | `wc_string.h` | Growable string with small-string optimization (64-byte struct, 31 chars inline) |
| `HashMap` | `hashmap.h` | Open-addressed hash map, Robin Hood hashing |
| `HashSet` | `hashset.h` | Open-addressed hash set, same hashing scheme as `HashMap` |
| `Stack` / `Queue` | `stack.h` / `queue.h` | Thin `GenVec` wrapper / circular buffer |
| `Arena` / `ChainArena` | `arena.h` / `chain_arena.h` | Bump allocator (fixed-size) and a chained, growable version |
| `BitVec` | `bit_vector.h` | Growable bit array over `GenVec` |
| `Matrixf` | `matrix.h` | Row-major float matrix: add/sub/scale/multiply/transpose/LU/determinant (`matrix_generic.h` for other element types) |
| `StrView` / `StringStore` | `views.h` | Non-owning string slices, and an append-only string store |
| `fast_math` | `fast_math.h` | Low-precision, fast approximations of sqrt/log/sin/cos/exp/pow, for when you don't need libm's precision |
| `random` | `random.h` | PCG pseudo-random generator |
| `wc_allocator` | `wc_allocator.h` | The allocator interface, libc backend, `wc_borrowed` |
| `wc_errno` | `wc_errno.h` | Expected-condition error codes (the third error tier above) |

Each container's header opens with a short doc comment describing its exact semantics, read that before reaching for the source.

## Building

CMake + Ninja/Make, no external dependencies:

```sh
cmake -B build -G Ninja        # or omit -G Ninja for Make
cmake --build build
./build/tests                  # run the test suite
./build/main                   # scratch executable, src/main.c
./build/example_allocators     # examples/allocators.c
```

Build types (`-DCMAKE_BUILD_TYPE=...`):

- **Debug** (default) — `-O0`, ASan + UBSan enabled.
- **DebugNoSan** — `-O0`, no sanitizers, for use with `lldb`/`gdb`.
- **Release** — `-O3 -DNDEBUG -ffast-math`.

`tools/make_single_header.py` amalgamates everything in `include/`/`src/` into the single-header files under `single_header/`, for dropping the whole library into a project without wiring up CMake.

## Testing

`ctest` runs four checks:

- `unit_tests`: 500+ unit tests across every component, plus a speed suite comparing POD vs. non-POD (owning) element paths for the hot operations (push, pop, clear, copy, hash map put/get). Containers are exercised on libc, `Arena`, `Arena_create_buf`, `ChainArena` and a test allocator that checks leaks, double frees and free sizes. Allocation-failure and dead-state paths are tested by forking and asserting the child aborts.
- `a5_realloc_n_compiles`: the typed realloc macro compiles under `-Werror`.
- `no_raw_alloc`: no raw `malloc`/`calloc`/`realloc`/`free` in `src/` or `include/` outside the libc backend. Library sources also include `src/wc_poison.h`, which applies `#pragma GCC poison`.
- `example_allocators`: the example program runs and checks its own results.

The Debug build runs everything under AddressSanitizer and UndefinedBehaviorSanitizer. Run the suite in Release too: some fatal checks only differ there.

## License

MIT — see [LICENSE](LICENSE).
