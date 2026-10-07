#include "arena.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_macros.h"
#include <stdio.h>

// TODO: unique ptr, shared ptr types using attribute cleanup??
// iterators?
// using [[discard]] vs attribute warn unused result and others like this



int main(void)
{
    Arena va = Arena_create(WC_MMAP, nKB(4));

    u8* d1 = Arena_alloc(&va, 10);
    d1[0] = 69;
    d1[9] = 69;

    u8* d2 = Arena_alloc(&va, 10);
    d2[0] = 69;
    d2[9] = 69;

    u8* d3 = Arena_alloc_aligned(&va, 4, 4);
    d3[0] = 69;
    d3[3] = 69;

    u8* d4 = Arena_alloc(&va, 16);
    d4[0] = 69;
    d4[15] = 69;

    LOG("arena usage: %lu", va.off);
    ArenaScratch vas = ArenaScratch_create(&va);
    for (int i = 0; i < 1000; i++) {
        u8* d5 = Arena_alloc(&va, nKB(1));
        d5[0] = 56;
        d5[nKB(1) - 1] = 50;
        ArenaScratch_scratch_off(vas);
    }
    ArenaScratch_destroy(vas);
    LOG("arena usage: %lu", va.off);


    wc_allocator va_alloc = Arena_create_allocator(&va);
    GenVec v = GenVec_create(&va_alloc, 5, sizeof(int), NULL);
    VEC_PUSH(&v, 1);
    VEC_PUSH(&v, 2);
    VEC_PUSH(&v, 3);
    VEC_PUSH(&v, 4);
    GenVec_print(&v, wc_print_int); putchar('\n');

    VEC_PUSH(&v, 5);
    VEC_PUSH(&v, 6);
    VEC_PUSH(&v, 7);
    GenVec_print(&v, wc_print_int); putchar('\n');

}
