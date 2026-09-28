#ifndef WC_VIEWS_H
#define WC_VIEWS_H

#include "arena.h"
#include "common.h"
#include "wc_string.h"


// non-owning view into any memory that stores a string
typedef struct {
    const char* ptr;
    u64         len;
} StrView;

StrView StrView_from_String(String* str) __attribute__((nonnull(1)));

StrView StrView_from_String_ex(String* str, u64 off, u64 len) __attribute__((nonnull(1)));

// return a view into a cstr.
// if arena, is passed, allocate the cstr into it and return it's view
StrView StrView_from_cstr(const char* cstr, u64 clen, Arena* a) __attribute__((nonnull(1)));

void StrView_print(StrView sv);



#define StringStore_NODE_SIZE 1015 // + 1 + 8 = 1024


typedef struct StringStore_node {
    union {
        char  buf[StringStore_NODE_SIZE + 1]; // last bit is NULL (0) -> heap is active
        char* heap;
    };
    struct StringStore_node* next;
} StringStore_node;

// append-only, immutable String storage with a chain Arena-like backing
// you get StrViews over the immutable Strings
typedef struct {
    StringStore_node* tail;
    StringStore_node* head;
    u32               tail_off; // how much of th tail node is used
    u32               num;      // total number of nodes
} StringStore;

void StringStore_create(StringStore* ss) __attribute__((nonnull(1)));
void StringStore_destroy(StringStore* ss);

StrView StringStore_cstr(StringStore* ss, const char* cstr, u64 clen) __attribute__((nonnull(1, 2)));

// TODO:
StrView StringStore_append(StringStore* ss, StrView sv1, StrView sv2);
StrView StringStore_append_cstr(StringStore* ss, StrView sv, const char* cstr, u64 clen);
// pass in StrViews...
StrView StringStore_path_join(StringStore* ss, ...);
// pass in path1, len1, path2, len2,...
StrView StringStore_path_join_cstr(StringStore* ss, ...);

#endif // WC_VIEWS_H
