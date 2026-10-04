#include "bench_support.h"
#include "hashmap.h"
#include "ubench.h"
#include "wc_allocator.h"
#include "wc_macros.h"
#include "wc_string.h"
#include <stdio.h>
#include <stdlib.h>

// ubench.h registers each benchmark with realloc() inside the UBENCH macro
// NOLINTBEGIN(bugprone-suspicious-realloc-usage)
UBENCH(hashmap, put_int)
{
    HashMap m = MAP_OF(int, int);
    for (int i = 0; i < N; i++) {
        MAP_PUT(&m, i, i);
    }
    UBENCH_DO_NOTHING(m.keys);
    HashMap_destroy(&m);
}

UBENCH(hashmap, put_int_reserved)
{
    HashMap m = MAP_OF(int, int);
    HashMap_reserve(&m, N);
    for (int i = 0; i < N; i++) {
        MAP_PUT(&m, i, i);
    }
    UBENCH_DO_NOTHING(m.keys);
    HashMap_destroy(&m);
}

UBENCH(hashmap, put_string)
{
    HashMap m = MAP_OF(String, int);
    char    key[32];
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        MAP_PUT_STR_INT(&m, key, i);
    }
    UBENCH_DO_NOTHING(m.keys);
    HashMap_destroy(&m);
}

UBENCH_EX(hashmap, get_int)
{
    HashMap m = MAP_OF(int, int);
    for (int i = 0; i < N; i++) {
        MAP_PUT(&m, i, i);
    }
    UBENCH_DO_BENCHMARK()
    {
        int sum = 0;
        for (int i = 0; i < N; i++) {
            sum += *(const int*)HashMap_get_ptr(&m, &i);
        }
        UBENCH_DO_NOTHING(&sum);
    }
    HashMap_destroy(&m);
}

UBENCH_EX(hashmap, get_string)
{
    HashMap m = MAP_OF(String, int);
    String* keys = malloc(sizeof(String) * N); // N strings are too big for the stack
    char    buf[32];
    for (int i = 0; i < N; i++) {
        snprintf(buf, sizeof(buf), "key_%d", i);
        keys[i] = String_from_cstr(WC_LIBC, buf);
        MAP_PUT_STR_INT(&m, buf, i);
    }
    UBENCH_DO_BENCHMARK()
    {
        int sum = 0;
        for (int i = 0; i < N; i++) {
            sum += *(const int*)HashMap_get_ptr(&m, &keys[i]);
        }
        UBENCH_DO_NOTHING(&sum);
    }
    for (int i = 0; i < N; i++) {
        String_destroy(&keys[i]);
    }
    free(keys);
    HashMap_destroy(&m);
}

// Times fill + clear + destroy together (HashMap_clear alone cannot be re-run on an
// emptied map). The fill dominates: snprintf, a key String, a heap-allocated value copy.
UBENCH(hashmap, fill_clear_string)
{
    HashMap m = MAP_OF(String, String);
    char    key[32];
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        MAP_PUT_STR_STR(&m, key, LONG_STR);
    }
    HashMap_clear(&m); // del_fn on every key and value
    HashMap_destroy(&m);
}

// NOLINTEND(bugprone-suspicious-realloc-usage)
