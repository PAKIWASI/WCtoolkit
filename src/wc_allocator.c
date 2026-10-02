#include "wc_allocator.h"
#include <stddef.h>
#include "wc_poison.h" // must stay last: bans raw malloc/free below



/* wc_borrowed: non-owning allocator (plan 3.2) */

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

// Positional on purpose: `realloc`/`free` are poisoned in library sources.
static const wc_alloc_vtable wc_borrowed_vt = {
    wc_borrowed_alloc, // alloc
    NULL,              // realloc: emulated; alloc fails, so growth fails and p stays valid
    wc_borrowed_free,  // free
};

const wc_allocator wc_borrowed = {.vt = &wc_borrowed_vt, .ctx = NULL};
