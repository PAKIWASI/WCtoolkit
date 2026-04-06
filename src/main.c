#include "gen_vector.h"
#include "hashmap.h"
#include "map_setup.h"
#include "wc_helpers.h"
#include "wc_macros.h"


int main(void)
{
    hashmap* map  = hashmap_create(
            sizeof(String),
            sizeof(genVec),
            wyhash_str,
            str_cmp,
            &wc_str_ops,
            &wc_vec_ops
    );

    String* s = string_from_cstr("hello");
    genVec* v = genVec_init_arr(5, sizeof(int), NULL, 
            (u8*)(int[5]){1, 2, 3, 4, 5});

    MAP_PUT_MOVE(map, s, v);

    hashmap_print(map, str_print, vec_print_int);

    hashmap_destroy(map);
    return 0;
}


