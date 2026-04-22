#include "String.h"
#include "hashmap.h"
#include "wc_helpers.h"
#include "wc_macros.h"


int main(void)
{
    hashmap* map = hashmap_create(
        sizeof(String),
        sizeof(String),
        wyhash_str,
        str_cmp,
        &wc_str_ops,
        &wc_str_ops
    );

    MAP_PUT_STR_STR(map, "hello", "world");
    MAP_PUT_STR_STR(map, "hello", "world_2");
    MAP_PUT_STR_STR(map, "wtf", "skjfdkfj");
    MAP_PUT_STR_STR(map, "a", "A");

    hashmap_print(map, str_print, str_print);

    return 0;
}
