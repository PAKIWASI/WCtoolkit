#include "wc_allocator.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>



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


/* wc_mmap: page-granular allocator backed by anonymous mmap
 *
 * Every allocation is its own mapping, so it costs one syscall and at least
 * one page. Use it for big, long-lived regions (an Arena reservation, large
 * ChainArena nodes), not for small blocks.
 *
 *   - failure returns NULL
 *   - any power-of-two alignment is honoured, including above the page size
 *   - fresh pages are zero-filled by the kernel and committed lazily
 */

static size_t wc_page_size(void)
{
    static size_t cached;
    size_t        ps = cached;
    if (ps == 0) {
        long v = sysconf(_SC_PAGESIZE);
        ps     = v > 0 ? (size_t)v : 4096;
        cached = ps;
    }
    return ps;
}

static void* wc_mmap_alloc(void* ctx, size_t size, size_t align)
{
    (void)ctx;
    const size_t page = wc_page_size();

    if (size > SIZE_MAX - page) { // page rounding would overflow
        return NULL;
    }

    if (align <= page) { // mmap already returns page-aligned addresses
        void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return p == MAP_FAILED ? NULL : p;
    }

    // align > page size: over-map, then unmap the unaligned head and the unused tail
    if (size > SIZE_MAX - page - align) {
        return NULL;
    }
    const size_t len = (size + page - 1) & ~(page - 1); // what the caller keeps
    const size_t big = len + align;
    uint8_t*          raw = mmap(NULL, big, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) {
        return NULL;
    }

    uint8_t*    aligned = (uint8_t*)(((uintptr_t)raw + align - 1) & ~((uintptr_t)align - 1));
    size_t head    = (size_t)(aligned - raw);
    size_t tail    = big - head - len;
    if (head) {
        munmap(raw, head);
    }
    if (tail) {
        munmap(aligned + len, tail);
    }
    return aligned;
}

static void wc_mmap_free(void* ctx, void* p, size_t size, size_t align)
{
    (void)ctx;
    (void)align;
    munmap(p, size); // the kernel rounds size up to whole pages, same as at alloc
}

static const wc_alloc_vtable wc_mmap_vt = {
    .alloc   = wc_mmap_alloc,
    .realloc = NULL, // emulated: alloc + copy + free
    .free    = wc_mmap_free,
};

const wc_allocator wc_mmap = {.vt = &wc_mmap_vt, .ctx = NULL};
