#include "views.h"
#include "arena.h"
#include "common.h"
#include "priority_queue.h"
#include "wc_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


StrView StrView_from_String(String* str)
{
    return (StrView){.ptr = String_data_ptr(str), .len = String_len(str)};
}

StrView StrView_from_String_ex(String* str, u64 off, u64 len)
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
#define RANGE_CUTOFF 8 // range if len 8 or less will not be added to the free ranges priority queue when freed


// < 0 means a has higher priority than b
static int strview_larger_range_cmp(const u8* a, const u8* b, u64 size)
{
    (void)size;
    if (((StrView*)a)->len > ((StrView*)b)->len) {
        return -1; // a has higher priority
    }
    if (((StrView*)a)->len == ((StrView*)b)->len) {
        return 0; // equal priority
    }
    // a < b, b has higher priority
    return 1;
}


void StringStore_create(StringStore* ss)
{
    StringStore_node* node = malloc(sizeof(StringStore_node));
    CHECK_FATAL(!node, "node malloc failed");

    node->next                       = NULL;
    node->buf[StringStore_NODE_SIZE] = 1; // heap inactive
    ss->head                         = node;
    ss->tail                         = node;
    ss->tail_off                     = 0;
    ss->num                          = 1;

    PriorityQueue_create_stk(&ss->free_ranges, 10, sizeof(StrView), NULL, strview_larger_range_cmp);
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

    PriorityQueue_destroy_stk(&ss->free_ranges);
}

static inline void add_node(StringStore* ss)
{
    StringStore_node* node = malloc(sizeof(StringStore_node));
    CHECK_FATAL(!node, "node malloc failed");

    node->next                       = NULL; // must terminate the chain for StringStore_destroy
    node->buf[StringStore_NODE_SIZE] = 1;    // heap inactive
    ss->tail->next                   = node;
    ss->tail                         = node;
    ss->tail_off                     = 0;
    ss->num++;
}

// TODO: when we create a new node and some space is left over from the old one, add that to free_ranges
// first we check if cstr can fit in any free range: check how much free space will remain if we put cstr
// into the highest priority range, if 50% or more will remain, check it' children to see if it fits there
StrView StringStore_cstr(StringStore* ss, const char* cstr, u64 clen)
{
    // Strings larger than a whole node go into a dedicated overflow node that
    // owns its buffer via the `heap` union member (malloc'd, node is head).
    // NOTE: the returned view is NOT NUL-terminated for these; only ever hand
    // (ptr, len) or memcpy out of it.
    if (clen > StringStore_NODE_SIZE) {
        StringStore_node* node = malloc(sizeof(StringStore_node));
        CHECK_FATAL(!node, "node malloc failed");

        node->heap                       = malloc(clen);
        node->buf[StringStore_NODE_SIZE] = 0; // `heap` is live; StringStore_destroy must free it
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

    if (node->buf[StringStore_NODE_SIZE] == 0) {
        free(node->heap);
    }
    free(node);
}
