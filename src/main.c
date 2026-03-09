

#include "new_str.h"
#include <stdio.h>
int main(void)
{
    str* str = str_create();

    for (u64 i = 0; i < 100; i++) {
        str_append_char(str, 'a');
    }

    str_print(str);
    printf("\n%d\n", str->sso);

    str_destroy(str);
    return 0;
}
