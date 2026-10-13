#include "views.h"
#include "common.h"
#include "wc_allocator.h"
#include "wc_string.h"

#include <stdalign.h>
#include <string.h>


String StrView_to_String(const wc_allocator* a, StrView sv)
{
    String s = String_with_cap(a, sv.len);
    if (sv.len == 0) {
        return s;
    }

    String_append_multi(&s, sv.data, sv.len);
    return s;
}

char* StrView_to_cstr(const wc_allocator* a, StrView sv)
{
    char* data = wc_alloc(a, sv.len + 1, WC_MAX_ALIGN);
    memcpy(data, sv.data, sv.len);
    data[sv.len] = '\0';
    return data;
}

bool StrView_to_buf(StrView sv, char* buf, u64 cap)
{
    if (sv.len > cap) { // buffer too small
        return false;
    }

    memcpy(buf, sv.data, cap);
    return true;
}

StrView StrView_trim(StrView sv, const char* set, u64 len)
{
    return StrView_trim_right(StrView_trim_left(sv, set, len), set, len);
}

StrView StrView_trim_left(StrView sv, const char* set, u64 len)
{
    u64 to_skip = 0;
    if (len == 0) {
        while (sv.data[to_skip] == ' ') {
            to_skip++;
        }
    } else {
        for (u64 i = 0; i < sv.len; i++) {
            bool matched = false;
            for (u64 j = 0; j < len; j++) {
                matched = false;
                if (sv.data[to_skip] == set[j]) {
                    to_skip++;
                    matched = true;
                }
            }
            if (!matched) {
                break;
            }
        }
    }
    return (StrView){.data = sv.data + to_skip, .len = sv.len - to_skip};
}

StrView StrView_trim_right(StrView sv, const char* set, u64 len)
{
    u64 len_reduced = sv.len;
    if (len == 0) {
        while (sv.data[len_reduced - 1] == ' ') {
            len_reduced--;
        }
    } else {
        while (len_reduced > 0) {
            bool matched = false;
            for (u64 j = 0; j < len; j++) {
                matched = false;
                if (sv.data[len_reduced - 1] == set[j]) {
                    len_reduced--;
                    matched = true;
                }
            }
            if (!matched) {
                break;
            }
        }
    }
    return (StrView){.data = sv.data, .len = len_reduced};
}

u64 StrView_find(StrView sv, StrView needle)
{
    for (u64 i = 0; i < sv.len; i++) {
        if (memcmp(sv.data + i, needle.data, needle.len) == 0) {
            return i;
        }
    }
    return WC_NOT_FOUND;
}

u64 StrView_find_char(StrView sv, char c)
{

    const char* p = memchr(sv.data, (unsigned char)c, sv.len);
    return p ? (u64)(p - sv.data) : WC_NOT_FOUND;
}

bool StrView_starts_with(StrView sv, StrView pfx)
{
    return memcmp(sv.data, pfx.data, pfx.len) == 0;
}

bool StrView_ends_with(StrView sv, StrView sfx)
{
    return memcmp(sv.data + sv.len - sfx.len, sfx.data, sfx.len) == 0;
}

bool StrView_contains(StrView sv, StrView n)
{
    return StrView_find(sv, n) != WC_NOT_FOUND;
}






void StringStore_create(StringStore* ss, const wc_allocator* backing, u64 block_cap)
{

}
