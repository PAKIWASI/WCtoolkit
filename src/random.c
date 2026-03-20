#include "random.h"
#include "fast_math.h"

#include <time.h>



static void pcg32_rand_seed_r(pcg32_random_t* rng, u64 seed, u64 seq);
static u32 pcg32_rand_r(pcg32_random_t* rng);
static u32 pcg32_rand_bounded_r(pcg32_random_t* rng, u32 bound);



// Initialize global state
static pcg32_random_t global_rng = PCG32_INITIALIZER;


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
    return (((double)a * 67108864.0) + (double)b) * 0x1.0p-53;
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



