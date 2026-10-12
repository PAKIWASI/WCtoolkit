#ifndef WC_VIEWS_H
#define WC_VIEWS_H

#include "common.h"
#include "wc_allocator.h"
#include "wc_string.h"


// non-owning view into any memory that stores a string
typedef struct {
    const char* ptr;
    u64         len;
} StrView;

StrView StrView_from_String(String* str) WC_NONULL(1);

StrView StrView_from_String_ex(String* str, u64 off, u64 len) WC_NONULL(1);

// View over existing memory. No allocation; `cstr` must outlive the view.
StrView StrView_from_cstr(const char* cstr, u64 clen) WC_NONULL(1);

// Copy clen bytes of `cstr` into memory from `a` (plus a NUL terminator) and
// view it. LIFETIME: the view borrows from `a`. With an arena it lives until
// the arena is reset/destroyed; with any freeing allocator (libc, test
// allocator) release it with StrView_free_copy using the same allocator.
StrView StrView_copy_cstr(const wc_allocator* a, const char* cstr, u64 clen) WC_NONULL(1, 2);

// Release a view returned by StrView_copy_cstr(a, ...). No-op on arenas.
void StrView_free_copy(const wc_allocator* a, StrView sv) WC_NONULL(1);

void StrView_print(StrView sv);


// TODO: use chain arena for a string store



#endif // WC_VIEWS_H
