// Zero-state and move-destination checks. All are WC_ASSERTs: they die in Debug
// and are stripped under NDEBUG, so the whole file is Debug-only.
#ifndef NDEBUG

#include "common.h"
#include "gen_vector.h"
#include "hashmap.h"
#include "priority_queue.h"
#include "queue.h"
#include "test_support.h"
#include "utest.h"
#include "wc_allocator.h"
#include "wc_string.h"

#include <string.h>

#if WC_HAS_FORK

static int cmp_int(const void* a, const void* b, u64 size)
{
    (void)size;
    return *(const int*)a - *(const int*)b;
}

// ── zeroed container: read/mutate gaps (Part 3) ──────────────────────────────

static void pq_get_zeroed(void)
{
    PriorityQueue pq;
    memset(&pq, 0, sizeof(pq));
    (void)PriorityQueue_get(&pq, 0);
}

static void pq_pop_zeroed(void)
{
    PriorityQueue pq;
    memset(&pq, 0, sizeof(pq));
    int out;
    PriorityQueue_pop(&pq, &out);
}

static void pq_remove_zeroed(void)
{
    PriorityQueue pq;
    memset(&pq, 0, sizeof(pq));
    PriorityQueue_remove(&pq, 0, NULL);
}

static void genvec_remove_range_zeroed(void)
{
    GenVec v;
    memset(&v, 0, sizeof(v));
    GenVec_remove_range(&v, 0, 0); // len 0 used to return before any check
}

static void string_insert_cstr_zeroed(void)
{
    String s;
    memset(&s, 0, sizeof(s));
    String_insert_cstr(&s, 0, ""); // empty text used to return silently
}

static void string_insert_string_zeroed(void)
{
    String s;
    memset(&s, 0, sizeof(s));
    String src = String_from_cstr(WC_LIBC, "abc");
    String_insert_String(&s, 0, &src);
}

UTEST(zero_state, read_mutate_gaps_die)
{
    EXPECT_DIES(pq_get_zeroed);
    EXPECT_DIES(pq_pop_zeroed);
    EXPECT_DIES(pq_remove_zeroed);
    EXPECT_DIES(genvec_remove_range_zeroed);
    EXPECT_DIES(string_insert_cstr_zeroed);
    EXPECT_DIES(string_insert_string_zeroed);
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

// ── move destination must be zeroed (Part 4) ─────────────────────────────────

static void genvec_move_live_dest(void)
{
    GenVec src  = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    GenVec dest = GenVec_create(WC_LIBC, 4, sizeof(int), NULL);
    GenVec_move(&dest, &src);
}

static void string_move_live_dest(void)
{
    String src  = String_from_cstr(WC_LIBC, "a");
    String dest = String_from_cstr(WC_LIBC, "b");
    String_move(&dest, &src);
}

static void hashmap_move_live_dest(void)
{
    HashMap src  = HashMap_create(WC_LIBC, sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
    HashMap dest = HashMap_create(WC_LIBC, sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
    HashMap_move(&dest, &src);
}

static void queue_move_live_dest(void)
{
    Queue src  = Queue_create(WC_LIBC, 4, sizeof(int), NULL);
    Queue dest = Queue_create(WC_LIBC, 4, sizeof(int), NULL);
    Queue_move(&dest, &src);
}

UTEST(zero_state, move_into_live_dest_dies)
{
    EXPECT_DIES(genvec_move_live_dest);
    EXPECT_DIES(string_move_live_dest);
    EXPECT_DIES(hashmap_move_live_dest);
    EXPECT_DIES(queue_move_live_dest);
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

#endif // WC_HAS_FORK
#endif // NDEBUG
