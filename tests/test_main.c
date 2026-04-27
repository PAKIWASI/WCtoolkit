#define WC_TEST_MAIN
#include "wc_test.h"


// Correctness suites 
void string_suite(void);
void arena_suite(void);
void gen_vector_suite(void);
void hashmap_suite(void);
void hashset_suite(void);
void stack_suite(void);
void queue_suite(void);
void matrix_suite(void);
void bit_vector_suite(void);
void fast_math_suite(void);
void complex_suite(void);



int main(void)
{
    // Correctness 
    string_suite();

    arena_suite();

    gen_vector_suite();

    bit_vector_suite();

    hashmap_suite();

    hashset_suite();

    stack_suite();

    queue_suite();

    matrix_suite();

    fast_math_suite();

    complex_suite();

    return WC_REPORT();
}

