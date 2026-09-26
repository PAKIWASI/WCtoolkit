#include "common.h"
#include "gen_vector.h"
#include "priority_queue.h"
#include "wc_macros.h"
#include <stdio.h>

static int int_compare(const u8* a, const u8* b, u64 size)
{
    (void)size;
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

int main(void)
{
    GenVec* vec = VEC_FROM_ARR(int, 10, ((int[10]){3, 2, 5, 9, 1, 10, 47, 0, 2, 1}));
    GenVec_print(vec, wc_print_int); putchar('\n');

    PriorityQueue* pq = PriorityQueue_from_vec(vec, int_compare);
    PriorityQueue_print(pq, wc_print_int);

    int p;
    for (int i = 0; i < 7; i++) {
        PriorityQueue_pop(pq, cast(p));
        printf("popped: %d\n", p);
        PriorityQueue_print(pq, wc_print_int);
    }

    GenVec_destroy(vec);
    PriorityQueue_destroy(pq);
    return 0;
}
