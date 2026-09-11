#ifndef WC_VIEWS_H
#define WC_VIEWS_H

#include "chain_arena.h"
#include "common.h"
#include "arena.h"
#include "String.h"


typedef struct {
    const char* ptr;
    u64         len;
} strview;

strview strview_from_string(String* str);

strview strview_from_string_explicit(String* str, u64 off, u64 len);

// allocate a cstr to an arena and return a view over it
// kinda like an append only store
strview strview_cstr_arena(Arena* a, const char* cstr, u64 clen);

void strview_print(strview sv);




// append-only, immutable string storage with chain arena backing
// you get strviews over the immutable strings
typedef struct {
    ChainArena arena;
} string_store;

string_store* string_store_create(u64 cap)
{

}




#endif // WC_VIEWS_H
