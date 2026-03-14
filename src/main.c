#include "String.h"
#include "hashmap.h"
#include "wc_helpers.h"



int main(void)
{
    hashmap* map = hashmap_create(sizeof(int), sizeof(String), NULL, NULL, NULL, &wc_str_ops);

    int a = 1;
    String* s = string_from_cstr("hello");

    hashmap_put(map, cast(a), castptr(s));
    a++;
    hashmap_put(map, cast(a), castptr(s));
    a++;
    hashmap_put(map, cast(a), castptr(s));
    a++;
    hashmap_put(map, cast(a), castptr(s));
    a++;

    hashmap_print(map, wc_print_int, str_print);

    hashmap_destroy(map);
}
