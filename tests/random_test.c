#include "wc_test.h"
#include "random.h"
#include <math.h>

/*
 * Testing a PRNG requires a different mindset than testing deterministic code.
 * We can't assert exact output values for unseeded calls, but we CAN assert:
 *
 *   1. Determinism   — same seed always produces the same sequence
 *   2. Range         — bounded outputs stay in [0, bound)
 *   3. Float range   — float/double outputs stay in [0, 1)
 *   4. Non-trivial   — not all zeros, not all the same value
 *   5. Statistics    — large samples should have the right shape
 *      (mean ≈ 0, stddev ≈ 1 for Gaussian; roughly uniform for bounded int)
 */


/* ── Determinism ─────────────────────────────────────────────────────────── */

static void test_same_seed_same_sequence(void)
{
    pcg32_rand_seed(42, 1);
    u32 a0 = pcg32_rand();
    u32 a1 = pcg32_rand();
    u32 a2 = pcg32_rand();

    pcg32_rand_seed(42, 1);
    WC_ASSERT_EQ_INT((int)pcg32_rand(), (int)a0);
    WC_ASSERT_EQ_INT((int)pcg32_rand(), (int)a1);
    WC_ASSERT_EQ_INT((int)pcg32_rand(), (int)a2);
}

static void test_different_seed_different_sequence(void)
{
    pcg32_rand_seed(1, 1);
    u32 a = pcg32_rand();

    pcg32_rand_seed(2, 1);
    u32 b = pcg32_rand();

    WC_ASSERT_TRUE(a != b);
}

static void test_different_seq_different_output(void)
{
    pcg32_rand_seed(1, 1);
    u32 a = pcg32_rand();

    pcg32_rand_seed(1, 2);
    u32 b = pcg32_rand();

    WC_ASSERT_TRUE(a != b);
}


/* ── Bounded integer ─────────────────────────────────────────────────────── */

static void test_bounded_always_in_range(void)
{
    pcg32_rand_seed(99, 7);
    for (int i = 0; i < 10000; i++) {
        u32 r = pcg32_rand_bounded(100);
        WC_ASSERT_TRUE(r < 100);
    }
}

static void test_bounded_uses_full_range(void)
{
    /* With 10000 samples over [0,100), every bucket should be hit at least once */
    pcg32_rand_seed(1, 1);
    u8 seen[100] = {0};
    for (int i = 0; i < 10000; i++) {
        seen[pcg32_rand_bounded(100)] = 1;
    }
    for (int i = 0; i < 100; i++) {
        WC_ASSERT_TRUE(seen[i]);
    }
}

static void test_bounded_not_all_same(void)
{
    pcg32_rand_seed(1, 1);
    u32 first = pcg32_rand_bounded(1000);
    int all_same = 1;
    for (int i = 0; i < 100; i++) {
        if (pcg32_rand_bounded(1000) != first) { all_same = 0; break; }
    }
    WC_ASSERT_FALSE(all_same);
}


/* ── Float uniform ───────────────────────────────────────────────────────── */

static void test_float_in_range(void)
{
    pcg32_rand_seed(1, 1);
    for (int i = 0; i < 10000; i++) {
        float f = pcg32_rand_float();
        WC_ASSERT_TRUE(f >= 0.0f && f < 1.0f);
    }
}

static void test_double_in_range(void)
{
    pcg32_rand_seed(1, 1);
    for (int i = 0; i < 10000; i++) {
        double d = pcg32_rand_double();
        WC_ASSERT_TRUE(d >= 0.0 && d < 1.0);
    }
}

static void test_float_range_custom(void)
{
    pcg32_rand_seed(1, 1);
    for (int i = 0; i < 1000; i++) {
        float f = pcg32_rand_float_range(-5.0f, 5.0f);
        WC_ASSERT_TRUE(f >= -5.0f && f < 5.0f);
    }
}

static void test_double_range_custom(void)
{
    pcg32_rand_seed(1, 1);
    for (int i = 0; i < 1000; i++) {
        double d = pcg32_rand_double_range(10.0, 20.0);
        WC_ASSERT_TRUE(d >= 10.0 && d < 20.0);
    }
}

static void test_float_not_constant(void)
{
    pcg32_rand_seed(1, 1);
    float first = pcg32_rand_float();
    int all_same = 1;
    for (int i = 0; i < 100; i++) {
        if (pcg32_rand_float() != first) { all_same = 0; break; }
    }
    WC_ASSERT_FALSE(all_same);
}


/* ── Gaussian statistics ─────────────────────────────────────────────────── */

static void test_gaussian_mean_near_zero(void)
{
    pcg32_rand_seed(1, 1);
    const int N = 50000;
    double    sum = 0.0;
    for (int i = 0; i < N; i++) sum += pcg32_rand_gaussian();
    double mean = sum / N;
    /* mean should be within 0.05 of 0 with this many samples */
    WC_ASSERT_TRUE(fabs(mean) < 0.05);
}

static void test_gaussian_stddev_near_one(void)
{
    pcg32_rand_seed(2, 1);
    const int N = 50000;
    double    sum = 0.0, sum_sq = 0.0;
    for (int i = 0; i < N; i++) {
        float x = pcg32_rand_gaussian();
        sum    += x;
        sum_sq += (double)x * x;
    }
    double mean = sum / N;
    double var  = (sum_sq / N) - mean * mean;
    double std  = sqrt(var);
    /* stddev should be within 0.05 of 1.0 */
    WC_ASSERT_TRUE(fabs(std - 1.0) < 0.05);
}

static void test_gaussian_68_rule(void)
{
    /* ~68% of values should fall in [-1, 1] */
    pcg32_rand_seed(3, 1);
    const int N    = 50000;
    int       in1s = 0;
    for (int i = 0; i < N; i++) {
        float x = pcg32_rand_gaussian();
        if (x >= -1.0f && x <= 1.0f) in1s++;
    }
    double pct = (double)in1s / N;
    /* expect 0.68 ± 0.02 */
    WC_ASSERT_TRUE(pct > 0.66 && pct < 0.70);
}

static void test_gaussian_custom_mean_stddev(void)
{
    pcg32_rand_seed(4, 1);
    const int N   = 50000;
    float     mu  = 10.0f;
    float     sig = 2.0f;
    double    sum = 0.0;
    for (int i = 0; i < N; i++) sum += pcg32_rand_gaussian_custom(mu, sig);
    double mean = sum / N;
    WC_ASSERT_TRUE(fabs(mean - mu) < 0.1);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void random_suite(void)
{
    WC_SUITE("Random (PCG32)");

    WC_RUN(test_same_seed_same_sequence);
    WC_RUN(test_different_seed_different_sequence);
    WC_RUN(test_different_seq_different_output);

    WC_RUN(test_bounded_always_in_range);
    WC_RUN(test_bounded_uses_full_range);
    WC_RUN(test_bounded_not_all_same);

    WC_RUN(test_float_in_range);
    WC_RUN(test_double_in_range);
    WC_RUN(test_float_range_custom);
    WC_RUN(test_double_range_custom);
    WC_RUN(test_float_not_constant);

    WC_RUN(test_gaussian_mean_near_zero);
    WC_RUN(test_gaussian_stddev_near_one);
    WC_RUN(test_gaussian_68_rule);
    WC_RUN(test_gaussian_custom_mean_stddev);
}
