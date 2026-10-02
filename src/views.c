#include "views.h"
#include "common.h"
#include "wc_allocator.h"
#include "wc_string.h"

#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


StrView StrView_from_String(String* str)
{
    return (StrView){.ptr = String_data_ptr(str), .len = String_len(str)};
}

StrView StrView_from_String_ex(String* str, u64 off, u64 len)
{
    CHECK_FATAL(off + len > String_len(str), "invalid range");
    return (StrView){.ptr = String_data_ptr(str) + off, .len = len};
}

StrView StrView_from_cstr(const char* cstr, u64 clen)
{
    return (StrView){.ptr = cstr, .len = clen};
}

StrView StrView_copy_cstr(wc_allocator a, const char* cstr, u64 clen)
{
    char* p = (char*)wc_alloc(a, clen + 1, 1); // +1 for the NUL terminator
    FATAL_IF(!p, "StrView_copy_cstr: allocation of %llu bytes failed", (unsigned long long)clen + 1);
    memcpy(p, cstr, clen);
    p[clen] = '\0'; // terminate explicitly: cstr[clen] may not be NUL
    return (StrView){.ptr = p, .len = clen};
}

void StrView_free_copy(wc_allocator a, StrView sv)
{
    if (sv.ptr) {
        wc_free(a, (void*)(uintptr_t)sv.ptr, sv.len + 1, 1);
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

static StringStore_node* new_node(wc_allocator a)
{
    StringStore_node* node = (StringStore_node*)wc_alloc(a, sizeof(StringStore_node), NODE_ALIGN);
    FATAL_IF(!node, "StringStore: node allocation failed");
    node->next                       = NULL; // must terminate the chain for StringStore_destroy
    node->buf[StringStore_NODE_SIZE] = 1;    // heap inactive
    return node;
}


StringStore StringStore_create(wc_allocator a)
{
    StringStore_node* node = new_node(a);
    return (StringStore){.tail = node, .head = node, .alloc = a, .tail_off = 0, .num = 1};
}


static void StringStore_destroy_node(wc_allocator a, StringStore_node* node)
{
    if (node->buf[StringStore_NODE_SIZE] == 0) {
        wc_free(a, node->heap, node->heap_len, 1);
    }
    wc_free(a, node, sizeof(StringStore_node), NODE_ALIGN);
}


void StringStore_destroy(StringStore* ss)
{
    wc_allocator      a    = ss->alloc; // read before zeroing
    StringStore_node* curr = ss->head;
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


StrView StringStore_cstr(StringStore* ss, const char* cstr, u64 clen)
{
    FATAL_IF(!ss->tail, "StringStore_cstr on zeroed/destroyed store");

    // Strings larger than a whole node go into a dedicated overflow node that
    // owns its buffer via the `heap` union member (from ss->alloc, node is head).
    // NOTE: the returned view is NOT NUL-terminated for these; only ever hand
    // (ptr, len) or memcpy out of it.
    if (clen > StringStore_NODE_SIZE) {
        StringStore_node* node = new_node(ss->alloc);

        node->heap = (char*)wc_alloc(ss->alloc, clen, 1);
        FATAL_IF(!node->heap, "StringStore: overflow allocation of %llu bytes failed", (unsigned long long)clen);
        node->heap_len                   = clen;
        node->buf[StringStore_NODE_SIZE] = 0; // `heap` is live; StringStore_destroy must free it

        memcpy(node->heap, cstr, clen);

        node->next = ss->head; // chain continues from the overflow node
        ss->head   = node;
        ss->num++;

        // Fresh empty tail so the old tail's data stays intact, its remaining
        // free space is abandoned (append-only store, rare path, acceptable)
        add_node(ss);
        return (StrView){.ptr = node->heap, .len = clen};
    }

    if (StringStore_NODE_SIZE - ss->tail_off < clen) {
        // we need another node
        add_node(ss);
    }

    char* ptr = TAIL_BUF_OFF(ss);
    memcpy(ptr, cstr, clen);
    ss->tail_off += (u32)clen;
    return (StrView){.ptr = ptr, .len = clen};
}



StrView StringStore_append(StringStore* ss, StrView sv1, StrView sv2)
{
}



