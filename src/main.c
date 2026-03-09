#include "String.h"
#include "common.h"
#include <stdio.h>


int main(void)
{
    String* str = string_from_cstr("helllo");

    string_print(str);
    printf("\n%lu\n%lu\n", str->size, str->capacity);
    print_hex(castptr(str), sizeof(String), 8);

    string_append_cstr(str, " what is up whataaaa");
    string_append_char(str, ' ');

    string_print(str);
    printf("\n%lu\n%lu\n", str->size, str->capacity);
    print_hex(castptr(str), sizeof(String), 8);

    string_pop_char(str);
    string_pop_char(str);
    string_pop_char(str);
    string_pop_char(str);
    string_pop_char(str);
    string_pop_char(str);
    string_pop_char(str);
    string_pop_char(str);
    string_pop_char(str);

    string_shrink_to_fit(str);

    string_print(str);
    printf("\n%lu\n%lu\n", str->size, str->capacity);
    print_hex(castptr(str), sizeof(String), 8);

    string_destroy(str);
}
