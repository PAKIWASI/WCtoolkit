# Testing

[Back to README](../README.md)

The tests live in `tests/` and build into one binary, `./build/tests`. They use a small harness (`tests/wc_test.h`), a death-test helper (`tests/wc_test_fatal.h`) and a checking allocator (`tests/wc_test_allocator.h`). None of these are part of the library.

```sh
cmake --build build && ./build/tests                    # Debug: ASan + UBSan
cmake -B build-rel -DCMAKE_BUILD_TYPE=Release && cmake --build build-rel && ./build-rel/tests
```

**Run both.** Release strips `WC_ASSERT` and turns on `-O3 -ffast-math`. Some bugs show up only there, such as a check that should survive Release but was written as an assert. The suite also includes a benchmark section (`speed_test.c`) comparing plain-data and owning element paths.

## Writing a test

Each test is a `static void` function with no arguments. Each suite runs its tests and is called from `tests/test_main.c`.

<!-- check: syntax-only, tests -->
```c
#include "wc_test.h"
#include "wc_macros.h"

static void test_push_and_read(void)
{
    GenVec v = VEC(int, 2);
    VEC_PUSH(&v, 7);
    WC_EXPECT_EQ_U64(GenVec_size(&v), 1);
    WC_EXPECT_EQ_INT(VEC_AT(&v, int, 0), 7);
    GenVec_destroy(&v);
}

void my_suite(void);
void my_suite(void)
{
    WC_SUITE("My feature");
    WC_RUN(test_push_and_read);
}
```

Then:
1. Declare and call `my_suite()` in `tests/test_main.c`.
2. Add the file to the `tests` target in `CMakeLists.txt`.

### Checks

Checks are **non-fatal**: a failure is counted and printed, and the test keeps running, so one run shows every failure. That's why they're `WC_EXPECT_*`. `WC_ASSERT` is the library's aborting invariant check, which is a different thing (see [Diagnostics](diagnostics.md)).

| Macro | Passes when |
|---|---|
| `WC_EXPECT(cond)`, `WC_EXPECT_TRUE(cond)` | `cond` is true |
| `WC_EXPECT_FALSE(cond)` | `cond` is false |
| `WC_EXPECT_EQ_INT(a, b)`, `WC_EXPECT_NEQ_INT(a, b)` | Compared as `long long`; prints both values on failure |
| `WC_EXPECT_EQ_U64(a, b)` | Compared as `unsigned long long` |
| `WC_EXPECT_EQ_STR(a, b)` | `strcmp(a, b) == 0` |
| `WC_EXPECT_NULL(p)`, `WC_EXPECT_NOT_NULL(p)` | |

### Running

| Macro | Meaning |
|---|---|
| `WC_SUITE("name")` | Prints a suite header |
| `WC_RUN(fn)` | Runs one test; prints `OK` or `FAIL (n assertion(s))` |
| `WC_RUN_XFAIL(fn)` | Runs a test for a **known, unfixed** defect. A failure prints `XFAIL` and counts as passed. A pass prints `XPASS` and counts as **failed**, telling you to switch it to `WC_RUN`. |
| `WC_REPORT()` | Prints the summary; returns `0` if everything passed. Use it as `main`'s return value. |

Exactly one file defines `WC_TEST_MAIN` before including `wc_test.h`: that's `test_main.c`, which owns the counters.

## Death tests

`WC_EXPECT_DIES(fn)` runs `void fn(void)` in a forked child with its output silenced, and passes if the child terminates by a non-zero exit or a signal. Use it for `FATAL_IF` and `WC_ASSERT` paths.

<!-- check: syntax-only, tests -->
```c
#include "wc_test.h"
#include "wc_test_fatal.h"
#include "wc_macros.h"

#include <string.h>

static void push_after_destroy(void)
{
    GenVec v = VEC(int, 2);
    GenVec_destroy(&v);
    int x = 1;
    GenVec_push(&v, &x);           // FATAL_IF: dead container
}

static void out_of_range(void)
{
    GenVec v = VEC(int, 2);
    (void)GenVec_get_ptr(&v, 5);   // WC_ASSERT: Debug only
}

static void test_deaths(void)
{
    WC_EXPECT_DIES(push_after_destroy);   // dies in every build
#ifndef NDEBUG
    WC_EXPECT_DIES(out_of_range);         // asserts don't exist in Release
#endif
}
```

Death tests need `fork()`, so they're POSIX only.

## The test allocator

`wc_test_alloc` wraps any allocator and checks every call against a record of live blocks. Run a container on it to prove it's leak-free and calls `free` with the right sizes:

<!-- check: syntax-only, tests -->
```c
#include "wc_test.h"
#include "wc_test_allocator.h"
#include "wc_macros.h"

static void test_vector_is_leak_free(void)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);                 // any backing allocator
    wc_allocator a = wc_test_alloc_allocator(&ta);

    GenVec v = VEC_OF_IN(a, String, 2);
    for (int i = 0; i < 50; i++) { VEC_PUSH_CSTR(&v, "a string long enough for the heap"); }
    GenVec_destroy(&v);

    WC_EXPECT_EQ_U64(ta.n_errors, 0);
    WC_EXPECT_EQ_U64(wc_test_alloc_destroy(&ta), 0);  // returns the number of leaked blocks
}
```

It detects:
- double frees, and reallocs of freed blocks;
- pointers it never handed out;
- `free` / `realloc` called with a size or alignment different from the block's last allocation;
- zero-size allocations, non-power-of-two alignments, and misaligned blocks from the backing allocator.

It also provides:

| Feature | How |
|---|---|
| Counters | `ta.n_alloc`, `ta.n_realloc`, `ta.n_free`, `ta.live_blocks`, `ta.live_bytes`, `ta.peak_bytes` |
| Fault injection | `wc_test_alloc_fail_at(&ta, n, sticky)`: the `n`-th allocation returns `NULL` (and every later one, if `sticky`) |
| Ownership | `wc_test_alloc_owns(&ta, p)` |
| Memory patterns | New bytes are filled with `0xBE`, freed bytes poisoned with `0xDD` |
| Leak report | `wc_test_alloc_destroy` prints each leaked block, frees it, and returns the count |

By default a contract violation prints and aborts (`WC_TA_ABORT`). Set `ta.mode = WC_TA_RECORD` to count errors instead; the allocator's own tests use this.

Fault injection pairs with death tests: fail the `n`-th allocation inside a forked child, and check that the container aborts cleanly instead of using `NULL`. `tests/gen_vector_alloc_test.c` does this for every allocation a workload makes.

## Known gaps

- The HashMap and HashSet zero-state death tests are inside `#if WC_HAS_FORK`, and nothing defines `WC_HAS_FORK`. They are compiled out until it is defined.
