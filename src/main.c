#include "common.h"
#include "views.h"
#include "wc_allocator.h"
#include "wc_string.h"
#include <stdio.h>
#include <string.h>



// TODO:
// iterators?
// msvc compatibility? or do we just use clang cl for compiling projects that use WCtoolkit
// we should not use raw attributes but use the WC_MACRO equivilants


int main(void)
{
    String s = String_from_cstr(WC_LIBC, "$      hello world  @ ");

    StrView_print(StrView_over_String(&s)); putchar('\n');

    const char* set = "$@ ";
    u64 len = strlen(set);

    StrView_print(StrView_trim_left(StrView_over_String(&s), set, len)); putchar('\n');

    StrView_print(StrView_trim_right(StrView_over_String(&s), set, len)); putchar('\n');

    StrView_print(StrView_trim(StrView_over_String(&s), set, len)); putchar('\n');

    String_destroy(&s);
    return 0;
}
