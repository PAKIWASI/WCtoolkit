#include "arena.h"
#include "common.h"
#include "views.h"
#include <string.h>


int main(void)
{
    Arena* a = arena_create(nKB(1));

    const char* cstr = "helllo";
    strview sv = strview_cstr_arena(a, cstr, strlen(cstr));
    ArenaScratch as = arena_scratch_begin(a);
    strview sv2 = strview_cstr_arena(a, cstr, strlen(cstr));
    strview_print(sv);
    strview_print(sv2);

    arena_scratch_end(as);

    const char* cstr2 = "worljkfj";
    strview sv3 = strview_cstr_arena(a, cstr2, strlen(cstr2));

    strview_print(sv);
    strview_print(sv2);
    strview_print(sv3);


    arena_release(a);
}
