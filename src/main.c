#include "String.h"
#include "hashmap.h"
#include "wc_helpers.h"
#include "wc_macros.h"
#include <string.h>


int main(void)
{
    hashmap* map = hashmap_create(sizeof(int), sizeof(String), NULL, NULL, NULL, &wc_str_ops);

    MAP_PUT_INT_STR(map, 1, "hefd");
    MAP_PUT_INT_STR(map, 3, "hefd");
    MAP_PUT_INT_STR(map, 5, "hefd");
    MAP_PUT_INT_STR(map, 7, "hefd");
    MAP_PUT_INT_STR(map, 9, "hefd");

    hashmap_print(map, wc_print_int, str_print);

    MAP_FOREACH_KEY(map, String, s) {
        string_print(s);
    }

    hashmap_destroy(map);
}


