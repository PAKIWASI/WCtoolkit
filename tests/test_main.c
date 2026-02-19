#include "wc_test.h"

/* ── Forward declarations — one per test file ───────────────────────────── */
void string_suite(void);
void arena_suite(void);
void gen_vector_suite(void);
void hashmap_suite(void);
void hashset_suite(void);
void stack_suite(void);
void queue_suite(void);
void matrix_suite(void);
void random_suite(void);
void bit_vector_suite(void);
void complex_suite(void);   /* vec-of-vec, vec-of-string, map-of-vec, macros */


int main(void)
{
    /* Core data structures */
    string_suite();
    arena_suite();
    gen_vector_suite();
    bit_vector_suite();

    /* Hash containers */
    hashmap_suite();
    hashset_suite();

    /* Adapters */
    stack_suite();
    queue_suite();

    /* Math */
    matrix_suite();
    random_suite();

    /* Complex / ownership patterns */
    complex_suite();

    return WC_REPORT();
}
