#include "String.h"
#include "gen_vector.h"
#include "hashset.h"
#include "wc_helpers.h"
#include "wc_macros.h"


int main(void)
{
    // hashset* set = hashset_create(sizeof(String), wyhash_str, str_cmp, &wc_str_ops);

    genVec* vec = genVec_init(10, sizeof(String), &wc_str_ops);
    VEC_PUSH_CSTR(vec, "hello0");
    VEC_PUSH_CSTR(vec, "hello1");
    VEC_PUSH_CSTR(vec, "hello2");
    VEC_PUSH_CSTR(vec, "hello3");
    VEC_PUSH_CSTR(vec, "hello4");
    genVec_print(vec, str_print); putchar('\n');

    hashset* set = SET_FROM_VEC(vec, wyhash_str, str_cmp);
    hashset_print(set, str_print);

    if (SET_INSERT_CSTR(set, "hello5")) {
        printf("already exist - not inserted\n");
    } else {
        printf("not exist - inserted\n");
    }

    String str; 
    string_create_stk(&str, "fkdjfkdj");
    SET_INSERT(set, str);
    hashset_print(set, str_print);

    genVec_destroy(vec);
    hashset_destroy(set);
    return 0;
}


