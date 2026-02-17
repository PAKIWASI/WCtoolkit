#ifndef FAST_MATH_H
#define FAST_MATH_H


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


#endif // FAST_MATH_H
