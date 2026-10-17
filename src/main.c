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
    StringStore ss;
    StringStore_create(&ss, WC_MMAP, nKB(4), false);


    return 0;
}
