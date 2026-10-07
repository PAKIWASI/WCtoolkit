#include "wc_allocator.h"
#include "common.h"
#include <stddef.h>
#include <sys/mman.h>



/* wc_libc: the only allocator with vt == NULL */

const wc_allocator wc_libc = {.vt = NULL, .ctx = NULL};

/* wc_borrowed: non-owning allocator */

static void* wc_borrowed_alloc(void* ctx, size_t size, size_t align)
{
    (void)ctx;
    (void)size;
    (void)align;
    return NULL; // can't allocate with a borrowed object's allocator
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


/* wc_mmap: block allocator that lives for the whole duration of the program */

// we only need alloc. realloc will do the manual alloc then copy. There is no free
static void* wc_mmap_alloc(void* ctx, size_t size, size_t align)
{
    (void)ctx;
    // we need to align the size to the desired boundry. so that size % align == 0
    size_t rounded = (size + align - 1) & ~(align - 1);

    u8* base =
        mmap(NULL,                        // let kernel choose the virtual address
             rounded,                     // the allocation size
             PROT_READ | PROT_WRITE,      // mapping is readable and writable
             MAP_PRIVATE | MAP_ANONYMOUS, // MAP_PRIVATE: writes are copy-on-write, MAP_ANONYMOUS: not backed by a file
             -1,                          // -1 for MAP_ANONYMOUS
             0);                          // ignored for MAP_ANONYMOUS

    return base;
}

static const wc_alloc_vtable wc_mmap_vt = {
    .alloc   = wc_mmap_alloc,
    .realloc = NULL,
    .free    = NULL,
};

const wc_allocator wc_mmap = {.vt = &wc_mmap_vt, .ctx = NULL};


