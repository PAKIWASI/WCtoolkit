#include "arena.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_macros.h"

#include <stdio.h>



int main(void)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));            // region comes from libc
    WC_SET_ALLOCATOR(Arena_allocator_legacy(&a)); // global -> arena (transitional, D11)

    GenVec* v = VEC_CREATE_OF(int, 5);

    for (int i = 0; i < 70; i++) {
        VEC_PUSH(v, i);
    }

    GenVec_print(v, wc_print_int); putchar('\n');

    print_hex(a.base, nKB(1), 32);

    WC_SET_ALLOCATOR((wc_libc_allocator));
    Arena_destroy(&a);
    return 0;
}
