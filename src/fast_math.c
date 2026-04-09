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
    guess = 0.5f * (guess + (x / guess));
    guess = 0.5f * (guess + (x / guess));
    guess = 0.5f * (guess + (x / guess));
    guess = 0.5f * (guess + (x / guess));
    
    return guess;
}


float fast_log(float x)
{
    if (x <= 0.0f) { return -1e10f; }  // Error value
    
    /* Reduce x to range [1/√2, √2] ≈ [0.707, 1.414] for best Taylor convergence.
        - Keeps |t| = |x-1| ≤ 0.414, where the 8-term series is accurate to ~1e-5
        - Uses logarithm property: ln(x × 2ⁿ) = ln(x) + n×ln(2)
    */
    int exp_adjust = 0;
    while (x > 1.4142135f) { x *= 0.5f; exp_adjust++; }
    while (x < 0.7071068f) { x *= 2.0f; exp_adjust--; }
    
    // Now compute ln(x) using ln(1+t) series where t = x-1
    float t = x - 1.0f;
    float t2 = t * t;
    float t3 = t2 * t;
    float t4 = t3 * t;
    float t5 = t4 * t;
    float t6 = t5 * t;
    float t7 = t6 * t;
    float t8 = t7 * t;
 
    float result = t - (t2/2.0f) + (t3/3.0f) - (t4/4.0f) + (t5/5.0f)
                     - (t6/6.0f) + (t7/7.0f) - (t8/8.0f);
    
    // Adjust for range reduction: ln(x * 2^n) = ln(x) + n*ln(2)
    result += 0.693147180559945f * (float)exp_adjust; // ln(2)
 
    return result;
}


float fast_sin(float x)
{
    // Normalize to [-π, π]
    while (x > PI) { x -= TWO_PI; }
    while (x < -PI) { x += TWO_PI; }
 
    // Fold to [-π/2, π/2] using sin(π - x) = sin(x)
    if (x >  PI / 2.0f) { x =  PI - x; }
    if (x < -PI / 2.0f) { x = -PI - x; }
 
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x3 * x2;
    float x7 = x5 * x2;
    float x9 = x7 * x2;
 
    return x - (x3/6.0f) + (x5/120.0f) - (x7/5040.0f) + (x9/362880.0f);
}


float fast_cos(float x)
{
    // Normalize to [-π, π]
    while (x >  PI) { x -= TWO_PI; }
    while (x < -PI) { x += TWO_PI; }
 
    // cos is even
    if (x < 0.0f) { x = -x; }
 
    // Fold to [0, π/2] using cos(π - x) = -cos(x)
    int negate = 0;
    if (x > PI / 2.0f) { x = PI - x; negate = 1; }
 
    // Taylor series for cos(x), converges well on [0, π/2]
    float x2 = x * x;
    float x4 = x2 * x2;
    float x6 = x4 * x2;
    float x8 = x6 * x2;
 
    float result = 1.0f - (x2/2.0f) + (x4/24.0f) - (x6/720.0f) + (x8/40320.0f);
    return negate ? -result : result;
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


