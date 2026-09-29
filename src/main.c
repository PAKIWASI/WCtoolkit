#include "arena.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_macros.h"

#include <stdio.h>



int main(void)
{
    Arena* a = Arena_create(nKB(4));
    WC_SET_ALLOCATOR(Arena_allocator(a));

    GenVec* v = VEC_CREATE_OF(int, 5);

    for (int i = 0; i < 20; i++) {
        VEC_PUSH(v, i);
    }

    GenVec_print(v, wc_print_int); putchar('\n');

    print_hex(a->base, 512, 32);

    GenVec_destroy(v);
    Arena_destroy(a);
    return 0;
}
