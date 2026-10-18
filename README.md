# WCtoolkit

A C11 data-structures toolkit with explicit ownership and allocators. No dependencies beyond libc.

```c
#include "arena.h"
#include "wc_macros.h"

int main(void)
{
    GenVec names = VEC_OF(String, 8);           // libc-backed, ops chosen from the type
    VEC_PUSH_CSTR(&names, "Wasi");               // String built in the vector's allocator
    VEC_FOREACH(&names, String, s) { String_append_char(s, '!'); }
    GenVec_destroy(&names);                      // destroys the Strings, then the buffer

    return 0;
}
```

## What's inside

| Area | Types |
|---|---|
| Containers | `GenVec`, `Stack`, `Queue`, `PriorityQueue`, `HashMap` (and sets: `SET_OF`), `BitVec` |
| Strings | `String` (23 chars inline), `StrView`, `StringStore` |
| Memory | `wc_allocator`, `Arena`, `ChainArena`, `WC_BORROWED` |
| Math | `Matrixf`, `fast_math`, PCG `random` |

## Three rules

1. **Values, not pointers.** Containers are returned by value and store elements inline.
2. **Every container stores its allocator.** libc, an arena, a stack buffer or your own vtable, all used the same way.
3. **No hidden cost.** No global allocator, no background work, no implicit allocation. A copy happens only when both sides keep the value; taking an element out is a move. See [Memory rules](docs/memory-rules.md).

## Build

Linux/macOS, with clang or gcc (Debug by default, under ASan + UBSan):

```sh
cmake -B build -G Ninja && cmake --build build
./build/tests                 # unit tests (utest.h)
./build/main                  # scratch program, src/main.c

cmake -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-rel
```

Windows releases use **MinGW-w64** (clang or gcc), not MSVC/clang-cl. The
default build type on Windows is `Release`. From an MSYS2 UCRT64 shell:

```sh
# pacman -S mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
CC=clang cmake -B build -G Ninja && cmake --build build
.\build\main.exe
```

Or cross-compile from Linux:

```sh
# sudo apt install mingw-w64
cmake -B build-win -G Ninja -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DWC_MARCH=x86-64-v2
cmake --build build-win       # build-win/main.exe
```

## Documentation

| Page | Covers |
|---|---|
| [Getting started](docs/getting-started.md) | Build types, using the library in a project, first program |
| [Conventions](docs/conventions.md) | Naming, create/destroy, copy/move, zero state, `void*` vs `u8*` |
| [Memory rules](docs/memory-rules.md) | Every allocation, copy, move and lifetime rule, in one place |
| [Ownership](docs/ownership.md) | `wc_container_ops`, copy vs move vs take out, writing your own ops, by-value vs by-pointer |
| [Allocators](docs/allocators.md) | libc, `Arena`, `ChainArena`, borrowed memory, scopes, lifetimes |
| [Containers](docs/containers.md) | Every container: API, complexity, behavior |
| [Strings](docs/strings.md) | `String`, `StrView`, `StringStore` |
| [Macros](docs/macros.md) | The type-checked macro layer |
| [Diagnostics](docs/diagnostics.md) | `FATAL_IF`, `WC_ASSERT`, `WARN_IF`, `LOG`, `wc_errno`, Debug vs Release |
| [Math](docs/math.md) | Matrices, `fast_math`, random numbers |
| [Testing](docs/testing.md) | Tests and benchmarks: layout, running, writing, death tests, checking allocator |

## License

MIT. See [LICENSE](LICENSE).

## Destroyed and moved-from containers

Using a destroyed or moved-from container (including passing a non-zeroed `dest`
to a `*_move`) is a caller bug. It is caught only in Debug builds, via
`WC_ASSERT`. In Release the behavior is undefined (silent corruption, e.g. `size`
underflowing on `swap_pop`/`remove`). A `dest` passed to `*_move` must be
zero-initialized.
