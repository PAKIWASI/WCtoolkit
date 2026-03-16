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

    string_append_cstr(s, " world");

    int* b = &a;
    hashmap_put_move(map, (u8**)cast(b), (u8**)cast(s));

    hashmap_print(map, wc_print_int, str_print);

    hashmap_destroy(map);
}
