#include "String.h"
#include "gen_vector.h"
#include "wc_helpers.h"
#include "wc_macros.h"


int run_string(void);
int run_genVec_subarr(void);


int main(void)
{
    genVec* vec = genVec_init(10, sizeof(String), &wc_str_ops);

    VEC_PUSH_CSTR(vec, "hello-0");
    VEC_PUSH_CSTR(vec, "hello-1");
    VEC_PUSH_CSTR(vec, "hello-2");
    VEC_PUSH_CSTR(vec, "hello-3");
    VEC_PUSH_CSTR(vec, "hello-4");
    VEC_PUSH_CSTR(vec, "hello-5");

    genVec_print(vec, str_print); putchar('\n');

    genVec_remove_range(vec, 3, 1);

    String* s = string_from_cstr("hello-3");
    printf("%lu\n", genVec_find(vec, (u8*)s, str_cmp));
    string_destroy(s);

    genVec_print(vec, str_print); putchar('\n');

    genVec_destroy(vec);

    run_string();

    run_genVec_subarr();
}


int run_string(void)
{
    String* s = string_from_cstr("hello");

    string_print(s); putchar('\n');

    string_remove_range(s, 1, 1);

    printf("%lu\n", string_find_char(s, 'e'));

    string_print(s); putchar('\n');

    string_destroy(s);
    return 0;
}

int run_genVec_subarr(void)
{
    genVec* v = genVec_init(10, sizeof(String), &wc_str_ops);

    VEC_PUSH_CSTR(v, "hello-0");
    VEC_PUSH_CSTR(v, "hello-1");
    VEC_PUSH_CSTR(v, "hello-2");
    VEC_PUSH_CSTR(v, "hello-3");
    VEC_PUSH_CSTR(v, "hello-4");
    VEC_PUSH_CSTR(v, "hello-5");

    genVec_print(v, str_print); putchar('\n');

    genVec* v2 = genVec_subarr(v, 1, 2);

    genVec_print(v2, str_print); putchar('\n');

    genVec_destroy(v);
    genVec_destroy(v2);
    return 0;
}


