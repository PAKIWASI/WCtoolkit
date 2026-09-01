#include "String.h"
#include <stdio.h>


int main(void)
{
    String s = {0};

    string_create_stk(&s, "hello");
    printf("sso: %d\n", string_is_sso(&s));

    string_append_cstr(&s, " world jfdkjfkdjjfdjfkdjfk");
    printf("sso: %d\n", string_is_sso(&s));

    string_destroy_stk(&s);
    return 0;
}
