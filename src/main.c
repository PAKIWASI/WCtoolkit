#include "gen_vector.h"
#include "wc_macros.h"

int main(void)
{
    genVec* vec = VEC(int, 10);

    genVec_destroy(vec);
}
