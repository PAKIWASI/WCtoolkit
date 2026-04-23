#ifndef WC_FAST_MATH_SINGLE_H
#define WC_FAST_MATH_SINGLE_H

/*
 * fast_math_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "fast_math_single.h"
 *
 * All other files just:
 *     #include "fast_math_single.h"
 */

/* ===== common.h ===== */
#ifndef WC_COMMON_H
#define WC_COMMON_H

/*
 * WCtoolkit
 * Copyright (c) 2026 Wasi Ullah (PAKIWASI)
 * Licensed under the MIT License. See LICENSE file for details.
 */



// LOGGING/ERRORS

#include <stdio.h>
#include <stdlib.h>

// ANSI Color Codes
#define WC_COLOR_RESET  "\033[0m"
#define WC_COLOR_RED    "\033[1;31m"
#define WC_COLOR_YELLOW "\033[1;33m"
#define WC_COLOR_GREEN  "\033[1;32m"
#define WC_COLOR_BLUE   "\033[1;34m"
#define WC_COLOR_CYAN   "\033[1;36m"



// TODO: warm paths ?

#define WARN(fmt, ...)                                            \
    do {                                                          \
        printf(WC_COLOR_YELLOW "[WARN]"                              \
                            " %s:%d:%s(): " fmt "\n" WC_COLOR_RESET, \
               __FILE__, __LINE__, __func__, ##__VA_ARGS__);      \
    } while (0)

#define FATAL(fmt, ...)                                         \
    do {                                                        \
        fprintf(stderr,                                         \
                WC_COLOR_RED "[FATAL]"                             \
                          " %s:%d:%s(): " fmt "\n" WC_COLOR_RESET, \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__);   \
        exit(EXIT_FAILURE);                                     \
    } while (0)

#define CHECK_WARN(cond, fmt, ...)                           \
    do {                                                     \
        if (__builtin_expect(!!(cond), 0)) {                 \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                    \
    } while (0)

#define CHECK_WARN_RET(cond, ret, fmt, ...)                  \
    do {                                                     \
        if (__builtin_expect(!!(cond), 0)) {                 \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                                      \
        }                                                    \
    } while (0)

#define CHECK_FATAL(cond, fmt, ...)                           \
    do {                                                      \
        if (__builtin_expect(!!(cond), 0)) {                  \
            FATAL("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                     \
    } while (0)

#define LOG(fmt, ...)                                       \
    do {                                                    \
        printf(WC_COLOR_CYAN "[LOG]"                           \
                          " : %s(): " fmt "\n" WC_COLOR_RESET, \
               __func__, ##__VA_ARGS__);                    \
    } while (0)


#define MALLOC(size, cap, name) ({\
    void* _mlcd = malloc(size * cap);\
    CHECK_FATAL(!_mlcd, "\"" #name "\"" " malloc failed");\
    _mlcd;\
})


// TYPES

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint8_t  b8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// #define false ((b8)0)
// #define true  ((b8)1)


// GENERIC FUNCTIONS
typedef void (*copy_fn)(u8* dest, const u8* src);
typedef void (*move_fn)(u8* dest, u8** src);
typedef void (*delete_fn)(u8* key);
typedef void (*print_fn)(const u8* elm);
typedef int (*compare_fn)(const u8* a, const u8* b, u64 size);


// Vtable: one instance shared across all vectors of the same type.
// Pass NULL for any callback not needed.
// For POD types, pass NULL for the whole ops pointer.
typedef struct {
    copy_fn   copy_fn; // Deep copy function for owned resources (or NULL)
    move_fn   move_fn; // Transfer ownership and null original (or NULL)
    delete_fn del_fn;  // Cleanup function for owned resources (or NULL)
} container_ops;


// CASTING

#define cast(x)    ((u8*)(&(x)))
#define castptr(x) ((u8*)(x))


// COMMON SIZES

#define KB (1 << 10)
#define MB (1 << 20)

#define nKB(n) ((u64)((n) * KB))
#define nMB(n) ((u64)((n) * MB))


// RAW BYTES TO HEX

static inline void print_hex(const u8* ptr, u64 size, u32 bytes_per_line)
{
    if (ptr == NULL || size == 0 || bytes_per_line == 0) {
        return;
    }

    // hex rep 0-15
    const char* hex = "0123456789ABCDEF";

    for (u64 i = 0; i < size; i++) {
        u8 val1 = ptr[i] >> 4;   // get upper 4 bits as num b/w 0-15
        u8 val2 = ptr[i] & 0x0F; // get lower 4 bits as num b/w 0-15

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


// TEST HELPERS

// Generic print functions for primitive types
static inline void wc_print_int(const u8* elm)
{
    printf("%d ", *(int*)elm);
}
static inline void wc_print_u32(const u8* elm)
{
    printf("%u ", *(u32*)elm);
}
static inline void wc_print_u64(const u8* elm)
{
    printf("%llu ", (unsigned long long)*(u64*)elm);
}
static inline void wc_print_float(const u8* elm)
{
    printf("%.2f ", *(float*)elm);
}
static inline void wc_print_char(const u8* elm)
{
    printf("%c ", *(char*)elm);
}
static inline void wc_print_cstr(const u8* elm)
{
    printf("%s ", (const char*)elm);
}

#endif /* WC_COMMON_H */

/* ===== fast_math.h ===== */
#ifndef WC_FAST_MATH_H
#define WC_FAST_MATH_H

/*
    Implement costy math functions using Numerical Methods
    This is faster than math lib, but with (way) less precision
    Use when you dont care about very precise values
    Originally written to be used for rng gaussian/ neural network weights init
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

/*
 By Taylor Series
e^x = 1 + x + x²/2! + x³/3! + x⁴/4! + ...
*/
float fast_exp(float x);


float fast_ceil(float x);


/*
    a^b = e^(b * ln(a))
    Uses exponentiation by squaring for the integer part of the exponent,
    and fast_exp(frac * fast_log(base)) for the fractional part.
*/
float fast_pow(float base, float exp);

#endif /* WC_FAST_MATH_H */

#ifdef WC_IMPLEMENTATION

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

/* We use the squaring trick from fast_exp function for the integer exponent, and for fractional exponent, we use the identity a^b = e^(b * ln(a)). So we used the fast_exp and fast_log functions already defined.*/ 

float fast_pow(float base, float exp) {

    // 1. Simple Edge Cases
    if (exp == 0.0f) return 1.0f;
    if (exp == 1.0f) return base;

    if (base == 0.0f) {
        return (exp > 0.0f) ? 0.0f : 1e38f; 
    }
    // May include other smaller exps like cube and power of 4.
    if (exp == 2.0f) {
        return base * base;
    }

    if(exp == 0.5f) {
        return fast_sqrt(base);
    }
    

    // negative exponents
    b8 is_negative_exp = 0;
    if (exp < 0.0f) {
        is_negative_exp = 1;
        exp = -exp;
    }

    // Borrowing from how the fast_exp function is implemented, we split the exponent into integer and fractional parts for better optimization.
    int int_part = (int)exp;
    float frac_part = exp - (float)int_part;
    
    float result = 1.0f;

    // Calculate fractional exponent part by using the fast_exp and fast_log funtions provided.
    if (frac_part > 0.0f) {
        float intermediate = frac_part * fast_log(base);
        // I think the overflow checking is handled gracefully in the fast_exp funtion and we dont actually need to check for it here. Need confirmation for this. will find out during testing.
        // if (intermediate > 88.0f) return is_negative_exp ? 0.0f : 1e38f; 
        // if (intermediate < -87.0f) return is_negative_exp ? 1e38f : 0.0f;
        result = fast_exp(intermediate);
    }

    // Incorporating integer part. Used exponentiation by squaring.
    //Didnt include overflow checks here, as checking in every iteration is more expensive than the native infinity calculations, also 1.f/result will give zero automatically if overflowed, so we get more optimization. Again, needs confirmation.
    while (int_part > 0) {

        if (int_part & 1) { 
            result *= base;
        }

        int_part >>= 1; 
        if (!int_part) break; 

        base *= base;
    }

    // Reciprocate if negative exp.
    return is_negative_exp ? (1.0f / result) : result;
}

#endif /* WC_FAST_MATH_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_FAST_MATH_SINGLE_H */
