#include "arena.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_macros.h"

#include <stdio.h>



int main(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1)); // region comes from libc

    // the vector's storage comes from the arena: no global allocator
    GenVec v = VEC_OF_IN(Arena_allocator(&a), int, 5);

    for (int i = 0; i < 70; i++) {
        VEC_PUSH(&v, i);
    }

    GenVec_print(&v, wc_print_int); putchar('\n');

    print_hex(a.base, nKB(1), 32);

    GenVec_destroy(&v); // last block of the arena: rewinds it
    Arena_destroy(&a);
    return 0;
}
