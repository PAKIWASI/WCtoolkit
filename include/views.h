#ifndef WC_VIEWS_H
#define WC_VIEWS_H

#include "String.h"
#include "arena.h"
#include "common.h"


// NOT COPYABLE: non-owning view into an arena or StringStore.
typedef struct {
    const char* ptr;
    u64         len;
} StrView;

StrView StrView_from_string(String* str) __attribute__((nonnull(1)));

StrView StrView_from_string_explicit(String* str, u64 off, u64 len) __attribute__((nonnull(1)));

// allocate a cstr to an arena and return a view over it
// kinda like an append only store
StrView StrView_cstr_arena(Arena* a, const char* cstr, u64 clen) __attribute__((nonnull(1, 2)));

void StrView_print(StrView sv);



#define StringStore_NODE_SIZE 1024


typedef struct StringStore_node {
    union {
        char  buf[StringStore_NODE_SIZE];
        char* heap;
    };
    struct StringStore_node* next;
    // 1 → `heap` is live (overflow node), 0 → `buf` is live. Without this flag
    // StringStore_destroy cannot tell which union member to free.
    int owns_heap;
} StringStore_node;

// append-only, immutable string storage with a chain arena-like backing
// you get StrViews over the immutable strings
typedef struct {
    StringStore_node* tail;
    StringStore_node* head;
    u32                tail_off; // how much of th tail node is used
    u32                num;      // total number of nodes
} StringStore;

void StringStore_create(StringStore* ss) __attribute__((nonnull(1)));

void StringStore_destroy(StringStore* ss);

StrView StringStore_cstr(StringStore* ss, const char* cstr, u64 clen) __attribute__((nonnull(1, 2)));

// Free a single node (and its heap buffer, if it owns one). Exported so the
// overflow nodes created inside StringStore_cstr can be destroyed explicitly.
void StringStore_destroy_node(StringStore_node* node);

#endif // WC_VIEWS_H
