# Math

[Back to README](../README.md)

| Module | Header | What it is |
|---|---|---|
| [`Matrixf`](#matrixf) | `matrix.h` | Row-major `float` matrix |
| [`fast_math`](#fast_math) | `fast_math.h` | Fast, low-precision `sqrt`, `log`, `exp`, `sin`, `cos`, `pow` |
| [`random`](#random) | `random.h` | PCG32 random numbers |

## Matrixf

A row-major `float` matrix that stores its allocator, 40 bytes. It follows the same create / destroy / copy / move rules as the containers.

```c
#include "matrix.h"

int main(void)
{
    Matrixf a = matrix_create_arr(WC_LIBC, 2, 3, (float[]){1, 2, 3, 4, 5, 6});
    Matrixf b = matrix_create_arr(WC_LIBC, 3, 2, (float[]){7, 8, 9, 10, 11, 12});

    float   out_data[4];
    Matrixf out = matrix_create_buf(2, 2, out_data);   // result on the stack
    matrix_xply(&out, &a, &b);                          // [[58, 64], [139, 154]]

    matrix_destroy(&a);
    matrix_destroy(&b);
    return 0;
}
```

| Group | Functions |
|---|---|
| Create | `matrix_create(a, m, n)` (contents uninitialized), `matrix_create_arr(a, m, n, arr)`, `matrix_create_buf(m, n, data)` (your memory; destroy frees nothing), `MATRIX(m, n)`, `MATRIX_IN(A, m, n)` |
| Lifetime | `matrix_destroy`, `matrix_copy(a, &src)`, `matrix_move(&dst, &src)` |
| Set / get | `matrix_set_val_arr(mat, count, arr)`, `matrix_set_val_arr2(mat, m, n, rows)`, `matrix_set_elm(mat, v, i, j)`, `matrix_get_elm(mat, i, j)`, `MATRIX_AT(mat, i, j)` |
| Element-wise | `matrix_add(out, a, b)`, `matrix_sub(out, a, b)`, `matrix_scale(mat, k)`, `matrix_div(mat, k)` |
| Multiply | `matrix_xply(out, a, b)` (blocked i-k-j), `matrix_xply_2(out, a, b)` (transposes `b` first) |
| Other | `matrix_T(out, mat)`, `matrix_LU_Decomp(L, U, mat)`, `matrix_det(mat)`, `matrix_inv(out, mat)`, `matrix_print(mat)` |

Rules:
- Dimensions must be greater than zero. A zero dimension aborts in every build.
- Every operation writes into an `out` you provide, with matching dimensions. Mismatched dimensions are a `WC_ASSERT`.
- `out` must not alias an input in `add`, `sub`, `xply`, `xply_2` and `T`. They are declared `restrict`.
- `matrix_inv(out, mat)` returns `false` for a singular matrix. It runs Gauss-Jordan with partial pivoting in place in `out`, so it allocates nothing and handles zero leading pivots.
- `matrix_T` works in square tiles of `WC_MAT_BLOCK` (default 16). Define it before building to tune for your cache.

Limitations to know:
- **LU has no pivoting.** A zero pivot is treated as singular, even when the matrix isn't. For example, `matrix_det` of `[[0, 1], [1, 0]]` should be `-1`. Instead it aborts in Debug, and in Release it divides by zero.
- **`matrix_xply_2` and `matrix_det` put temporaries on the stack**, sized `n × k` and `2 × n × n` floats. Large matrices (a 1000×1000 `det` needs about 8 MB) will overflow the stack. Use `matrix_xply` for large inputs.

## fast_math

Approximations from Newton-Raphson iteration and Taylor series. They're faster than libm and much less precise. Use them where approximate is fine, such as random-number shaping or weight initialization. Avoid them for anything numerically sensitive.

| Function | Method | Tested against libm within |
|---|---|---|
| `fast_sqrt(x)` | Newton-Raphson | 1e-4 relative |
| `fast_log(x)` | Series for `ln(1 + x)` | 1e-4 relative |
| `fast_exp(x)` | Taylor series | 1e-4 relative |
| `fast_sin(x)`, `fast_cos(x)` | Taylor series after range folding | 1e-3 absolute |
| `fast_pow(b, e)` | Squaring for the integer part, `exp(f * log(b))` for the fraction | |
| `fast_ceil(x)` | Exact | Bit-exact with `ceilf` |

The tolerances are the ones `tests/fast_math_test.c` checks over its test inputs, not guarantees for every input. `PI`, `TWO_PI` and `LN2` are defined as `float` constants.

## random

PCG32 (XSH-RR): 64 bits of state, 32-bit output, statistically strong and fast. It is not cryptographically secure.

```c
#include "random.h"

#include <stdio.h>

int main(void)
{
    WC_Pcg32 rng = PCG32_INITIALIZER;
    pcg32_rand_seed(&rng, 42, 1);                   // reproducible: same seed, same sequence
    u32   die = pcg32_rand_bounded(&rng, 6) + 1;    // 1..6, no modulo bias
    float u   = pcg32_rand_float(&rng);             // [0, 1)
    float g   = pcg32_rand_gaussian_custom(&rng, 100.0f, 15.0f);
    printf("%u %f %f\n", die, u, g);
    return 0;
}
```

| Function | Returns |
|---|---|
| `pcg32_rand_seed(&rng, seed, seq)` | Seeds; `seq` selects one of 2^63 independent streams |
| `pcg32_rand_seed_time(&rng)` | Seeds from `time(NULL)`. Two calls in the same second give the same sequence. |
| `pcg32_rand(&rng)` | Uniform `u32` |
| `pcg32_rand_bounded(&rng, n)` | Uniform in `[0, n)`, no modulo bias; `n > 0` |
| `pcg32_rand_float(&rng)`, `pcg32_rand_double(&rng)` | Uniform in `[0, 1)`; the double uses 53 random bits |
| `pcg32_rand_float_range(&rng, lo, hi)`, `pcg32_rand_double_range(&rng, lo, hi)` | Uniform in `[lo, hi)` |
| `pcg32_rand_gaussian(&rng)` | Normal, mean 0, standard deviation 1 (Box-Muller using `fast_math`) |
| `pcg32_rand_gaussian_custom(&rng, mean, sd)` | Normal with the given mean and standard deviation |

**Every function takes the generator.** There is no global RNG: each `WC_Pcg32` holds its own state, including the cached second Box-Muller value, so independent generators never interfere. Use one per thread.
