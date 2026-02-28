#include "wc_test.h"
#include "wc_macros.h"
#include "gen_vector.h"
#include "wc_errno.h"
#include <bits/time.h>


/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Simple int vec — no copy/move/del needed */
static genVec* int_vec(u64 cap) {
    return genVec_init(cap, sizeof(int), NULL);
}

static void push_ints(genVec* v, int count) {
    for (int i = 0; i < count; i++) {
        genVec_push(v, (u8*)&i);
    }
}


/* ── Init ────────────────────────────────────────────────────────────────── */

static void test_init_zero_cap(void)
{
    genVec* v = genVec_init(0, sizeof(int), NULL);
    WC_ASSERT_NOT_NULL(v);
    WC_ASSERT_EQ_U64(genVec_size(v), 0);
    WC_ASSERT_EQ_U64(genVec_capacity(v), 0);
    WC_ASSERT_TRUE(genVec_empty(v));
    genVec_destroy(v);
}

static void test_init_with_cap(void)
{
    genVec* v = int_vec(8);
    WC_ASSERT_EQ_U64(genVec_size(v), 0);
    WC_ASSERT_EQ_U64(genVec_capacity(v), 8);
    genVec_destroy(v);
}

static void test_init_val(void)
{
    int val  = 42;
    genVec* v = genVec_init_val(5, (u8*)&val, sizeof(int), NULL);
    WC_ASSERT_EQ_U64(genVec_size(v), 5);
    for (u64 i = 0; i < 5; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, i), 42);
    }
    genVec_destroy(v);
}

static void test_init_stk(void)
{
    genVec v;
    genVec_init_stk(4, sizeof(int), NULL, &v);
    WC_ASSERT_EQ_U64(genVec_size(&v), 0);
    WC_ASSERT_EQ_U64(genVec_capacity(&v), 4);
    genVec_destroy_stk(&v);
}

static void test_init_arr(void)
{
    genVec* v = VEC_FROM_ARR(int, 4, ((int[4]){1,2,3,4}));

    WC_ASSERT_EQ_U64(genVec_size(v), 4);
    WC_ASSERT_EQ_U64(v->data_size, sizeof(int));

    genVec_destroy(v);
}


/* ── Push / Pop ──────────────────────────────────────────────────────────── */

static void test_push_grows_size(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 3);
    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    genVec_destroy(v);
}

static void test_push_triggers_growth(void)
{
    genVec* v = int_vec(2);
    push_ints(v, 10); /* force multiple reallocations */
    WC_ASSERT_EQ_U64(genVec_size(v), 10);
    /* values must survive realloc */
    for (int i = 0; i < 10; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, (u64)i), i);
    }
    genVec_destroy(v);
}

static void test_pop_reduces_size(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 3);
    genVec_pop(v, NULL);
    WC_ASSERT_EQ_U64(genVec_size(v), 2);
    genVec_destroy(v);
}

static void test_pop_copies_value(void)
{
    genVec* v = int_vec(4);
    int val   = 99;
    genVec_push(v, (u8*)&val);
    int out = 0;
    genVec_pop(v, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 99);
    genVec_destroy(v);
}

static void test_pop_empty_sets_errno(void)
{
    genVec* v = int_vec(4);
    wc_errno  = WC_OK;
    genVec_pop(v, NULL);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_EMPTY);
    genVec_destroy(v);
}


/* ── Get ─────────────────────────────────────────────────────────────────── */

static void test_get_ptr(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 4);
    for (int i = 0; i < 4; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, (u64)i), i);
    }
    genVec_destroy(v);
}

static void test_get_copies(void)
{
    genVec* v = int_vec(4);
    int val   = 7;
    genVec_push(v, (u8*)&val);
    int out = 0;
    genVec_get(v, 0, (u8*)&out);
    WC_ASSERT_EQ_INT(out, 7);
    genVec_destroy(v);
}

static void test_front_back(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 4); /* 0 1 2 3 */
    WC_ASSERT_EQ_INT(*(int*)genVec_front(v), 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_back(v),  3);
    genVec_destroy(v);
}

static void test_front_empty_sets_errno(void)
{
    genVec* v = int_vec(4);
    wc_errno  = WC_OK;
    genVec_front(v);
    WC_ASSERT_EQ_INT(wc_errno, WC_ERR_EMPTY);
    genVec_destroy(v);
}


/* ── Insert / Remove ─────────────────────────────────────────────────────── */

static void test_insert_front(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 3); /* 0 1 2 */
    int x = 99;
    genVec_insert(v, 0, (u8*)&x);
    WC_ASSERT_EQ_U64(genVec_size(v), 4);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 99);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 0);
    genVec_destroy(v);
}

static void test_insert_mid(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 4); /* 0 1 2 3 */
    int x = 55;
    genVec_insert(v, 2, (u8*)&x);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 55);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 3), 2);
    genVec_destroy(v);
}

static void test_remove_front(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 3); /* 0 1 2 */
    genVec_remove(v, 0, NULL);
    WC_ASSERT_EQ_U64(genVec_size(v), 2);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 1);
    genVec_destroy(v);
}

static void test_remove_mid(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 4); /* 0 1 2 3 */
    genVec_remove(v, 1, NULL);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 2);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 3);
    genVec_destroy(v);
}

static void test_remove_range(void)
{
    genVec* v = int_vec(8);
    push_ints(v, 6); /* 0 1 2 3 4 5 */
    genVec_remove_range(v, 1, 3); /* remove 1,2,3 */
    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 4);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 5);
    genVec_destroy(v);
}


/* ── Replace ─────────────────────────────────────────────────────────────── */

static void test_replace(void)
{
    genVec* v = int_vec(4);
    push_ints(v, 3); /* 0 1 2 */
    int x = 77;
    genVec_replace(v, 1, (u8*)&x);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 77);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 2);
    genVec_destroy(v);
}


/* ── Reserve ─────────────────────────────────────────────────────────────── */

static void test_reserve_grows_capacity(void)
{
    genVec* v = int_vec(4);
    genVec_reserve(v, 100);
    WC_ASSERT_TRUE(genVec_capacity(v) >= 100);
    WC_ASSERT_EQ_U64(genVec_size(v), 0); /* size unchanged */
    genVec_destroy(v);
}

static void test_reserve_does_not_shrink(void)
{
    genVec* v = int_vec(100);
    genVec_reserve(v, 4);
    WC_ASSERT_TRUE(genVec_capacity(v) >= 100);
    genVec_destroy(v);
}

static void test_reserve_val(void)
{
    genVec* v = int_vec(0);
    int val   = 5;
    genVec_reserve_val(v, 10, (u8*)&val);
    WC_ASSERT_EQ_U64(genVec_size(v), 10);
    for (u64 i = 0; i < 10; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, i), 5);
    }
    genVec_destroy(v);
}


/* ── Clear / Reset ───────────────────────────────────────────────────────── */

static void test_clear_keeps_capacity(void)
{
    genVec* v = int_vec(8);
    push_ints(v, 5);
    u64 cap_before = genVec_capacity(v);
    genVec_clear(v);
    WC_ASSERT_EQ_U64(genVec_size(v), 0);
    WC_ASSERT_EQ_U64(genVec_capacity(v), cap_before);
    genVec_destroy(v);
}

static void test_reset_frees_memory(void)
{
    genVec* v = int_vec(8);
    push_ints(v, 5);
    genVec_reset(v);
    WC_ASSERT_EQ_U64(genVec_size(v), 0);
    WC_ASSERT_EQ_U64(genVec_capacity(v), 0);
    WC_ASSERT_NULL(v->data);
    genVec_destroy(v);
}


/* ── Copy / Move ─────────────────────────────────────────────────────────── */

static void test_copy(void)
{
    genVec* src = int_vec(4);
    push_ints(src, 4);

    genVec dest;
    genVec_init_stk(0, sizeof(int), NULL, &dest);
    genVec_copy(&dest, src);

    WC_ASSERT_EQ_U64(genVec_size(&dest), 4);
    for (int i = 0; i < 4; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(&dest, (u64)i), i);
    }

    /* independence: modify src, dest must be unaffected */
    int x = 999;
    genVec_replace(src, 0, (u8*)&x);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(&dest, 0), 0);

    genVec_destroy(src);
    genVec_destroy_stk(&dest);
}

static void test_move_nulls_src(void)
{
    genVec* src = int_vec(4);
    push_ints(src, 4);

    genVec dest;
    genVec_init_stk(0, sizeof(int), NULL, &dest);
    genVec_move(&dest, &src);

    WC_ASSERT_NULL(src);
    WC_ASSERT_EQ_U64(genVec_size(&dest), 4);
    genVec_destroy_stk(&dest);
}


/* ── insert_multi ────────────────────────────────────────────────────────── */

static void test_insert_multi(void)
{
    genVec* v   = int_vec(4);
    int     arr[] = {10, 20, 30};
    genVec_insert_multi(v, 0, (u8*)arr, 3);
    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 10);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 20);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 30);
    genVec_destroy(v);
}

static void test_insert_multi_mid(void)
{
    genVec* v = int_vec(8);
    push_ints(v, 3); /* 0 1 2 */
    int arr[] = {10, 20};
    genVec_insert_multi(v, 1, (u8*)arr, 2);
    /* expected: 0 10 20 1 2 */
    WC_ASSERT_EQ_U64(genVec_size(v), 5);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 0), 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 1), 10);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 2), 20);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 3), 1);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(v, 4), 2);
    genVec_destroy(v);
}


#include <time.h>

/* ── Timer helpers ───────────────────────────────────────────────────────── */

typedef struct { struct timespec t; } Ts;

// static inline Ts ts_now(void)
// {
//     Ts ts;
//     clock_gettime(CLOCK_MONOTONIC, &ts.t);
//     return ts;
// }

#define ts_now() \
    ({\
        Ts ts;\
        clock_gettime(CLOCK_MONOTONIC, &ts.t);\
        ts;\
     })

/* Returns elapsed time in milliseconds */
static inline double ts_elapsed_ms(Ts start, Ts end)
{
    double s = (double)(end.t.tv_sec  - start.t.tv_sec)  * 1e3;
    double n = (double)(end.t.tv_nsec - start.t.tv_nsec) * 1e-6;
    return s + n;
}

/* ── Bench config ────────────────────────────────────────────────────────── */

#define SMALL_N   100000ULL     /* 100k  — insertion / removal benchmarks  */
#define LARGE_N  1000000ULL     /* 1M    — push / pop / access benchmarks  */

/* ── Formatting ──────────────────────────────────────────────────────────── */

static void print_header(const char* suite)
{
    printf("\n\033[1;36m══ %-50s ══\033[0m\n", suite);
    printf("  %-44s %10s  %12s\n", "test", "n", "time (ms)");
    printf("  %s\n", "-----------------------------------------------------------------------");
}

static void print_result(const char* name, u64 n, double ms)
{
    const char* color = ms < 10.0  ? "\033[1;32m"
                      : ms < 100.0 ? "\033[1;33m"
                                   : "\033[1;31m";
    printf("  %-44s %10llu  %s%10.3f\033[0m\n",
           name, (unsigned long long)n, color, ms);
}

/* ── Prevent dead-code elimination ───────────────────────────────────────── */

/* Write result to a volatile sink so the compiler can't optimize away loops */
static volatile int  g_int_sink;
static volatile u64  g_u64_sink;

/* ═══════════════════════════════════════════════════════════════════════════
 * Benchmarks
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── 1. Push (sequential) ─────────────────────────────────────────────────
 *
 * Measures raw append throughput including all realloc growth.
 * Two variants:
 *   cold  — fresh vector starting at capacity 0
 *   warm  — vector pre-reserved to LARGE_N (zero allocations during push)
 */
static void bench_push_cold(void)
{
    print_header("Push throughput");

    genVec* v = genVec_init(0, sizeof(int), NULL);

    Ts t0 = ts_now();
    for (int i = 0; i < (int)LARGE_N; i++) {
        genVec_push(v, (u8*)&i);
    }
    Ts t1 = ts_now();

    g_u64_sink = genVec_size(v);
    genVec_destroy(v);
    print_result("push x1M  (cold, cap=0, realloc path)", LARGE_N, ts_elapsed_ms(t0, t1));
}

static void bench_push_warm(void)
{
    genVec* v = genVec_init(LARGE_N, sizeof(int), NULL);

    Ts t0 = ts_now();
    for (int i = 0; i < (int)LARGE_N; i++) {
        genVec_push(v, (u8*)&i);
    }
    Ts t1 = ts_now();

    g_u64_sink = genVec_size(v);
    genVec_destroy(v);
    print_result("push x1M  (warm, pre-reserved, no realloc)", LARGE_N, ts_elapsed_ms(t0, t1));
}


/* ── 2. Pop ───────────────────────────────────────────────────────────────
 *
 * Pop-all from a full vector.  Also exercises the shrink heuristic.
 * Variant A: discard (NULL out), variant B: copy each popped value.
 */
static void bench_pop_discard(void)
{
    print_header("Pop throughput");

    genVec* v = genVec_init(LARGE_N, sizeof(int), NULL);
    for (int i = 0; i < (int)LARGE_N; i++) genVec_push(v, (u8*)&i);

    Ts t0 = ts_now();
    while (!genVec_empty(v)) {
        genVec_pop(v, NULL);
    }
    Ts t1 = ts_now();

    genVec_destroy(v);
    print_result("pop x1M   (discard)", LARGE_N, ts_elapsed_ms(t0, t1));
}

static void bench_pop_copy(void)
{
    genVec* v = genVec_init(LARGE_N, sizeof(int), NULL);
    for (int i = 0; i < (int)LARGE_N; i++) genVec_push(v, (u8*)&i);

    int out = 0;
    Ts t0 = ts_now();
    while (!genVec_empty(v)) {
        genVec_pop(v, (u8*)&out);
    }
    Ts t1 = ts_now();

    g_int_sink = out;
    genVec_destroy(v);
    print_result("pop x1M   (copy to out)", LARGE_N, ts_elapsed_ms(t0, t1));
}


/* ── 3. Sequential read (get_ptr) ─────────────────────────────────────────
 *
 * Hot cache: stride-1 access through a fully populated vector.
 * Baseline for memory bandwidth.
 */
static void bench_seq_read(void)
{
    print_header("Sequential read");

    genVec* v = genVec_init(LARGE_N, sizeof(int), NULL);
    for (int i = 0; i < (int)LARGE_N; i++) genVec_push(v, (u8*)&i);

    int sum = 0;
    Ts t0 = ts_now();
    for (u64 i = 0; i < LARGE_N; i++) {
        sum += *(int*)genVec_get_ptr(v, i);
    }
    Ts t1 = ts_now();

    g_int_sink = sum;
    genVec_destroy(v);
    print_result("get_ptr x1M  (sequential)", LARGE_N, ts_elapsed_ms(t0, t1));
}


/* ── 4. Random access (get_ptr) ───────────────────────────────────────────
 *
 * Shuffled index array: forces cache misses for large N.
 * Highlights the cost of the index → pointer arithmetic.
 */
static void bench_random_read(void)
{
    const u64 N = LARGE_N;

    genVec* v = genVec_init(N, sizeof(int), NULL);
    for (int i = 0; i < (int)N; i++) genVec_push(v, (u8*)&i);

    /* Build a shuffled index array */
    u64* idx = malloc(N * sizeof(u64));
    for (u64 i = 0; i < N; i++) idx[i] = i;
    /* Fisher-Yates with a fixed seed for reproducibility */
    srand(0xDEADBEEF);
    for (u64 i = N - 1; i > 0; i--) {
        u64 j  = (u64)rand() % (i + 1);
        u64 tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
    }

    int sum = 0;
    Ts t0 = ts_now();
    for (u64 i = 0; i < N; i++) {
        sum += *(int*)genVec_get_ptr(v, idx[i]);
    }
    Ts t1 = ts_now();

    g_int_sink = sum;
    free(idx);
    genVec_destroy(v);
    print_result("get_ptr x1M  (random / cache-miss)", N, ts_elapsed_ms(t0, t1));
}


/* ── 5. Insert at front ───────────────────────────────────────────────────
 *
 * Worst case: every insert shifts the entire vector one position right.
 * O(n²) in total; use SMALL_N.
 */
static void bench_insert_front(void)
{
    print_header("Insert / Remove (front = worst case, SMALL_N=100k)");

    genVec* v = genVec_init(SMALL_N, sizeof(int), NULL);
    int x = 1;

    Ts t0 = ts_now();
    for (u64 i = 0; i < SMALL_N; i++) {
        genVec_insert(v, 0, (u8*)&x);
    }
    Ts t1 = ts_now();

    g_u64_sink = genVec_size(v);
    genVec_destroy(v);
    print_result("insert front x100k  (O(n) shift per op)", SMALL_N, ts_elapsed_ms(t0, t1));
}


/* ── 6. Remove from front ─────────────────────────────────────────────────
 *
 * Mirror of insert_front: each remove shifts the rest left.
 */
static void bench_remove_front(void)
{
    genVec* v = genVec_init(SMALL_N, sizeof(int), NULL);
    for (int i = 0; i < (int)SMALL_N; i++) genVec_push(v, (u8*)&i);

    Ts t0 = ts_now();
    while (!genVec_empty(v)) {
        genVec_remove(v, 0, NULL);
    }
    Ts t1 = ts_now();

    genVec_destroy(v);
    print_result("remove front x100k  (O(n) shift per op)", SMALL_N, ts_elapsed_ms(t0, t1));
}


/* ── 7. insert_multi vs. repeated insert ─────────────────────────────────
 *
 * Append 10k elements in one batch vs. 10k individual inserts.
 * Shows the memmove-once advantage of insert_multi.
 */
static void bench_insert_multi_vs_single(void)
{
    const u64 BATCH = 10000;
    print_header("insert_multi vs. repeated single insert (at front, n=10k)");

    /* Prepare source array */
    int* src = malloc(BATCH * sizeof(int));
    for (u64 i = 0; i < BATCH; i++) src[i] = (int)i;

    /* single inserts */
    {
        genVec* v = genVec_init(BATCH, sizeof(int), NULL);
        int x = 1;
        Ts t0 = ts_now();
        for (u64 i = 0; i < BATCH; i++) {
            genVec_insert(v, 0, (u8*)&x);
        }
        Ts t1 = ts_now();
        g_u64_sink = genVec_size(v);
        genVec_destroy(v);
        print_result("single insert x10k  (each shifts all)", BATCH, ts_elapsed_ms(t0, t1));
    }

    /* insert_multi */
    {
        genVec* v = genVec_init(BATCH, sizeof(int), NULL);
        Ts t0 = ts_now();
        genVec_insert_multi(v, 0, (u8*)src, BATCH);
        Ts t1 = ts_now();
        g_u64_sink = genVec_size(v);
        genVec_destroy(v);
        print_result("insert_multi x10k   (one memmove)", BATCH, ts_elapsed_ms(t0, t1));
    }

    free(src);
}


/* ── 8. genVec_copy (deep copy) ──────────────────────────────────────────
 *
 * Time to deep-copy a 1M-element vector (memcpy path, no copy_fn).
 */
static void bench_copy(void)
{
    print_header("genVec_copy (1M ints)");

    genVec* src = genVec_init(LARGE_N, sizeof(int), NULL);
    for (int i = 0; i < (int)LARGE_N; i++) genVec_push(src, (u8*)&i);

    genVec dest;
    genVec_init_stk(0, sizeof(int), NULL, &dest);

    Ts t0 = ts_now();
    genVec_copy(&dest, src);
    Ts t1 = ts_now();

    g_u64_sink = genVec_size(&dest);
    genVec_destroy_stk(&dest);
    genVec_destroy(src);
    print_result("genVec_copy x1M  (calloc + memcpy)", LARGE_N, ts_elapsed_ms(t0, t1));
}


/* ── 9. reserve_val ───────────────────────────────────────────────────────
 *
 * Fill 1M slots via reserve_val (single realloc + memcpy fill).
 */
static void bench_reserve_val(void)
{
    print_header("genVec_reserve_val (1M ints)");

    genVec* v = genVec_init(0, sizeof(int), NULL);
    int val   = 42;

    Ts t0 = ts_now();
    genVec_reserve_val(v, LARGE_N, (u8*)&val);
    Ts t1 = ts_now();

    g_u64_sink = genVec_size(v);
    genVec_destroy(v);
    print_result("reserve_val x1M", LARGE_N, ts_elapsed_ms(t0, t1));
}


/* ── 10. Contiguous element size comparison ──────────────────────────────
 *
 * Push throughput for small (int), medium (32-byte), and large (256-byte)
 * elements. Highlights how data_size affects memcpy overhead.
 */

typedef struct { char pad[32];  } Elm32;
typedef struct { char pad[256]; } Elm256;

static void bench_element_sizes(void)
{
    print_header("Push throughput by element size (100k ops)");

    /* int (4 bytes) */
    {
        genVec* v = genVec_init(0, sizeof(int), NULL);
        int x = 7;
        Ts t0 = ts_now();
        for (u64 i = 0; i < SMALL_N; i++) genVec_push(v, (u8*)&x);
        Ts t1 = ts_now();
        g_u64_sink = genVec_size(v);
        genVec_destroy(v);
        print_result("push int       (4 B)", SMALL_N, ts_elapsed_ms(t0, t1));
    }

    /* 32-byte struct */
    {
        genVec* v = genVec_init(0, sizeof(Elm32), NULL);
        Elm32 x; memset(&x, 0xAB, sizeof(x));
        Ts t0 = ts_now();
        for (u64 i = 0; i < SMALL_N; i++) genVec_push(v, (u8*)&x);
        Ts t1 = ts_now();
        g_u64_sink = genVec_size(v);
        genVec_destroy(v);
        print_result("push Elm32     (32 B)", SMALL_N, ts_elapsed_ms(t0, t1));
    }

    /* 256-byte struct */
    {
        genVec* v = genVec_init(0, sizeof(Elm256), NULL);
        Elm256 x; memset(&x, 0xAB, sizeof(x));
        Ts t0 = ts_now();
        for (u64 i = 0; i < SMALL_N; i++) genVec_push(v, (u8*)&x);
        Ts t1 = ts_now();
        g_u64_sink = genVec_size(v);
        genVec_destroy(v);
        print_result("push Elm256    (256 B)", SMALL_N, ts_elapsed_ms(t0, t1));
    }
}


/* ── main ────────────────────────────────────────────────────────────────── */

int speed_benchmark(void)
{
    printf("\033[1;35mgenVec speed benchmark\033[0m\n");
    printf("Compiled: " __DATE__ " " __TIME__ "\n");
    printf("GENVEC_GROWTH=%.2f  GENVEC_SHRINK_AT=%.2f  GENVEC_SHRINK_BY=%.2f\n",
           (double)GENVEC_GROWTH, (double)GENVEC_SHRINK_AT, (double)GENVEC_SHRINK_BY);

    bench_push_cold();
    bench_push_warm();
    bench_pop_discard();
    bench_pop_copy();
    bench_seq_read();
    bench_random_read();
    bench_insert_front();
    bench_remove_front();
    bench_insert_multi_vs_single();
    bench_copy();
    bench_reserve_val();
    bench_element_sizes();

    printf("\n\033[1;36mNote:\033[0m times are wall-clock (CLOCK_MONOTONIC).\n");
    printf("Run 2-3x to account for warm-up; compare relative timings.\n");
    printf("Green < 10ms, Yellow < 100ms, Red >= 100ms.\n\n");

    return 0;
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void gen_vector_suite(void)
{
    WC_SUITE("genVec");

    /* init */
    WC_RUN(test_init_zero_cap);
    WC_RUN(test_init_with_cap);
    WC_RUN(test_init_val);
    WC_RUN(test_init_stk);
    WC_RUN(test_init_arr);

    /* push / pop */
    WC_RUN(test_push_grows_size);
    WC_RUN(test_push_triggers_growth);
    WC_RUN(test_pop_reduces_size);
    WC_RUN(test_pop_copies_value);
    WC_RUN(test_pop_empty_sets_errno);

    /* get */
    WC_RUN(test_get_ptr);
    WC_RUN(test_get_copies);
    WC_RUN(test_front_back);
    WC_RUN(test_front_empty_sets_errno);

    /* insert / remove */
    WC_RUN(test_insert_front);
    WC_RUN(test_insert_mid);
    WC_RUN(test_remove_front);
    WC_RUN(test_remove_mid);
    WC_RUN(test_remove_range);

    /* replace */
    WC_RUN(test_replace);

    /* reserve */
    WC_RUN(test_reserve_grows_capacity);
    WC_RUN(test_reserve_does_not_shrink);
    WC_RUN(test_reserve_val);

    /* clear / reset */
    WC_RUN(test_clear_keeps_capacity);
    WC_RUN(test_reset_frees_memory);

    /* copy / move */
    WC_RUN(test_copy);
    WC_RUN(test_move_nulls_src);

    /* insert_multi */
    WC_RUN(test_insert_multi);
    WC_RUN(test_insert_multi_mid);

    WC_RUN(speed_benchmark);
}


