#include "views.h"
#include "String.h"
#include "arena.h"
#include "common.h"

#include <stdio.h>
#include <string.h>


strview strview_from_string(String* str)
{
    return (strview){.ptr = string_data_ptr(str), .len = string_len(str)};
}

strview strview_from_string_explicit(String* str, u64 off, u64 len)
{
    CHECK_FATAL(off + len >= string_len(str), "invalid range");
    return (strview){.ptr = string_data_ptr(str) + off, .len = len};
}

strview strview_cstr_arena(Arena* a, const char* cstr, u64 clen)
{
    char* p = ARENA_ALLOC_N(a, char, clen + 1); // for NULL Terminator
    memcpy(p, cstr, clen + 1);
    return (strview){.ptr = p, .len = clen};
}

void strview_print(strview sv)
{
    putchar('\"');
    for (u64 i = 0; i < sv.len; i++) {
        putchar(sv.ptr[i]);
    }
    putchar('\"');
    putchar('\n');
}

