cmake_minimum_required(VERSION 3.20)

set(CMAKE_C_COMPILER clang)

project(
    WCtoolkit
    VERSION 0.7.0
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
set(CMAKE_C_FLAGS_DEBUG
"-g -O0 -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined -march=native -Rpass=loop-vectorize")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG
"-fsanitize=address -fsanitize=undefined")

# DebugNoSan: debug info, no sanitizers - clean for codelldb
set(CMAKE_C_FLAGS_DEBUGNOSAN
"-g -O0 -fno-omit-frame-pointer -march=native")
set(CMAKE_EXE_LINKER_FLAGS_DEBUGNOSAN "")

set(VALID_BUILD_TYPES Debug Release DebugNoSan)
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${VALID_BUILD_TYPES})

# Release: full optimisation
set(CMAKE_C_FLAGS_RELEASE                           # this flag doesn't work in debug with Asan
"-O3 -DNDEBUG -march=native -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -ffast-math")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "")


# Shared source list
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
    src/wc_errno.c
)


# Main executable
add_executable(main
    src/main.c
    ${LIB_SOURCES}
)
target_include_directories(main PRIVATE include)


# Test executable 
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
    tests/fast_math_test.c
    tests/complex_test.c
    ${LIB_SOURCES}
)
target_include_directories(tests PRIVATE include tests tests/speed_tests)
target_link_libraries(tests m)

# Register with CTest so `ctest` works from the build directory
enable_testing()
add_test(NAME unit_tests COMMAND tests)


# Usage

# Debug build (default):
#   cmake -B build -G Ninja
#   cmake --build build
#   ./build/main
#   ./build/tests
#
# Release build:
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
