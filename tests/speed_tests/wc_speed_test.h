#ifndef WC_SPEED_TEST_H
#define WC_SPEED_TEST_H

/*
 * speed_test.h — Lightweight throughput benchmarking for WCtoolkit
 * ================================================================
 *
 * USAGE
 * -----
 *   WC_BENCH_BEGIN("genVec push 1M ints");
 *   for (int i = 0; i < 1000000; i++) { VEC_PUSH(v, i); }
 *   WC_BENCH_END();
 *
 * Output: "  genVec push 1M ints          123.45 ms   8.11 Mops/s"
 *
 * Each WC_BENCH_BEGIN/END pair is self-contained — no global state needed.
 * ops_count is optional; pass 0 to suppress the ops/s column.
 */

#include "common.h"
#include <stdio.h>
#include <time.h>

// Nanoseconds since an arbitrary epoch (monotonic)
static inline u64 wc_bench_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((u64)ts.tv_sec * 1000000000ULL) + (u64)ts.tv_nsec;
}

#define WC_BENCH_SUITE(name) printf("\n\033[1;33m══ SPEED: %s ══\033[0m\n", (name))

/*
 * WC_BENCH(label, ops_count, body)
 * Runs body, prints elapsed time and optionally throughput.
 * ops_count: number of logical operations performed (0 = skip throughput).
 */
#define WC_BENCH(label, ops_count, body)                             \
    do {                                                             \
        u64        _t0 = wc_bench_ns();                              \
        {body} u64 _t1 = wc_bench_ns();                              \
        double     _ms = (double)(_t1 - _t0) / 1e6;                  \
        printf("  %-44s %8.2f ms", (label), _ms);                    \
        u64 _ops = (u64)(ops_count);                                 \
        if (_ops > 0 && _t1 > _t0) {                                 \
            double _mops = (double)_ops / ((_t1 - _t0) / 1e9) / 1e6; \
            printf("  %7.2f Mops/s", _mops);                         \
        }                                                            \
        printf("\n");                                                \
    } while (0)

// Convenience: repeat body N times and count each iteration as one op 
#define WC_BENCH_N(label, n, body) WC_BENCH(label, (n), for (u64 _wbn_i = 0; _wbn_i < (u64)(n); _wbn_i++){body})

#endif /* WC_SPEED_TEST_H */
