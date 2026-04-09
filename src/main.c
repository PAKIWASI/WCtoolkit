#include "gen_vector.h"
#include "wc_helpers.h"
#include "wc_macros.h"


int main(void)
{
    genVec* v = genVec_init(10, sizeof(u32), &wc_str_ops);
    VEC_PUSH_CSTR(v, "ehllo");

    printf("ehllo");

    genVec_destroy(v);
    return 0;
}
