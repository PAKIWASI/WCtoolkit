#ifndef WC_VIEWS_H
#define WC_VIEWS_H

#include "chain_arena.h"
#include "common.h"
#include "wc_string.h"

// TODO: expand views and build an immutable string store using chain arena


// non-owning view into any memory that stores a string
typedef struct {
    const char* ptr;
    u64         len;
} StrView;


StrView StrView_from_String(String* str) WC_NONULL(1);

StrView StrView_from_String_ex(String* str, u64 off, u64 len) WC_NONULL(1);

// View over existing memory. No allocation; `cstr` must outlive the view.
StrView StrView_from_cstr(const char* cstr, u64 clen) WC_NONULL(1);

void StrView_print(StrView sv);


// A stringstore is a chain arena that provides immutable string storage and operations on them,
// filepath storage and operations etc. it gives out StrViews, optional String wrappers for read, find operations.
//
typedef ChainArena StringStore;



#endif // WC_VIEWS_H
