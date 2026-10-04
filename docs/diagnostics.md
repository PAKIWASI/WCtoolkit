# Diagnostics

[Back to README](../README.md)

The toolkit reports problems at three levels, and the build type decides which checks exist.

| Level | Mechanism | Debug | Release (`NDEBUG`) | For |
|---|---|---|---|---|
| Absolute failure | `FATAL_IF` | Aborts | **Aborts** | Allocation failed; mutating a dead (zeroed) container |
| Programmer error | `WC_ASSERT` | Aborts | **Removed**; the condition isn't evaluated | Bad index, wrong element type, API misuse |
| Expected condition | `wc_errno` | Sets code, returns | Sets code, returns | Pop on empty, arena full |

`WARN_IF` and `LOG_IF` report something and carry on, in every build.

## The macros

All defined in `common.h`. Every one is an expression of type `void`, so it works anywhere an expression does.

### In every build

| Macro | Behavior |
|---|---|
| `FATAL(fmt, ...)` | Call the fatal handler. The default prints `[FATAL] file:line:func(): message` to stderr and exits with `EXIT_FAILURE`. |
| `FATAL_IF(cond, fmt, ...)` | `FATAL` if `cond` is **true** |
| `WARN(fmt, ...)` | Print `[WARN] ...` to stderr, continue |
| `WARN_IF(cond, fmt, ...)` | `WARN` if `cond` is true |
| `WARN_IF_RET(cond, ret, fmt, ...)` | `WARN` and `return ret` from the **calling function** if `cond` is true. Leave `ret` empty in a `void` function. |
| `LOG(fmt, ...)` | Print `[LOG] func(): message` to stderr, like every other diagnostic |
| `LOG_IF(cond, fmt, ...)` | `LOG` if `cond` is true |

### Custom fatal handler

```c
typedef void (*wc_fatal_fn)(const char* file, int line, const char* func, const char* msg);
wc_fatal_fn wc_set_fatal_handler(wc_fatal_fn fn);   // returns the previous one; NULL = default
```

Every `FATAL` formats its message and calls the installed handler. The handler must not return: abort, exit, or `longjmp` out (that's how a test can check that something is fatal without forking). If it does return, the program exits anyway. The handler is global, not thread-local: set it once at startup.

### Removed under `NDEBUG`

| Macro | Behavior |
|---|---|
| `WC_ASSERT(cond, fmt, ...)` | Aborts if `cond` is **false**. `cond` is the invariant that must hold, such as `i < size`. |

In Release, `WC_ASSERT` becomes `(void)sizeof(!(cond))`:
- **Not evaluated:** function calls and side effects in `cond` do not run.
- **Still compiled:** a typo or a renamed field inside an assert still breaks the Release build. Variables used only in asserts don't produce unused warnings.

## Which one to use

- If continuing would corrupt memory **even in a correct program**, use `FATAL_IF`. The standard case is an allocator returning `NULL`.
- If the failure means the **caller has a bug**, use `WC_ASSERT`. Out-of-range index, element size mismatch, mismatched matrix dimensions.
- If the condition is **normal at runtime** and the caller may want to react, set `wc_errno` and return.
- Never `WC_ASSERT` an allocation: in Release it disappears and the code continues with `NULL`.

Note the opposite polarity: `FATAL_IF(i >= n, ...)` and `WC_ASSERT(i < n, ...)` describe the same check.

```c
#include "common.h"

#include <stdlib.h>

static int at(const int* xs, u64 n, u64 i)
{
    WC_ASSERT(i < n, "index %llu out of range %llu", (unsigned long long)i, (unsigned long long)n);
    return xs[i];
}

int main(void)
{
    int* xs = malloc(4 * sizeof(int));
    FATAL_IF(!xs, "out of memory");          // survives Release
    for (int i = 0; i < 4; i++) { xs[i] = i; }
    LOG_IF(at(xs, 4, 3) == 3, "last element is %d", 3);
    WARN_IF(xs[0] != 0, "unexpected first element");
    free(xs);
    return 0;
}
```

## wc_errno

`wc_errno` is a thread-local error code for **expected** conditions. Functions that hit one set it and return early (`NULL`, `0`, or nothing). They never clear it, so reset it yourself before the calls you want to check:

```c
#include "arena.h"
#include "wc_errno.h"

int main(void)
{
    Arena arena;
    Arena_create(&arena, WC_LIBC, 64);

    wc_errno = WC_OK;
    void* p = Arena_alloc(&arena, 1024);       // too big
    int full = (p == NULL && wc_errno == WC_ERR_FULL);

    Arena_destroy(&arena);
    return full ? 0 : 1;
}
```

| Code | Set by |
|---|---|
| `WC_OK` | Nothing; reset value |
| `WC_ERR_FULL` | `Arena_alloc` / `Arena_alloc_aligned` when the arena is exhausted; `ChainArena_alloc` / `ChainArena_alloc_aligned` when its backing allocator can't supply a new node |
| `WC_ERR_EMPTY` | Pop and peek on an empty container: `GenVec_pop` / `front` / `back`, `Stack_pop` / `peek`, `Queue_pop` / `pop_back` / `peek` / `peek_ptr`, `BitVec_pop` |

Because the code is never cleared automatically, you can check a whole batch at once: reset, make several calls, then check once.

Inside the library, `WC_SET_RET(code, cond, ret)` sets `wc_errno` and returns, and `WC_PROPAGATE_RET(ret)` returns if a callee already set it.

## Branch prediction and code layout

- Failure paths are wrapped in `WC_UNLIKELY(...)` and success paths in `WC_LIKELY(...)`. Both are `__builtin_expect` and are available to your code too.
- `FATAL` and `WARN` report through `wc_fatal_report` / `wc_warn_report`. These are `cold, noinline` functions defined once in `wc_errno.c`.
- As a result, the compiler moves every failure branch out of the hot code into `.text.unlikely`. A check costs one comparison and one not-taken branch.
- `LOG_IF` has no hint: a log condition has no predictable direction.
- Growth branches (`size >= capacity`) are marked unlikely, which is correct by construction because growth is amortized.

Don't add `WC_LIKELY` / `WC_UNLIKELY` to branches that depend on your data, such as hash probing or sorting. A wrong hint costs more than no hint. Use profile-guided optimization (`-fprofile-generate`, then `-fprofile-use`) for those.

## Testing that something aborts

The test harness can assert that a call terminates. See [Testing](testing.md#death-tests). Remember that a `WC_ASSERT` death test only makes sense in Debug: guard it with `#ifndef NDEBUG`.
