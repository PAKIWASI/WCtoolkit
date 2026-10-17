#include "wc_allocator.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif



/* the global libc allocator */

static inline void* wc_libc_alloc(void* ctx, size_t n, size_t align)
{
    (void)ctx;
    if (align <= WC_MAX_ALIGN) {
        return malloc(n); // malloc promises it's result is aligned if for max_align_t (16 bytes)
    }

    // if more alignment is needed, we need aligned alloc
#ifdef _WIN32
    return _aligned_malloc(n, align);
#else
    size_t rounded = (n + align - 1) & ~(align - 1); // aligned_alloc: size % align == 0
    if (rounded < n) {
        return NULL; // overflow
    }
    return aligned_alloc(align, rounded);
#endif
}

static inline void wc_libc_free(void* ctx, void* p, size_t size, size_t align)
{
    (void)ctx;
    (void)size;
    (void)align;
#ifdef _WIN32
    if (align > WC_MAX_ALIGN) {
        _aligned_free(p); // pairs with _aligned_malloc above
        return;
    }
#endif
    free(p);
}

static inline void* wc_libc_realloc(void* ctx, void* p, size_t old_n, size_t n, size_t align)
{
    if (align <= WC_MAX_ALIGN) {
        return realloc(p, n); // realloc garentees 16-byte alinment, like malloc
    }

    // libc realloc doesn't preserve alignment above max_align_t: alloc, copy, free
    void* q = wc_libc_alloc(ctx, n, align);
    if (!q) {
        return NULL; // p left untouched
    }
    memcpy(q, p, old_n < n ? old_n : n);
    wc_libc_free(ctx, p, old_n, align);
    return q;
}

static const wc_alloc_vtable wc_libc_vt = {.alloc = wc_libc_alloc, .realloc = wc_libc_realloc, .free = wc_libc_free};
const wc_allocator           wc_libc    = {.vt = &wc_libc_vt, .ctx = NULL};


/* wc_borrowed: non-owning allocator */

static void* wc_borrowed_alloc(void* ctx, size_t size, size_t align)
{
    (void)ctx;
    (void)size;
    (void)align;
    return NULL; // can't allocate with a borrowed object's allocator. FAIL
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
#ifdef _WIN32
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        ps = si.dwPageSize > 0 ? (size_t)si.dwPageSize : 4096;
#else
        long v = sysconf(_SC_PAGESIZE);
        ps     = v > 0 ? (size_t)v : 4096;
#endif
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

#ifdef _WIN32
    if (align <= page) { // VirtualAlloc always returns allocation-granularity (64K) aligned pages
        return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); // NULL on failure
    }

    // align > page size: over-map, then release the unaligned head and the unused tail
    if (size > SIZE_MAX - page - align) {
        return NULL;
    }
    const size_t len = (size + page - 1) & ~(page - 1); // what the caller keeps
    const size_t big = len + align;
    uint8_t*     raw = VirtualAlloc(NULL, big, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (raw == NULL) {
        return NULL;
    }
    uint8_t* aligned = (uint8_t*)(((uintptr_t)raw + align - 1) & ~((uintptr_t)align - 1));
    size_t   head    = (size_t)(aligned - raw);
    size_t   tail    = big - head - len;
    if (head) {
        VirtualFree(raw, head, MEM_DECOMMIT); // pages become inaccessible, like munmap
    }
    if (tail) {
        VirtualFree(aligned + len, tail, MEM_DECOMMIT);
    }
    return aligned;
#else
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
    uint8_t*     raw = mmap(NULL, big, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) {
        return NULL;
    }

    uint8_t* aligned = (uint8_t*)(((uintptr_t)raw + align - 1) & ~((uintptr_t)align - 1));
    size_t   head    = (size_t)(aligned - raw);
    size_t   tail    = big - head - len;
    if (head) {
        munmap(raw, head);
    }
    if (tail) {
        munmap(aligned + len, tail);
    }
    return aligned;
#endif
}

static void wc_mmap_free(void* ctx, void* p, size_t size, size_t align)
{
    (void)ctx;
    (void)align;
#ifdef _WIN32
    (void)size;
    MEMORY_BASIC_INFORMATION mbi; // the caller's pointer is interiour to the over-mapped region
    if (VirtualQuery(p, &mbi, sizeof(mbi))) {
        VirtualFree(mbi.AllocationBase, 0, MEM_RELEASE); // releases the whole reservation, decommitted ends included
    }
#else
    munmap(p, size); // the kernel rounds size up to whole pages, same as at alloc
#endif
}

static const wc_alloc_vtable wc_mmap_vt = {
    .alloc   = wc_mmap_alloc,
    .realloc = NULL, // emulated: alloc + copy + free
    .free    = wc_mmap_free,
};

const wc_allocator wc_mmap = {.vt = &wc_mmap_vt, .ctx = NULL};
