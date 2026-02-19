#ifndef WC_RANDOM_SINGLE_H
#define WC_RANDOM_SINGLE_H

/*
 * random_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "random_single.h"
 *
 * All other files just:
 *     #include "random_single.h"
 */

/* ===== common.h ===== */
#ifndef WC_COMMON_H
#define WC_COMMON_H

/*
 * C Data Structures Library
 * Copyright (c) 2026 Wasi Ullah (PAKIWASI)
 * Licensed under the MIT License. See LICENSE file for details.
 */



// LOGGING/ERRORS

#include <stdio.h>
#include <stdlib.h>

// ANSI Color Codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_CYAN    "\033[1;36m"

#define WARN(fmt, ...)                                                                       \
    do {                                                                                     \
        printf(COLOR_YELLOW "[WARN]" " %s:%d:%s(): " fmt "\n" COLOR_RESET, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    } while (0)

#define FATAL(fmt, ...)                                                                                \
    do {                                                                                               \
        fprintf(stderr, COLOR_RED "[FATAL]" " %s:%d:%s(): " fmt "\n" COLOR_RESET, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        exit(EXIT_FAILURE);                                                                            \
    } while (0)

#define ASSERT_WARN(cond, fmt, ...)                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            WARN("Assertion failed: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                               \
    } while (0)

#define ASSERT_WARN_RET(cond, ret, fmt, ...)                            \
    do {                                                                \
        if (!(cond)) {                                                  \
            WARN("Assertion failed: (%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                                                 \
        }                                                               \
    } while (0)

#define ASSERT_FATAL(cond, fmt, ...)                                     \
    do {                                                                 \
        if (!(cond)) {                                                   \
            FATAL("Assertion failed: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                                \
    } while (0)

#define CHECK_WARN(cond, fmt, ...)                           \
    do {                                                     \
        if ((cond)) {                                        \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                    \
    } while (0)

#define CHECK_WARN_RET(cond, ret, fmt, ...)                  \
    do {                                                     \
        if ((cond)) {                                        \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                                      \
        }                                                    \
    } while (0)

#define CHECK_FATAL(cond, fmt, ...)                           \
    do {                                                      \
        if (cond) {                                           \
            FATAL("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                     \
    } while (0)

#define LOG(fmt, ...)                               \
    do {                                            \
        printf(COLOR_CYAN "[LOG]" " : %s(): " fmt "\n" COLOR_RESET, __func__, ##__VA_ARGS__); \
    } while (0)

// TYPES

#include <stdint.h>

typedef uint8_t  u8;
typedef uint8_t  b8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define false ((b8)0)
#define true  ((b8)1)


// GENERIC FUNCTIONS
typedef void (*copy_fn)(u8* dest, const u8* src);
typedef void (*move_fn)(u8* dest, u8** src);
typedef void (*delete_fn)(u8* key);
typedef void (*print_fn)(const u8* elm);
typedef int  (*compare_fn)(const u8* a, const u8* b, u64 size);
typedef void (*for_each_fn)(u8* elm); 


// CASTING

#define cast(x)    ((u8*)(&(x)))
#define castptr(x) ((u8*)(x))


// COMMON SIZES

#define KB (1 << 10)
#define MB (1 << 20)

#define nKB(n) ((u64)((n) * KB))
#define nMB(n) ((u64)((n) * MB))


// RAW BYTES TO HEX

void print_hex(const u8* ptr, u64 size, u32 bytes_per_line);

#endif /* WC_COMMON_H */

/* ===== fast_math.h ===== */
#ifndef WC_FAST_MATH_H
#define WC_FAST_MATH_H

/*
    Implement costy math functions using Numerical Methods
    This is faster than math lib, but with (way) less precision
    Use when you dont care about very precise values
    Originally written to be used for rng gaussian 
*/

#define PI     3.14159265359f
#define TWO_PI 6.28318530718f
#define LN2    0.693147180559945f


/* Fast sqrt using Newton-Raphson iteration (Thank u Professor Abrar)

For √x, we solve: f(n) = n² - x = 0
Newton-Raphson iteration: next = current - f(current)/f'(current)
For square root: next = 0.5 * (current + x/current)
*/
float fast_sqrt(float x);


// Natural log using series expansion: ln(1+x) = x - x²/2 + x³/3 - x⁴/4 + ...
// converges for |x| < 1
float fast_log(float x);

/*
Uses Taylor series expansion around x = 0:
sin(x) = x - x³/3! + x⁵/5! - x⁷/7! + ...
       = x - x³/6 + x⁵/120 - x⁷/5040 + ...
*/
float fast_sin(float x);

/*
Uses the trigonometric identity:
cos(x) = sin(x + π/2)
*/
float fast_cos(float x);

// TODO: e^x
/*
 By Taylor Series
e^x = 1 + x + x²/2! + x³/3! + x⁴/4! + ...
*/
float fast_exp(float x);


float fast_ceil(float x);
/*

Option 2: Range Reduction

Use e^x = e^(integer) × e^(fraction)
Compute e^(integer) by repeated squaring
Compute e^(fraction) using Taylor series
*/

// TODO: higher precision versions ?
// by increasing iteration count / Taylor series terms

#endif /* WC_FAST_MATH_H */

/* ===== random.h ===== */
#ifndef WC_RANDOM_H
#define WC_RANDOM_H

/*
    This Implimentation is Based on:
    *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
    Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
    And Also:
    https://github.com/imneme/pcg-c-basic
*/

/* PCG - Permuted Congruential Generator
 
    A simple Linear Congruential Generator (LCG) does:
        next_state = (xplier * curr_state + increment) % mod
    This is pretty weak and predictable
    In 3D, it forms 2D "layers"
    
    PCG takes this LCG and scrambles it using permutation function (hash fn)
    It halfes the output bits - 64 bits of state output a 32 bit random number

    Key Idea: Use some bits to decide how to scramble the other bits
        You have 64 bits, you use top few bits as "control code"
        You use that code to decide how to shuffle/rotate/shift the remaining

    Permutation Used (XSH RR):
    XSH RR = "XOR Shift High, Random Rotate"
    1. XOR high bits with full state, shift right
    2. Use top 5 bits to determine rotation amount
    3. Rotate the result by that amount
*/





/*
   PCG32 Random Number Generator State
   
   This structure holds the internal state of the RNG. Users typically don't
   need to interact with this directly - just use the global RNG functions.
   
   Fields:
     state - The 64-bit internal state (all possible values)
     inc   - The increment/sequence selector (MUST be ODD for full period)
             This determines which of 2^63 possible random sequences to Use
*/
typedef struct {
    u64 state;    // RNG state - advances with each random number generated
    u64 inc;      // Sequence selector - must be odd (ensures full period LCG)
} pcg32_random_t; // Private Internal RNG State


// Default initializer with pre-chosen values for state and increment.
#define PCG32_INITIALIZER {0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL}



// SEEDING FUNCTIONS
// ============================================================================

/*
   Seed the global RNG with specific values
   
   This is the primary way to initialize the random number generator.
   The same seed + sequence will always produce the same sequence of rands.
   
   Parameters:
     seed - Initial seed value (any 64-bit value works)
     seq  - Sequence selector (chooses which of 2^63 random sequences to use)
*/
void pcg32_rand_seed(u64 seed, u64 seq);


/*
   Seed using current system time (second precision)
   
   Uses time(NULL) to get seconds since Unix epoch.
   Good for general use when you want "random" behavior.
   
   Note: If called multiple times in the same second, will produce the
         same sequence. Use pcg32_rand_seed_time_hp() for finer precision.
*/
void pcg32_rand_seed_time(void);


// INTEGER RANDOM GENERATION
// ============================================================================

/*
   Generate a random 32-bit unsigned integer
   
   Returns a uniformly distributed value in the full range [0, 2^32-1].
   This is the core function - all other randoms are built on this.
   
   Algorithm (XSH RR - XOR Shift High, Random Rotate):
   
   Returns: Random u32 in range [0, 4294967295]
*/
u32 pcg32_rand(void);


/*
   Generate a random integer in range [0, bound)
   
   Returns a uniformly distributed value in [0, bound-1] with no modulo bias.
   
   Parameters:
     bound - Upper bound (exclusive), must be > 0
   
   Returns: Random u32 in range [0, bound-1]
*/
u32 pcg32_rand_bounded(u32 bound);


// FLOATING POINT UNIFORM DISTRIBUTION
// ============================================================================

/*
   Generate a random float in [0, 1)
   
   Returns a uniformly distributed float using the full 32-bit random value.
   
   Returns: Random float in range [0.0, 1.0) - note: 0.0 possible, 1.0 never
 */
float pcg32_rand_float(void);


/*
   Generate a random double in [0, 1)
   
   Returns a uniformly distributed double using TWO 32-bit random values
   for better precision (53 bits vs float's 23 bits).
   
   Returns: Random double in [0.0, 1.0) with ~15-16 digits of precision
*/
double pcg32_rand_double(void);


/*
   Generate a random float in range [min, max)
   
   Scales the [0, 1) uniform distribution to [min, max).
   
   Formula: min + rand_float() * (max - min)
   
   Parameters:
     min - Minimum value (inclusive)
     max - Maximum value (exclusive)
   
   Returns: Random float in [min, max)
*/
float pcg32_rand_float_range(float min, float max);


/*
   Generate a random double in range [min, max)
   
   Like pcg32_rand_float_range() but with double precision.
   
   Parameters:
     min - Minimum value (inclusive)
     max - Maximum value (exclusive)
   
   Returns: Random double in [min, max)
*/
double pcg32_rand_double_range(double min, double max);


// GAUSSIAN (NORMAL) DISTRIBUTION
// ============================================================================

/*
   Generate a random float from standard normal distribution
   
   Returns a value from the Gaussian (bell curve) distribution with:
     - Mean (μ) = 0
     - Standard deviation (σ) = 1
   
   Distribution properties:
     - Range: Theoretically [-∞, +∞], but extreme values are rare
     - ~68% of values fall in [-1, +1]
     - ~95% of values fall in [-2, +2]
     - ~99.7% of values fall in [-3, +3]
     - Values near 0 are most common (peak of bell curve)
   
   Algorithm: Box-Muller Transform
   
   Implementation notes:
     - Uses fast approximations: fast_sqrt(), fast_log(), fast_sin(), fast_cos()
     - Avoids U1 = 0 since ln(0) is undefined
     - Caches second value to avoid wasting computation
   
   Returns: Random float from N(0, 1) distribution
*/
float pcg32_rand_gaussian(void);


/*
   Generate a random float from custom normal distribution
   
   Returns a value from Gaussian distribution with specified mean and
   standard deviation.
   
   Formula: Z = standard_normal * stddev + mean
     Where standard_normal is from N(0, 1)
   
   Parameters:
     mean   - Center of the distribution (μ)
     stddev - Spread of the distribution (σ), must be > 0
   
   Distribution properties:
     - ~68% of values fall in [mean - stddev, mean + stddev]
     - ~95% of values fall in [mean - 2*stddev, mean + 2*stddev]
     - ~99.7% of values fall in [mean - 3*stddev, mean + 3*stddev]
   
   Returns: Random float from N(mean, stddev²) distribution
*/
float pcg32_rand_gaussian_custom(float mean, float stddev);

#endif /* WC_RANDOM_H */

#ifdef WC_IMPLEMENTATION

/* ===== common.c ===== */
#ifndef WC_COMMON_IMPL
#define WC_COMMON_IMPL

void print_hex(const u8* ptr, u64 size, u32 bytes_per_line) 
{
    if (ptr == NULL | size == 0 | bytes_per_line == 0) { return; }

    // hex rep 0-15
    const char* hex = "0123456789ABCDEF";
    
    for (u64 i = 0; i < size; i++) 
    {
        u8 val1 = ptr[i] >> 4;      // get upper 4 bits as num b/w 0-15
        u8 val2 = ptr[i] & 0x0F;    // get lower 4 bits as num b/w 0-15
        
        printf("%c%c", hex[val1], hex[val2]);
        
        // Add space or newline appropriately
        if ((i + 1) % bytes_per_line == 0) {
            printf("\n");
        } else if (i < size - 1) {
            printf(" ");
        }
    }

    // Add final newline if we didn't just print one
    if (size % bytes_per_line != 0) {
        printf("\n");
    }
}

#endif /* WC_COMMON_IMPL */

/* ===== fast_math.c ===== */
#ifndef WC_FAST_MATH_IMPL
#define WC_FAST_MATH_IMPL

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

#endif /* WC_FAST_MATH_IMPL */

/* ===== random.c ===== */
#ifndef WC_RANDOM_IMPL
#define WC_RANDOM_IMPL

#include <time.h>
void pcg32_rand_seed_r(pcg32_random_t* rng, u64 seed, u64 seq);
u32 pcg32_rand_r(pcg32_random_t* rng);
u32 pcg32_rand_bounded_r(pcg32_random_t* rng, u32 bound);



// Initialize global state
pcg32_random_t global_rng = PCG32_INITIALIZER;



void pcg32_rand_seed_r(pcg32_random_t* rng, u64 seed, u64 seq)
{
    rng->state = 0;
    //Set increment from sequence number. 
    //Left shift by 1 and OR with 1 ensures it's always odd
    //(required for the LCG to have full period).
    rng->inc = (seq << 1) | 1;
    // Run the generator once to scramble the initial state.
    pcg32_rand_r(rng);
    // Mix in the seed value to the state.
    rng->state += seed;
    // Run the generator again to further scramble.
    pcg32_rand_r(rng);
}

// public
void pcg32_rand_seed(u64 seed, u64 seq)
{
    pcg32_rand_seed_r(&global_rng, seed, seq);
}

u32 pcg32_rand_r(pcg32_random_t* rng)
{
    // Save old state
    u64 oldstate = rng->state;
    // LCG step: Advance the internal state using the Linear Congruential Generator formula:
    // Multiply by a specific constant (carefully chosen multiplier)
    // Add the increment (ensure it's odd with | 1)
    // This happens modulo 2^64 automatically due to overflow
    rng->state = (oldstate * 6364136223846793005ULL) + (rng->inc | 1);
    // Permutation step (XSH RR - XOR Shift High, Random Rotate):
    // Shift state right by 18 bits
    // XOR with the original state
    // Shift result right by 27 bits
    // Cast to 32-bit (takes lower 32 bits)
    // This creates a 32-bit value from the 64-bit state
    u32 xorshifted = (u32)(((oldstate >> 18) ^ oldstate) >> 27);
    // Use top 5 bits to decide rotation amount (0-31)
    u32 rot = oldstate >> 59;
    // rotate x by r -> (x >> r) | (x << (32 - r))
    // Random rotation: Rotate xorshifted right by rot bits:
    // xorshifted >> rot: Shift right by rot
    // xorshifted << ((-rot) & 31): Shift left by (32-rot), the & 31 ensures rotation stays in valid range
    // OR them together to complete the rotation
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

u32 pcg32_rand(void)
{
    return pcg32_rand_r(&global_rng);
}



u32 pcg32_rand_bounded_r(pcg32_random_t* rng, u32 bound)
{
    // To avoid bias, we need to make the range of the RNG a multiple of
    // bound, which we do by dropping output less than a threshold.
    // A naive scheme to calculate the threshold would be to do
    //
    //     uint32_t threshold = 0x100000000ull % bound;
    //
    // but 64-bit div/mod is slower than 32-bit div/mod (especially on
    // 32-bit platforms).  In essence, we do
    //
    //     uint32_t threshold = (0x100000000ull-bound) % bound;
    //
    // because this version will calculate the same modulus, but the LHS
    // value is less than 2^32.

    uint32_t threshold = -bound % bound;

    // Uniformity guarantees that this loop will terminate.  In practice, it
    // should usually terminate quickly; on average (assuming all bounds are
    // equally likely), 82.25% of the time, we can expect it to require just
    // one iteration.  In the worst case, someone passes a bound of 2^31 + 1
    // (i.e., 2147483649), which invalidates almost 50% of the range.  In 
    // practice, bounds are typically small and only a tiny amount of the range
    // is eliminated.
    for (;;) {
        uint32_t r = pcg32_rand_r(rng);
        if (r >= threshold) {
            return r % bound;
        }
    }
}

u32 pcg32_rand_bounded(u32 bound)
{
    return pcg32_rand_bounded_r(&global_rng, bound);
}


void pcg32_rand_seed_time(void)
{
    // Get current time in seconds since epoch
    time_t t = time(NULL);
    
    // Use time as seed
    // For sequence, we could use a constant or derive from time
    // Using a shifted version of time for sequence provides variation
    u64 seed = (u64)t;
    u64 seq = (u64)t ^ 0xda3e39cb94b95bdbULL;  // XOR with a constant for variation
    
    pcg32_rand_seed(seed, seq);
}


float pcg32_rand_float(void)
{
    // Method 1: Divide by 2^32
    // This gives uniform distribution in [0, 1)
    // 0x1.0p-32f is the float literal for 2^-32 (1.0 * 2^-32)
    return (float)pcg32_rand() * 0x1.0p-32f;
}

double pcg32_rand_double(void)
{
    // Combine two 32-bit random numbers for 53 bits of precision
    // (double has 53 bits of mantissa precision)
    
    // Get upper 27 bits from first random number
    u32 a = pcg32_rand() >> 5;  // Use top 27 bits
    // Get lower 26 bits from second random number  
    u32 b = pcg32_rand() >> 6;  // Use top 26 bits
    
    // Combine into 53-bit value and scale to [0, 1)
    // 0x1.0p-53 is the double literal for 2^-53
    return ((double)a * 67108864.0 + (double)b) * 0x1.0p-53;
}

// Generate a float in range [min, max)
float pcg32_rand_float_range(float min, float max)
{
    // Scale [0, 1) to [min, max)
    return min + (pcg32_rand_float() * (max - min));
}

// Generate a double in range [min, max)
double pcg32_rand_double_range(double min, double max)
{
    // Scale [0, 1) to [min, max)
    return min + (pcg32_rand_double() * (max - min));
}



// Box-Muller transform to generate Gaussian random numbers
// This generates TWO independent Gaussian values from TWO uniform randoms
// We cache one value for the next call to avoid wasting computation

// TODO: This breaks with multiple RNG instances
static float gaussian_spare_float = 0.0f;
static b8 has_spare_float = false;

float pcg32_rand_gaussian(void)
{
    // If we have a cached value from previous call, use it
    if (has_spare_float) {
        has_spare_float = false;
        return gaussian_spare_float;
    }
    
    // Box-Muller transform
    // Converts two uniform randoms U1, U2 in [0,1) into two independent Gaussians
    // Formula: Z0 = sqrt(-2 * ln(U1)) * cos(2π * U2)
    //          Z1 = sqrt(-2 * ln(U1)) * sin(2π * U2)
    
    float u1, u2;
    
    // Get two random numbers in (0, 1)
    // We avoid exactly 0 for u1 because ln(0) is undefined
    do {
        u1 = pcg32_rand_float();
    } while (u1 == 0.0f);
    
    u2 = pcg32_rand_float();
    
    // Calculate the Box-Muller transform
    // sqrt(-2 * ln(u1))
    float mag = fast_sqrt(-2.0f * fast_log(u1));
    
    // 2π * u2
    // const float TWO_PI = 6.28318530718f;
    float angle = TWO_PI * u2;
    
    // Generate two independent Gaussian values
    float z0 = mag * fast_cos(angle);
    float z1 = mag * fast_sin(angle);
    
    // Save one for next call
    gaussian_spare_float = z1;
    has_spare_float = true;
    
    // Return the other
    return z0;
}

float pcg32_rand_gaussian_custom(float mean, float stddev)
{
    // Standard normal * stddev + mean
    return (pcg32_rand_gaussian() * stddev) + mean;
}


// Private Math Fuctions

#endif /* WC_RANDOM_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_RANDOM_SINGLE_H */
