#include "String.h"
#include <stdio.h>


int main(void)
{
    String* str = string_from_cstr("hello");

    char cstr[10] = {0};

    string_to_cstr_buf(str, cstr, 10);

    string_print(str); putchar('\n');

    printf("%s\n", cstr);

    string_destroy(str);
}


