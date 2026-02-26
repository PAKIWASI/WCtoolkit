

#include "String.h"
#include "gen_vector.h"
#include "wc_helpers.h"
#include "wc_macros.h"
int main(void)
{
    genVec_ops ops = VEC_MAKE_OPS(str_copy, str_move, str_del);
    genVec* vec = VEC_CX(String, 10, &ops);

    VEC_PUSH_CSTR(vec, "hello");
    VEC_PUSH_CSTR(vec, "hello");
    VEC_PUSH_CSTR(vec, "hello");
    VEC_PUSH_CSTR(vec, "hello");
    VEC_PUSH_CSTR(vec, "hello");

    genVec_print(vec, str_print);

    genVec_destroy(vec);
    return 0;
}
