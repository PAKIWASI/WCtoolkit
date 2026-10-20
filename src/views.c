#include "views.h"
#include "chain_arena.h"
#include "common.h"
#include "hashmap.h"
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
    if (sv.len) {
        memcpy(data, sv.data, sv.len);
    }
    data[sv.len] = '\0';
    return data;
}

bool StrView_to_buf(StrView sv, char* buf, u64 cap)
{
    if (sv.len > cap) { // buffer too small
        return false;
    }

    if (sv.len) {
        memcpy(buf, sv.data, sv.len);
    }
    return true;
}

// all ascii chars that are always trimmable (whitespace: \t \n \v \f \r and space)
static const char trimmable_ascii[] = {9, 10, 11, 12, 13, 32};

// The default chars are always active. Every char of `set` is then made active on top of them.
// A NULL set adds nothing, leaving just the defaults. `set` is NUL-terminated.
static void trim_table_fill(bool active[256], const char* set)
{
    for (u64 i = 0; i < sizeof(trimmable_ascii); i++) {
        active[(u8)trimmable_ascii[i]] = true;
    }
    if (set == NULL) {
        return;
    }
    u64 len = strlen(set);
    for (u64 i = 0; i < len; i++) {
        active[(u8)set[i]] = true;
    }
}

static StrView trim_left_table(StrView sv, const bool active[256])
{
    if (!sv.data) {
        return (StrView){0};
    }
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

StrView StrView_trim(StrView sv, const char* set)
{
    bool active_trim[256] = {false};
    trim_table_fill(active_trim, set); // built once, shared by both sides
    return trim_right_table(trim_left_table(sv, active_trim), active_trim);
}

StrView StrView_trim_left(StrView sv, const char* set)
{
    bool active_trim[256] = {false};
    trim_table_fill(active_trim, set);
    return trim_left_table(sv, active_trim);
}

StrView StrView_trim_right(StrView sv, const char* set)
{
    bool active_trim[256] = {false};
    trim_table_fill(active_trim, set);
    return trim_right_table(sv, active_trim);
}

u64 StrView_find(StrView sv, StrView needle)
{
    if (needle.len > sv.len) {
        return WC_NOT_FOUND;
    }
    for (u64 i = 0; i <= sv.len - needle.len; i++) {
        if (memcmp(sv.data + i, needle.data, needle.len) == 0) {
            return i;
        }
    }
    return WC_NOT_FOUND;
}

u64 StrView_findr(StrView sv, StrView needle)
{
    if (needle.len > sv.len) {
        return WC_NOT_FOUND;
    }
    // i --> 0 tests the old value, then decrements, so the body sees sv.len - needle.len ... 0
    for (u64 i = sv.len - needle.len + 1; i-- > 0;) {
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
    const char* p = memchr(sv.data, c, sv.len);
    return p ? (u64)(p - sv.data) : WC_NOT_FOUND;
}

u64 StrView_findr_char(StrView sv, char c)
{
    // memrchr not working?
    for (u64 i = sv.len; i-- > 0;) {
        if (sv.data[i] == c) {
            return i;
        }
    }
    return WC_NOT_FOUND;
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


// String Store

// Copy `sv` into the store and NUL-terminate it. With interning on, return the
// already-stored view for equal contents instead of allocating a duplicate.
static StrView stringstore_copy(StringStore* ss, StrView sv)
{
    if (!sv.data) {
        return (StrView){0};
    }
    if (ss->interning_active) {
        const StrView* found = (const StrView*)HashMap_get_key_ptr(&ss->interned, &sv);
        if (found) {
            return *found;
        }
    }

    char* data = ChainArena_alloc(&ss->store, sv.len + 1);
    FATAL_IF(!data, "chain arena alloc failed");
    if (sv.len) {
        memcpy(data, sv.data, sv.len);
    }
    data[sv.len] = 0;

    StrView stored = {.data = data, .len = sv.len};
    if (ss->interning_active) {
        HashMap_put(&ss->interned, &stored, NULL);
    }
    return stored;
}

void StringStore_create(StringStore* ss, const wc_allocator* alloc, u64 block_size, bool intern)
{
    ChainArena_create(&ss->store, alloc, block_size);
    ss->interned         = (HashMap){0};
    ss->interning_active = intern;
    if (intern) {
        // A set of StrViews. The keys are non-owning; the chain arena owns the bytes.
        ss->interned = HashMap_create(alloc, sizeof(StrView), 0, wc_strview_hash, wc_strview_cmp, NULL, NULL);
    }
}

void StringStore_destroy(StringStore* ss)
{
    if (ss->interning_active) {
        HashMap_destroy(&ss->interned);
    }
    ChainArena_destroy(&ss->store);
}

StrView StringStore_str(StringStore* ss, const String* s)
{
    return stringstore_copy(ss, StrView_over_String(s));
}

StrView StringStore_cstr(StringStore* ss, const char* cstr)
{
    return stringstore_copy(ss, StrView_over_cstr(cstr));
}

StrView StringStore_sv(StringStore* ss, StrView sv)
{
    return stringstore_copy(ss, sv);
}



// Filepaths

bool WCpath_valid(StrView p)
{
    if (!p.data || p.len == 0) {
        return false;
    }
    for (u64 i = 0; i < p.len; i++) {
        if (p.data[i] == '\0') { // would truncate the path as a C string
            return false;
        }
        // more than one '/' together
        if (p.data[i] == '/' && i + 1 < p.len && p.data[i + 1] == '/') {
            return false;
        }
    }
    return true;
}


// length with the trailing '/' removed, but a lone "/" stays
static u64 path_trimmed_len(StrView p)
{
    u64 n = p.len;
    if (n > 1 && p.data[n - 1] == '/') {
        n--;
    }
    return n;
}

bool WCpath_is_absolute(StrView p)
{
    return (p.len > 0 && p.data[0] == '/') != 0;
}

StrView WCpath_basename(StrView p)
{
    if (p.len == 0) {
        return (StrView){0};
    }
    u64 n = path_trimmed_len(p);
    if (n == 1 && p.data[0] == '/') {
        return StrView_prefix(p, 1); // "/" -> "/"
    }
    u64 i = n;
    while (i > 0 && p.data[i - 1] != '/') {
        i--;
    }
    return StrView_slice(p, i, n - i);
}

StrView WCpath_dirname(StrView p)
{
    if (!p.data || p.len == 0) {
        return (StrView){0};
    }

    u64 n = path_trimmed_len(p);
    if (n == 1 && p.data[0] == '/') {
        return StrView_prefix(p, 1);
    }

    u64 i = n;
    while (i > 0 && p.data[i - 1] != '/') { // skip basename
        i--;
    }
    // we reached the beginning but no '/' found: no directory component.
    // That is a successful result, not an error: return a valid empty view.
    if (i == 0) {
        return StrView_prefix(p, 0);
    }
    // drop the separator before the basename, keep a lone root "/"
    if (i > 1) {
        i--;
    }

    // we found a '/', everthing from the start (including root) till here is the dirname
    return StrView_prefix(p, i);
}

// index of the extension dot in a basename, or WC_NOT_FOUND. A leading dot
// doesn't count; "." and ".." have no extension.
static u64 ext_dot(StrView b)
{
    if (b.len == 2 && b.data[0] == '.' && b.data[1] == '.') {
        return WC_NOT_FOUND;
    }
    for (u64 i = b.len; i-- > 1;) {
        if (b.data[i] == '.') {
            return i;
        }
    }
    return WC_NOT_FOUND;
}

StrView WCpath_extension(StrView p)
{
    StrView b   = WCpath_basename(p);
    u64     dot = ext_dot(b);
    if (dot == WC_NOT_FOUND) {
        return (StrView){0};
    }
    return StrView_suffix(b, dot + 1);
}

StrView WCpath_stem(StrView p)
{
    StrView b   = WCpath_basename(p);
    u64     dot = ext_dot(b);
    return dot == WC_NOT_FOUND ? b : StrView_prefix(b, dot);
}

// `ext` may be given with or without the leading dot: "txt" and ".txt" both match "c.txt"
// Case-sensitive. An empty `ext` matches only paths that have no extension.
bool WCpath_has_extension(StrView p, StrView ext)
{
    if (ext.len > 0 && ext.data[0] == '.') {
        ext = StrView_suffix(ext, 1);
    }
    return StrView_equals(WCpath_extension(p), ext);
}


// component iteration

// Yields only real names. Empty components ("a//b", leading or trailing '/') are skipped.
// Check WCpath_is_absolute(p) separately if the root matters. "." and ".." are yielded as-is.
WCPathIter WCpath_components(StrView p)
{
    return (WCPathIter){.src = p, .pos = 0};
}

StrView WCpathIter_next(WCPathIter* it)
{
    StrView s = it->src;
    u64     i = it->pos;

    while (i < s.len && s.data[i] == '/') { // skip separator run
        i++;
    }
    if (i >= s.len) {
        it->pos = s.len;
        return (StrView){0};
    }

    u64 start = i;
    while (i < s.len && s.data[i] != '/') { // advance over the name
        i++;
    }
    it->pos = i;
    return (StrView){.data = s.data + start, .len = i - start};
}

// Join rules (POSIX / Python os.path.join style):
//   - `b` absolute          -> result is just `b`
//   - `a` empty             -> result is just `b`
//   - `b` empty             -> result is just `a` (no trailing '/' added)
//   - otherwise             -> a + '/' + b, with the '/' omitted if `a` already ends in one
typedef struct {
    bool use_a;
    bool sep;
} JoinPlan;

static JoinPlan join_plan(StrView a, StrView b)
{
    // can't join with an absolute path
    if (a.len == 0 || WCpath_is_absolute(b)) {
        return (JoinPlan){.use_a = false, .sep = false};
    }
    bool sep = (b.len > 0 && a.data[a.len - 1] != '/') != 0;
    return (JoinPlan){.use_a = true, .sep = sep};
}

static u64 join_len(JoinPlan plan, StrView a, StrView b)
{
    return (plan.use_a ? a.len : 0) + (plan.sep ? 1 : 0) + b.len;
}

// writes exactly join_len(plan, a, b) bytes, no NUL
static void join_write(char* dst, JoinPlan plan, StrView a, StrView b)
{
    u64 n = 0;
    if (plan.use_a && a.len) {
        memcpy(dst, a.data, a.len);
        n += a.len;
    }
    if (plan.sep) {
        dst[n++] = '/';
    }
    if (b.len) {
        memcpy(dst + n, b.data, b.len);
    }
}

String WCPath_join_str(const wc_allocator* alloc, StrView a, StrView b)
{
    JoinPlan plan = join_plan(a, b);
    u64      len  = join_len(plan, a, b);
    String   s    = String_with_cap(alloc, len);

    if (plan.use_a && a.len) {
        String_append_multi(&s, a.data, a.len);
    }
    if (plan.sep) {
        String_append_multi(&s, "/", 1);
    }
    if (b.len) {
        String_append_multi(&s, b.data, b.len);
    }
    return s;
}

StrView WCPath_join_sv(StringStore* ss, StrView a, StrView b)
{
    JoinPlan plan = join_plan(a, b);
    u64      len  = join_len(plan, a, b);

    char* data = ChainArena_alloc(&ss->store, len + 1);
    FATAL_IF(!data, "chain arena alloc failed");

    join_write(data, plan, a, b);
    data[len] = 0;
    return (StrView){.data = data, .len = len};
}

// Purely lexical (never touches the filesystem, so symlinks are not considered):
//   - duplicate and trailing '/' collapse     "a//b/"      -> "a/b"
//   - "." components are dropped              "a/./b"      -> "a/b"
//   - ".." pops the previous component        "a/b/../c"   -> "a/c"
//   - ".." at the root of an absolute path    "/../a"      -> "/a"
//   - leading ".." of a relative path stays   "../../a"    -> "../../a"
//   - empty result                            ""  "a/.."   -> "."   ;   "/.."  -> "/"
StrView WCPath_normalize(StringStore* ss, StrView p)
{
    // Output is never longer than the input, except the "." for an empty result. +1 for NUL.
    char* dst = ChainArena_alloc(&ss->store, p.len + 2);
    FATAL_IF(!dst, "chain arena alloc failed");

    bool absolute = WCpath_is_absolute(p);
    u64  n        = 0;
    if (absolute) {
        dst[n++] = '/';
    }
    // `keep` = length of the prefix that ".." must not pop: the root and any leading ".."s
    u64 keep = n;

    WCPathIter it = WCpath_components(p);
    for (StrView c = WCpathIter_next(&it); StrView_is_valid(c); c = WCpathIter_next(&it)) {
        if (StrView_equals_cstr(c, ".")) {
            continue;
        }

        if (StrView_equals_cstr(c, "..")) {
            if (n > keep) { // pop the last component
                while (n > keep && dst[n - 1] != '/') {
                    n--;
                }
                if (n > keep && dst[n - 1] == '/') { // drop the separator before it
                    n--;
                }
                continue;
            }
            if (absolute) { // "/.." == "/"
                continue;
            }
            // relative, nothing to pop: keep the ".." and protect it from later pops
            if (n > 0) {
                dst[n++] = '/';
            }
            dst[n++] = '.';
            dst[n++] = '.';
            keep     = n;
            continue;
        }

        if (n > 0 && dst[n - 1] != '/') {
            dst[n++] = '/';
        }
        memcpy(dst + n, c.data, c.len);
        n += c.len;
    }

    if (n == 0) {
        dst[n++] = '.';
    }
    dst[n] = 0;
    return (StrView){.data = dst, .len = n};
}



