cmake_minimum_required(VERSION 3.20)

# Prefer clang, fall back to gcc
if(NOT CMAKE_C_COMPILER)
  find_program(WC_CLANG_PATH clang)
  find_program(WC_GCC_PATH gcc)
  if(WC_CLANG_PATH)
    set(CMAKE_C_COMPILER ${WC_CLANG_PATH})
  elseif(WC_GCC_PATH)
    set(CMAKE_C_COMPILER ${WC_GCC_PATH})
  endif()
endif()

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
set(CMAKE_COLOR_DIAGNOSTICS ON)

message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")

# Default to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Debug: sanitizers + no optimisation
set(CMAKE_C_FLAGS_DEBUG
"-g -O0 -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined -march=native")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG
"-fsanitize=address -fsanitize=undefined")

# DebugNoSan: debug info, no sanitizers - clean for lldb/gdb
set(CMAKE_C_FLAGS_DEBUGNOSAN
"-g -O0 -fno-omit-frame-pointer -march=native")
set(CMAKE_EXE_LINKER_FLAGS_DEBUGNOSAN "")

set(VALID_BUILD_TYPES Debug Release DebugNoSan)
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${VALID_BUILD_TYPES})

# Release: full optimisation
set(CMAKE_C_FLAGS_RELEASE
"-O3 -DNDEBUG -march=native -ffast-math")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "")

# Clang-only vectorization diagnostics — gcc rejects -Rpass. Re-add per compiler.
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  String(APPEND CMAKE_C_FLAGS_DEBUG " -Rpass=loop-vectorize")
  String(APPEND CMAKE_C_FLAGS_RELEASE " -Rpass=loop-vectorize -Rpass-missed=loop-vectorize")
endif()


option(WC_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

set(WC_WARN_FLAGS
    -Wall
    -Wextra
    -Wundef
    -Wshadow
    -Wformat=2
    -Wconversion
    -Wpointer-arith
    -Wwrite-Strings
    -Wsign-conversion
    -Wnull-dereference
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wno-unused-parameter
    -Wold-style-definition
    -Wimplicit-fallthrough
)

if(WC_WARNINGS_AS_ERRORS)
  list(APPEND WC_WARN_FLAGS -Werror)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  list(APPEND WC_WARN_FLAGS -Wno-nonnull-compare)
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  list(APPEND WC_WARN_FLAGS -Wno-pointer-bool-conversion)
endif()
message(STATUS "Warnings-as-errors: ${WC_WARNINGS_AS_ERRORS}")


# Shared source list
set(LIB_SOURCES
    src/Arena.c
    src/chain_Arena.c
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
    src/views.c
)


# Main executable
add_executable(main
    src/main.c
    ${LIB_SOURCES}
)
target_include_directories(main PRIVATE include)
target_compile_options(main PRIVATE ${WC_WARN_FLAGS})


# Test executable
add_executable(tests
    tests/test_main.c
    tests/String_test.c
    tests/Arena_test.c
    tests/gen_vector_test.c
    tests/hashmap_test.c
    tests/hashset_test.c
    tests/Stack_Queue_test.c
    tests/matrix_test.c
    tests/bit_vector_test.c
    tests/fast_math_test.c
    tests/complex_test.c
    tests/macros_test.c
    tests/test_views.c
    tests/speed_test.c
    ${LIB_SOURCES}
)
target_include_directories(tests PRIVATE include tests)
target_compile_options(tests PRIVATE ${WC_WARN_FLAGS})
target_link_libraries(tests m)

# Register with CTest so `ctest` works from the build directory
enable_testing()
add_test(NAME unit_tests COMMAND tests)


# Example projects

# # wcjson
# add_executable(wcjson
#     examples/wcjson/wcjson_main.c
#     examples/wcjson/wcjson.c
#     ${LIB_SOURCES}
# )
# target_include_directories(wcjson PRIVATE include)
# target_link_libraries(wcjson)
#
# # Leetcode
# add_executable(leetcode
#     examples/leetcode.c
#     ${LIB_SOURCES}
# )
# target_include_directories(leetcode PRIVATE include)
# target_link_libraries(leetcode)
