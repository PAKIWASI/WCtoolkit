/*
 * speed_test.c — Performance tests for genVec and hashmap
 *
 * Tests are grouped by the operation changed. Each test runs the operation
 * N times and prints nanoseconds per operation so you can compare POD vs
 * complex paths directly, and see before/after if you revert is_pod.
 *
 */
#include "wc_test.h"
#include "gen_vector.h"
#include "hashmap.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


// ─── Timing helpers ──────────────────────────────────────────────────────────

static inline u64 ns_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
}

// Prints "  label: X ns/op  (N ops)" and returns ns/op
static u64 bench(const char* label, u64 n_ops, u64 start, u64 end)
{
    u64 total_ns = end - start;
    u64 ns_per   = n_ops ? total_ns / n_ops : 0;
    printf("  %-44s %6llu ns/op  (%llu ops)\n",
           label,
           (unsigned long long)ns_per,
           (unsigned long long)n_ops);
    return ns_per;
}

// Sanity-check: POD path should be <= cx path (or within 5% noise)
#define ASSERT_FASTER_OR_EQUAL(pod_ns, cx_ns) \
    WC_ASSERT((pod_ns) <= (cx_ns) + ((cx_ns) / 20))


// ─── Fake complex-type ops (heap string shell — minimal overhead) ─────────────

typedef struct { char* ptr; u64 len; } FakeStr;

static void fakestr_copy(u8* dest, const u8* src)
{
    const FakeStr* s = (const FakeStr*)src;
    FakeStr*       d = (FakeStr*)dest;
    d->len = s->len;
    d->ptr = malloc(s->len + 1);
    memcpy(d->ptr, s->ptr, s->len + 1);
}

static void fakestr_move(u8* dest, u8** src)
{
    FakeStr* d = (FakeStr*)dest;
    FakeStr* s = *(FakeStr**)src;
    *d   = *s;
    s->ptr = NULL;
    s->len = 0;
}

static void fakestr_del(u8* elm)
{
    FakeStr* s = (FakeStr*)elm;
    free(s->ptr);
    s->ptr = NULL;
}

static const container_ops fakestr_ops = { fakestr_copy, fakestr_move, fakestr_del };

static FakeStr make_fakestr(const char* s)
{
    FakeStr f;
    f.len = strlen(s);
    f.ptr = malloc(f.len + 1);
    memcpy(f.ptr, s, f.len + 1);
    return f;
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 1: push (single-element copy hot path)
// ═══════════════════════════════════════════════════════════════════════════════

#define PUSH_N 1000000

static void bench_push_pod(void)
{
    genVec* v = genVec_init(PUSH_N, sizeof(int), NULL);
    int val   = 42;

    u64 t0 = ns_now();
    for (int i = 0; i < PUSH_N; i++) {
        genVec_push(v, (u8*)&val);
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(v->size, PUSH_N);
    bench("push POD (int)", PUSH_N, t0, t1);
    genVec_destroy(v);
}

static void bench_push_cx(void)
{
    genVec* v = genVec_init(PUSH_N, sizeof(FakeStr), &fakestr_ops);

    u64 t0 = ns_now();
    for (int i = 0; i < PUSH_N; i++) {
        FakeStr f = make_fakestr("hello");
        genVec_push(v, (u8*)&f);
        free(f.ptr); // push copied it, we own the original
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(v->size, PUSH_N);
    bench("push CX (FakeStr, copy)", PUSH_N, t0, t1);
    genVec_destroy(v);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 2: clear / destroy (bulk del loop — biggest POD win)
// ═══════════════════════════════════════════════════════════════════════════════

#define CLEAR_N  500000
#define CLEAR_REP 20      // repeat clear+refill so timing is stable

static void bench_clear_pod(void)
{
    genVec* v = genVec_init(CLEAR_N, sizeof(int), NULL);
    int val   = 7;
    for (int i = 0; i < CLEAR_N; i++) genVec_push(v, (u8*)&val);

    u64 t0 = ns_now();
    for (int r = 0; r < CLEAR_REP; r++) {
        genVec_clear(v);
        for (int i = 0; i < CLEAR_N; i++) genVec_push(v, (u8*)&val);
    }
    u64 t1 = ns_now();

    bench("clear POD (500k ints) x20", (u64)CLEAR_REP * CLEAR_N, t0, t1);
    genVec_destroy(v);
}

static void bench_clear_cx(void)
{
    u64 total_ns = 0;

    for (int r = 0; r < CLEAR_REP; r++) {
        genVec* v = genVec_init(CLEAR_N, sizeof(FakeStr), &fakestr_ops);
        for (int i = 0; i < CLEAR_N; i++) {
            FakeStr f = make_fakestr("hi");
            genVec_push(v, (u8*)&f);
            free(f.ptr);
        }
        u64 t0 = ns_now();
        genVec_clear(v);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
        genVec_destroy(v);
    }

    u64 ns_per = total_ns / ((u64)CLEAR_REP * CLEAR_N);
    printf("  %-44s %6llu ns/op  (%d ops)\n",
           "clear CX (500k FakeStr) x20",
           (unsigned long long)ns_per,
           CLEAR_REP * CLEAR_N);
}

static void bench_destroy_pod(void)
{
    // Measure destroy of a large POD vec (should be essentially free post-fix)
    #define DESTROY_N 1000000
    #define DESTROY_REP 50

    u64 total_ns = 0;
    int val = 1;

    for (int r = 0; r < DESTROY_REP; r++) {
        genVec* v = genVec_init(DESTROY_N, sizeof(int), NULL);
        for (int i = 0; i < DESTROY_N; i++) genVec_push(v, (u8*)&val);
        u64 t0 = ns_now();
        genVec_destroy(v);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
    }

    u64 ns_per = total_ns / DESTROY_REP;
    printf("  %-44s %6llu ns  total  (%d reps of 1M)\n",
           "destroy POD (1M ints)",
           (unsigned long long)ns_per,
           DESTROY_REP);
}

static void bench_destroy_cx(void)
{
    u64 total_ns = 0;

    for (int r = 0; r < DESTROY_REP; r++) {
        genVec* v = genVec_init(DESTROY_N, sizeof(FakeStr), &fakestr_ops);
        for (int i = 0; i < DESTROY_N; i++) {
            FakeStr f = make_fakestr("world");
            genVec_push(v, (u8*)&f);
            free(f.ptr);
        }
        u64 t0 = ns_now();
        genVec_destroy(v);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
    }

    u64 ns_per = total_ns / DESTROY_REP;
    printf("  %-44s %6llu ns  total  (%d reps of 1M)\n",
           "destroy CX (1M FakeStr)",
           (unsigned long long)ns_per,
           DESTROY_REP);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 3: genVec_copy (bulk copy — can SIMD after fix)
// ═══════════════════════════════════════════════════════════════════════════════

#define COPY_N   1000000
#define COPY_REP 20

static void bench_vec_copy_pod(void)
{
    genVec* src = genVec_init(COPY_N, sizeof(int), NULL);
    int val = 99;
    for (int i = 0; i < COPY_N; i++) genVec_push(src, (u8*)&val);

    genVec dest;
    memset(&dest, 0, sizeof(dest));
    dest.is_pod = 1; // init dest so destroy_stk is safe on first call inside copy

    u64 t0 = ns_now();
    for (int r = 0; r < COPY_REP; r++) {
        genVec_copy(&dest, src);
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(dest.size, COPY_N);
    bench("genVec_copy POD (1M ints) x20", (u64)COPY_REP * COPY_N, t0, t1);

    genVec_destroy_stk(&dest);
    genVec_destroy(src);
}

static void bench_vec_copy_cx(void)
{
    genVec* src = genVec_init(COPY_N, sizeof(FakeStr), &fakestr_ops);
    for (int i = 0; i < COPY_N; i++) {
        FakeStr f = make_fakestr("copy");
        genVec_push(src, (u8*)&f);
        free(f.ptr);
    }

    genVec dest;
    memset(&dest, 0, sizeof(dest));
    dest.ops    = &fakestr_ops;
    dest.is_pod = 0;

    u64 t0 = ns_now();
    for (int r = 0; r < COPY_REP; r++) {
        genVec_copy(&dest, src);
    }
    u64 t1 = ns_now();

    bench("genVec_copy CX (1M FakeStr) x20", (u64)COPY_REP * COPY_N, t0, t1);

    genVec_destroy_stk(&dest);
    genVec_destroy(src);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 4: init_val (broadcast fill — POD loop vs copy_fn loop)
// ═══════════════════════════════════════════════════════════════════════════════

#define INITVAL_N   2000000
#define INITVAL_REP 10

static void bench_init_val_pod(void)
{
    int val = 42;
    u64 t0  = ns_now();
    for (int r = 0; r < INITVAL_REP; r++) {
        genVec* v = genVec_init_val(INITVAL_N, (u8*)&val, sizeof(int), NULL);
        genVec_destroy(v);
    }
    u64 t1 = ns_now();
    bench("init_val POD (2M ints) x10", (u64)INITVAL_REP * INITVAL_N, t0, t1);
}

static void bench_init_val_cx(void)
{
    FakeStr val = make_fakestr("init");
    u64 t0 = ns_now();
    for (int r = 0; r < INITVAL_REP; r++) {
        genVec* v = genVec_init_val(INITVAL_N, (u8*)&val, sizeof(FakeStr), &fakestr_ops);
        genVec_destroy(v);
    }
    u64 t1 = ns_now();
    bench("init_val CX (2M FakeStr) x10", (u64)INITVAL_REP * INITVAL_N, t0, t1);
    free(val.ptr);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 5: remove_range (bulk del loop)
// ═══════════════════════════════════════════════════════════════════════════════

#define RANGE_N   500000
#define RANGE_REP 20

static void bench_remove_range_pod(void)
{
    u64 total_ns = 0;
    int val = 5;

    for (int r = 0; r < RANGE_REP; r++) {
        genVec* v = genVec_init(RANGE_N, sizeof(int), NULL);
        for (int i = 0; i < RANGE_N; i++) genVec_push(v, (u8*)&val);
        u64 t0 = ns_now();
        genVec_remove_range(v, 0, RANGE_N);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
        genVec_destroy(v);
    }

    u64 ns_per = total_ns / ((u64)RANGE_REP * RANGE_N);
    printf("  %-44s %6llu ns/op  (%d reps of 500k)\n",
           "remove_range POD (all 500k)",
           (unsigned long long)ns_per, RANGE_REP);
}

static void bench_remove_range_cx(void)
{
    u64 total_ns = 0;

    for (int r = 0; r < RANGE_REP; r++) {
        genVec* v = genVec_init(RANGE_N, sizeof(FakeStr), &fakestr_ops);
        for (int i = 0; i < RANGE_N; i++) {
            FakeStr f = make_fakestr("range");
            genVec_push(v, (u8*)&f);
            free(f.ptr);
        }
        u64 t0 = ns_now();
        genVec_remove_range(v, 0, RANGE_N);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
        genVec_destroy(v);
    }

    u64 ns_per = total_ns / ((u64)RANGE_REP * RANGE_N);
    printf("  %-44s %6llu ns/op  (%d reps of 500k)\n",
           "remove_range CX (all 500k)",
           (unsigned long long)ns_per, RANGE_REP);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 6: hashmap_put + hashmap_get (hot path)
// ═══════════════════════════════════════════════════════════════════════════════

#define MAP_N 500000

static void bench_map_put_pod(void)
{
    // int -> int map
    hashmap* map = hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);

    u64 t0 = ns_now();
    for (int i = 0; i < MAP_N; i++) {
        hashmap_put(map, (u8*)&i, (u8*)&i);
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(map->size, MAP_N);
    bench("hashmap_put POD int->int", MAP_N, t0, t1);
    hashmap_destroy(map);
}

static void bench_map_put_cx(void)
{
    // FakeStr -> FakeStr map (both key and val have copy/del)
    hashmap* map = hashmap_create(sizeof(FakeStr), sizeof(FakeStr),
                                  NULL, NULL, &fakestr_ops, &fakestr_ops);

    u64 t0 = ns_now();
    for (int i = 0; i < MAP_N; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "key_%d", i);
        FakeStr k = make_fakestr(buf);
        FakeStr v = make_fakestr("val");
        hashmap_put(map, (u8*)&k, (u8*)&v);
        free(k.ptr);
        free(v.ptr);
    }
    u64 t1 = ns_now();

    bench("hashmap_put CX FakeStr->FakeStr", MAP_N, t0, t1);
    hashmap_destroy(map);
}

static void bench_map_get_pod(void)
{
    hashmap* map = hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
    for (int i = 0; i < MAP_N; i++) hashmap_put(map, (u8*)&i, (u8*)&i);

    int out  = 0;
    int hits = 0;
    u64 t0   = ns_now();
    for (int i = 0; i < MAP_N; i++) {
        hits += hashmap_get(map, (u8*)&i, (u8*)&out);
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_INT(hits, MAP_N);
    bench("hashmap_get POD int->int", MAP_N, t0, t1);
    hashmap_destroy(map);
}

static void bench_map_get_cx(void)
{
    hashmap* map = hashmap_create(sizeof(FakeStr), sizeof(FakeStr),
                                  NULL, NULL, &fakestr_ops, &fakestr_ops);
    for (int i = 0; i < MAP_N; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "key_%d", i);
        FakeStr k = make_fakestr(buf);
        FakeStr v = make_fakestr("val");
        hashmap_put(map, (u8*)&k, (u8*)&v);
        free(k.ptr); free(v.ptr);
    }

    int hits = 0;
    u64 t0   = ns_now();
    for (int i = 0; i < MAP_N; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "key_%d", i);
        FakeStr k   = make_fakestr(buf);
        FakeStr out = {0};
        hits += hashmap_get(map, (u8*)&k, (u8*)&out);
        free(k.ptr);
        free(out.ptr);
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_INT(hits, MAP_N);
    bench("hashmap_get CX FakeStr->FakeStr", MAP_N, t0, t1);
    hashmap_destroy(map);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 7: hashmap_clear / hashmap_destroy (bulk del loop)
// ═══════════════════════════════════════════════════════════════════════════════

#define MCLR_N   200000
#define MCLR_REP 20

static void bench_map_clear_pod(void)
{
    hashmap* map = hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
    for (int i = 0; i < MCLR_N; i++) hashmap_put(map, (u8*)&i, (u8*)&i);

    u64 t0 = ns_now();
    for (int r = 0; r < MCLR_REP; r++) {
        hashmap_clear(map);
        for (int i = 0; i < MCLR_N; i++) hashmap_put(map, (u8*)&i, (u8*)&i);
    }
    u64 t1 = ns_now();

    bench("hashmap_clear POD (200k) x20", (u64)MCLR_REP * MCLR_N, t0, t1);
    hashmap_destroy(map);
}

static void bench_map_clear_cx(void)
{
    u64 total_ns = 0;

    for (int r = 0; r < MCLR_REP; r++) {
        hashmap* map = hashmap_create(sizeof(FakeStr), sizeof(FakeStr),
                                      NULL, NULL, &fakestr_ops, &fakestr_ops);
        for (int i = 0; i < MCLR_N; i++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "k%d", i);
            FakeStr k = make_fakestr(buf);
            FakeStr v = make_fakestr("v");
            hashmap_put(map, (u8*)&k, (u8*)&v);
            free(k.ptr); free(v.ptr);
        }
        u64 t0 = ns_now();
        hashmap_clear(map);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
        hashmap_destroy(map);
    }

    u64 ns_per = total_ns / ((u64)MCLR_REP * MCLR_N);
    printf("  %-44s %6llu ns/op  (%d reps of 200k)\n",
           "hashmap_clear CX FakeStr (200k) x20",
           (unsigned long long)ns_per, MCLR_REP);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 8: pop (single-element, copy + del path)
// ═══════════════════════════════════════════════════════════════════════════════

#define POP_N 500000

static void bench_pop_pod(void)
{
    genVec* v = genVec_init(POP_N, sizeof(int), NULL);
    int val   = 1;
    for (int i = 0; i < POP_N; i++) genVec_push(v, (u8*)&val);

    int out = 0;
    u64 t0  = ns_now();
    for (int i = 0; i < POP_N; i++) genVec_pop(v, (u8*)&out);
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(v->size, 0);
    bench("pop POD (int)", POP_N, t0, t1);
    genVec_destroy(v);
}

static void bench_pop_cx(void)
{
    genVec* v = genVec_init(POP_N, sizeof(FakeStr), &fakestr_ops);
    for (int i = 0; i < POP_N; i++) {
        FakeStr f = make_fakestr("pop");
        genVec_push(v, (u8*)&f);
        free(f.ptr);
    }

    u64 t0 = ns_now();
    for (int i = 0; i < POP_N; i++) genVec_pop(v, NULL);
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(v->size, 0);
    bench("pop CX (FakeStr, del)", POP_N, t0, t1);
    genVec_destroy(v);
}


// ═══════════════════════════════════════════════════════════════════════════════
// Suites
// ═══════════════════════════════════════════════════════════════════════════════

void suite_push(void)
{
    WC_SUITE("push  (1M ops)");
    WC_RUN(bench_push_pod);
    WC_RUN(bench_push_cx);
}

void suite_clear_destroy(void)
{
    WC_SUITE("clear / destroy  (bulk del loop — biggest POD win)");
    WC_RUN(bench_clear_pod);
    WC_RUN(bench_clear_cx);
    WC_RUN(bench_destroy_pod);
    WC_RUN(bench_destroy_cx);
}

void suite_copy(void)
{
    WC_SUITE("genVec_copy  (bulk memcpy vs per-element copy_fn)");
    WC_RUN(bench_vec_copy_pod);
    WC_RUN(bench_vec_copy_cx);
}

void suite_init_val(void)
{
    WC_SUITE("init_val  (broadcast fill)");
    WC_RUN(bench_init_val_pod);
    WC_RUN(bench_init_val_cx);
}

void suite_remove_range(void)
{
    WC_SUITE("remove_range  (bulk del loop)");
    WC_RUN(bench_remove_range_pod);
    WC_RUN(bench_remove_range_cx);
}

void suite_map_put_get(void)
{
    WC_SUITE("hashmap put / get  (hot path)");
    WC_RUN(bench_map_put_pod);
    WC_RUN(bench_map_put_cx);
    WC_RUN(bench_map_get_pod);
    WC_RUN(bench_map_get_cx);
}

void suite_map_clear(void)
{
    WC_SUITE("hashmap_clear  (bulk del loop)");
    WC_RUN(bench_map_clear_pod);
    WC_RUN(bench_map_clear_cx);
}

void suite_pop(void)
{
    WC_SUITE("pop  (500k ops, copy + del path)");
    WC_RUN(bench_pop_pod);
    WC_RUN(bench_pop_cx);
}

extern int speed_suite(void)
{
    printf("\n=== WCtoolkit speed tests ===\n");
    printf("(lower ns/op is better; POD should always be <= CX)\n");

    suite_push();
    suite_pop();
    suite_clear_destroy();
    suite_remove_range();
    suite_copy();
    suite_init_val();
    suite_map_put_get();
    suite_map_clear();

    return WC_REPORT();
}


