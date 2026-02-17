#include "fast_math.h"

#include "common.h"



float fast_sqrt(float x)
{
    if (x <= 0.0f) { return 0.0f; }
    
    /* Initial guess using bit hack (speeds convergence)
        - Treats the float as an integer, shifts right by 1 (approximate division by 2)
        - The magic constant `0x1fbd1df5` adjusts the exponent bias
        - Gives a surprisingly good initial guess, making convergence faster
    */
    union { float f; u32 i; } conv = { .f = x };
    conv.i = 0x1fbd1df5 + (conv.i >> 1);
    float guess = conv.f;
    
    // Newton-Raphson: next = 0.5 * (guess + x/guess)
    // 3-4 iterations gives good precision
    guess = 0.5f * (guess + x / guess);
    guess = 0.5f * (guess + x / guess);
    guess = 0.5f * (guess + x / guess);
    guess = 0.5f * (guess + x / guess);
    
    return guess;
}

float fast_log(float x)
{
    if (x <= 0.0f) { return -1e10f; }  // Error value
    
    /* Reduce x to range [0.5, 1.5] for better convergence
        - Brings x into [0.5, 1.5] range where t = x-1 is small
        - Uses logarithm property: `ln(x × 2ⁿ) = ln(x) + n×ln(2)`
        - After computing ln(x) in reduced range, adds back `n × 0.693147...` (ln(2))
    */
    int exp_adjust = 0;
    while (x > 1.5f) { x *= 0.5f; exp_adjust++; }
    while (x < 0.5f) { x *= 2.0f; exp_adjust--; }
    
    // Now compute ln(x) using ln(1+t) series where t = x-1
    float t = x - 1.0f;
    float t2 = t * t;
    float t3 = t2 * t;
    float t4 = t3 * t;
    float t5 = t4 * t;
    
    float result = t - (t2/2.0f) + (t3/3.0f) - (t4/4.0f) + (t5/5.0f);
    
    // Adjust for range reduction: ln(x * 2^n) = ln(x) + n*ln(2)
    result += 0.693147180559945f * (float)exp_adjust; // ln(2)

    return result;
}

float fast_sin(float x)
{
    // Reduce to [-π, π]
    // const float PI = 3.14159265359f;
    // const float TWO_PI = 6.28318530718f;
    
    // Normalize to [-π, π]
    //- Reduces angle to [-π, π] where Taylor series converges well
    //- Sine is periodic with period 2π
    while (x > PI) { x -= TWO_PI; }
    while (x < -PI) { x += TWO_PI; }
    
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x3 * x2;
    float x7 = x5 * x2;
    
    return x - (x3/6.0f) + (x5/120.0f) - (x7/5040.0f);
}

float fast_cos(float x)
{
    // cos(x) = sin(x + π/2)
    const float HALF_PI = 1.57079632679f;
    return fast_sin(x + HALF_PI);
}


/*
e^x = e^(n+r) = e^n × e^r
where n is an integer and r ∈ [0, 1)
This splits the problem into two easier parts:

e^r (fractional part) - use Taylor series
e^n (integer part) - use repeated squaring

Part 1: Fractional Component (e^r)
For r ∈ [0, 1), the Taylor series converges rapidly:
e^r = 1 + r + r²/2! + r³/3! + r⁴/4! + r⁵/5! + r⁶/6! + ...
Why range reduction helps:

When r is small, higher-order terms become negligible quickly
With r < 1, just 7 terms gives good accuracy
Without reduction, large x would need many more terms


Part 2: Integer Component (e^n)
Uses exponentiation by squaring - a clever algorithm:
For positive n (e.g., e^5):
e^5 = e^(101 in binary)
    = e^(4+1)
    = e^4 × e^1
    = (e^2)^2 × e
This reduces multiplications from O(n) to O(log n).
*/
float fast_exp(float x)
{
    // Clamp extreme values to prevent overflow
    if (x > 88.0f) { return 1e38f; }  // e^88 ≈ 1.65e38 (near float max)
    if (x < -87.0f) { return 0.0f; }  // e^-87 ≈ 0
    
    // Range reduction: e^x = e^(n) * e^(r)
    // where x = n + r, n is integer, r in [0, 1)
    int n = (int)x;
    float r = x - (float)n;
    
    // Handle negative n
    if (x < 0.0f && r != 0.0f) {
        n -= 1;
        r = x - (float)n;  // Now r is positive in [0, 1)
    }
    
    // Compute e^r using Taylor series: e^r = 1 + r + r²/2! + r³/3! + ...
    // Since r ∈ [0, 1), series converges quickly
    float r2 = r * r;
    float r3 = r2 * r;
    float r4 = r3 * r;
    float r5 = r4 * r;
    float r6 = r5 * r;
    
    float exp_r = 1.0f + r + (r2/2.0f) + (r3/6.0f) + (r4/24.0f) + (r5/120.0f) + (r6/720.0f);
    
    // Compute e^n using repeated squaring
    // e^n = (e^1)^n, where e ≈ 2.718281828
    const float E = 2.718281828f;
    float exp_n = 1.0f;
    
    if (n > 0) {
        // Positive exponent: multiply by e^1 n times (optimized with squaring)
        float base = E;
        int exp = n;
        while (exp > 0) {
            if (exp & 1) { exp_n *= base; }
            base *= base;
            exp >>= 1;
        }
    } else if (n < 0) {
        // Negative exponent: divide by e^1 |n| times
        float base = E;
        int exp = -n;
        while (exp > 0) {
            if (exp & 1) { exp_n *= base; }
            base *= base;
            exp >>= 1;
        }
        exp_n = 1.0f / exp_n;
    }
    // if n == 0, exp_n remains 1.0
    
    return exp_n * exp_r;
}


float fast_ceil(float x)
{
    // Cast to int truncates towards zero
    int i = (int)x;
    
    // If x is already an integer, return it
    if (x == (float)i) {
        return x;
    }
    
    // If x is positive and not an integer, add 1
    if (x > 0.0f) {
        return (float)(i + 1);
    }
    
    // If x is negative, truncation already rounded towards zero (up)
    return (float)i;
}


