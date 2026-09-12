#ifndef WC_VIEWS_H
#define WC_VIEWS_H

#include "String.h"
#include "arena.h"
#include "common.h"


// NOT COPYABLE: non-owning view into an arena or string_store.
typedef struct {
    const char* ptr;
    u64         len;
} strview;

strview strview_from_string(String* str) __attribute__((nonnull(1)));

strview strview_from_string_explicit(String* str, u64 off, u64 len) __attribute__((nonnull(1)));

// allocate a cstr to an arena and return a view over it
// kinda like an append only store
strview strview_cstr_arena(Arena* a, const char* cstr, u64 clen) __attribute__((nonnull(1, 2)));

void strview_print(strview sv);



#define STRING_STORE_NODE_SIZE 1024


typedef struct string_store_node {
    union {
        char  buf[STRING_STORE_NODE_SIZE];
        char* heap;
    };
    struct string_store_node* next;
} string_store_node;

// append-only, immutable string storage with a chain arena-like backing
// you get strviews over the immutable strings
typedef struct {
    string_store_node* tail;
    string_store_node* head;
    u32                tail_off; // how much of th tail node is used
    u32                num;      // total number of nodes
} string_store;

void string_store_create(string_store* ss) __attribute__((nonnull(1)));

void string_store_destroy(string_store* ss);

strview string_store_cstr(string_store* ss, const char* cstr, u64 clen) __attribute__((nonnull(1, 2)));

#endif // WC_VIEWS_H
