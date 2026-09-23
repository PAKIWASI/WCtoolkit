#include "common.h"
#include "gen_vector.h"
#include "map_setup.h"
#include "priority_queue.h"
#include "wc_macros.h"
#include <stdio.h>

int main(void)
{
    GenVec* vec = VEC_FROM_ARR(int, 5, ((int[5]){3, 2, 5, 9, 1}));
    GenVec_print(vec, wc_print_int); putchar('\n');

    PriorityQueue* pq = PriorityQueue_from_vec(vec, default_compare);
    // GenVec_print(&pq->arr, wc_print_int);


    GenVec_destroy(vec);
    // PriorityQueue_destroy(pq);
    return 0;
}
