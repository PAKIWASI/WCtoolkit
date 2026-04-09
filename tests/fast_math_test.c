#include "wc_test.h"
#include "fast_math.h"

#include <math.h>   // sqrtf, logf, sinf, cosf, expf, ceilf — reference values


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

#define EPS_TIGHT  1e-4f   // relative tolerance for sqrt, exp, log
#define EPS_TRIG   1e-3f   // absolute tolerance for sin, cos (Taylor accumulates)
#define EPS_EXACT  0.0f    // ceil must be bit-exact with ceilf

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

static void test_sqrt_zero(void)
{
    WC_ASSERT_TRUE(fast_sqrt(0.0f) == 0.0f);
}

static void test_sqrt_one(void)
{
    WC_ASSERT_TRUE(rel_close(fast_sqrt(1.0f), 1.0f, EPS_TIGHT));
}

static void test_sqrt_four(void)
{
    WC_ASSERT_TRUE(rel_close(fast_sqrt(4.0f), 2.0f, EPS_TIGHT));
}

static void test_sqrt_perfect_squares(void)
{
    float cases[] = {9.0f, 16.0f, 25.0f, 49.0f, 100.0f, 10000.0f};
    for (int i = 0; i < 6; i++) {
        WC_ASSERT_TRUE(rel_close(fast_sqrt(cases[i]), sqrtf(cases[i]), EPS_TIGHT));
    }
}

static void test_sqrt_non_integer(void)
{
    float cases[] = {2.0f, 3.0f, 0.5f, 0.1f, 7.5f, 123.456f};
    for (int i = 0; i < 6; i++) {
        WC_ASSERT_TRUE(rel_close(fast_sqrt(cases[i]), sqrtf(cases[i]), EPS_TIGHT));
    }
}

static void test_sqrt_large(void)
{
    WC_ASSERT_TRUE(rel_close(fast_sqrt(1e8f), sqrtf(1e8f), EPS_TIGHT));
    WC_ASSERT_TRUE(rel_close(fast_sqrt(1e12f), sqrtf(1e12f), EPS_TIGHT));
}

static void test_sqrt_small(void)
{
    WC_ASSERT_TRUE(rel_close(fast_sqrt(1e-4f), sqrtf(1e-4f), EPS_TIGHT));
    WC_ASSERT_TRUE(rel_close(fast_sqrt(1e-6f), sqrtf(1e-6f), EPS_TIGHT));
}

static void test_sqrt_negative_returns_zero(void)
{
    // implementation clamps negative input to 0
    WC_ASSERT_TRUE(fast_sqrt(-1.0f) == 0.0f);
    WC_ASSERT_TRUE(fast_sqrt(-100.0f) == 0.0f);
}

static void test_sqrt_roundtrip(void)
{
    // sqrt(x)^2 should recover x within tolerance
    float cases[] = {2.0f, 7.0f, 42.0f, 0.3f};
    for (int i = 0; i < 4; i++) {
        float s = fast_sqrt(cases[i]);
        WC_ASSERT_TRUE(rel_close(s * s, cases[i], EPS_TIGHT));
    }
}


// fast_log  (natural log)

static void test_log_one(void)
{
    // ln(1) = 0 — use absolute tolerance since ref is 0
    WC_ASSERT_TRUE(abs_close(fast_log(1.0f), 0.0f, EPS_TIGHT));
}

static void test_log_e(void)
{
    // ln(e) = 1
    const float E = 2.718281828f;
    WC_ASSERT_TRUE(rel_close(fast_log(E), 1.0f, EPS_TIGHT));
}

static void test_log_powers_of_two(void)
{
    // ln(2^n) = n * ln(2) — exercises the range-reduction path
    float cases[] = {2.0f, 4.0f, 8.0f, 16.0f, 32.0f};
    for (int i = 0; i < 5; i++) {
        WC_ASSERT_TRUE(rel_close(fast_log(cases[i]), logf(cases[i]), EPS_TIGHT));
    }
}

static void test_log_general(void)
{
    float cases[] = {0.5f, 0.1f, 1.5f, 3.0f, 10.0f, 100.0f, 1000.0f};
    for (int i = 0; i < 7; i++) {
        WC_ASSERT_TRUE(rel_close(fast_log(cases[i]), logf(cases[i]), EPS_TIGHT));
    }
}

static void test_log_small_positive(void)
{
    float cases[] = {0.01f, 0.001f, 1e-4f};
    for (int i = 0; i < 3; i++) {
        WC_ASSERT_TRUE(rel_close(fast_log(cases[i]), logf(cases[i]), EPS_TIGHT));
    }
}

static void test_log_nonpositive_returns_sentinel(void)
{
    // implementation returns -1e10f for x <= 0
    WC_ASSERT_TRUE(fast_log(0.0f)  < -1e9f);
    WC_ASSERT_TRUE(fast_log(-1.0f) < -1e9f);
}

static void test_log_exp_inverse(void)
{
    // log(exp(x)) ≈ x
    float cases[] = {0.0f, 0.5f, 1.0f, 2.0f, 5.0f};
    for (int i = 0; i < 5; i++) {
        WC_ASSERT_TRUE(abs_close(fast_log(fast_exp(cases[i])), cases[i], EPS_TIGHT));
    }
}


// fast_sin

static void test_sin_zero(void)
{
    WC_ASSERT_TRUE(abs_close(fast_sin(0.0f), 0.0f, EPS_TRIG));
}

static void test_sin_half_pi(void)
{
    WC_ASSERT_TRUE(abs_close(fast_sin(PI / 2.0f), 1.0f, EPS_TRIG));
}

static void test_sin_pi(void)
{
    WC_ASSERT_TRUE(abs_close(fast_sin(PI), 0.0f, EPS_TRIG));
}

static void test_sin_neg_half_pi(void)
{
    WC_ASSERT_TRUE(abs_close(fast_sin(-PI / 2.0f), -1.0f, EPS_TRIG));
}

static void test_sin_general(void)
{
    float cases[] = {0.1f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f};
    for (int i = 0; i < 7; i++) {
        WC_ASSERT_TRUE(abs_close(fast_sin(cases[i]), sinf(cases[i]), EPS_TRIG));
    }
}

static void test_sin_negative(void)
{
    float cases[] = {-0.5f, -1.0f, -2.0f, -PI / 4.0f};
    for (int i = 0; i < 4; i++) {
        WC_ASSERT_TRUE(abs_close(fast_sin(cases[i]), sinf(cases[i]), EPS_TRIG));
    }
}

static void test_sin_odd_symmetry(void)
{
    // sin(-x) = -sin(x)
    float cases[] = {0.3f, 1.2f, 2.7f};
    for (int i = 0; i < 3; i++) {
        WC_ASSERT_TRUE(abs_close(fast_sin(-cases[i]), -fast_sin(cases[i]), EPS_TRIG));
    }
}

static void test_sin_large_angle_wraps(void)
{
    // Periodicity: sin(x + 2π) ≈ sin(x)
    float cases[] = {0.5f, 1.0f, 2.0f};
    for (int i = 0; i < 3; i++) {
        WC_ASSERT_TRUE(abs_close(fast_sin(cases[i] + TWO_PI), fast_sin(cases[i]), EPS_TRIG));
        WC_ASSERT_TRUE(abs_close(fast_sin(cases[i] + (4.0f * PI)), fast_sin(cases[i]), EPS_TRIG));
    }
}


// fast_cos

static void test_cos_zero(void)
{
    WC_ASSERT_TRUE(abs_close(fast_cos(0.0f), 1.0f, EPS_TRIG));
}

static void test_cos_half_pi(void)
{
    WC_ASSERT_TRUE(abs_close(fast_cos(PI / 2.0f), 0.0f, EPS_TRIG));
}

static void test_cos_pi(void)
{
    WC_ASSERT_TRUE(abs_close(fast_cos(PI), -1.0f, EPS_TRIG));
}

static void test_cos_general(void)
{
    float cases[] = {0.1f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f};
    for (int i = 0; i < 7; i++) {
        WC_ASSERT_TRUE(abs_close(fast_cos(cases[i]), cosf(cases[i]), EPS_TRIG));
    }
}

static void test_cos_even_symmetry(void)
{
    // cos(-x) = cos(x)
    float cases[] = {0.3f, 1.2f, 2.7f};
    for (int i = 0; i < 3; i++) {
        WC_ASSERT_TRUE(abs_close(fast_cos(-cases[i]), fast_cos(cases[i]), EPS_TRIG));
    }
}

static void test_sin_cos_pythagorean(void)
{
    // sin²(x) + cos²(x) = 1
    float cases[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, PI / 4.0f};
    for (int i = 0; i < 6; i++) {
        float s = fast_sin(cases[i]);
        float c = fast_cos(cases[i]);
        WC_ASSERT_TRUE(abs_close((s * s) + (c * c), 1.0f, EPS_TRIG));
    }
}


// fast_exp

static void test_exp_zero(void)
{
    // e^0 = 1
    WC_ASSERT_TRUE(rel_close(fast_exp(0.0f), 1.0f, EPS_TIGHT));
}

static void test_exp_one(void)
{
    // e^1 = e
    WC_ASSERT_TRUE(rel_close(fast_exp(1.0f), expf(1.0f), EPS_TIGHT));
}

static void test_exp_small_positive(void)
{
    float cases[] = {0.1f, 0.5f, 0.9f};
    for (int i = 0; i < 3; i++) {
        WC_ASSERT_TRUE(rel_close(fast_exp(cases[i]), expf(cases[i]), EPS_TIGHT));
    }
}

static void test_exp_integers(void)
{
    float cases[] = {1.0f, 2.0f, 3.0f, 5.0f, 10.0f};
    for (int i = 0; i < 5; i++) {
        WC_ASSERT_TRUE(rel_close(fast_exp(cases[i]), expf(cases[i]), EPS_TIGHT));
    }
}

static void test_exp_negative(void)
{
    float cases[] = {-0.5f, -1.0f, -2.0f, -5.0f, -10.0f};
    for (int i = 0; i < 5; i++) {
        WC_ASSERT_TRUE(rel_close(fast_exp(cases[i]), expf(cases[i]), EPS_TIGHT));
    }
}

static void test_exp_fractional(void)
{
    float cases[] = {0.25f, 0.75f, 1.5f, 2.7f, 3.14f};
    for (int i = 0; i < 5; i++) {
        WC_ASSERT_TRUE(rel_close(fast_exp(cases[i]), expf(cases[i]), EPS_TIGHT));
    }
}

static void test_exp_clamp_large(void)
{
    // x > 88: implementation returns 1e38 sentinel
    WC_ASSERT_TRUE(fast_exp(89.0f) >= 1e37f);
    WC_ASSERT_TRUE(fast_exp(1000.0f) >= 1e37f);
}

static void test_exp_clamp_small(void)
{
    // x < -87: implementation returns 0
    WC_ASSERT_TRUE(fast_exp(-88.0f) == 0.0f);
    WC_ASSERT_TRUE(fast_exp(-1000.0f) == 0.0f);
}

static void test_exp_log_inverse(void)
{
    // exp(log(x)) ≈ x
    float cases[] = {0.5f, 1.0f, 2.0f, 10.0f, 50.0f};
    for (int i = 0; i < 5; i++) {
        WC_ASSERT_TRUE(rel_close(fast_exp(fast_log(cases[i])), cases[i], EPS_TIGHT));
    }
}

static void test_exp_product_rule(void)
{
    // e^(a+b) = e^a * e^b
    float a = 1.5f, b = 2.5f;
    WC_ASSERT_TRUE(rel_close(fast_exp(a + b), fast_exp(a) * fast_exp(b), EPS_TIGHT));
}


// fast_ceil

static void test_ceil_exact_integers(void)
{
    float cases[] = {0.0f, 1.0f, -1.0f, 5.0f, -5.0f, 100.0f};
    for (int i = 0; i < 6; i++) {
        WC_ASSERT_TRUE(fast_ceil(cases[i]) == ceilf(cases[i]));
    }
}

static void test_ceil_positive_fractions(void)
{
    float cases[] = {0.1f, 0.5f, 0.9f, 1.1f, 2.7f, 99.001f};
    for (int i = 0; i < 6; i++) {
        WC_ASSERT_TRUE(fast_ceil(cases[i]) == ceilf(cases[i]));
    }
}

static void test_ceil_negative_fractions(void)
{
    float cases[] = {-0.1f, -0.5f, -0.9f, -1.1f, -2.7f, -99.001f};
    for (int i = 0; i < 6; i++) {
        WC_ASSERT_TRUE(fast_ceil(cases[i]) == ceilf(cases[i]));
    }
}

static void test_ceil_zero(void)
{
    WC_ASSERT_TRUE(fast_ceil(0.0f) == 0.0f);
}

static void test_ceil_result_is_integer(void)
{
    // ceil output must always be a whole number
    float cases[] = {1.3f, -2.7f, 0.01f, -0.01f, 5.5f, -5.5f};
    for (int i = 0; i < 6; i++) {
        float c = fast_ceil(cases[i]);
        WC_ASSERT_TRUE(c == (float)(int)c);
    }
}

static void test_ceil_result_geq_input(void)
{
    float cases[] = {0.1f, -0.9f, 3.3f, -3.3f, 0.0f, -1.0f};
    for (int i = 0; i < 6; i++) {
        WC_ASSERT_TRUE(fast_ceil(cases[i]) >= cases[i]);
    }
}


// Suite entry point

void fast_math_suite(void)
{
    WC_SUITE("fast_sqrt");
    WC_RUN(test_sqrt_zero);
    WC_RUN(test_sqrt_one);
    WC_RUN(test_sqrt_four);
    WC_RUN(test_sqrt_perfect_squares);
    WC_RUN(test_sqrt_non_integer);
    WC_RUN(test_sqrt_large);
    WC_RUN(test_sqrt_small);
    WC_RUN(test_sqrt_negative_returns_zero);
    WC_RUN(test_sqrt_roundtrip);

    WC_SUITE("fast_log");
    WC_RUN(test_log_one);
    WC_RUN(test_log_e);
    WC_RUN(test_log_powers_of_two);
    WC_RUN(test_log_general);
    WC_RUN(test_log_small_positive);
    WC_RUN(test_log_nonpositive_returns_sentinel);
    WC_RUN(test_log_exp_inverse);

    WC_SUITE("fast_sin");
    WC_RUN(test_sin_zero);
    WC_RUN(test_sin_half_pi);
    WC_RUN(test_sin_pi);
    WC_RUN(test_sin_neg_half_pi);
    WC_RUN(test_sin_general);
    WC_RUN(test_sin_negative);
    WC_RUN(test_sin_odd_symmetry);
    WC_RUN(test_sin_large_angle_wraps);

    WC_SUITE("fast_cos");
    WC_RUN(test_cos_zero);
    WC_RUN(test_cos_half_pi);
    WC_RUN(test_cos_pi);
    WC_RUN(test_cos_general);
    WC_RUN(test_cos_even_symmetry);
    WC_RUN(test_sin_cos_pythagorean);

    WC_SUITE("fast_exp");
    WC_RUN(test_exp_zero);
    WC_RUN(test_exp_one);
    WC_RUN(test_exp_small_positive);
    WC_RUN(test_exp_integers);
    WC_RUN(test_exp_negative);
    WC_RUN(test_exp_fractional);
    WC_RUN(test_exp_clamp_large);
    WC_RUN(test_exp_clamp_small);
    WC_RUN(test_exp_log_inverse);
    WC_RUN(test_exp_product_rule);

    WC_SUITE("fast_ceil");
    WC_RUN(test_ceil_exact_integers);
    WC_RUN(test_ceil_positive_fractions);
    WC_RUN(test_ceil_negative_fractions);
    WC_RUN(test_ceil_zero);
    WC_RUN(test_ceil_result_is_integer);
    WC_RUN(test_ceil_result_geq_input);
}




