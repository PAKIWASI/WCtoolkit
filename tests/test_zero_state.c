// Zero-state and move-destination round-trips. The illegal half is enforced by
// WC_ASSERTs in Debug, so the whole file is compiled out under NDEBUG.
#ifndef NDEBUG

#include "common.h"
#include "gen_vector.h"
#include "hashmap.h"
#include "priority_queue.h"
#include "queue.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_string.h"

#include <string.h>

static int cmp_int(const void* a, const void* b, u64 size)
{
    (void)size;
    return *(const int*)a - *(const int*)b;
}

UTEST(zero_state, live_containers_pass)
{
    PriorityQueue pq = PriorityQueue_create(WC_LIBC, 4, sizeof(int), NULL, cmp_int);
    int           x  = 3;
    PriorityQueue_push(&pq, &x);
    EXPECT_EQ(*(const int*)PriorityQueue_get(&pq, 0), 3);
    PriorityQueue_destroy(&pq);

    String s = String_from_cstr(WC_LIBC, "ac");
    String_insert_cstr(&s, 1, "b");
    String_insert_cstr(&s, 0, "");
    String_destroy(&s);
}

UTEST(zero_state, move_into_zeroed_dest_passes)
{
    GenVec gsrc = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    GenVec gdst = {0};
    GenVec_move(&gdst, &gsrc);
    GenVec_destroy(&gdst);

    String ssrc = String_from_cstr(WC_LIBC, "a");
    String sdst = {0};
    String_move(&sdst, &ssrc);
    String_destroy(&sdst);

    Queue qsrc = Queue_create(WC_LIBC, 4, sizeof(int), NULL);
    Queue qdst = {0};
    Queue_move(&qdst, &qsrc);
    Queue_destroy(&qdst);

    HashMap msrc = HashMap_create(WC_LIBC, sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
    HashMap mdst = {0};
    HashMap_move(&mdst, &msrc);
    HashMap_destroy(&mdst);
}

#endif // NDEBUG
