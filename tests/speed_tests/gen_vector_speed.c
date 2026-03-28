#include "wc_speed_test.h"
#include "gen_vector.h"
#include "wc_macros.h"
#include "wc_helpers.h"
#include "String.h"

#define N 1000000
#define N_MED 100000

void gen_vector_speed_suite(void)
{
    WC_BENCH_SUITE("genVec");

    /* ── POD push ────────────────────────────────────────────────────── */
    WC_BENCH("push 1M ints (pre-reserved)", N, {
        genVec* v = genVec_init(N, sizeof(int), NULL);
        for (int i = 0; i < N; i++) { VEC_PUSH(v, i); }
        genVec_destroy(v);
    });

    WC_BENCH("push 1M ints (no reserve, grows)", N, {
        genVec* v = genVec_init(0, sizeof(int), NULL);
        for (int i = 0; i < N; i++) { VEC_PUSH(v, i); }
        genVec_destroy(v);
    });

    /* ── POD random access ───────────────────────────────────────────── */
    WC_BENCH("get_ptr 1M sequential reads", N, {
        genVec* v = genVec_init(N, sizeof(int), NULL);
        for (int i = 0; i < N; i++) { VEC_PUSH(v, i); }
        volatile int sink = 0;
        for (int i = 0; i < N; i++) { sink += *(int*)genVec_get_ptr(v, (u64)i); }
        genVec_destroy(v);
    });

    /* ── POD pop ─────────────────────────────────────────────────────── */
    WC_BENCH("pop 1M ints", N, {
        genVec* v = genVec_init(N, sizeof(int), NULL);
        for (int i = 0; i < N; i++) { VEC_PUSH(v, i); }
        for (int i = 0; i < N; i++) { genVec_pop(v, NULL); }
        genVec_destroy(v);
    });

    /* ── Insert at front (expensive — shift) ─────────────────────────── */
    WC_BENCH("insert_front 10K ints", 10000, {
        genVec* v = genVec_init(10000, sizeof(int), NULL);
        for (int i = 0; i < 10000; i++) {
            int x = i;
            genVec_insert(v, 0, (u8*)&x);
        }
        genVec_destroy(v);
    });

    /* ── swap_pop (O(1) delete) ──────────────────────────────────────── */
    WC_BENCH("swap_pop 100K ints from back", N_MED, {
        genVec* v = genVec_init(N_MED, sizeof(int), NULL);
        for (int i = 0; i < N_MED; i++) { VEC_PUSH(v, i); }
        while (v->size > 0) { genVec_swap_pop(v, v->size - 1, NULL); }
        genVec_destroy(v);
    });

    /* ── String (owned) push/destroy ─────────────────────────────────── */
    WC_BENCH("push 100K String (copy)", N_MED, {
        genVec* v = VEC_OF_STR(N_MED);
        for (int i = 0; i < N_MED; i++) { VEC_PUSH_CSTR(v, "benchmark_string"); }
        genVec_destroy(v);
    });

    /* ── genVec_find linear scan ─────────────────────────────────────── */
    WC_BENCH("find (worst case, 100K ints)", N_MED, {
        genVec* v = genVec_init(N_MED, sizeof(int), NULL);
        for (int i = 0; i < N_MED; i++) { VEC_PUSH(v, i); }
        int target = N_MED - 1;
        genVec_find(v, (u8*)&target, NULL);
        genVec_destroy(v);
    });

    /* ── copy ────────────────────────────────────────────────────────── */
    WC_BENCH("copy 100K int vec", N_MED, {
        genVec* src = genVec_init(N_MED, sizeof(int), NULL);
        for (int i = 0; i < N_MED; i++) { VEC_PUSH(src, i); }
        genVec dest;
        genVec_init_stk(0, sizeof(int), NULL, &dest);
        genVec_copy(&dest, src);
        genVec_destroy(src);
        genVec_destroy_stk(&dest);
    });
}


