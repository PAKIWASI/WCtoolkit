# Getting started

[Back to README](../README.md)

## Requirements

- A C11 compiler with GNU extensions: GCC or Clang. The library uses statement expressions `({ ... })`, `typeof`, `__attribute__((cleanup))` and `__builtin_expect`.
- CMake 3.20+ and Ninja or Make.
- libc. Nothing else.

## Build

```sh
cmake -B build -G Ninja             # omit -G Ninja to use Make
cmake --build build
./build/tests                       # unit tests
./build/example_allocators          # examples/allocators.c
./build/main                        # scratch program, src/main.c
./build/bench                       # benchmarks: build Release for real numbers
```

CMake prefers Clang and falls back to GCC. Pass `-DCMAKE_C_COMPILER=gcc` to choose.

### Build types

Pass `-DCMAKE_BUILD_TYPE=<type>`:

| Type | Flags | Use it for |
|---|---|---|
| `Debug` (default) | `-O0`, AddressSanitizer + UndefinedBehaviorSanitizer | Day-to-day development and tests |
| `DebugNoSan` | `-O0`, no sanitizers | `gdb` / `lldb` sessions |
| `Release` | `-O3 -DNDEBUG -march=native -ffast-math` | Benchmarks (`./build-rel/bench`) and shipping |

`Release` defines `NDEBUG`, which strips every `WC_ASSERT`. Run the tests in both `Debug` and `Release`: some behavior differs between them by design. See [Diagnostics](diagnostics.md).

If your Clang has no sanitizer runtime installed, the `Debug` link fails. Use GCC or `DebugNoSan`.

`-DWC_WARNINGS_AS_ERRORS=ON` adds `-Werror`. Three warnings are always errors: `incompatible-pointer-types`, `implicit-function-declaration` and `int-conversion`.

## Using it in your project

There is no installed library target. Add the headers and the sources to your own build:

```cmake
file(GLOB WC_SOURCES path/to/wctoolkit/src/*.c)
list(REMOVE_ITEM WC_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/path/to/wctoolkit/src/main.c)

add_executable(app main.c ${WC_SOURCES})
target_include_directories(app PRIVATE path/to/wctoolkit/include)
target_link_libraries(app m)        # libm: needed by the test suite, harmless otherwise
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

    // A map of String -> int, also on libc
    HashMap ages = MAP_OF(String, int);
    MAP_PUT_STR_INT(&ages, "ada", 36);
    MAP_PUT_STR_INT(&ages, "alan", 41);

    printf("ada = %d\n", MAP_GET_CSTR(&ages, int, "ada"));   // lookup builds no String
    HashMap_destroy(&ages);

    // The same kind of work on an arena that frees itself
    ARENA_SCOPE(tmp, nKB(4)) {
        GenVec words = VEC_OF_IN(tmp, String, 4);
        VEC_PUSH_CSTR(&words, "no destroy needed");
    }
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
