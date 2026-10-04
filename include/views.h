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

StrView StrView_from_String(String* str) __attribute__((nonnull(1)));

StrView StrView_from_String_ex(String* str, u64 off, u64 len) __attribute__((nonnull(1)));

// View over existing memory. No allocation; `cstr` must outlive the view.
StrView StrView_from_cstr(const char* cstr, u64 clen) __attribute__((nonnull(1)));

// Copy clen bytes of `cstr` into memory from `a` (plus a NUL terminator) and
// view it. LIFETIME: the view borrows from `a`. With an arena it lives until
// the arena is reset/destroyed; with any freeing allocator (libc, test
// allocator) release it with StrView_free_copy using the same allocator.
StrView StrView_copy_cstr(const wc_allocator* a, const char* cstr, u64 clen) __attribute__((nonnull(1, 2)));

// Release a view returned by StrView_copy_cstr(a, ...). No-op on arenas.
void StrView_free_copy(const wc_allocator* a, StrView sv) __attribute__((nonnull(1)));

void StrView_print(StrView sv);



#define StringStore_NODE_SIZE 1015 // + 1 + 8 = 1024


typedef struct StringStore_node {
    union {
        char buf[StringStore_NODE_SIZE + 1]; // last byte 0 -> overflow `heap` is active
        struct {
            char* heap;     // overflow buffer, allocated from the store's allocator
            u64   heap_len; // its size, needed to free it through wc_free
        };
    };
    struct StringStore_node* next;
} StringStore_node;

// append-only, immutable String storage with a chain Arena-like backing
// you get StrViews over the immutable Strings
// Nodes and overflow buffers come from `alloc`, which the store keeps (32 bytes).
// Views stay valid until StringStore_destroy. Zeroed store is dead: only
// destroy or re-create it; StringStore_cstr on it is an unconditional FATAL.
typedef struct {
    StringStore_node*   tail;
    StringStore_node*   head;
    const wc_allocator* alloc;
    u32                 tail_off; // how much of the tail node is used
    u32                 num;      // total number of nodes
} StringStore;

_Static_assert(sizeof(StringStore) == 32, "StringStore layout: 2 ptrs + allocator ptr + 2 u32");

StringStore StringStore_create(const wc_allocator* a) __attribute__((nonnull(1), warn_unused_result));
// Frees every node through ss->alloc and zeroes the struct. Safe on zeroed stores.
void StringStore_destroy(StringStore* ss) __attribute__((nonnull(1)));

StrView StringStore_cstr(StringStore* ss, const char* cstr, u64 clen) __attribute__((nonnull(1, 2)));

// Store sv1 followed by sv2 as one new string. Either view may point into
// this same store: existing data never moves.
StrView StringStore_append(StringStore* ss, StrView sv1, StrView sv2) __attribute__((nonnull(1)));
// Store sv followed by clen bytes of cstr as one new string.
StrView StringStore_append_cstr(StringStore* ss, StrView sv, const char* cstr, u64 clen) __attribute__((nonnull(1, 3)));


#endif // WC_VIEWS_H
