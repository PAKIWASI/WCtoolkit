#include "wc_speed_test.h"
#include "hashmap.h"
#include "wc_helpers.h"
#include "wc_macros.h"

#define N     100000
#define N_STR 50000

void hashmap_speed_suite(void)
{
    WC_BENCH_SUITE("HashMap");

    WC_BENCH("put 100K int->int", N, {
        hashmap* m = hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
        for (int i = 0; i < N; i++) {
            int v = i * 2;
            hashmap_put(m, (u8*)&i, (u8*)&v);
        }
        hashmap_destroy(m);
    });

    WC_BENCH("get 100K int->int (all hits)", N, {
        hashmap* m = hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
        for (int i = 0; i < N; i++) {
            int v = i;
            hashmap_put(m, (u8*)&i, (u8*)&v);
        }
        int out;
        for (int i = 0; i < N; i++) { hashmap_get(m, (u8*)&i, (u8*)&out); }
        hashmap_destroy(m);
    });

    WC_BENCH("has 100K int (50% miss)", N, {
        hashmap* m = hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
        for (int i = 0; i < N; i++) {
            int v = i;
            hashmap_put(m, (u8*)&i, (u8*)&v);
        }
        for (int i = 0; i < N; i++) {
            int k = (i % 2 == 0) ? i : (i + N);
            hashmap_has(m, (u8*)&k);
        }
        hashmap_destroy(m);
    });

    WC_BENCH("del 100K int->int (all present)", N, {
        hashmap* m = hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);
        for (int i = 0; i < N; i++) {
            int v = i;
            hashmap_put(m, (u8*)&i, (u8*)&v);
        }
        for (int i = 0; i < N; i++) { hashmap_del(m, (u8*)&i, NULL); }
        hashmap_destroy(m);
    });

    // WC_BENCH("put 50K String->String (owned)", N_STR, {
    //     hashmap* m = hashmap_create(sizeof(String), sizeof(String),
    //                                 wyhash_str, str_cmp,
    //                                 &wc_str_ops, &wc_str_ops);
    //     char kbuf[32], vbuf[32];
    //     for (int i = 0; i < N_STR; i++) {
    //         snprintf(kbuf, sizeof(kbuf), "key_%d", i);
    //         snprintf(vbuf, sizeof(vbuf), "val_%d", i);
    //         MAP_PUT_STR_STR(m, kbuf, vbuf);
    //     }
    //     hashmap_destroy(m);
    // });
}
