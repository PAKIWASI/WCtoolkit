#include "String.h"
#include "gen_vector.h"
#include "wc_helpers.h"
#include "wc_macros.h"
#include <stdio.h>



int main(void)
{
    container_ops ops = VEC_MAKE_OPS(str_copy, str_move, str_del);
    genVec* vec = VEC_CX(String, 10, &ops);

    VEC_PUSH_CSTR(vec, "hel");
    VEC_PUSH_CSTR(vec, "orldd");
    VEC_PUSH_CSTR(vec, "paki");
    VEC_PUSH_CSTR(vec, "wasi");

    genVec_view view = genVec_view_create(vec, 1, 2);

    VECVIEW_FOREACH(view, String, ss) {
        string_print(ss);
        putchar('\n');
    }

    genVec_destroy(vec);
    return 0;
}
