/*
 * A5 compile check: WC_REALLOC_N(T, p, n) must compile when used.
 *
 * Today it expands to wc_realloc_aligned((p), sizeof(T) * (n), alignof(T)),
 * i.e. 3 arguments to a 4-parameter function, so this file FAILS to compile.
 * The ctest entry `a5_realloc_n_compiles` is marked WILL_FAIL until Phase 1.
 * Phase 1: rewrite against WC2_REALLOC_N(a, T, p, old_n, n), drop WILL_FAIL.
 */
#include "wc_allocator.h"

int* a5_grow(int* p, size_t old_n, size_t n);

int* a5_grow(int* p, size_t old_n, size_t n)
{
    (void)old_n;
    return WC_REALLOC_N(int, p, n);
}
