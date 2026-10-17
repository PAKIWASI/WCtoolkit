#include "common.h"
#include "views.h"
#include "wc_allocator.h"
#include <stdio.h>
#include <string.h>



// TODO:
// iterators?
// msvc compatibility? or do we just use clang cl for compiling projects that use WCtoolkit
// we should not use raw attributes but use the WC_MACRO equivilants


int main(void)
{
    StringStore ss;
    StringStore_create(&ss, WC_MMAP, nKB(4), false);

    const char* c1 = "/a/b/c.txt";
    u64 len1 = strlen(c1);
    StrView p1 = StringStore_cstr(&ss, c1, len1);
    WC_ASSERT(WCpath_valid(p1), "path not valid");
    WC_ASSERT(WCpath_is_absolute(p1), "path not absolute");

    StrView_print(WCpath_basename(p1)); putchar('\n');
    StrView_print(WCpath_dirname(p1)); putchar('\n');
    StrView_print(WCpath_extension(p1)); putchar('\n');
    StrView_print(WCpath_stem(p1)); putchar('\n');


    StringStore_destroy(&ss);
    return 0;
}
