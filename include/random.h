#ifndef RANDOM_H
#define RANDOM_H

#include "common.h"

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
} pcg32_random_t;


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



#endif // RANDOM_H
