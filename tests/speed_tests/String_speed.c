#include "wc_speed_test.h"
#include "String.h"

#define N      100000
#define N_LONG 10000

void string_speed_suite(void)
{
    WC_BENCH_SUITE("String");

    WC_BENCH("from_cstr (short, SSO) x100K", N, {
        for (int i = 0; i < N; i++) {
            String* s = string_from_cstr("hello");
            string_destroy(s);
        }
    });

    WC_BENCH("append_char until 1000 chars x1K strings", N_LONG, {
        for (int i = 0; i < N_LONG; i++) {
            String* s = string_create();
            for (int j = 0; j < 1000; j++) { string_append_char(s, 'x'); }
            string_destroy(s);
        }
    });

    WC_BENCH("append_cstr (short) x100K", N, {
        String* s = string_create();
        for (int i = 0; i < N; i++) { string_append_cstr(s, "ab"); }
        string_destroy(s);
    });

    WC_BENCH("find_cstr (hit, mid-string) x100K", N, {
        String* s = string_from_cstr("the quick brown fox jumps over the lazy dog");
        for (int i = 0; i < N; i++) { string_find_cstr(s, "fox"); }
        string_destroy(s);
    });

    WC_BENCH("compare equal strings x100K", N, {
        String* a = string_from_cstr("benchmark_string_value");
        String* b = string_from_cstr("benchmark_string_value");
        for (int i = 0; i < N; i++) { string_compare(a, b); }
        string_destroy(a);
        string_destroy(b);
    });

    WC_BENCH("copy 100-char string x100K", N, {
        String* src = string_create();
        for (int j = 0; j < 100; j++) { string_append_char(src, 'a'); }
        for (int i = 0; i < N; i++) {
            String dst;
            dst.size = 0; dst.capacity = STR_SSO_SIZE;
            string_copy(&dst, src);
            string_destroy_stk(&dst);
        }
        string_destroy(src);
    });
}
