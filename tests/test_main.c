#define WC_TEST_MAIN
#include "wc_test.h"
#include "wc_speed_test.h"


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

// Speed suites 
void gen_vector_speed_suite(void);
void string_speed_suite(void);
void hashmap_speed_suite(void);
void hashset_speed_suite(void);
void arena_speed_suite(void);
// void matrix_speed_suite(void);


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

    int result = WC_REPORT();

    // Speed (always runs, doesn't affect exit code) 
    printf("\n\033[1;36m════════════════ SPEED BENCHMARKS ════════════════\033[0m\n");
    gen_vector_speed_suite();
    string_speed_suite();
    hashmap_speed_suite();
    hashset_speed_suite();
    arena_speed_suite();
    // matrix_speed_suite();
    printf("\n");

    return result;
}

