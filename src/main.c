#include "String.h"
#include "gen_vector.h"
#include "wc_helpers.h"
#include "wc_macros.h"



int main(void)
{
    container_ops ops = VEC_MAKE_OPS(str_copy, str_move, str_del);
    genVec* vec = VEC_CX(String, 10, &ops);

    VEC_PUSH_CSTR(vec, "hel");
    VEC_PUSH_CSTR(vec, "hel");
    VEC_PUSH_CSTR(vec, "hel");
    VEC_PUSH_CSTR(vec, "hel");

    genVec v2;
    genVec_move(&v2, &vec);

    genVec_print(&v2, str_print);

    genVec_destroy_stk(&v2);
    genVec_destroy(vec);
    return 0;
}
