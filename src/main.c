#include "String.h"
#include "hashmap.h"
#include "wc_helpers.h"
#include "wc_macros.h"



int main(void)
{
    hashmap* map = hashmap_create(sizeof(int), sizeof(String), NULL, NULL, NULL, &wc_str_ops);

    MAP_PUT_INT_STR(map, 1, "hello");
    MAP_PUT_INT_STR(map, 2, "world");

    hashmap_print(map, wc_print_int, str_print);

    int o_key = 1;
    int n_key = 3;
    MAP_PUT(map, cast(n_key), hashmap_get_ptr(map, cast(o_key)));

    hashmap_destroy(map);
}
