# Testing

[Back to README](../README.md)

Everything lives in `tests/`: one test file and at most one benchmark file per component.

| File | What it is |
|---|---|
| `test_<component>.c` | The tests for one component, for example `test_hashmap.c` |
| `bench_<component>.c` | Benchmarks for one component |
| `main.c`, `bench_main.c` | One-line entry points for the two binaries |
| `utest.h`, `ubench.h` | [utest.h](https://github.com/sheredom/utest.h) and [ubench.h](https://github.com/sheredom/ubench.h), single-header, public domain, vendored unchanged |
| `test_support.h` | Includes `utest.h` and adds `EXPECT_DIES` |
| `bench_support.h` | Includes `ubench.h`, sets `N`, shared strings |
| `wc_test_allocator.c/.h` | The checking allocator |

## Running

```sh
cmake -B build && cmake --build build
./build/tests                              # all tests, under ASan + UBSan
./build/tests --filter=hashmap.*           # one component
./build/tests --list-tests

cmake -B build-rel -DCMAKE_BUILD_TYPE=Release && cmake --build build-rel
./build-rel/tests                          # the same tests, with asserts stripped
./build-rel/bench                          # benchmarks: Release only
./build-rel/bench --filter=memory.* --output=bench.csv
```

**Run the tests in both Debug and Release.** Release strips `WC_ASSERT` and enables `-O3 -ffast-math`, and some bugs only show there.

**Run benchmarks only in Release.** Debug numbers are measured under the sanitizers and mean nothing. Under Debug, ubench may also take minutes to reach a stable result.

## Writing a test

Add a `UTEST(component, name)` to the component's file. That's all: it registers itself, and CMake picks up any new `tests/test_*.c` automatically.

<!-- check: syntax-only, tests -->
```c
#include "test_support.h"
#include "wc_macros.h"

UTEST(gen_vector, push_then_read)
{
    GenVec v = VEC(int, 2);
    VEC_PUSH(&v, 7);
    EXPECT_EQ(GenVec_size(&v), 1u);
    EXPECT_EQ(VEC_AT(&v, int, 0), 7);
    GenVec_destroy(&v);
}
```

`EXPECT_*` records a failure and continues the test. `ASSERT_*` records it and stops the test. The ones in use:

| Check | Passes when |
|---|---|
| `EXPECT_TRUE(c)`, `EXPECT_FALSE(c)` | `c` is true / false |
| `EXPECT_EQ(a, b)`, `EXPECT_NE(a, b)` | `a == b` / `a != b`; prints both values on failure |
| `EXPECT_LT`, `EXPECT_LE`, `EXPECT_GT`, `EXPECT_GE` | The comparison holds |
| `EXPECT_STREQ(a, b)` | `strcmp(a, b) == 0` |
| `EXPECT_NEAR(a, b, eps)` | `a` and `b` differ by at most `eps` |
| `EXPECT_DIES(fn)` | `fn()` terminates; see below |

`EXPECT_EQ` compares in the types you give it. Comparing an unsigned value with a signed literal is a `-Wsign-compare` warning, so write `1u`, or cast: `EXPECT_EQ(GenVec_size(&v), (u64)n)`.

A check only works directly inside a `UTEST` body. A helper function should return a result, which the test then checks: `EXPECT_TRUE(workload(&v))`.

## Death tests

`EXPECT_DIES(fn)` runs `void fn(void)` in a forked child with its output silenced, and passes if the child terminates (non-zero exit or a signal). Use it for `FATAL_IF` and `WC_ASSERT` paths.

<!-- check: syntax-only, tests -->
```c
#include "test_support.h"
#include "wc_macros.h"

static void push_after_destroy(void)
{
    GenVec v = VEC(int, 2);
    GenVec_destroy(&v);
    int x = 1;
    GenVec_push(&v, &x);                   // FATAL_IF: dead container
}

static void out_of_range(void)
{
    GenVec v = VEC(int, 2);
    (void)GenVec_get_ptr(&v, 5);           // WC_ASSERT: Debug only
}

UTEST(gen_vector, misuse_dies)
{
    EXPECT_DIES(push_after_destroy);       // in every build
#ifndef NDEBUG
    EXPECT_DIES(out_of_range);             // asserts don't exist in Release
#endif
}
```

Death tests need `fork()`, so they're POSIX only.

## The checking allocator

`wc_test_alloc` wraps any allocator and checks every call against a record of live blocks. Run a container on it to prove it's leak-free and frees with the right sizes:

<!-- check: syntax-only, tests -->
```c
#include "test_support.h"
#include "wc_macros.h"
#include "wc_test_allocator.h"

UTEST(gen_vector, strings_leak_free)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    wc_allocator a = wc_test_alloc_allocator(&ta);

    GenVec v = VEC_OF_IN(a, String, 2);
    for (int i = 0; i < 50; i++) { VEC_PUSH_CSTR(&v, "a string long enough for the heap"); }
    GenVec_destroy(&v);

    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);     // number of leaked blocks
}
```

It catches:
- double frees, and reallocs of freed blocks;
- pointers it never handed out;
- `free` / `realloc` with a size or alignment different from the block's last allocation;
- zero-size allocations, non-power-of-two alignments, and misaligned blocks from the backing allocator.

| Feature | How |
|---|---|
| Counters | `ta.n_alloc`, `ta.n_realloc`, `ta.n_free`, `ta.live_blocks`, `ta.live_bytes`, `ta.peak_bytes` |
| Fault injection | `wc_test_alloc_fail_at(&ta, n, sticky)`: the `n`-th allocation returns `NULL` (and every later one if `sticky`) |
| Ownership | `wc_test_alloc_owns(&ta, p)` |
| Memory patterns | New bytes filled with `0xBE`, freed bytes poisoned with `0xDD` |
| Leak report | `wc_test_alloc_destroy` prints each leaked block, frees it, returns the count |

A contract violation prints and aborts by default. Set `ta.mode = WC_TA_RECORD` to count errors instead; the allocator's own tests do this.

## Writing a benchmark

Add a `UBENCH(component, name)` to `tests/bench_<component>.c`. ubench repeats the body until the timing is stable, then reports the mean and a 99% confidence interval. Each body works on `N` (1000) elements, so results read as "time per 1000".

<!-- check: syntax-only, tests -->
```c
#include "bench_support.h"

UBENCH(gen_vector, push_int)
{
    GenVec v = VEC(int, 0);
    for (int i = 0; i < N; i++) { VEC_PUSH(&v, i); }
    UBENCH_DO_NOTHING(v.data);                    // keep the work from being optimized away
    GenVec_destroy(&v);
}

UBENCH_EX(gen_vector, copy_int)                   // setup outside the timed part
{
    GenVec src = VEC(int, N);
    for (int i = 0; i < N; i++) { VEC_PUSH(&src, i); }
    UBENCH_DO_BENCHMARK()
    {
        GenVec c = GenVec_copy(WC_LIBC, &src);
        UBENCH_DO_NOTHING(c.data);
        GenVec_destroy(&c);
    }
    GenVec_destroy(&src);
}
```

Each benchmark comes in pairs where it makes a point: plain data vs an owning type (`push_int` / `push_string`), copy vs move (`push_struct_copy` / `push_struct_move`), or one allocator vs another (`memory.*`).

## Known gaps

- The HashMap and HashSet zero-state death tests sit inside `#if WC_HAS_FORK`, and nothing defines `WC_HAS_FORK`, so they're compiled out.
