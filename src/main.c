#include "String.h"
#include "gen_vector.h"
#include "wc_helpers.h"
#include "wc_macros.h"




int main(void)
{
    genVec* vec = genVec_init(10, sizeof(String), &wc_str_ptr_ops);

    VEC_PUSH_CSTR(vec, "hello0");
    VEC_PUSH_CSTR(vec, "hello1");
    VEC_PUSH_CSTR(vec, "hello2");
    VEC_PUSH_CSTR(vec, "hello3");
    VEC_PUSH_CSTR(vec, "hello4");
    VEC_PUSH_CSTR(vec, "hello5");

    genVec_print(vec, str_print); putchar('\n');

    String* s = string_from_cstr("hello3");

    printf("%lu\n", genVec_find(vec, (u8*)s, str_cmp));

    string_destroy(s);
    genVec_destroy(vec);
}

