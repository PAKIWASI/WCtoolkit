#include "views.h"
#include "common.h"
#include "wc_allocator.h"
#include "wc_string.h"

#include <stdalign.h>
#include <stdint.h>
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

// all ascii chars that are always trimmable (whitespace: \t \n \v \f \r and space)
static const char trimmable_ascii[] = {9, 10, 11, 12, 13, 32};

// The default chars are always active. Every char of `set` is then made active on top of them.
// A NULL set or len 0 adds nothing, leaving just the defaults
static void trim_table_fill(bool active[256], const char* set, u64 len)
{
    for (u64 i = 0; i < sizeof(trimmable_ascii); i++) {
        active[(u8)trimmable_ascii[i]] = true;
    }
    if (set == NULL) {
        return;
    }
    for (u64 i = 0; i < len; i++) {
        active[(u8)set[i]] = true;
    }
}

static StrView trim_left_table(StrView sv, const bool active[256])
{
    u64 to_skip = 0;
    while (to_skip < sv.len && active[(u8)sv.data[to_skip]]) {
        to_skip++;
    }
    return (StrView){.data = sv.data + to_skip, .len = sv.len - to_skip};
}

static StrView trim_right_table(StrView sv, const bool active[256])
{
    u64 len_reduced = sv.len;
    while (len_reduced > 0 && active[(u8)sv.data[len_reduced - 1]]) {
        len_reduced--;
    }
    return (StrView){.data = sv.data, .len = len_reduced};
}

StrView StrView_trim(StrView sv, const char* set, u64 len)
{
    bool active_trim[256] = {false};
    trim_table_fill(active_trim, set, len); // built once, shared by both sides
    return trim_right_table(trim_left_table(sv, active_trim), active_trim);
}

StrView StrView_trim_left(StrView sv, const char* set, u64 len)
{
    bool active_trim[256] = {false};
    trim_table_fill(active_trim, set, len);
    return trim_left_table(sv, active_trim);
}

StrView StrView_trim_right(StrView sv, const char* set, u64 len)
{
    bool active_trim[256] = {false};
    trim_table_fill(active_trim, set, len);
    return trim_right_table(sv, active_trim);
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
    if (sv.len == 0) {
        return WC_NOT_FOUND;
    }
    const char* p = memchr(sv.data, (unsigned char)c, sv.len);
    return p ? (u64)(p - sv.data) : WC_NOT_FOUND;
}

bool StrView_starts_with(StrView sv, StrView pfx)
{
    if (pfx.len > sv.len) {
        return false;
    }
    return (pfx.len == 0 || memcmp(sv.data, pfx.data, pfx.len) == 0) != 0;
}

bool StrView_ends_with(StrView sv, StrView sfx)
{
    if (sfx.len > sv.len) {
        return false;
    }
    return (sfx.len == 0 || memcmp(sv.data + sv.len - sfx.len, sfx.data, sfx.len) == 0) != 0;
}

bool StrView_contains(StrView sv, StrView n)
{
    return StrView_find(sv, n) != WC_NOT_FOUND;
}


// parse

// FIX: slow as fuck
//
// Shared decimal parser: whole view must be digits, value must be <= max
static bool parse_uint_max(StrView sv, u64 max, u64* out)
{
    if (sv.len == 0) {
        return false;
    }
    u64 v = 0;
    for (u64 i = 0; i < sv.len; i++) {
        u32 d = (u32)(sv.data[i] - '0'); // wraps to >9 for non-digits
        if (d > 9) {
            return false;
        }
        if (v > (max - d) / 10) { // v * 10 + d would exceed max
            return false;
        }
        v = (v * 10) + d;
    }
    *out = v;
    return true;
}

bool StrView_parse_u8(StrView sv, u8* out)
{
    u64 v;
    if (!parse_uint_max(sv, UINT8_MAX, &v)) {
        return false;
    }
    *out = (u8)v;
    return true;
}

bool StrView_parse_u16(StrView sv, u16* out)
{
    u64 v;
    if (!parse_uint_max(sv, UINT16_MAX, &v)) {
        return false;
    }
    *out = (u16)v;
    return true;
}

bool StrView_parse_u32(StrView sv, u32* out)
{
    u64 v;
    if (!parse_uint_max(sv, UINT32_MAX, &v)) {
        return false;
    }
    *out = (u32)v;
    return true;
}

bool StrView_parse_u64(StrView sv, u64* out)
{
    return parse_uint_max(sv, UINT64_MAX, out);
}

// consumption

StrView StrView_take_left(StrView* sv, u64 n)
{
    if (n > sv->len) {
        return (StrView){0}; // invalid
    }
    StrView head = {.data = sv->data, .len = n};
    sv->data += n;
    sv->len -= n;
    return head;
}

StrView StrView_take_right(StrView* sv, u64 n)
{
    if (n > sv->len) {
        return (StrView){0}; // invalid
    }
    StrView tail = {.data = sv->data + (sv->len - n), .len = n};
    sv->len -= n;
    return tail;
}

