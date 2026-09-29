
#include "common.h"
#include "gen_vector.h"
#include "wc_macros.h"
#include <stdio.h>



int main(void)
{
    GenVec* v = GenVec_create(10, sizeof(int), NULL);

    VEC_PUSH(v, 1);
    VEC_PUSH(v, 2);
    VEC_PUSH(v, 3);
    VEC_PUSH(v, 4);

    GenVec_print(v, wc_print_int); putchar('\n');

    GenVec_destroy(v);
    return 0;
}
