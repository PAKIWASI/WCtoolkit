#include "gen_vector.h"
#include "wc_helpers.h"

#include <net/ethernet.h>


int main(void)
{
    genVec* vec = genVec_init(10, sizeof(String), &wc_str_ops);

    String* s = string_create();

    for (int i = 0; i < 1000; i++) {
        for (int j = i; j > 0; j--) {
            string_append_char(s, 'a');
        } 

        genVec_push(vec, (u8*)s);
    }

    // VEC_FOREACH(vec, String, str) {
    //     string_print(str); putchar('\n');
    // }

    print_hex(vec->data, vec->capacity * sizeof(String), 32);

    string_destroy(s);
    genVec_destroy(vec);
}


