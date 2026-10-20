#ifndef WC_VIEWS_H
#define WC_VIEWS_H

#include "chain_arena.h"
#include "common.h"
#include "hashmap.h"
#include "wc_allocator.h"
#include "wc_comp.h"
#include "wc_hash.h"
#include "wc_string.h"

#include <stdio.h>
#include <string.h>


// TODO: expand views


// non-owning view into any memory that stores a string
//
// `.data == NULL` means invalid: functions that can fail return (StrView){0}.
// A successful view may still be empty (`len == 0`); in that case `data` stays
// non-NULL, so StrView_is_valid separates "empty" from "error".
typedef struct {
    const char* data;
    u64         len;
} StrView;

wc_nodiscard static wc_always_inline bool StrView_is_valid(StrView sv)
{
    return sv.data != NULL;
}

// creation

// Create a non-owning StrView over an already existing string.
// An empty String has a NULL data pointer internally; substitute a static empty
// string so the resulting view stays valid.
wc_nodiscard wc_nonull(1) static wc_always_inline StrView StrView_over_String(const String* str)
{
    const char* data = String_data_ptr(str);
    return (StrView){.data = data ? data : "", .len = String_len(str)};
}

// Sub-range of a String. Returns (StrView){0} if the range is out of bounds.
wc_nodiscard wc_nonull(1) static wc_always_inline StrView StrView_over_String_ex(const String* str, u64 off, u64 len)
{
    u64         slen = String_len(str);
    const char* data = String_data_ptr(str);
    if (off > slen || len > slen - off) {
        return (StrView){0};
    }
    return (StrView){.data = (data ? data : "") + off, .len = len};
}

// Non-owning view over a NUL-terminated C string. The length is computed
// internally with strlen.
wc_nodiscard wc_nonull(1) static wc_always_inline StrView StrView_over_cstr(const char* cstr)
{
    return (StrView){.data = cstr, .len = strlen(cstr)};
}

// conversion

// Allocate a new String using `a` with content from `sv`.
wc_nodiscard wc_nonull(1) String StrView_to_String(const wc_allocator* a, StrView sv); // one alloc
// Allocate a cstr (null terminated) with content from `sv`. Caller must free using wc_free with the same allocator
wc_nodiscard wc_nonull(1) char* StrView_to_cstr(const wc_allocator* a, StrView sv);
// copy data from `sv` into `buf`. returns false if buf too small. NOT null terminated
wc_nodiscard wc_nonull(2) bool StrView_to_buf(StrView sv, char* buf, u64 cap);

// slicing

// Get a sub-view over another view. Returns (StrView){0} if the range is out of
// bounds (overflow-safe). Slicing an invalid view stays invalid.
wc_nodiscard static wc_always_inline StrView StrView_slice(StrView sv, u64 start, u64 len)
{
    if (!sv.data || start > sv.len || len > sv.len - start) {
        return (StrView){0};
    }
    return (StrView){.data = sv.data + start, .len = len};
}
// Get the first n chars from `sv`. Returns (StrView){0} if n > sv.len.
wc_nodiscard static wc_always_inline StrView StrView_prefix(StrView sv, u64 n)
{
    return StrView_slice(sv, 0, n);
}
// Get all the chars starting from index `start` till the end.
// Returns (StrView){0} if start > sv.len.
wc_nodiscard static wc_always_inline StrView StrView_suffix(StrView sv, u64 start)
{
    if (start > sv.len) {
        return (StrView){0};
    }
    return StrView_slice(sv, start, sv.len - start);
}

// trim

// trim all chars from the NUL-terminated `set` from left and right of `sv`.
// leave `set = NULL` to trim whitespaces.
wc_nodiscard StrView StrView_trim(StrView sv, const char* set);
// trim all chars from the NUL-terminated `set` from the left of `sv`.
// leave `set = NULL` to trim whitespaces.
wc_nodiscard StrView StrView_trim_left(StrView sv, const char* set);
// trim all chars from the NUL-terminated `set` from the right of `sv`.
// leave `set = NULL` to trim whitespaces.
wc_nodiscard StrView StrView_trim_right(StrView sv, const char* set);

// search

wc_nodiscard u64  StrView_find(StrView sv, StrView needle);  // WC_NOT_FOUND if absent
wc_nodiscard u64  StrView_findr(StrView sv, StrView needle); // WC_NOT_FOUND if absent
wc_nodiscard u64  StrView_find_char(StrView sv, char c);
wc_nodiscard u64  StrView_findr_char(StrView sv, char c);
wc_nodiscard bool StrView_starts_with(StrView sv, StrView pfx);
wc_nodiscard bool StrView_ends_with(StrView sv, StrView sfx);
wc_nodiscard bool StrView_contains(StrView sv, StrView n);



// Splitting a view is just prefix/suffix on a value: nothing is mutated.
//   StrView head = StrView_prefix(sv, n);
//   sv           = StrView_suffix(sv, n);
// Both return (StrView){0} when n/start is past the end.


// output

static inline void StrView_print(StrView sv)
{
    for (u64 i = 0; i < sv.len; i++) {
        putchar(sv.data[i]);
    }
}

// hashing / comparison

wc_nodiscard static wc_always_inline u64 StrView_hash(StrView sv)
{
    return wc_hash(sv.data, sv.len);
}

wc_nodiscard static wc_always_inline int StrView_compare(StrView a, StrView b)
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

wc_nodiscard static wc_always_inline bool StrView_equals(StrView a, StrView b)
{
    return (a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0)) != 0;
}

// C-string convenience
wc_nodiscard wc_nonull(2) static wc_always_inline bool StrView_equals_cstr(StrView sv, const char* cstr)
{
    return StrView_equals(sv, StrView_over_cstr(cstr));
}

// Hash/compare wrappers for StrView-keyed maps and sets. The map stores the
// 16-byte view; these functions hash and compare the bytes it points at.
static inline u64 wc_strview_hash(const void* key, u64 size)
{
    (void)size;
    StrView v;
    memcpy(&v, key, sizeof(v));
    return StrView_hash(v);
}

static inline int wc_strview_cmp(const void* a, const void* b, u64 size)
{
    (void)size;
    return StrView_compare(*(const StrView*)a, *(const StrView*)b);
}


// A stringstore is a chain arena that provides immutable string storage and operations on them,
// filepath storage and operations etc. it gives out StrViews, optional String wrappers for read, find operations.
// NOTE: All ORIGINAL StrViews from the store are NUL-terminated. However, that's not gonna hold for subviews obtained via trim/slice
typedef struct {
    ChainArena store;
    HashMap    interned;         // StrView -> {} (a set)
    bool       interning_active; // optional
    // probe the interned set: return the existing view if found, else alloc + insert. A deduped store must be append-only
    // This makes interning O(1) amortized and idempotent: repeated StringStore_cstr(&ss, "x") return the same view every time
} StringStore;

wc_nonull(1, 2) void StringStore_create(StringStore* ss, const wc_allocator* alloc, u64 block_size, bool intern);
wc_nonull(1) void StringStore_destroy(StringStore* ss);

// Copy the bytes into the store; return a view. NUL-terminate too (1 extra byte),
// so sv.data works as a C string, makes StringStore a drop-in for argv-style APIs.
wc_nodiscard wc_nonull(1, 2) StrView StringStore_str(StringStore* ss, const String* s);
wc_nodiscard wc_nonull(1, 2) StrView StringStore_cstr(StringStore* ss, const char* cstr);
wc_nodiscard wc_nonull(1) StrView StringStore_sv(StringStore* ss, StrView sv);

// TODO: extensive testing needed
// filepath storage/handling (currently linux only)
// store paths normally as strings in the stringstore (or wherever) and use these operations to work on them
// We assume actual filepaths are passed to these functions, we are not asserting filepath validity. Check WCpath_valid to assert path validity.

// A path is valid if it is a non-NULL, non-empty view with no embedded NUL and no "//".
// This is the contract the other WCpath_* functions assume. "/", "a/b/" and "../x" are valid.
wc_nodiscard bool WCpath_valid(StrView p);

wc_nodiscard bool    WCpath_is_absolute(StrView p); // starts with '/'
wc_nodiscard StrView WCpath_basename(StrView p);    // "/a/b/c.txt" -> "c.txt"
wc_nodiscard StrView WCpath_dirname(StrView p);     // "/a/b/c.txt" -> "/a/b"; "a" -> valid empty
wc_nodiscard StrView WCpath_extension(StrView p);   // "c.txt"      -> "txt"   (no dot)
wc_nodiscard StrView WCpath_stem(StrView p);        // "c.txt" -> "c"; "Makefile" -> "Makefile"
wc_nodiscard bool    WCpath_has_extension(StrView p, StrView ext);

// Component iteration
typedef struct {
    StrView src;
    u64     pos;
} WCPathIter;

wc_nodiscard WCPathIter WCpath_components(StrView p);
// Return the next component, or (StrView){0} when the path is exhausted.
// Components are never empty: runs of '/' (and leading/trailing ones) are skipped.
wc_nodiscard wc_nonull(1) StrView WCpathIter_next(WCPathIter* it);

// Path joining (Allocation-returning)

// Join paths `a` and `b` and return a String allocated using `alloc`
wc_nodiscard wc_nonull(1) String WCPath_join_str(const wc_allocator* alloc, StrView a, StrView b);
// Join paths `a` and `b` and return a StrView stored in `ss`
wc_nodiscard wc_nonull(1) StrView WCPath_join_sv(StringStore* ss, StrView a, StrView b);

wc_nodiscard wc_nonull(1) StrView WCPath_normalize(StringStore* ss, StrView p); // collapses "." and ".."



#endif // WC_VIEWS_H
