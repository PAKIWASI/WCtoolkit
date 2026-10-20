#include "common.h"
#include "views.h"
#include "wc_allocator.h"
#include <stdio.h>



// TODO:
// iterators?
// msvc compatibility? or do we just use clang cl for compiling projects that use WCtoolkit
// we should not use raw attributes but use the WC_MACRO equivilants


int main(void)
{
    StringStore ss;
    StringStore_create(&ss, WC_LIBC, nKB(4), false);

    const char* c1 = "/a/b/c.txt";
    StrView p1 = StringStore_cstr(&ss, c1);
    WC_ASSERT(WCpath_valid(p1), "path not valid");
    WC_ASSERT(WCpath_is_absolute(p1), "path not absolute");

    StrView_print(WCpath_basename(p1)); putchar('\n');
    StrView_print(WCpath_dirname(p1)); putchar('\n');
    StrView_print(WCpath_extension(p1)); putchar('\n');
    StrView_print(WCpath_stem(p1)); putchar('\n');

    // StringStore_destroy(&ss); // BUG: hangs when commented out when alloc is WC_LIBC
    return 0;
}
