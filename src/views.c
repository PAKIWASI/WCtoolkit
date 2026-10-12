#include "views.h"
#include "common.h"
#include "wc_allocator.h"
#include "wc_string.h"

#include <stdalign.h>
#include <stdio.h>
#include <string.h>


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

StrView StrView_copy_cstr(const wc_allocator* a, const char* cstr, u64 clen)
{
    char* p = wc_alloc(a, clen + 1, 1); // +1 for the NUL terminator
    FATAL_IF(!p, "StrView_copy_cstr: allocation of %llu bytes failed", (unsigned long long)clen + 1);
    memcpy(p, cstr, clen);
    p[clen] = '\0'; // terminate explicitly: cstr[clen] may not be NUL
    return (StrView){.ptr = p, .len = clen};
}

void StrView_free_copy(const wc_allocator* a, StrView sv)
{
    if (sv.ptr) {
        wc_free(a, (char*)sv.ptr, sv.len + 1, 1); // our own allocation: dropping const is fine
    }
}

void StrView_print(StrView sv)
{
    for (u64 i = 0; i < sv.len; i++) {
        putchar(sv.ptr[i]);
    }
}
