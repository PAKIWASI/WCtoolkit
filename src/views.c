#include "views.h"
#include "String.h"
#include "arena.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


strview strview_from_string(String* str)
{
    return (strview){.ptr = string_data_ptr(str), .len = string_len(str)};
}

strview strview_from_string_explicit(String* str, u64 off, u64 len)
{
    CHECK_FATAL(off + len >= string_len(str), "invalid range");
    return (strview){.ptr = string_data_ptr(str) + off, .len = len};
}

strview strview_cstr_arena(Arena* a, const char* cstr, u64 clen)
{
    char* p = ARENA_ALLOC_N(a, char, clen + 1); // for NULL Terminator
    memcpy(p, cstr, clen + 1);
    return (strview){.ptr = p, .len = clen};
}

void strview_print(strview sv)
{
    putchar('\"');
    for (u64 i = 0; i < sv.len; i++) {
        putchar(sv.ptr[i]);
    }
    putchar('\"');
    putchar('\n');
}



#define TAIL_BUF_OFF(ss) ((ss)->tail->buf + (ss)->tail_off)

void string_store_create(string_store* ss)
{
    string_store_node* node = malloc(sizeof(string_store_node));
    CHECK_FATAL(!node, "node malloc failed");

    node->next   = NULL;
    ss->head     = node;
    ss->tail     = node;
    ss->tail_off = 0;
    ss->num      = 1;
}

void string_store_destroy(string_store* ss)
{
    if (!ss || !ss->head) {
        return;
    }

    string_store_node* curr = ss->head;
    string_store_node* next = NULL;
    do {
        next = curr->next;
        free(curr);
        curr = next;
    } while (curr);
}

static inline void add_node(string_store* ss)
{
    string_store_node* node = malloc(sizeof(string_store_node));
    ss->tail->next          = node;
    ss->tail                = node;
    ss->tail_off            = 0;
    ss->num++;
}

strview string_store_cstr(string_store* ss, const char* cstr, u64 clen)
{
    CHECK_FATAL(!ss, "ss is null");
    CHECK_FATAL(!cstr, "ss is null");

    if (STRING_STORE_NODE_SIZE - ss->tail_off < clen) {
        // we need another node
        add_node(ss);
    }

    char* ptr = TAIL_BUF_OFF(ss);
    memcpy(ptr, cstr, clen);
    return (strview){.ptr = ptr, .len = clen};
}
