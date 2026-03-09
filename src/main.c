

#include "common.h"
#include "new_str.h"
#include <stdio.h>
int main(void)
{
    str s;
    str_create_stk(&s, "hello");

    str_print(&s);
    printf("\n%d\n%lu\n", s.sso, sizeof(str));
    print_hex(cast(s), sizeof(str), 8);

    for (u64 i = 0; i < 100; i++) {
        str_append_char(&s, 'a');
    }

    str_print(&s);
    printf("\n%d\n%lu\n", s.sso, sizeof(str));
    print_hex(cast(s), sizeof(str), 8);

    str_destroy_stk(&s);


    return 0;
}
