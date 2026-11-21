#include "hashmap.h"
#include "wc_macros.h"
#include "wc_string.h"

#include <stdio.h>


// TODO: we return struct by values from create functions. is it inefficient?

int main(void)
{
    HashMap m = MAP_OF(String, String);

    MAP_PUT_STR_STR(&m, "hello", "World");
    MAP_PUT_STR_STR(&m, "hello", "world");
    MAP_PUT_STR_STR(&m, "hlo", "World");
    MAP_PUT_STR_STR(&m, "ho", "World");

    HashMap_print(&m, wc_str_print, wc_str_print);

    String s = String_from_cstr_borrow("hello");

    String v;
    HashMap_del(&m, &s, &v);

    String_print(&s);
    putchar('\n');
    String_print(&v);
    putchar('\n');

    HashMap_print(&m, wc_str_print, wc_str_print);

    String_destroy(&v);
    HashMap_destroy(&m);
    return 0;
}
