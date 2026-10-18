cmake_minimum_required(VERSION 3.20)

# Prefer clang, fall back to gcc. Windows builds use MinGW-w64 clang/gcc, or
# an x86_64-w64-mingw32 cross toolchain. MSVC / clang-cl are not supported.
if(NOT CMAKE_C_COMPILER AND DEFINED ENV{CC})
  set(CMAKE_C_COMPILER "$ENV{CC}")
endif()

if(NOT CMAKE_C_COMPILER)
  find_program(WC_CLANG_PATH clang)
  if(CMAKE_HOST_WIN32 AND WC_CLANG_PATH)
    # A clang from llvm.org targets MSVC on Windows and needs the MSVC
    # toolchain; skip it. MSYS2's clang targets MinGW and is accepted.
    execute_process(COMMAND ${WC_CLANG_PATH} -dumpmachine
        OUTPUT_VARIABLE WC_CLANG_TARGET OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(WC_CLANG_TARGET MATCHES "msvc")
      set(WC_CLANG_PATH "")
    endif()
  endif()

  find_program(WC_GCC_PATH gcc)
  if(WC_CLANG_PATH)
    set(CMAKE_C_COMPILER ${WC_CLANG_PATH})
  elseif(WC_GCC_PATH)
    set(CMAKE_C_COMPILER ${WC_GCC_PATH})
  endif()

  if(CMAKE_HOST_WIN32 AND NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "No MinGW-w64 clang/gcc found.\n"
        "Windows builds are MinGW-w64 only:\n"
        "  - native: open an MSYS2 UCRT64 shell (pacman -S mingw-w64-ucrt-x86_64-clang ...)\n"
        "  - cross:  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_SYSTEM_NAME=Windows")
  endif()
endif()

project(
    WCtoolkit
    VERSION 0.8.0
    DESCRIPTION "A C systems toolkit where ownership is part of the API"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

message(STATUS "C Compiler: ${CMAKE_C_COMPILER} (${CMAKE_C_COMPILER_ID})")

# defaults to Debug for development
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# -march value. Cross compilers usually reject "native": pass
# -DWC_MARCH=x86-64-v2 (or -DWC_MARCH= to omit the flag).
set(WC_MARCH native CACHE STRING "Value for -march (empty to omit)")
set(WC_MARCH_FLAG "")
if(WC_MARCH)
  set(WC_MARCH_FLAG " -march=${WC_MARCH}")
endif()

if(WIN32)
  # Windows is release-only: no sanitizer runtime, plain Debug if used.
  set(CMAKE_C_FLAGS_DEBUG "-g -O0${WC_MARCH_FLAG}")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "")
  set(CMAKE_C_FLAGS_DEBUGNOSAN "-g -O0${WC_MARCH_FLAG}")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUGNOSAN "")
  # Static libgcc/libwinpthread: a single self-contained main.exe.
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-static")
else()
  # Debug: sanitizers + no optimisation
  set(CMAKE_C_FLAGS_DEBUG
  "-g -O0 -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined${WC_MARCH_FLAG}")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG
  "-fsanitize=address -fsanitize=undefined")

  # DebugNoSan: debug info, no sanitizers - clean for lldb/gdb
  set(CMAKE_C_FLAGS_DEBUGNOSAN
  "-g -O0 -fno-omit-frame-pointer${WC_MARCH_FLAG}")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUGNOSAN "")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "")
endif()

set(VALID_BUILD_TYPES Debug Release DebugNoSan)
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${VALID_BUILD_TYPES})

# Release: full optimisation. -ffast-math is NOT global: it lets the compiler assume
# no NaN/inf and reassociate floats, which would silently change every float path in
# the library and its tests. It is applied to src/fast_math.c only
set(CMAKE_C_FLAGS_RELEASE
"-O3 -DNDEBUG${WC_MARCH_FLAG}")

option(WC_VECTORIZE_REPORT "Clang loop-vectorize remarks for every src/*.c (disables LTO)" OFF)

# Whole-program optimisation in Release: containers live in separate .c files, so
# without LTO every push/hash/compare is an opaque call from the caller's side.
# LTO is skipped while the vectorize report is on: with LTO the vectorizer runs at link
# time, so the per-loop remarks would never show up at compile time.
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT WC_VECTORIZE_REPORT)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT WC_IPO_OK OUTPUT WC_IPO_MSG LANGUAGES C)
  if(WC_IPO_OK)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(STATUS "LTO not available: ${WC_IPO_MSG}")
  endif()
endif()

set_source_files_properties(src/fast_math.c PROPERTIES
    COMPILE_OPTIONS "$<$<CONFIG:Release>:-ffast-math>")

# Clang-only vectorization report (gcc rejects -Rpass). Off by default: it emits one
# remark per loop in every file, which buries real warnings. Scoped to the files where
# vectorization matters.

# Benchmark element count per iteration (override: -DWC_BENCH_N=1000).
set(WC_BENCH_N 100000 CACHE STRING "Elements per benchmark iteration")


option(WC_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

set(WC_WARN_FLAGS
    -Wall
    -Wextra
    -Wundef
    -Wshadow
    -Wformat=2
    -Wconversion
    -Wpointer-arith
    -Wwrite-strings
    -Wsign-conversion
    -Wnull-dereference
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wno-unused-parameter
    -Wold-style-definition
    -Wimplicit-fallthrough
    -Werror=incompatible-pointer-types
    -Werror=implicit-function-declaration
    -Werror=int-conversion
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
    src/arena.c
    src/chain_arena.c
    src/bit_vector.c
    src/fast_math.c
    src/gen_vector.c
    src/hashmap.c
    src/queue.c
    src/priority_queue.c
    src/random.c
    src/stack.c
    src/wc_allocator.c
    src/wc_string.c
    src/wc_errno.c
    src/views.c
)


# Vectorization report for the library sources (not tests or headers' callers).
if(WC_VECTORIZE_REPORT AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
  set_property(SOURCE ${LIB_SOURCES} APPEND PROPERTY COMPILE_OPTIONS
      -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize)
endif()


# Main executable
add_executable(main
    src/main.c
    ${LIB_SOURCES}
)
target_include_directories(main PRIVATE include)
target_compile_options(main PRIVATE ${WC_WARN_FLAGS})


# Test executable
# Tests: one tests/test_*.c per component, all registered through utest.h.
# Debug build (default on Linux) runs them under ASan + UBSan.
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.c)
add_executable(tests
    tests/main.c
    tests/wc_test_allocator.c
    ${TEST_SOURCES}
    ${LIB_SOURCES}
)
target_include_directories(tests PRIVATE include tests)
target_compile_options(tests PRIVATE ${WC_WARN_FLAGS})
# libm is separate on Linux/macOS; the MinGW CRT includes it.
if(NOT WIN32)
  target_link_libraries(tests m)
endif()
# test_checking_allocator / test_wc_allocator hand stack buffers to allocators on
# purpose. Their #pragma covers compile time; with LTO, GCC re-checks at link time.
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_link_options(tests PRIVATE -Wno-free-nonheap-object)
endif()
#
#
# # Benchmarks: one tests/bench_*.c per component, registered through ubench.h.
# # Meaningful only in a Release build (Debug runs them under the sanitizers).
# file(GLOB BENCH_SOURCES CONFIGURE_DEPENDS tests/bench_*.c)
# add_executable(bench
#     ${BENCH_SOURCES}
#     ${LIB_SOURCES}
# )
# target_include_directories(bench PRIVATE include tests)
# target_compile_options(bench PRIVATE ${WC_WARN_FLAGS})
# target_compile_definitions(bench PRIVATE BENCH_N=${WC_BENCH_N})
# target_link_libraries(bench m)


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
