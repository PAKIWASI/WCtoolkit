#include "views.h"
#include "common.h"
#include "wc_allocator.h"
#include "wc_string.h"

#include <stdalign.h>
#include <stdio.h>
#include <string.h>


StrView StrView_from_String(String* str)
{
    return (StrView){.ptr = String_data_ptr(str), .len = String_len(str)};
}

StrView StrView_from_String_ex(String* str, u64 off, u64 len)
{
    WC_ASSERT(off + len <= String_len(str), "invalid range");
    return (StrView){.ptr = String_data_ptr(str) + off, .len = len};
}

StrView StrView_from_cstr(const char* cstr, u64 clen)
{
    return (StrView){.ptr = cstr, .len = clen};
}

StrView StrView_copy_cstr(const wc_allocator* a, const char* cstr, u64 clen)
{
    char* p = wc_alloc(a, clen + 1, 1); // +1 for the NUL terminator
    FATAL_IF(!p, "StrView_copy_cstr: allocation of %llu bytes failed", (unsigned long long)clen + 1);
    memcpy(p, cstr, clen);
    p[clen] = '\0'; // terminate explicitly: cstr[clen] may not be NUL
    return (StrView){.ptr = p, .len = clen};
}

void StrView_free_copy(const wc_allocator* a, StrView sv)
{
    if (sv.ptr) {
        wc_free(a, (char*)sv.ptr, sv.len + 1, 1); // our own allocation: dropping const is fine
    }
}

void StrView_print(StrView sv)
{
    for (u64 i = 0; i < sv.len; i++) {
        putchar(sv.ptr[i]);
    }
}



#define TAIL_BUF_OFF(ss) ((ss)->tail->buf + (ss)->tail_off)


#define NODE_ALIGN alignof(StringStore_node)

static StringStore_node* new_node(const wc_allocator* a)
{
    StringStore_node* node = wc_alloc(a, sizeof(StringStore_node), NODE_ALIGN);
    FATAL_IF(!node, "StringStore: node allocation failed");
    node->next                       = NULL; // must terminate the chain for StringStore_destroy
    node->buf[StringStore_NODE_SIZE] = 1;    // heap inactive
    return node;
}


StringStore StringStore_create(const wc_allocator* a)
{
    StringStore_node* node = new_node(a);
    return (StringStore){.tail = node, .head = node, .alloc = a, .tail_off = 0, .num = 1};
}


static void StringStore_destroy_node(const wc_allocator* a, StringStore_node* node)
{
    if (node->buf[StringStore_NODE_SIZE] == 0) {
        wc_free(a, node->heap, node->heap_len, 1);
    }
    wc_free(a, node, sizeof(StringStore_node), NODE_ALIGN);
}


void StringStore_destroy(StringStore* ss)
{
    const wc_allocator* a    = ss->alloc; // read before zeroing
    StringStore_node*   curr = ss->head;
    while (curr) {
        StringStore_node* next = curr->next;
        StringStore_destroy_node(a, curr);
        curr = next;
    }
    memset(ss, 0, sizeof(*ss));
}

static inline void add_node(StringStore* ss)
{
    StringStore_node* node = new_node(ss->alloc);
    ss->tail->next         = node;
    ss->tail               = node;
    ss->tail_off           = 0;
    ss->num++;
}


// Reserve `len` contiguous bytes in the store. Never moves existing data, so
// callers may copy from views that point into this same store.
static char* store_reserve(StringStore* ss, u64 len)
{
    FATAL_IF(!ss->tail, "StringStore used after destroy (zeroed store)");

    // Strings larger than a whole node go into a dedicated overflow node that
    // owns its buffer via the `heap` union member (from ss->alloc, node is head).
    // NOTE: views of these are NOT NUL-terminated; use (ptr, len).
    if (len > StringStore_NODE_SIZE) {
        StringStore_node* node = new_node(ss->alloc);

        node->heap = (char*)wc_alloc(ss->alloc, len, 1);
        FATAL_IF(!node->heap, "StringStore: overflow allocation of %llu bytes failed", (unsigned long long)len);
        node->heap_len                   = len;
        node->buf[StringStore_NODE_SIZE] = 0; // `heap` is live; StringStore_destroy must free it

        node->next = ss->head; // chain continues from the overflow node
        ss->head   = node;
        ss->num++;

        // Fresh empty tail so the old tail's data stays intact, its remaining
        // free space is abandoned (append-only store, rare path, acceptable)
        add_node(ss);
        return node->heap;
    }

    if (StringStore_NODE_SIZE - ss->tail_off < len) {
        add_node(ss); // doesn't fit in the current tail
    }

    char* ptr = TAIL_BUF_OFF(ss);
    ss->tail_off += (u32)len;
    return ptr;
}


StrView StringStore_cstr(StringStore* ss, const char* cstr, u64 clen)
{
    char* p = store_reserve(ss, clen);
    memcpy(p, cstr, clen);
    return (StrView){.ptr = p, .len = clen};
}


StrView StringStore_append(StringStore* ss, StrView sv1, StrView sv2)
{
    char* p = store_reserve(ss, sv1.len + sv2.len);
    memcpy(p, sv1.ptr, sv1.len);
    memcpy(p + sv1.len, sv2.ptr, sv2.len);
    return (StrView){.ptr = p, .len = sv1.len + sv2.len};
}


StrView StringStore_append_cstr(StringStore* ss, StrView sv, const char* cstr, u64 clen)
{
    return StringStore_append(ss, sv, (StrView){.ptr = cstr, .len = clen});
}
