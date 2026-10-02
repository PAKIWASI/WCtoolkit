#include "fast_math.h"
#include "utest.h"

#include <math.h>


/* Tolerance 
 * fast_* functions trade precision for speed.
 * Tolerances are set to what the implementations actually achieve:
 *   fast_sqrt  — 4 Newton-Raphson iterations: ~1e-6 relative
 *   fast_log   — 5-term Taylor + range reduction: ~1e-4 relative
 *   fast_sin   — 4-term Taylor + range reduction: ~1e-4 absolute
 *   fast_cos   — delegated to fast_sin: ~1e-4 absolute
 *   fast_exp   — 7-term Taylor + repeated squaring: ~1e-4 relative
 *   fast_ceil  — exact integer arithmetic: exact
*/

#define EPS_TIGHT 1e-4f // relative tolerance for sqrt, exp, log
#define EPS_TRIG  1e-3f // absolute tolerance for sin, cos (Taylor accumulates)
#define EPS_EXACT 0.0f  // ceil must be bit-exact with ceilf

// Relative error: |got - ref| / (|ref| + guard)
static int rel_close(float got, float ref, float eps)
{
    float denom = fabsf(ref) + 1e-10f;
    return fabsf(got - ref) / denom <= eps;
}

// Absolute error
static int abs_close(float got, float ref, float eps)
{
    return fabsf(got - ref) <= eps;
}


// fast_sqrt

UTEST(fast_math, sqrt_zero)
{
    EXPECT_TRUE(fast_sqrt(0.0f) == 0.0f);
}

UTEST(fast_math, sqrt_one)
{
    EXPECT_TRUE(rel_close(fast_sqrt(1.0f), 1.0f, EPS_TIGHT));
}

UTEST(fast_math, sqrt_four)
{
    EXPECT_TRUE(rel_close(fast_sqrt(4.0f), 2.0f, EPS_TIGHT));
}

UTEST(fast_math, sqrt_perfect_squares)
{
    float cases[] = {9.0f, 16.0f, 25.0f, 49.0f, 100.0f, 10000.0f};
    for (int i = 0; i < 6; i++) {
        EXPECT_TRUE(rel_close(fast_sqrt(cases[i]), sqrtf(cases[i]), EPS_TIGHT));
    }
}

UTEST(fast_math, sqrt_non_integer)
{
    float cases[] = {2.0f, 3.0f, 0.5f, 0.1f, 7.5f, 123.456f};
    for (int i = 0; i < 6; i++) {
        EXPECT_TRUE(rel_close(fast_sqrt(cases[i]), sqrtf(cases[i]), EPS_TIGHT));
    }
}

UTEST(fast_math, sqrt_large)
{
    EXPECT_TRUE(rel_close(fast_sqrt(1e8f), sqrtf(1e8f), EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_sqrt(1e12f), sqrtf(1e12f), EPS_TIGHT));
}

UTEST(fast_math, sqrt_small)
{
    EXPECT_TRUE(rel_close(fast_sqrt(1e-4f), sqrtf(1e-4f), EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_sqrt(1e-6f), sqrtf(1e-6f), EPS_TIGHT));
}

UTEST(fast_math, sqrt_negative_returns_zero)
{
    // implementation clamps negative input to 0
    EXPECT_TRUE(fast_sqrt(-1.0f) == 0.0f);
    EXPECT_TRUE(fast_sqrt(-100.0f) == 0.0f);
}

UTEST(fast_math, sqrt_roundtrip)
{
    // sqrt(x)^2 should recover x within tolerance
    float cases[] = {2.0f, 7.0f, 42.0f, 0.3f};
    for (int i = 0; i < 4; i++) {
        float s = fast_sqrt(cases[i]);
        EXPECT_TRUE(rel_close(s * s, cases[i], EPS_TIGHT));
    }
}


// fast_log  (natural log)

UTEST(fast_math, log_one)
{
    // ln(1) = 0 — use absolute tolerance since ref is 0
    EXPECT_TRUE(abs_close(fast_log(1.0f), 0.0f, EPS_TIGHT));
}

UTEST(fast_math, log_e)
{
    // ln(e) = 1
    const float E = 2.718281828f;
    EXPECT_TRUE(rel_close(fast_log(E), 1.0f, EPS_TIGHT));
}

UTEST(fast_math, log_powers_of_two)
{
    // ln(2^n) = n * ln(2) — exercises the range-reduction path
    float cases[] = {2.0f, 4.0f, 8.0f, 16.0f, 32.0f};
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(rel_close(fast_log(cases[i]), logf(cases[i]), EPS_TIGHT));
    }
}

UTEST(fast_math, log_general)
{
    float cases[] = {0.5f, 0.1f, 1.5f, 3.0f, 10.0f, 100.0f, 1000.0f};
    for (int i = 0; i < 7; i++) {
        EXPECT_TRUE(rel_close(fast_log(cases[i]), logf(cases[i]), EPS_TIGHT));
    }
}

UTEST(fast_math, log_small_positive)
{
    float cases[] = {0.01f, 0.001f, 1e-4f};
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(rel_close(fast_log(cases[i]), logf(cases[i]), EPS_TIGHT));
    }
}

UTEST(fast_math, log_nonpositive_returns_sentinel)
{
    // implementation returns -1e10f for x <= 0
    EXPECT_TRUE(fast_log(0.0f) < -1e9f);
    EXPECT_TRUE(fast_log(-1.0f) < -1e9f);
}

UTEST(fast_math, log_exp_inverse)
{
    // log(exp(x)) ≈ x
    float cases[] = {0.0f, 0.5f, 1.0f, 2.0f, 5.0f};
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(abs_close(fast_log(fast_exp(cases[i])), cases[i], EPS_TIGHT));
    }
}


// fast_sin

UTEST(fast_math, sin_zero)
{
    EXPECT_TRUE(abs_close(fast_sin(0.0f), 0.0f, EPS_TRIG));
}

UTEST(fast_math, sin_half_pi)
{
    EXPECT_TRUE(abs_close(fast_sin(PI / 2.0f), 1.0f, EPS_TRIG));
}

UTEST(fast_math, sin_pi)
{
    EXPECT_TRUE(abs_close(fast_sin(PI), 0.0f, EPS_TRIG));
}

UTEST(fast_math, sin_neg_half_pi)
{
    EXPECT_TRUE(abs_close(fast_sin(-PI / 2.0f), -1.0f, EPS_TRIG));
}

UTEST(fast_math, sin_general)
{
    float cases[] = {0.1f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f};
    for (int i = 0; i < 7; i++) {
        EXPECT_TRUE(abs_close(fast_sin(cases[i]), sinf(cases[i]), EPS_TRIG));
    }
}

UTEST(fast_math, sin_negative)
{
    float cases[] = {-0.5f, -1.0f, -2.0f, -PI / 4.0f};
    for (int i = 0; i < 4; i++) {
        EXPECT_TRUE(abs_close(fast_sin(cases[i]), sinf(cases[i]), EPS_TRIG));
    }
}

UTEST(fast_math, sin_odd_symmetry)
{
    // sin(-x) = -sin(x)
    float cases[] = {0.3f, 1.2f, 2.7f};
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(abs_close(fast_sin(-cases[i]), -fast_sin(cases[i]), EPS_TRIG));
    }
}

UTEST(fast_math, sin_large_angle_wraps)
{
    // Periodicity: sin(x + 2π) ≈ sin(x)
    float cases[] = {0.5f, 1.0f, 2.0f};
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(abs_close(fast_sin(cases[i] + TWO_PI), fast_sin(cases[i]), EPS_TRIG));
        EXPECT_TRUE(abs_close(fast_sin(cases[i] + (4.0f * PI)), fast_sin(cases[i]), EPS_TRIG));
    }
}


// fast_cos

UTEST(fast_math, cos_zero)
{
    EXPECT_TRUE(abs_close(fast_cos(0.0f), 1.0f, EPS_TRIG));
}

UTEST(fast_math, cos_half_pi)
{
    EXPECT_TRUE(abs_close(fast_cos(PI / 2.0f), 0.0f, EPS_TRIG));
}

UTEST(fast_math, cos_pi)
{
    EXPECT_TRUE(abs_close(fast_cos(PI), -1.0f, EPS_TRIG));
}

UTEST(fast_math, cos_general)
{
    float cases[] = {0.1f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f};
    for (int i = 0; i < 7; i++) {
        EXPECT_TRUE(abs_close(fast_cos(cases[i]), cosf(cases[i]), EPS_TRIG));
    }
}

UTEST(fast_math, cos_even_symmetry)
{
    // cos(-x) = cos(x)
    float cases[] = {0.3f, 1.2f, 2.7f};
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(abs_close(fast_cos(-cases[i]), fast_cos(cases[i]), EPS_TRIG));
    }
}

UTEST(fast_math, sin_cos_pythagorean)
{
    // sin²(x) + cos²(x) = 1
    float cases[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, PI / 4.0f};
    for (int i = 0; i < 6; i++) {
        float s = fast_sin(cases[i]);
        float c = fast_cos(cases[i]);
        EXPECT_TRUE(abs_close((s * s) + (c * c), 1.0f, EPS_TRIG));
    }
}


// fast_exp

UTEST(fast_math, exp_zero)
{
    // e^0 = 1
    EXPECT_TRUE(rel_close(fast_exp(0.0f), 1.0f, EPS_TIGHT));
}

UTEST(fast_math, exp_one)
{
    // e^1 = e
    EXPECT_TRUE(rel_close(fast_exp(1.0f), expf(1.0f), EPS_TIGHT));
}

UTEST(fast_math, exp_small_positive)
{
    float cases[] = {0.1f, 0.5f, 0.9f};
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(rel_close(fast_exp(cases[i]), expf(cases[i]), EPS_TIGHT));
    }
}

UTEST(fast_math, exp_integers)
{
    float cases[] = {1.0f, 2.0f, 3.0f, 5.0f, 10.0f};
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(rel_close(fast_exp(cases[i]), expf(cases[i]), EPS_TIGHT));
    }
}

UTEST(fast_math, exp_negative)
{
    float cases[] = {-0.5f, -1.0f, -2.0f, -5.0f, -10.0f};
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(rel_close(fast_exp(cases[i]), expf(cases[i]), EPS_TIGHT));
    }
}

UTEST(fast_math, exp_fractional)
{
    float cases[] = {0.25f, 0.75f, 1.5f, 2.7f, 3.14f};
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(rel_close(fast_exp(cases[i]), expf(cases[i]), EPS_TIGHT));
    }
}

UTEST(fast_math, exp_clamp_large)
{
    // x > 88: implementation returns 1e38 sentinel
    EXPECT_TRUE(fast_exp(89.0f) >= 1e37f);
    EXPECT_TRUE(fast_exp(1000.0f) >= 1e37f);
}

UTEST(fast_math, exp_clamp_small)
{
    // x < -87: implementation returns 0
    EXPECT_TRUE(fast_exp(-88.0f) == 0.0f);
    EXPECT_TRUE(fast_exp(-1000.0f) == 0.0f);
}

UTEST(fast_math, exp_log_inverse)
{
    // exp(log(x)) ≈ x
    float cases[] = {0.5f, 1.0f, 2.0f, 10.0f, 50.0f};
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(rel_close(fast_exp(fast_log(cases[i])), cases[i], EPS_TIGHT));
    }
}

UTEST(fast_math, exp_product_rule)
{
    // e^(a+b) = e^a * e^b
    float a = 1.5f, b = 2.5f;
    EXPECT_TRUE(rel_close(fast_exp(a + b), fast_exp(a) * fast_exp(b), EPS_TIGHT));
}


// fast_ceil

UTEST(fast_math, ceil_exact_integers)
{
    float cases[] = {0.0f, 1.0f, -1.0f, 5.0f, -5.0f, 100.0f};
    for (int i = 0; i < 6; i++) {
        EXPECT_TRUE(fast_ceil(cases[i]) == ceilf(cases[i]));
    }
}

UTEST(fast_math, ceil_positive_fractions)
{
    float cases[] = {0.1f, 0.5f, 0.9f, 1.1f, 2.7f, 99.001f};
    for (int i = 0; i < 6; i++) {
        EXPECT_TRUE(fast_ceil(cases[i]) == ceilf(cases[i]));
    }
}

UTEST(fast_math, ceil_negative_fractions)
{
    float cases[] = {-0.1f, -0.5f, -0.9f, -1.1f, -2.7f, -99.001f};
    for (int i = 0; i < 6; i++) {
        EXPECT_TRUE(fast_ceil(cases[i]) == ceilf(cases[i]));
    }
}

UTEST(fast_math, ceil_zero)
{
    EXPECT_TRUE(fast_ceil(0.0f) == 0.0f);
}

UTEST(fast_math, ceil_result_is_integer)
{
    // ceil output must always be a whole number
    float cases[] = {1.3f, -2.7f, 0.01f, -0.01f, 5.5f, -5.5f};
    for (int i = 0; i < 6; i++) {
        float c = fast_ceil(cases[i]);
        EXPECT_TRUE(c == (float)(int)c);
    }
}

UTEST(fast_math, ceil_result_geq_input)
{
    float cases[] = {0.1f, -0.9f, 3.3f, -3.3f, 0.0f, -1.0f};
    for (int i = 0; i < 6; i++) {
        EXPECT_TRUE(fast_ceil(cases[i]) >= cases[i]);
    }
}


// fast_pow

UTEST(fast_math, pow_zero_exponent)
{
    EXPECT_TRUE(fast_pow(2.0f, 0.0f) == 1.0f);
    EXPECT_TRUE(fast_pow(0.5f, 0.0f) == 1.0f);
    EXPECT_TRUE(fast_pow(0.0f, 0.0f) == 1.0f);
}

UTEST(fast_math, pow_one_exponent)
{
    EXPECT_TRUE(fast_pow(2.0f, 1.0f) == 2.0f);
    EXPECT_TRUE(fast_pow(0.5f, 1.0f) == 0.5f);
}

UTEST(fast_math, pow_zero_base)
{
    EXPECT_TRUE(fast_pow(0.0f, 2.0f) == 0.0f);
    EXPECT_TRUE(fast_pow(0.0f, 0.5f) == 0.0f);
    // Negative exponent for zero base returns sentinel (1e38)
    EXPECT_TRUE(fast_pow(0.0f, -1.0f) >= 1e37f);
}

UTEST(fast_math, pow_square)
{
    EXPECT_TRUE(rel_close(fast_pow(3.0f, 2.0f), 9.0f, EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(-4.0f, 2.0f), 16.0f, EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(0.5f, 2.0f), 0.25f, EPS_TIGHT));
}

UTEST(fast_math, pow_sqrt)
{
    EXPECT_TRUE(rel_close(fast_pow(4.0f, 0.5f), 2.0f, EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(2.0f, 0.5f), sqrtf(2.0f), EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(0.25f, 0.5f), 0.5f, EPS_TIGHT));
}

UTEST(fast_math, pow_integer_exponent)
{
    EXPECT_TRUE(rel_close(fast_pow(2.0f, 3.0f), 8.0f, EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(3.0f, 4.0f), 81.0f, EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(2.0f, 10.0f), 1024.0f, EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(1.1f, 2.0f), 1.21f, EPS_TIGHT));
}

UTEST(fast_math, pow_negative_exponent)
{
    EXPECT_TRUE(rel_close(fast_pow(2.0f, -1.0f), 0.5f, EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(2.0f, -2.0f), 0.25f, EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(10.0f, -3.0f), 0.001f, EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(0.5f, -1.0f), 2.0f, EPS_TIGHT));
}

UTEST(fast_math, pow_fractional_exponent)
{
    EXPECT_TRUE(rel_close(fast_pow(2.0f, 1.5f), powf(2.0f, 1.5f), EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(3.0f, 0.33f), powf(3.0f, 0.33f), EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(0.5f, 0.5f), powf(0.5f, 0.5f), EPS_TIGHT));
}

UTEST(fast_math, pow_general)
{
    float bases[] = {1.5f, 2.7f, 0.8f};
    float exps[]  = {2.2f, -1.1f, 0.5f};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            EXPECT_TRUE(rel_close(fast_pow(bases[i], exps[j]), powf(bases[i], exps[j]), EPS_TIGHT));
        }
    }
}

UTEST(fast_math, pow_large_result)
{
    // Should handle large results gracefully (approaching float max)
    EXPECT_TRUE(fast_pow(10.0f, 30.0f) > 1e29f);
}

UTEST(fast_math, pow_small_result)
{
    // Should handle very small results (approaching 0)
    EXPECT_TRUE(fast_pow(0.1f, 40.0f) < 1e-37f);
}

UTEST(fast_math, pow_negative_base)
{
    // Integer exponents should work
    EXPECT_TRUE(rel_close(fast_pow(-2.0f, 2.0f), 4.0f, EPS_TIGHT));
    EXPECT_TRUE(rel_close(fast_pow(-2.0f, 3.0f), -8.0f, EPS_TIGHT));

    // Fractional exponents with negative base: implementation returns 0
    // (consistent with fast_sqrt and fast_log returning sentinels)
    EXPECT_TRUE(fast_pow(-2.0f, 0.5f) == 0.0f);
    EXPECT_TRUE(fast_pow(-2.0f, 1.5f) == 0.0f);
}
