#ifndef WC_ALLOCATOR_H
#define WC_ALLOCATOR_H

#include <stddef.h>
#include <stdlib.h>


typedef struct {
    void* (*alloc)(size_t);
    void* (*realloc)(void*, size_t);
    void  (*free)(void*);
    void* ctx;
} wc_allocator;

wc_allocator wc_default_allocator = {
    .alloc   = malloc,
    .realloc = realloc,
    .free    = free,
    .ctx     = NULL,
};



#endif // WC_ALLOCATOR_H
