#include "wc_allocator.h"
#include <stddef.h>



/* wc_libc: the only allocator with vt == NULL */

const wc_allocator wc_libc = {.vt = NULL, .ctx = NULL};


/* wc_borrowed: non-owning allocator */

static void* wc_borrowed_alloc(void* ctx, size_t size, size_t align)
{
    (void)ctx;
    (void)size;
    (void)align;
    return NULL;
}

static void wc_borrowed_free(void* ctx, void* p, size_t size, size_t align)
{
    (void)ctx;
    (void)p;
    (void)size;
    (void)align;
}

static const wc_alloc_vtable wc_borrowed_vt = {
    .alloc   = wc_borrowed_alloc,
    .realloc = NULL, // emulated: alloc fails, so growth fails and p stays valid
    .free    = wc_borrowed_free,
};

const wc_allocator wc_borrowed = {.vt = &wc_borrowed_vt, .ctx = NULL};
