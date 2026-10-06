#include "arena.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_macros.h"
#include <stdio.h>


int main(void)
{
    Varena va = Varena_create(0);
    const wc_allocator vallocator = Varena_create_allocator(&va);

    GenVec v = GenVec_create(&vallocator, 5, sizeof(int), NULL);
    VEC_PUSH(&v, 1);
    VEC_PUSH(&v, 2);
    VEC_PUSH(&v, 3);
    VEC_PUSH(&v, 4);
    GenVec_print(&v, wc_print_int); putchar('\n');

    VEC_PUSH(&v, 5);
    VEC_PUSH(&v, 6);

    GenVec_print(&v, wc_print_int); putchar('\n');

    GenVec_print(&v, wc_print_int); putchar('\n');

    LOG("arena usage: %lu", va.off);
    VarenaScratch vas = VarenaScratch_create(&va);
    for (int i = 0; i < 10; i++) {
        int* data = VARENA_ALLOC_N(&va, int, 10);
        LOG("arena usage: %lu", va.off);
        VarenaScratch_scratch_off(vas);
    }
    VarenaScratch_destroy(vas);
    LOG("arena usage: %lu", va.off);



    return 0;
}
