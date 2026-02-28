cmake_minimum_required(VERSION 3.20)

set(CMAKE_C_COMPILER clang)

project(
    WCtoolkit
    VERSION 0.6.0
    DESCRIPTION "A C systems toolkit where ownership is part of the API"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")

# Default to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Debug: sanitizers + no optimisation
set(CMAKE_C_FLAGS_DEBUG   "-g -O0 -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "-fsanitize=address -fsanitize=undefined")

# Release: full optimisation
set(CMAKE_C_FLAGS_RELEASE   "-O3 -DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "")


# ── Shared source list ────────────────────────────────────────────────────
set(LIB_SOURCES
    src/arena.c
    src/bit_vector.c
    src/fast_math.c
    src/gen_vector.c
    src/hashmap.c
    src/hashset.c
    src/matrix.c
    src/Queue.c
    src/random.c
    src/Stack.c
    src/String.c
    src/wc_errno.c      # defines _Thread_local wc_errno
)


# ── Main executable ───────────────────────────────────────────────────────
add_executable(main
    src/main.c
    ${LIB_SOURCES}
)
target_include_directories(main PRIVATE include)


# ── Test executable ───────────────────────────────────────────────────────
add_executable(tests
    tests/test_main.c
    tests/string_test.c
    tests/arena_test.c
    tests/gen_vector_test.c
    tests/hashmap_test.c
    tests/hashset_test.c
    tests/stack_queue_test.c
    tests/matrix_test.c
    tests/bit_vector_test.c
    tests/complex_test.c
    ${LIB_SOURCES}
)
target_include_directories(tests PRIVATE include tests)
target_link_libraries(tests)

# Register with CTest so `ctest` works from the build directory
enable_testing()
add_test(NAME unit_tests COMMAND tests)


# Example programs
add_executable(examples
    examples/examples.c
    examples/json_parser.c
    ${LIB_SOURCES}
)
target_include_directories(examples PRIVATE include)

# ── Usage comments ───────────────────────────────────────────────────────
# Debug build (default):
#   cmake -B build -G Ninja
#   cmake --build build
#   ./build/tests
#
# Release build:
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Run via CTest (shows pass/fail summary):
#   ctest --test-dir build --output-on-failure
