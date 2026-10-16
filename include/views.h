#ifndef WC_VIEWS_H
#define WC_VIEWS_H

#include "chain_arena.h"
#include "common.h"
#include "hashmap.h"
#include "wc_allocator.h"
#include "wc_hash.h"
#include "wc_string.h"
#include <stdio.h>
#include <string.h>


// TODO: expand views


// non-owning view into any memory that stores a string
typedef struct {
    const char* data;
    u64         len;
} StrView;

// creation

// Create a non-owning StrView over an already existing string
WC_NODISCARD WC_NONULL(1) static inline StrView StrView_over_String(const String* str)
{
    return (StrView){.data = String_data_ptr(str), .len = String_len(str)};
}

WC_NODISCARD WC_NONULL(1) static inline StrView StrView_over_String_ex(const String* str, u64 off, u64 len)
{
    WC_ASSERT(off + len <= String_len(str), "invalid range");
    return (StrView){.data = String_data_ptr(str) + off, .len = len};
}

WC_NODISCARD WC_NONULL(1) static inline StrView StrView_over_cstr(const char* cstr, u64 clen)
{
    return (StrView){.data = cstr, .len = clen};
}

// conversion

// Allocate a new String using `a` with content from `sv`.
WC_NODISCARD String StrView_to_String(const wc_allocator* a, StrView sv) WC_NONULL(1); // one alloc
// Allocate a cstr (null terminated) with content from `sv`. Caller must free using wc_free with the same allocator
WC_NODISCARD char* StrView_to_cstr(const wc_allocator* a, StrView sv) WC_NONULL(1);
// copy data from `sv` into `buf`. returns false if buf too small. NOT null terminated
WC_NODISCARD bool StrView_to_buf(StrView sv, char* buf, u64 cap) WC_NONULL(2);

// slicing

// Get a sub-view over another view
WC_NODISCARD static inline StrView StrView_slice(StrView sv, u64 start, u64 len)
{
    WC_ASSERT(start <= sv.len && len <= sv.len - start, "out of range"); // overflow-safe
    return (StrView){.data = sv.data + start, .len = len};
}
// Get the first n chars from `sv`
WC_NODISCARD static inline StrView StrView_prefix(StrView sv, u64 n)
{
    return StrView_slice(sv, 0, n);
}
// Get all the chars starting from index `start` till the end
WC_NODISCARD static inline StrView StrView_suffix(StrView sv, u64 start)
{
    WC_ASSERT(start <= sv.len, "out of range");
    return StrView_slice(sv, start, sv.len - start);
}

// trim

// trim all chars from `set` from left and right of `sv`. leave `set = NUL` to trim whitespaces
WC_NODISCARD StrView StrView_trim(StrView sv, const char* set, u64 len);
// trim all chars from `set` from the left of `sv`. leave `set = NUL` to trim whitespaces
WC_NODISCARD StrView StrView_trim_left(StrView sv, const char* set, u64 len);
// trim all chars from `set` from the right of `sv`. leave `set = NUL` to trim whitespaces
WC_NODISCARD StrView StrView_trim_right(StrView sv, const char* set, u64 len);

// search

WC_NODISCARD u64 StrView_find(StrView sv, StrView needle); // WC_NOT_FOUND if absent
WC_NODISCARD u64 StrView_find_char(StrView sv, char c);
bool             StrView_starts_with(StrView sv, StrView pfx);
bool             StrView_ends_with(StrView sv, StrView sfx);
bool             StrView_contains(StrView sv, StrView n);

// parse

// Parse an unsigned decimal integer from the WHOLE view: digits only, at least one, leading zeros allowed.
// No sign, no whitespace, no base prefix. Returns false on empty input, any other character, or overflow
// of the target type. `*out` is only written on success.
WC_NODISCARD WC_NONULL(2) bool StrView_parse_u8(StrView sv, u8* out);
WC_NODISCARD WC_NONULL(2) bool StrView_parse_u16(StrView sv, u16* out);
WC_NODISCARD WC_NONULL(2) bool StrView_parse_u32(StrView sv, u32* out);
WC_NODISCARD WC_NONULL(2) bool StrView_parse_u64(StrView sv, u64* out);

// consumption: take n bytes from a view and shorten it (from the left or the right)

// A StrView with `.data == NULL` is invalid. The take functions return one to signal failure.
WC_NODISCARD static inline bool StrView_is_valid(StrView sv)
{
    return sv.data != NULL;
}

// Remove the first `n` bytes of `*sv` and return them. If `n > sv->len`, returns an invalid view
// (`.data == NULL`) and leaves `*sv` untouched. Ignore the result to just skip bytes.
WC_NODISCARD StrView StrView_take_left(StrView* sv, u64 n) WC_NONULL(1);
// Remove the last `n` bytes of `*sv` and return them. If `n > sv->len`, returns an invalid view
// (`.data == NULL`) and leaves `*sv` untouched. Ignore the result to just drop bytes.
WC_NODISCARD StrView StrView_take_right(StrView* sv, u64 n) WC_NONULL(1);


// output

static inline void StrView_print(StrView sv)
{
    for (u64 i = 0; i < sv.len; i++) {
        putchar(sv.data[i]);
    }
}

// hashing / comparison

WC_NODISCARD static inline u64 StrView_hash(StrView sv)
{
    return wc_hash(sv.data, sv.len);
}

WC_NODISCARD static inline int StrView_compare(StrView a, StrView b)
{
    u64 min = a.len < b.len ? a.len : b.len;
    if (min) {
        int c = memcmp(a.data, b.data, min);
        if (c) {
            return c;
        }
    }
    return (a.len > b.len) - (a.len < b.len); // sign of len difference
}

static inline bool StrView_equals(StrView a, StrView b)
{
    return (a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0)) != 0;
}

// C-string convenience
WC_NONULL(2) static inline bool StrView_equals_cstr(StrView sv, const char* cstr)
{
    return StrView_equals(sv, StrView_over_cstr(cstr, strlen(cstr)));
}


// TODO: do this

// A stringstore is a chain arena that provides immutable string storage and operations on them,
// filepath storage and operations etc. it gives out StrViews, optional String wrappers for read, find operations.
// all strings in the store are null terminated
typedef struct {
    ChainArena store;
    HashMap*   interned;         // StrView -> {} (a set)
    bool       interning_active; // optional
    // probe the interned set: return the existing view if found, else alloc + insert. A deduped store must be append-only
    // This makes interning O(1) amortized and idempotent, repeated StringStore_store_cstr(&ss, "x") return the same view every time
} StringStore;

WC_NONULL(1, 2) void StringStore_create(StringStore* ss, const wc_allocator* backing, u64 block_cap, bool intern);
WC_NONULL(1) void StringStore_destroy(StringStore* ss);

// Copy bytes into the store; return a view. NUL-terminate too (1 extra byte),
// so sv.data works as a C string, makes StringStore a drop-in for argv-style APIs.
WC_NODISCARD WC_NONULL(1, 2) StrView StringStore_store(StringStore* ss, const String* s);
WC_NODISCARD WC_NONULL(1, 2) StrView StringStore_store_cstr(StringStore* ss, const char* cstr);
WC_NODISCARD WC_NONULL(1) StrView StringStore_store_sv(StringStore* ss, StrView sv);



// filepath storage/handling
// store paths normally as strings in the stringstore (or wherever) and use these operations to work on them

bool                 WCpath_is_absolute(StrView p); // starts with '/'
WC_NODISCARD StrView WCpath_basename(StrView p);    // "/a/b/c.txt" -> "c.txt"
WC_NODISCARD StrView WCpath_dirname(StrView p);     // "/a/b/c.txt" -> "/a/b"
WC_NODISCARD StrView WCpath_extension(StrView p);   // "c.txt"      -> "txt"   (no dot)
WC_NODISCARD StrView WCpath_stem(StrView p);        // "c.txt"      -> "c"
bool                 WCpath_has_extension(StrView p, StrView ext);

// Component iteration
typedef struct {
    StrView src;
    u64     pos;
} WCPathIter;

WC_NODISCARD WCPathIter WCpath_components(StrView p);
bool                    WCpathIter_next(WCPathIter* it, StrView* out);

// Allocation-returning
WC_NODISCARD String  Path_join(const wc_allocator* alloc, StrView a, StrView b);
WC_NODISCARD StrView Path_join_sv(StringStore* ss, StrView a, StrView b);
WC_NODISCARD StrView Path_normalize(StringStore* ss, StrView p); // collapses "." and ".."



#endif // WC_VIEWS_H
