#include "views.h"
#include "arena.h"
#include "common.h"
#include "wc_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


StrView StrView_from_String(String* str)
{
    return (StrView){.ptr = String_data_ptr(str), .len = String_len(str)};
}

StrView StrView_from_String_explicit(String* str, u64 off, u64 len)
{
    CHECK_FATAL(off + len >= String_len(str), "invalid range");
    return (StrView){.ptr = String_data_ptr(str) + off, .len = len};
}

StrView StrView_cstr_Arena(Arena* a, const char* cstr, u64 clen)
{
    char* p = ARENA_ALLOC_N(a, char, clen + 1); // for NULL Terminator
    memcpy(p, cstr, clen + 1);
    return (StrView){.ptr = p, .len = clen};
}

void StrView_print(StrView sv)
{
    for (u64 i = 0; i < sv.len; i++) {
        putchar(sv.ptr[i]);
    }
}



#define TAIL_BUF_OFF(ss) ((ss)->tail->buf + (ss)->tail_off)

void StringStore_create(StringStore* ss)
{
    StringStore_node* node = malloc(sizeof(StringStore_node));
    CHECK_FATAL(!node, "node malloc failed");

    node->next      = NULL;
    node->owns_heap = 0;
    ss->head        = node;
    ss->tail        = node;
    ss->tail_off    = 0;
    ss->num         = 1;
}

void StringStore_destroy(StringStore* ss)
{
    if (!ss || !ss->head) {
        return;
    }

    StringStore_node* curr = ss->head;
    StringStore_node* next = NULL;
    do {
        next = curr->next;
        StringStore_destroy_node(curr);
        curr = next;
    } while (curr);
}

static inline void add_node(StringStore* ss)
{
    StringStore_node* node = malloc(sizeof(StringStore_node));
    CHECK_FATAL(!node, "node malloc failed");

    node->next              = NULL; // must terminate the chain for StringStore_destroy
    node->owns_heap         = 0;
    ss->tail->next          = node;
    ss->tail                = node;
    ss->tail_off            = 0;
    ss->num++;
}

StrView StringStore_cstr(StringStore* ss, const char* cstr, u64 clen)
{
    // Strings larger than a whole node go into a dedicated overflow node that
    // owns its buffer via the `heap` union member (malloc'd, node is head).
    // NOTE: the returned view is NOT NUL-terminated for these; only ever hand
    // (ptr, len) or memcpy out of it.
    if (clen > StringStore_NODE_SIZE) {
        StringStore_node* node = malloc(sizeof(StringStore_node));
        CHECK_FATAL(!node, "node malloc failed");

        node->heap      = malloc(clen);
        node->owns_heap = 1; // `heap` is live; StringStore_destroy must free it
        CHECK_FATAL(!node->heap, "overflow node malloc failed");

        memcpy(node->heap, cstr, clen);

        node->next = ss->head; // chain continues from the overflow node
        ss->head   = node;
        ss->num++;

        // Fresh empty tail so the old tail's data stays intact — its remaining
        // free space is abandoned (append-only store, rare path, acceptable).
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


void StringStore_destroy_node(StringStore_node* node)
{
    if (!node) {
        return;
    }

    if (node->owns_heap) {
        free(node->heap);
    }
    free(node);
}


