// A5 regression: the typed realloc macro must compile when used.
// Built by ctest with -fsyntax-only -Werror; never linked or run.
#include "wc_allocator.h"

int a5_realloc_n_use(void);

int a5_realloc_n_use(void)
{
    wc_allocator a = WC_LIBC;
    int*         p = WC_NEW_N(a, int, 4);
    p              = WC_REALLOC_N(a, int, p, 4, 8);
    WC_DELETE_N(a, int, p, 8);
    return 0;
}
