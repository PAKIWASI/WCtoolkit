/*
 * speed_test.c — Performance tests for GenVec and HashMap
 *
 * Tests are grouped by the operation changed. Each test runs the operation
 * N times and prints nanoseconds per operation so you can compare POD vs
 * complex paths directly, and see before/after if you revert is_pod.
 *
 */
#include "wc_test.h"
#include "gen_vector.h"
#include "HashMap.h"
#include "String.h"
#include "wc_helpers.h"
#include "views.h"
#include "arena.h"
#include "chain_arena.h"

#include <time.h>
#include <string.h>
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


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 1: push (single-element copy hot path)
// ═══════════════════════════════════════════════════════════════════════════════

#define PUSH_N 1000000

static void bench_push_pod(void)
{
    GenVec* v = GenVec_create(PUSH_N, sizeof(int), NULL);
    int val   = 42;

    u64 t0 = ns_now();
    for (int i = 0; i < PUSH_N; i++) {
        GenVec_push(v, (u8*)&val);
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(v->size, PUSH_N);
    bench("push POD (int)", PUSH_N, t0, t1);
    GenVec_destroy(v);
}

static void bench_push_cx(void)
{
    // Each element is a String by value (SSO path for short strings).
    GenVec* v = GenVec_create(PUSH_N, sizeof(String), &wc_str_ops);

    u64 t0 = ns_now();
    for (int i = 0; i < PUSH_N; i++) {
        String s;
        string_create_stk("hello", &s);
        GenVec_push(v, (u8*)&s);
        string_destroy_stk(&s); // push deep-copied it; we own the original
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(v->size, PUSH_N);
    bench("push CX (String, copy)", PUSH_N, t0, t1);
    GenVec_destroy(v);
}


// BUG: cx showing lower time than pod. some timer error?
// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 2: clear / destroy (bulk del loop — biggest POD win)
// ═══════════════════════════════════════════════════════════════════════════════

#define CLEAR_N  500000
#define CLEAR_REP 20

static void bench_clear_pod(void)
{
    u64 total_ns = 0;
    int val      = 7;

    for (int r = 0; r < CLEAR_REP; r++) {
        GenVec* v = GenVec_create(CLEAR_N, sizeof(int), NULL);
        for (int i = 0; i < CLEAR_N; i++) GenVec_push(v, (u8*)&val);

        u64 t0 = ns_now();
        GenVec_clear(v);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
        GenVec_destroy(v);
    }

    u64 ns_per = total_ns / ((u64)CLEAR_REP * CLEAR_N);
    printf("  %-44s %6llu ns/op  (%d ops)\n",
           "clear POD (500k ints) x20",
           (unsigned long long)ns_per,
           CLEAR_REP * CLEAR_N);
}

static void bench_clear_cx(void)
{
    u64 total_ns = 0;

    for (int r = 0; r < CLEAR_REP; r++) {
        GenVec* v = GenVec_create(CLEAR_N, sizeof(String), &wc_str_ops);
        for (int i = 0; i < CLEAR_N; i++) {
            String s;
            string_create_stk("hi", &s);
            GenVec_push(v, (u8*)&s);
            string_destroy_stk(&s);
        }
        u64 t0 = ns_now();
        GenVec_clear(v);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
        GenVec_destroy(v);
    }

    u64 ns_per = total_ns / ((u64)CLEAR_REP * CLEAR_N);
    printf("  %-44s %6llu ns/op  (%d ops)\n",
           "clear CX (500k String) x20",
           (unsigned long long)ns_per,
           CLEAR_REP * CLEAR_N);
}

#define DESTROY_N   1000000
#define DESTROY_REP 50

static void bench_destroy_pod(void)
{
    u64 total_ns = 0;
    int val = 1;

    for (int r = 0; r < DESTROY_REP; r++) {
        GenVec* v = GenVec_create(DESTROY_N, sizeof(int), NULL);
        for (int i = 0; i < DESTROY_N; i++) GenVec_push(v, (u8*)&val);
        u64 t0 = ns_now();
        GenVec_destroy(v);
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
        GenVec* v = GenVec_create(DESTROY_N, sizeof(String), &wc_str_ops);
        for (int i = 0; i < DESTROY_N; i++) {
            String s;
            string_create_stk("world", &s);
            GenVec_push(v, (u8*)&s);
            string_destroy_stk(&s);
        }
        u64 t0 = ns_now();
        GenVec_destroy(v);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
    }

    u64 ns_per = total_ns / DESTROY_REP;
    printf("  %-44s %6llu ns  total  (%d reps of 1M)\n",
           "destroy CX (1M String)",
           (unsigned long long)ns_per,
           DESTROY_REP);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 3: GenVec_copy (bulk copy — can SIMD after fix)
// ═══════════════════════════════════════════════════════════════════════════════

#define COPY_N   1000000
#define COPY_REP 20

static void bench_vec_copy_pod(void)
{
    GenVec* src = GenVec_create(COPY_N, sizeof(int), NULL);
    int val = 99;
    for (int i = 0; i < COPY_N; i++) GenVec_push(src, (u8*)&val);

    GenVec dest;
    memset(&dest, 0, sizeof(dest));
    // dest.is_pod = 1; // init dest so destroy_stk is safe on first call inside copy

    u64 t0 = ns_now();
    for (int r = 0; r < COPY_REP; r++) {
        GenVec_copy(&dest, src);
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(dest.size, COPY_N);
    bench("GenVec_copy POD (1M ints) x20", (u64)COPY_REP * COPY_N, t0, t1);

    GenVec_destroy_stk(&dest);
    GenVec_destroy(src);
}

static void bench_vec_copy_cx(void)
{
    GenVec* src = GenVec_create(COPY_N, sizeof(String), &wc_str_ops);
    for (int i = 0; i < COPY_N; i++) {
        String s;
        string_create_stk("copy", &s);
        GenVec_push(src, (u8*)&s);
        string_destroy_stk(&s);
    }

    GenVec dest;
    memset(&dest, 0, sizeof(dest));
    dest.ops    = &wc_str_ops;
    // dest.is_pod = 0;

    u64 t0 = ns_now();
    for (int r = 0; r < COPY_REP; r++) {
        GenVec_copy(&dest, src);
    }
    u64 t1 = ns_now();

    bench("GenVec_copy CX (1M String) x20", (u64)COPY_REP * COPY_N, t0, t1);

    GenVec_destroy_stk(&dest);
    GenVec_destroy(src);
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
        GenVec* v = GenVec_create_val(INITVAL_N, (u8*)&val, sizeof(int), NULL);
        GenVec_destroy(v);
    }
    u64 t1 = ns_now();
    bench("init_val POD (2M ints) x10", (u64)INITVAL_REP * INITVAL_N, t0, t1);
}

static void bench_init_val_cx(void)
{
    // Use a short string so it stays SSO; this tests the copy_fn broadcast.
    String val;
    string_create_stk("init", &val);

    u64 t0 = ns_now();
    for (int r = 0; r < INITVAL_REP; r++) {
        GenVec* v = GenVec_create_val(INITVAL_N, (u8*)&val, sizeof(String), &wc_str_ops);
        GenVec_destroy(v);
    }
    u64 t1 = ns_now();

    bench("init_val CX (2M String) x10", (u64)INITVAL_REP * INITVAL_N, t0, t1);
    string_destroy_stk(&val);
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
        GenVec* v = GenVec_create(RANGE_N, sizeof(int), NULL);
        for (int i = 0; i < RANGE_N; i++) GenVec_push(v, (u8*)&val);
        u64 t0 = ns_now();
        GenVec_remove_range(v, 0, RANGE_N);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
        GenVec_destroy(v);
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
        GenVec* v = GenVec_create(RANGE_N, sizeof(String), &wc_str_ops);
        for (int i = 0; i < RANGE_N; i++) {
            String s;
            string_create_stk("range", &s);
            GenVec_push(v, (u8*)&s);
            string_destroy_stk(&s);
        }
        u64 t0 = ns_now();
        GenVec_remove_range(v, 0, RANGE_N);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
        GenVec_destroy(v);
    }

    u64 ns_per = total_ns / ((u64)RANGE_REP * RANGE_N);
    printf("  %-44s %6llu ns/op  (%d reps of 500k)\n",
           "remove_range CX (all 500k)",
           (unsigned long long)ns_per, RANGE_REP);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 6: HashMap_put + HashMap_get (hot path)
// ═══════════════════════════════════════════════════════════════════════════════

#define MAP_N 500000

static void bench_map_put_pod(void)
{
    // int -> int map
    HashMap* map = HashMap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);

    u64 t0 = ns_now();
    for (int i = 0; i < MAP_N; i++) {
        HashMap_put(map, (u8*)&i, (u8*)&i);
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(map->size, MAP_N);
    bench("HashMap_put POD int->int", MAP_N, t0, t1);
    HashMap_destroy(map);
}

static void bench_map_put_cx(void)
{
    // String -> String map (both key and val have copy/del via wc_str_ops)
    HashMap* map = HashMap_create(sizeof(String), sizeof(String),
                                  wyhash_str, str_cmp, &wc_str_ops, &wc_str_ops);

    u64 t0 = ns_now();
    for (int i = 0; i < MAP_N; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "key_%d", i);
        String k, v;
        string_create_stk(buf, &k);
        string_create_stk("val", &v);
        HashMap_put(map, (u8*)&k, (u8*)&v);
        string_destroy_stk(&k);
        string_destroy_stk(&v);
    }
    u64 t1 = ns_now();

    bench("HashMap_put CX String->String", MAP_N, t0, t1);
    HashMap_destroy(map);
}

static void bench_map_get_pod(void)
{
    HashMap* map = HashMap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
    for (int i = 0; i < MAP_N; i++) HashMap_put(map, (u8*)&i, (u8*)&i);

    int out  = 0;
    int hits = 0;
    u64 t0   = ns_now();
    for (int i = 0; i < MAP_N; i++) {
        hits += HashMap_get(map, (u8*)&i, (u8*)&out);
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_INT(hits, MAP_N);
    bench("HashMap_get POD int->int", MAP_N, t0, t1);
    HashMap_destroy(map);
}

static void bench_map_get_cx(void)
{
    HashMap* map = HashMap_create(sizeof(String), sizeof(String),
                                  wyhash_str, str_cmp, &wc_str_ops, &wc_str_ops);
    for (int i = 0; i < MAP_N; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "key_%d", i);
        String k, v;
        string_create_stk(buf, &k);
        string_create_stk("val", &v);
        HashMap_put(map, (u8*)&k, (u8*)&v);
        string_destroy_stk(&k);
        string_destroy_stk(&v);
    }

    int hits = 0;
    u64 t0   = ns_now();
    for (int i = 0; i < MAP_N; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "key_%d", i);
        String k, out;
        string_create_stk(buf, &k);
        hits += HashMap_get(map, (u8*)&k, (u8*)&out);
        string_destroy_stk(&k);
        string_destroy_stk(&out);
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_INT(hits, MAP_N);
    bench("HashMap_get CX String->String", MAP_N, t0, t1);
    HashMap_destroy(map);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 7: HashMap_clear / HashMap_destroy (bulk del loop)
// ═══════════════════════════════════════════════════════════════════════════════

#define MCLR_N   200000
#define MCLR_REP 20

static void bench_map_clear_pod(void)
{
    u64 total_ns = 0;

    for (int r = 0; r < MCLR_REP; r++) {
        HashMap* map = HashMap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
        for (int i = 0; i < MCLR_N; i++) HashMap_put(map, (u8*)&i, (u8*)&i);

        u64 t0 = ns_now();
        HashMap_clear(map);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
        HashMap_destroy(map);
    }

    u64 ns_per = total_ns / ((u64)MCLR_REP * MCLR_N);
    printf("  %-44s %6llu ns/op  (%d reps of 200k)\n",
           "HashMap_clear POD (200k) x20",
           (unsigned long long)ns_per, MCLR_REP);
}

static void bench_map_clear_cx(void)
{
    u64 total_ns = 0;

    for (int r = 0; r < MCLR_REP; r++) {
        HashMap* map = HashMap_create(sizeof(String), sizeof(String),
                                      wyhash_str, str_cmp, &wc_str_ops, &wc_str_ops);
        for (int i = 0; i < MCLR_N; i++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "k%d", i);
            String k, v;
            string_create_stk(buf, &k);
            string_create_stk("v", &v);
            HashMap_put(map, (u8*)&k, (u8*)&v);
            string_destroy_stk(&k);
            string_destroy_stk(&v);
        }
        u64 t0 = ns_now();
        HashMap_clear(map);
        u64 t1 = ns_now();
        total_ns += t1 - t0;
        HashMap_destroy(map);
    }

    u64 ns_per = total_ns / ((u64)MCLR_REP * MCLR_N);
    printf("  %-44s %6llu ns/op  (%d reps of 200k)\n",
           "HashMap_clear CX String (200k) x20",
           (unsigned long long)ns_per, MCLR_REP);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE 8: pop (single-element, copy + del path)
// ═══════════════════════════════════════════════════════════════════════════════

#define POP_N 500000

static void bench_pop_pod(void)
{
    GenVec* v = GenVec_create(POP_N, sizeof(int), NULL);
    int val   = 1;
    for (int i = 0; i < POP_N; i++) GenVec_push(v, (u8*)&val);

    int out = 0;
    u64 t0  = ns_now();
    for (int i = 0; i < POP_N; i++) GenVec_pop(v, (u8*)&out);
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(v->size, 0);
    bench("pop POD (int)", POP_N, t0, t1);
    GenVec_destroy(v);
}

static void bench_pop_cx(void)
{
    GenVec* v = GenVec_create(POP_N, sizeof(String), &wc_str_ops);
    for (int i = 0; i < POP_N; i++) {
        String s;
        string_create_stk("pop", &s);
        GenVec_push(v, (u8*)&s);
        string_destroy_stk(&s);
    }

    u64 t0 = ns_now();
    for (int i = 0; i < POP_N; i++) GenVec_pop(v, NULL);
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(v->size, 0);
    bench("pop CX (String, del)", POP_N, t0, t1);
    GenVec_destroy(v);
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
    WC_SUITE("GenVec_copy  (bulk memcpy vs per-element copy_fn)");
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
    WC_SUITE("HashMap put / get  (hot path)");
    WC_RUN(bench_map_put_pod);
    WC_RUN(bench_map_put_cx);
    WC_RUN(bench_map_get_pod);
    WC_RUN(bench_map_get_cx);
}

void suite_map_clear(void)
{
    WC_SUITE("HashMap_clear  (bulk del loop)");
    WC_RUN(bench_map_clear_pod);
    WC_RUN(bench_map_clear_cx);
}

void suite_pop(void)
{
    WC_SUITE("pop  (500k ops, copy + del path)");
    WC_RUN(bench_pop_pod);
    WC_RUN(bench_pop_cx);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE: complex owned type (struct owning a String AND a GenVec)
//
// This is the case a plain memcpy-based container can't handle at all, and
// where move semantics (as opposed to always-copy semantics) matter most:
// a value that owns *two* independent heap allocations.
//
//   typedef struct { String name; GenVec scores; } Person;
//
// bench_complex_push_copy — construct a Person on the stack each iteration,
//   GenVec_push() it (invokes person_copy: deep-copies both the String's
//   heap buffer and the GenVec's heap buffer), then destroy the stack copy.
//   This is what you pay every time if all you have is copy semantics
//   (e.g. inserting by value with no move constructor).
//
// bench_complex_push_move — heap-allocate a Person* shell each iteration,
//   GenVec_push_move() it (invokes person_move: one memcpy of the 40-ish
//   byte Person shell, free the old shell, done — the String's and
//   GenVec's underlying buffers are never touched, ownership just
//   relocates). This is the toolkit's move path doing what copying can't:
//   O(1) transfer regardless of how much data the owned members hold.
// ═══════════════════════════════════════════════════════════════════════════════

#define PERSON_N        200000
#define PERSON_SCORES_N 64 // enough ints to force GenVec's heap path and matter

// Long enough to blow past String's SSO inline capacity (23 bytes) so the
// name always owns a real heap buffer too — otherwise we'd only be
// benchmarking the GenVec field.
static inline int person_name_fmt(char* buf, u64 bufsize, int i)
{
    return snprintf(buf, bufsize, "person_number_%d_with_a_reasonably_long_name", i);
}

typedef struct {
    String name;
    GenVec scores;
} Person;

static void person_init(Person* p, int i)
{
    char buf[64];
    person_name_fmt(buf, sizeof(buf), i);
    string_create_stk(buf, &p->name);

    GenVec_create_stk((u64)PERSON_SCORES_N, sizeof(int), NULL, &p->scores);
    for (int j = 0; j < PERSON_SCORES_N; j++) {
        int v = i + j;
        GenVec_push(&p->scores, (u8*)&v);
    }
}

static void person_del(u8* elm)
{
    Person* p = (Person*)elm;
    string_destroy_stk(&p->name);
    GenVec_destroy_stk(&p->scores);
}

static void person_copy(u8* dest, const u8* src)
{
    const Person* s = (const Person*)src;
    Person*       d = (Person*)dest;

    // string_copy is safe on raw/uninitialised dest memory (dest is fully
    // re-initialised, not read first).
    string_copy(&d->name, &s->name);

    // GenVec_copy is NOT safe on raw dest — it assumes dest is already a
    // live, valid GenVec and destroys its old contents first. Bring dest
    // into a valid empty state so that destroy is a safe no-op, matching
    // the idiom the toolkit's own wc_vec_ops.copy_fn effectively achieves.
    GenVec_create_stk(0, sizeof(int), NULL, &d->scores);
    GenVec_copy(&d->scores, &s->scores);
}

static void person_move(u8* dest, u8** src)
{
    Person* s = *(Person**)src;
    memcpy(dest, s, sizeof(Person)); // shell only — name/scores buffers just change owner
    free(s);
    *src = NULL;
}

static const container_ops person_ops = { person_copy, person_move, person_del };

static void bench_complex_push_copy(void)
{
    GenVec* v = GenVec_create((u64)PERSON_N, sizeof(Person), &person_ops);

    // Build every source Person up front — this construction cost (string
    // heap alloc + GenVec growth) is identical in the move benchmark below,
    // so it must NOT be inside the timed region or it drowns out the one
    // thing we're actually comparing: what GenVec_push does with it.
    Person* pool = malloc((u64)PERSON_N * sizeof(Person));
    CHECK_FATAL(!pool, "malloc failed");
    for (int i = 0; i < PERSON_N; i++) person_init(&pool[i], i);

    u64 t0 = ns_now();
    for (int i = 0; i < PERSON_N; i++) {
        GenVec_push(v, (u8*)&pool[i]); // copy path: deep-copies name + scores
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(v->size, (u64)PERSON_N);
    bench("push complex (copy: String+GenVec)", PERSON_N, t0, t1);

    for (int i = 0; i < PERSON_N; i++) person_del((u8*)&pool[i]);
    free(pool);
    GenVec_destroy(v);
}

static void bench_complex_push_move(void)
{
    GenVec* v = GenVec_create((u64)PERSON_N, sizeof(Person), &person_ops);

    // Same construction cost as the copy benchmark, just heap-shelled and
    // pre-built outside the timed region for the same reason.
    Person** pool = malloc((u64)PERSON_N * sizeof(Person*));
    CHECK_FATAL(!pool, "malloc failed");
    for (int i = 0; i < PERSON_N; i++) {
        pool[i] = malloc(sizeof(Person));
        CHECK_FATAL(!pool[i], "malloc failed");
        person_init(pool[i], i);
    }

    u64 t0 = ns_now();
    for (int i = 0; i < PERSON_N; i++) {
        GenVec_push_move(v, (u8**)&pool[i]); // move path: O(1) shell relocation
    }
    u64 t1 = ns_now();

    WC_ASSERT_EQ_U64(v->size, (u64)PERSON_N);
    bench("push complex (move: String+GenVec)", PERSON_N, t0, t1);

    free(pool); // each pool[i] already freed+nulled by person_move
    GenVec_destroy(v);
}

void suite_complex_owned_type(void)
{
    WC_SUITE("complex owned type  (struct owning a String + a GenVec)");
    WC_RUN(bench_complex_push_copy);
    WC_RUN(bench_complex_push_move);
}


// ═══════════════════════════════════════════════════════════════════════════════
// SUITE: string storage strategies
//
// Compares four ways to accumulate many short, immutable strings:
//   - StringStore   append-only, chained fixed-size buffer (bump allocator,
//                    node reuse, no per-string malloc, no individual free)
//   - chain_arena    generic chained bump arena (same idea, not string-specific)
//   - Arena          single fixed-size bump arena (no chaining/growth)
//   - malloc         one malloc+memcpy per string, one free per string
//   - String (SSO)   the toolkit's dynamic string type (heap alloc per String,
//                    plus a second heap alloc once a string exceeds SSO size)
// ═══════════════════════════════════════════════════════════════════════════════

#define STRSTORE_N 500000

// Deterministic pseudo-varied lengths so we're not just measuring one code path.
static inline int strstore_fmt(char* buf, u64 bufsize, int i)
{
    return snprintf(buf, bufsize, "key_%d_%s", i, (i % 7 == 0) ? "abcdefghij" : "x");
}

static void bench_ss_StringStore(void)
{
    StringStore ss;
    StringStore_create(&ss);

    char buf[64];
    u64  t0 = ns_now();
    for (int i = 0; i < STRSTORE_N; i++) {
        int len = strstore_fmt(buf, sizeof(buf), i);
        (void)StringStore_cstr(&ss, buf, (u64)len);
    }
    u64 t1 = ns_now();

    bench("StringStore_cstr", STRSTORE_N, t0, t1);
    StringStore_destroy(&ss);
}

static void bench_ss_chain_arena(void)
{
    ChainArena* ca = chain_arena_create();

    char buf[64];
    u64  t0 = ns_now();
    for (int i = 0; i < STRSTORE_N; i++) {
        int   len = strstore_fmt(buf, sizeof(buf), i);
        char* p   = (char*)chain_arena_alloc(ca, (u64)len);
        memcpy(p, buf, (u64)len);
    }
    u64 t1 = ns_now();

    bench("chain_arena_alloc + memcpy", STRSTORE_N, t0, t1);
    chain_arena_destroy(ca);
}

static void bench_ss_arena(void)
{
    // Sized generously up front — Arena doesn't grow, unlike the other three.
    Arena* a = arena_create(STRSTORE_N * 32ULL);

    char buf[64];
    u64  t0 = ns_now();
    for (int i = 0; i < STRSTORE_N; i++) {
        int   len = strstore_fmt(buf, sizeof(buf), i);
        char* p   = (char*)arena_alloc(a, (u64)len);
        memcpy(p, buf, (u64)len);
    }
    u64 t1 = ns_now();

    bench("Arena (fixed) alloc + memcpy", STRSTORE_N, t0, t1);
    arena_destroy(a);
}

static void bench_ss_malloc(void)
{
    char** ptrs = malloc(STRSTORE_N * sizeof(char*));
    CHECK_FATAL(!ptrs, "malloc failed");

    char buf[64];
    u64  t0 = ns_now();
    for (int i = 0; i < STRSTORE_N; i++) {
        int   len = strstore_fmt(buf, sizeof(buf), i);
        char* p   = malloc((u64)len);
        memcpy(p, buf, (u64)len);
        ptrs[i] = p;
    }
    u64 t1 = ns_now();

    bench("malloc + memcpy (1 free/string)", STRSTORE_N, t0, t1);

    for (int i = 0; i < STRSTORE_N; i++) {
        free(ptrs[i]);
    }
    free(ptrs);
}

static void bench_ss_string_sso(void)
{
    GenVec* v = GenVec_create(STRSTORE_N, sizeof(String), &wc_str_ops);

    char buf[64];
    u64  t0 = ns_now();
    for (int i = 0; i < STRSTORE_N; i++) {
        strstore_fmt(buf, sizeof(buf), i);
        String s;
        string_create_stk(buf, &s);
        GenVec_push(v, (u8*)&s);
        string_destroy_stk(&s);
    }
    u64 t1 = ns_now();

    bench("String (SSO) create + push+copy", STRSTORE_N, t0, t1);
    GenVec_destroy(v);
}

void suite_string_storage(void)
{
    WC_SUITE("string storage strategies  (500k mixed-length strings)");
    WC_RUN(bench_ss_StringStore);
    WC_RUN(bench_ss_chain_arena);
    WC_RUN(bench_ss_arena);
    WC_RUN(bench_ss_malloc);
    WC_RUN(bench_ss_string_sso);
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
    suite_string_storage();
    suite_complex_owned_type();

    return WC_REPORT();
}


