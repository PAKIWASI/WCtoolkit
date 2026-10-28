# Getting started

[Back to README](../README.md)

## Requirements

- A C11 compiler with GNU extensions: GCC or Clang. The library uses statement expressions `({ ... })`, `typeof`, `__attribute__((cleanup))` and `__builtin_expect`.
- Linux/macOS: any recent Clang or GCC.
- Windows: MinGW-w64 Clang or GCC only — MSYS2's UCRT64 environment, or an `x86_64-w64-mingw32` cross toolchain.
- CMake 3.20+ and Ninja or Make.
- libc

## Build

Linux/macOS:

```sh
cmake -B build -G Ninja             # omit -G Ninja to use Make
cmake --build build
./build/tests                       # unit tests
./build/main                        # scratch program, src/main.c
```

Windows (MinGW-w64). The default build type there is `Release`, and the exe is statically linked (`-static`). From an MSYS2 UCRT64 shell:

```sh
# pacman -S mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
CC=clang cmake -B build -G Ninja
cmake --build build                 # build/main.exe
```

Cross-compiling from Linux:

```sh
# sudo apt install mingw-w64
cmake -B build-win -G Ninja -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DWC_MARCH=x86-64-v2
cmake --build build-win             # build-win/main.exe
```

CMake prefers Clang and falls back to GCC. Pass `-DCMAKE_C_COMPILER=...` to choose.

### Build types

Pass `-DCMAKE_BUILD_TYPE=<type>`:

| Type | Flags | Use it for |
|---|---|---|
| `Debug` (Linux default) | `-O0`, AddressSanitizer + UndefinedBehaviorSanitizer | Day-to-day development and tests |
| `DebugNoSan` | `-O0`, no sanitizers | `gdb` / `lldb` sessions |
| `Release` (Windows default) | `-O3 -DNDEBUG`, LTO | Shipping |

`Release` defines `NDEBUG`, which strips every `WC_ASSERT`. Run the tests in both `Debug` and `Release`: some behavior differs between them by design. See [Diagnostics](diagnostics.md).

`-ffast-math` is deliberately not global: it is applied to `src/fast_math.c` only, so it cannot silently change float behavior in the containers.

`-march=native` is the default. Cross compilers usually reject `native`: pass `-DWC_MARCH=x86-64-v2` (a sane baseline) or `-DWC_MARCH=` to omit the flag.

Windows builds are release-only, so `Debug` there has no sanitizers. If your Clang has no sanitizer runtime installed on Linux, the `Debug` link fails; use `DebugNoSan`.

`-DWC_WARNINGS_AS_ERRORS=ON` adds `-Werror`. Three warnings are always errors: `incompatible-pointer-types`, `implicit-function-declaration` and `int-conversion`.

## Using it in your project

There is no installed library target. Add the headers and the sources to your own build:

```cmake
file(GLOB WC_SOURCES path/to/wctoolkit/src/*.c)
list(REMOVE_ITEM WC_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/path/to/wctoolkit/src/main.c)

add_executable(app main.c ${WC_SOURCES})
target_include_directories(app PRIVATE path/to/wctoolkit/include)
if(NOT WIN32)
  target_link_libraries(app m)      # libm is separate on Linux/macOS
endif()
```

Leave out `src/main.c`: it is the toolkit's own scratch program.

### Single headers

To drop one component into a project without the build setup, generate single headers:

```sh
python3 tools/make_single_header.py --all      # or: arena hashmap ...
python3 tools/make_single_header.py --list     # components and their dependencies
```

Each `single_header/<name>_single.h` carries everything that component needs. In exactly one `.c` file, `#define WC_IMPLEMENTATION` before including it. Several single headers can be included together.

## A first program

```c
#include "arena.h"
#include "hashmap.h"
#include "wc_macros.h"

#include <stdio.h>

int main(void)
{
    // A vector of ints on libc
    GenVec v = VEC(int, 4);
    for (int i = 0; i < 10; i++) {
        VEC_PUSH(&v, i * i);
    }
    printf("v[3] = %d\n", VEC_AT(&v, int, 3));
    GenVec_destroy(&v);

    // A map of StrView -> int. The store owns the key bytes, so it must
    // outlive the map.
    StringStore ss;
    StringStore_create(&ss, WC_LIBC, nKB(4), true);
    HashMap ages = MAP_OF(StrView, int);
    MAP_PUT(&ages, StringStore_cstr(&ss, "ada"), 36);
    MAP_PUT(&ages, StringStore_cstr(&ss, "alan"), 41);

    printf("ada = %d\n", MAP_GET_CSTR(&ages, int, "ada"));   // transient view, no allocation
    HashMap_destroy(&ages);
    StringStore_destroy(&ss);

    // The same kind of work on an arena: one destroy, no per-container destroy
    Arena tmp;
    Arena_create(&tmp, WC_LIBC, nKB(4));
    GenVec words = VEC_OF_IN(Arena_allocator(&tmp), String, 4);
    VEC_PUSH_CSTR(&words, "no destroy needed");
    Arena_destroy(&tmp);   // frees `words` and its Strings too
    return 0;
}
```

## Compile-time tunables

Define these on the command line (`-D...`) for the **whole build**, library sources included. Defining one in a single file before an include gives that file a different value from the library's compiled code.

| Macro | Default | Effect |
|---|---|---|
| `GENVEC_MIN_CAPACITY` | `8` | First `GenVec` capacity when growing from 0 (then 1.5×) |
| `QUEUE_MIN_CAP` | `8` | First `Queue` capacity when growing from 0, and the `shrink_to_fit` floor |
| `STRING_GROWTH` | `1.5F` | `String` capacity multiplier on growth |
| `ARENA_DEFAULT_SIZE` | `nKB(4)` | Default arena capacity |
| `CHAIN_ARENA_NODE_SIZE` | `nKB(4)` | Bytes per `ChainArena` node, header included |
| `WC_MAT_BLOCK` | `16` | Tile edge for the blocked `matrix_T` |

The hash table load factor (`0.75`) is fixed. The initial capacity is `HASHMAP_INIT_CAPACITY` (`16`, a power of two) in `hashmap.h`.

## Where to go next

- [Conventions](conventions.md): the rules every API follows.
- [Containers](containers.md): what each container does and costs.
- [Allocators](allocators.md): when to use an arena instead of libc.
