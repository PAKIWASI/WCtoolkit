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
    // Keys are interned: the map stores a 16-byte StrView, not a String.
    StringStore ss;
    StringStore_create(&ss, WC_LIBC, nKB(64), true);
    HashMap m = MAP_OF(StrView, int);
    char    key[32];
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        StrView k = StringStore_cstr(&ss, key);
        MAP_PUT(&m, k, i);
    }
    UBENCH_DO_NOTHING(m.keys);
    HashMap_destroy(&m);
    StringStore_destroy(&ss);
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
    StringStore ss;
    StringStore_create(&ss, WC_LIBC, nKB(64), true);
    HashMap  m    = MAP_OF(StrView, int);
    StrView* keys = malloc(sizeof(StrView) * N); // N views are too big for the stack
    char     buf[32];
    for (int i = 0; i < N; i++) {
        snprintf(buf, sizeof(buf), "key_%d", i);
        keys[i] = StringStore_cstr(&ss, buf);
        MAP_PUT(&m, keys[i], i);
    }
    UBENCH_DO_BENCHMARK()
    {
        int sum = 0;
        for (int i = 0; i < N; i++) {
            sum += *(const int*)HashMap_get_ptr(&m, &keys[i]);
        }
        UBENCH_DO_NOTHING(&sum);
    }
    free(keys);
    HashMap_destroy(&m);
    StringStore_destroy(&ss);
}

// Times fill + clear + destroy together (HashMap_clear alone cannot be re-run on an
// emptied map). The fill dominates: snprintf, interning the key, a heap-allocated value copy.
UBENCH(hashmap, fill_clear_string)
{
    StringStore ss;
    StringStore_create(&ss, WC_LIBC, nKB(64), true);
    HashMap m = MAP_OF(StrView, String);
    char    key[32];
    for (int i = 0; i < N; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        MAP_PUT_SV_STR(&m, StringStore_cstr(&ss, key), LONG_STR);
    }
    HashMap_clear(&m); // del_fn on every value; key bytes stay in the store
    HashMap_destroy(&m);
    StringStore_destroy(&ss);
}

// NOLINTEND(bugprone-suspicious-realloc-usage)
