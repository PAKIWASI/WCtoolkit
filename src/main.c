#include "arena.h"
#include "chain_arena.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_macros.h"
#include <stdio.h>

// TODO:
// iterators?

static int run1(void)
{
    ChainArena ca;
    ChainArena_create(&ca, WC_MMAP, nKB(4));

    volatile u8* d1 = ChainArena_alloc(&ca, 10);
    d1[0]           = 69;
    d1[9]           = 69;

    volatile u8* large = ChainArena_alloc(&ca, nKB(8));
    large[0]           = 69;
    large[nKB(1)]      = 69;
    large[nKB(4)]      = 69;

    GenVec v = GenVec_create(ChainArena_allocator(&ca), 5, sizeof(int), NULL);
    VEC_PUSH(&v, 1);
    VEC_PUSH(&v, 2);
    VEC_PUSH(&v, 3);
    VEC_PUSH(&v, 4);
    GenVec_print(&v, wc_print_int);
    putchar('\n');

    VEC_PUSH(&v, 5);
    VEC_PUSH(&v, 6);
    VEC_PUSH(&v, 7);
    GenVec_print(&v, wc_print_int);
    putchar('\n');


    ChainArenaScratch cas = ChainArenaScratch_create(&ca);

    // BUG: gdb output look kinda wrong?
    volatile u8* normal  = ChainArena_alloc(&ca, nKB(3)); // same node
    normal[nKB(2)]       = 69;
    volatile u8* normal2 = ChainArena_alloc(&ca, nKB(3)); // new node
    normal2[nKB(2)]      = 69;
    volatile u8* normal3 = ChainArena_alloc(&ca, nKB(3)); // new node
    normal3[nKB(2)]      = 69;

    volatile u8* huge = ChainArena_alloc(&ca, nMB(2)); // seperate big node
    huge[nMB(1)]      = 69;

    return 0;
}


int main(void)
{
    return run1();

    Arena va;
    Arena_create(&va, WC_MMAP, nKB(4));

    u8* d1 = Arena_alloc(&va, 10);
    d1[0]  = 69;
    d1[9]  = 69;

    u8* d2 = Arena_alloc(&va, 10);
    d2[0]  = 69;
    d2[9]  = 69;

    u8* d3 = Arena_alloc_aligned(&va, 4, 4);
    d3[0]  = 69;
    d3[3]  = 69;

    u8* d4 = Arena_alloc(&va, 16);
    d4[0]  = 69;
    d4[15] = 69;

    LOG("arena usage: %lu", va.off);
    ArenaScratch vas = ArenaScratch_create(&va);
    for (int i = 0; i < 1000; i++) {
        u8* d5         = Arena_alloc(&va, nKB(1));
        d5[0]          = 56;
        d5[nKB(1) - 1] = 50;
        ArenaScratch_to_mark(vas);
    }
    LOG("arena usage: %lu", va.off);


    GenVec v = GenVec_create(Arena_allocator(&va), 5, sizeof(int), NULL);
    VEC_PUSH(&v, 1);
    VEC_PUSH(&v, 2);
    VEC_PUSH(&v, 3);
    VEC_PUSH(&v, 4);
    GenVec_print(&v, wc_print_int);
    putchar('\n');

    VEC_PUSH(&v, 5);
    VEC_PUSH(&v, 6);
    VEC_PUSH(&v, 7);
    GenVec_print(&v, wc_print_int);
    putchar('\n');
}
