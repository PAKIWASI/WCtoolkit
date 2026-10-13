#include "views.h"
#include "common.h"
#include "wc_string.h"

#include <stdalign.h>
#include <stdio.h>


StrView StrView_from_String(String* str)
{
    return (StrView){.ptr = String_data_ptr(str), .len = String_len(str)};
}

StrView StrView_from_String_ex(String* str, u64 off, u64 len)
{
    WC_ASSERT(off + len <= String_len(str), "invalid range");
    return (StrView){.ptr = String_data_ptr(str) + off, .len = len};
}

StrView StrView_from_cstr(const char* cstr, u64 clen)
{
    return (StrView){.ptr = cstr, .len = clen};
}

void StrView_print(StrView sv)
{
    for (u64 i = 0; i < sv.len; i++) {
        putchar(sv.ptr[i]);
    }
}
