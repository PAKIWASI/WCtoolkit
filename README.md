# WCtoolkit

A C11 data-structures and utility toolkit built around explicit ownership and value semantics. No dependencies beyond libc.

Containers don't guess how to copy, move, or free your data — you tell them once via a small `container_ops` vtable, and every container (vector, map, set, stack, queue) reuses the same three callbacks. Plain-old-data types (`int`, `float`, flat structs) just pass `NULL` and get raw `memcpy`.

```c
static const container_ops String_ops = { str_copy, str_move, str_del };

GenVec* names = GenVec_create(8, sizeof(String), &String_ops);

VEC_PUSH_CSTR(names, "Wasi");          // heap String, moved in, zero copies
VEC_FOREACH(names, String, s) { String_append_char(s, '!'); }

String out;
GenVec_pop(names, castptr(&out));      // take ownership of the last element
String_destroy_stk(&out);

GenVec_destroy(names);
```

## Design in three rules

1. **Value semantics, not pointer chasing.** Elements live inline inside containers (a `GenVec` of `String` stores actual `String` structs contiguously), not scattered behind pointers. By-pointer storage is supported when you need stable addresses, but by-value is the default.
2. **Every type has a heap and a Stack variant.** `GenVec_create` returns a heap pointer; `GenVec_create_stk` fills in a struct you own. `_destroy` frees the struct and its data; `_destroy_stk` frees only the data, leaving the struct reusable. The `_stk` suffix always means *you own the struct, the library only owns what's inside it*.
3. **No hidden cost.** No garbage collection, no background threads, no implicit allocation. Growth factors and hash table load are compile-time constants you can override before including a header.

## API conventions

- **Receiver first**: `GenVec_push(vec, x)`, `GenVec_copy(dest, src)`, `GenVec_create_stk(vec, ...)` — the instance being acted on is always the first argument.
- **Moves take `T**` and null the source**: `String_move(dest, &src)` leaves `src == NULL`. `_move` variants (`push_move`, `insert_move`, `put_move`, ...) exist wherever ownership can transfer; plain variants copy.
- **`_copy` functions require a raw/uninitialized destination.** They overwrite `dest` field-by-field and never free or read what was there before — copying into an already-populated container leaks its old buffer. This is documented at every `_copy` declaration.
- **Lookups return `WC_NOT_FOUND`** (`(u64)-1`), never `-1` on an unsigned type.
- **Constructors never return `NULL`** — they abort on allocation failure (`CHECK_FATAL`), so call sites don't need to null-check. Expected runtime conditions (pop on empty, Arena full) instead set `wc_errno` and return `0`/`NULL`/`false`.
- **Heap allocators are `warn_unused_result`** — dropping a `GenVec_create()` return value is a compiler warning, not a silent leak.

A type-checked macro layer (`VEC_PUSH`, `VEC_FOREACH`, `MAP_PUT`, ...) sits on top of the `void*`-based C API, catching element-type mismatches at compile time. Macros never allocate or free on their own — they always expand to the underlying C calls, so you can drop to the plain API at any point with no behavior change.

## Components

| Component | Header | What it is |
|---|---|---|
| `GenVec` | `gen_vector.h` | Generic growable vector, the base every other container is built on |
| `String` | `wc_string.h` | Growable string with small-string optimization (24-byte inline buffer) |
| `HashMap` | `hashmap.h` | Open-addressed hash map, Robin Hood hashing |
| `HashSet` | `hashset.h` | Open-addressed hash set, same hashing scheme as `HashMap` |
| `Stack` / `Queue` | `stack.h` / `queue.h` | Thin `GenVec` wrapper / circular buffer |
| `Arena` / `ChainArena` | `arena.h` / `chain_arena.h` | Bump allocator (fixed-size) and a chained, growable version |
| `BitVec` | `bit_vector.h` | Growable bit array over `GenVec` |
| `Matrixf` | `matrix.h` | Row-major float matrix: add/sub/scale/multiply/transpose/LU/determinant |
| `StrView` / `StringStore` | `views.h` | Non-owning string slices, and an append-only interned string arena |
| `fast_math` | `fast_math.h` | Low-precision, fast approximations of sqrt/log/sin/cos/exp/pow — for when you don't need libm's precision |
| `random` | `random.h` | PCG pseudo-random generator |
| `wc_errno` | `wc_errno.h` | The two-tier error model described above |

Each container's header opens with a short doc comment describing its exact semantics — read that before reaching for the source.

## Building

CMake + Ninja/Make, no external dependencies:

```sh
cmake -B build -G Ninja        # or omit -G Ninja for Make
cmake --build build
./build/tests                  # run the test suite
./build/main                   # scratch executable, src/main.c
```

Build types (`-DCMAKE_BUILD_TYPE=...`):

- **Debug** (default) — `-O0`, ASan + UBSan enabled.
- **DebugNoSan** — `-O0`, no sanitizers, for use with `lldb`/`gdb`.
- **Release** — `-O3 -DNDEBUG -ffast-math`.

`tools/make_single_header.py` amalgamates everything in `include/`/`src/` into the single-header files under `single_header/`, for dropping the whole library into a project without wiring up CMake.

## Testing

`ctest` (or `./build/tests` directly) runs ~400 unit tests across every component, plus a speed suite comparing POD vs. non-POD (owning) element paths for the hot operations (push, pop, clear, copy, hash map put/get). The test binary is built with AddressSanitizer and UndefinedBehaviorSanitizer by default — a clean run means no leaks, no UB, no out-of-bounds access.

## License

MIT — see [LICENSE](LICENSE).
