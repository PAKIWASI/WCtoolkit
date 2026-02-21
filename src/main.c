#include "String.h"
#include "wc_helpers.h"
#include "wc_macros.h"


int main(void)
{
    genVec* vec = VEC_CX(String, 10, str_copy, str_move, str_del);

    String str;
    string_create_stk(&str, "hello");
    VEC_PUSH(vec, str);
    VEC_PUSH_COPY(vec, str);

    String* s = string_from_cstr("wat");
    VEC_PUSH_MOVE(vec, s);

    s = string_create();
    genVec_pop(vec, castptr(s));
    string_destroy(s);


    genVec_print(vec, str_print);

    string_destroy(s);
    string_destroy_stk(&str);
    genVec_destroy(vec);
    return 0;
}
