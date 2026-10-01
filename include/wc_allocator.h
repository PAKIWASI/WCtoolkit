#ifndef WC_ALLOCATOR_H
#define WC_ALLOCATOR_H

#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define WC_MAX_ALIGN alignof(max_align_t)


/* libc fallbacks */

static inline void* wc_libc_alloc(size_t n, size_t align)
{
    if (align <= WC_MAX_ALIGN) {
        return malloc(n);
    }

    size_t rounded = (n + align - 1) & ~(align - 1); // aligned_alloc: size % align == 0
    if (rounded < n) {
        return NULL; // overflow
    }
#ifdef _MSC_VER
    return _aligned_malloc(rounded, align);
#else
    return aligned_alloc(align, rounded);
#endif
}

static inline void wc_libc_free(void* p, size_t align)
{
#ifdef _MSC_VER
    if (align > WC_MAX_ALIGN) {
        _aligned_free(p); // MSVC can't free() these
        return;
    }
#endif
    (void)align;
    free(p); // POSIX/glibc: fine for both
}

static inline void* wc_libc_realloc(void* p, size_t old_n, size_t n, size_t align)
{
    if (align <= WC_MAX_ALIGN) {
        return realloc(p, n);
    }

    // libc realloc doesn't preserve alignment above max_align_t: alloc, copy, free
    void* q = wc_libc_alloc(n, align);
    if (!q) {
        return NULL; // p left untouched
    }
    memcpy(q, p, old_n < n ? old_n : n);
    wc_libc_free(p, align);
    return q;
}



/*
 * ALLOCATOR API
 *
 * No global allocator exists: every data structure receives its allocator at
 * construction, stores it, and uses it for every allocation.
 *
 * An allocator is a (vtable, ctx) pair, 16 bytes, passed and stored BY VALUE.
 * A zero-initialised allocator is libc: `WC_LIBC`.
 *
 * Contract (the wc_* wrappers enforce it, backends may rely on it):
 *   - callbacks are never called with p == NULL or size == 0
 *   - `align` is a power of two >= 1 and is an ADDRESS alignment
 *   - `free` / `realloc` receive the exact size and align of the block's last
 *     alloc/realloc (wc_test_alloc verifies this; arenas ignore it)
 *   - `realloc` failure returns NULL and leaves p valid
 *   - missing `realloc` is emulated (alloc + copy + free); missing `free` is a no-op
 *   - wc_alloc(a, 0, ..) returns NULL without calling the backend
 *   - wc_realloc(a, NULL, 0, n, ..) == wc_alloc(a, n, ..)
 *   - wc_realloc(a, p, old, 0, ..) frees p and returns NULL
 *
*/

typedef struct {
    void* (*alloc)(void* ctx, size_t size, size_t align);
    void* (*realloc)(void* ctx, void* p, size_t old_size, size_t new_size, size_t align); // may be NULL
    void (*free)(void* ctx, void* p, size_t size, size_t align);                          // may be NULL
} wc_alloc_vtable;

typedef struct {
    const wc_alloc_vtable* vt;  // NULL => libc
    void*                  ctx; // Arena*, ChainArena*, test allocator, ...
} wc_allocator;

_Static_assert(sizeof(wc_allocator) == 16, "wc_allocator must stay 16 bytes (stored by value in every container)");

#define WC_LIBC ((wc_allocator){0})

// Non-owning allocator: alloc -> NULL, free -> no-op. A container built on it
// wraps caller memory, cannot grow, and frees nothing on destroy.
extern const wc_allocator wc_borrowed;

static inline int wc_is_libc(wc_allocator a)
{
    return a.vt == NULL;
}

static inline int wc_same(wc_allocator a, wc_allocator b)
{
    return a.vt == b.vt && a.ctx == b.ctx;
}

#define WC_ALLOC_ASSERT_ALIGN(align) \
    assert((align) != 0 && ((align) & ((align) - 1)) == 0 && "align must be a power of two >= 1")

static inline void* wc_alloc(wc_allocator a, size_t n, size_t align)
{
    WC_ALLOC_ASSERT_ALIGN(align);
    if (n == 0 || n > (size_t)PTRDIFF_MAX) { // > PTRDIFF_MAX: overflowed size (see wc_mul)
        return NULL;
    }
    return a.vt ? a.vt->alloc(a.ctx, n, align) : wc_libc_alloc(n, align);
}

static inline void wc_free(wc_allocator a, void* p, size_t n, size_t align)
{
    WC_ALLOC_ASSERT_ALIGN(align);
    if (!p) {
        return;
    }
    if (!a.vt) {
        wc_libc_free(p, align);
        return;
    }
    if (a.vt->free) {
        a.vt->free(a.ctx, p, n, align);
    }
}

static inline void* wc_realloc(wc_allocator a, void* p, size_t old_n, size_t n, size_t align)
{
    WC_ALLOC_ASSERT_ALIGN(align);
    if (!p) {
        return wc_alloc(a, n, align);
    }
    if (n == 0) {
        wc_free(a, p, old_n, align);
        return NULL;
    }
    if (n > (size_t)PTRDIFF_MAX) {
        return NULL; // overflowed size: fail, p stays valid
    }
    if (!a.vt) {
        return wc_libc_realloc(p, old_n, n, align);
    }
    if (a.vt->realloc) {
        return a.vt->realloc(a.ctx, p, old_n, n, align);
    }

    // backend without realloc: emulate
    void* q = a.vt->alloc(a.ctx, n, align);
    if (!q) {
        return NULL; // p left untouched
    }
    memcpy(q, p, old_n < n ? old_n : n);
    wc_free(a, p, old_n, align);
    return q;
}

// count * size, saturating to SIZE_MAX on overflow. SIZE_MAX can never be
// satisfied, so an overflowing request fails in the backend instead of wrapping
// to a small size. (Returning 0 would be wrong: realloc to 0 means free.)
static inline size_t wc_mul(size_t count, size_t size)
{
    size_t r;
    return (int)__builtin_mul_overflow(count, size, &r) ? SIZE_MAX : r;
}

// Smallest safe alignment for elements of `elm_size` bytes (plan D7): the
// largest power of two dividing elm_size, capped at WC_MAX_ALIGN. Valid because
// sizeof(T) is always a multiple of alignof(T).
static inline size_t wc_align_for_size(size_t elm_size)
{
    if (elm_size == 0) {
        return 1;
    }
    size_t lowbit = elm_size & (~elm_size + 1);
    return lowbit < WC_MAX_ALIGN ? lowbit : WC_MAX_ALIGN;
}


/* typed helpers (allocator first) */

#define WC_NEW(a, T)            ((T*)wc_alloc((a), sizeof(T), alignof(T)))
#define WC_NEW_N(a, T, n)       ((T*)wc_alloc((a), wc_mul((n), sizeof(T)), alignof(T)))
#define WC_DELETE(a, T, p)      wc_free((a), (p), sizeof(T), alignof(T))
#define WC_DELETE_N(a, T, p, n) wc_free((a), (p), wc_mul((n), sizeof(T)), alignof(T))
#define WC_REALLOC_N(a, T, p, old_n, n) \
    ((T*)wc_realloc((a), (p), wc_mul((old_n), sizeof(T)), wc_mul((n), sizeof(T)), alignof(T)))



#endif // WC_ALLOCATOR_H
