#define WC_TEST_MAIN
#include "wc_test.h"


// Correctness suites 
void String_suite(void);
void Arena_suite(void);
void gen_vector_suite(void);
void HashMap_suite(void);
void HashSet_suite(void);
void Stack_suite(void);
void Queue_suite(void);
void matrix_suite(void);
void bit_vector_suite(void);
void fast_math_suite(void);
void complex_suite(void);
void macros_suite(void);
void views_suite(void);

int speed_suite(void);


int main(void)
{
    // Correctness 
    String_suite();

    Arena_suite();

    gen_vector_suite();

    bit_vector_suite();

    HashMap_suite();

    HashSet_suite();

    Stack_suite();

    Queue_suite();

    matrix_suite();

    fast_math_suite();

    complex_suite();

    macros_suite();

    views_suite();

    speed_suite();

    return WC_REPORT();
}

