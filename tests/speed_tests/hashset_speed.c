#include "wc_speed_test.h"
#include "hashset.h"
#include "wc_helpers.h"
#include "wc_macros.h"

#define N     100000
#define N_STR 50000

void hashset_speed_suite(void)
{
    WC_BENCH_SUITE("HashSet");

    WC_BENCH("insert 100K ints", N, {
        hashset* s = hashset_create(sizeof(int), NULL, NULL, NULL);
        for (int i = 0; i < N; i++) { hashset_insert(s, (u8*)&i); }
        hashset_destroy(s);
    });

    WC_BENCH("has 100K ints (all present)", N, {
        hashset* s = hashset_create(sizeof(int), NULL, NULL, NULL);
        for (int i = 0; i < N; i++) { hashset_insert(s, (u8*)&i); }
        for (int i = 0; i < N; i++) { hashset_has(s, (u8*)&i); }
        hashset_destroy(s);
    });

    WC_BENCH("remove 100K ints", N, {
        hashset* s = hashset_create(sizeof(int), NULL, NULL, NULL);
        for (int i = 0; i < N; i++) { hashset_insert(s, (u8*)&i); }
        for (int i = 0; i < N; i++) { hashset_remove(s, (u8*)&i); }
        hashset_destroy(s);
    });

    WC_BENCH("insert 50K Strings", N_STR, {
        hashset* s = hashset_create(sizeof(String), wyhash_str, str_cmp, &wc_str_ops);
        for (int i = 0; i < N_STR; i++) { SET_INSERT_CSTR(s, "bench_key"); }
        hashset_destroy(s);
    });
}
