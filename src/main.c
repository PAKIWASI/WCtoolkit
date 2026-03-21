#include "String.h"
#include "gen_vector.h"
#include "wc_helpers.h"
#include "wc_macros.h"


static int run_string(void);
static int run_genVec_subarr(void);


int main(void)
{
    genVec* vec = genVec_init(10, sizeof(String), &wc_str_ops);

    VEC_PUSH_CSTR(vec, "0hello");
    VEC_PUSH_CSTR(vec, "1hello");
    VEC_PUSH_CSTR(vec, "2hello");
    VEC_PUSH_CSTR(vec, "3hello");
    VEC_PUSH_CSTR(vec, "4hello");
    VEC_PUSH_CSTR(vec, "5hello");

    genVec_print(vec, str_print); putchar('\n');

    genVec_remove_range(vec, 1, 3);

    String* s = string_from_cstr("2hello");
    printf("%lu\n", genVec_find(vec, (u8*)s, str_cmp));
    string_destroy(s);

    genVec_print(vec, str_print); putchar('\n');

    genVec_destroy(vec);

    run_string();
    // run_genVec_subarr();
}


int run_string(void)
{
    String* s = string_from_cstr("012345");

    string_print(s); putchar('\n');

    String* s2 = string_substr(s, 1, 4);

    string_print(s2);

    string_destroy(s);
    string_destroy(s2);
    return 0;
}

int run_genVec_subarr(void)
{
    genVec* v = genVec_init(10, sizeof(String), &wc_str_ops);

    VEC_PUSH_CSTR(v, "0hello");
    VEC_PUSH_CSTR(v, "1hello");
    VEC_PUSH_CSTR(v, "2hello");
    VEC_PUSH_CSTR(v, "3hello");
    VEC_PUSH_CSTR(v, "4hello");
    VEC_PUSH_CSTR(v, "5hello");

    genVec_print(v, str_print); putchar('\n');

    genVec* v2 = genVec_subarr(v, 1, 2);

    genVec_print(v2, str_print); putchar('\n');

    genVec_destroy(v);
    genVec_destroy(v2);
    return 0;
}


