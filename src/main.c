#include "common.h"
#include "hashmap.h"
#include "wc_allocator.h"
#include "wc_macros.h"
#include "wc_string.h"



int main(void)
{
    HashMap m = MAP_OF(String, String);

    MAP_PUT_STR_STR(&m, "hello", "World");
    MAP_PUT_STR_STR(&m, "hello", "world");
    MAP_PUT_STR_STR(&m, "hlo", "World");
    MAP_PUT_STR_STR(&m, "ho", "World");

    HashMap_print(&m, wc_str_print, wc_str_print);

    String s = String_from_cstr(WC_LIBC, "hello");
    WC_ASSERT(HashMap_has(&m, &s), );


    String_destroy(&s);
    HashMap_destroy(&m);
    return 0;
}
