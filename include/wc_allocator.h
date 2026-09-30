#ifndef WC_ALLOCATOR_H
#define WC_ALLOCATOR_H

#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// TODO: test

/*
 * Allocator type for WCtoolkit: arbitrary ctx, explicit sizes and alignment.
 *
 * Callback contract (callbacks are never called with ptr == NULL, and
 * `align` is always a power of two >= WC_MAX_ALIGN):
 *   alloc  : return a block of `size` bytes aligned to `align`, or NULL
 *   realloc: on failure return NULL and leave ptr valid
 *   free   : never fails; `size`/`align` match the alloc/realloc that made ptr
 *
 * A custom allocator is defined by `alloc` being set. A missing `realloc` is
 * emulated (alloc + copy + free); a missing `free` is a no-op (arenas).
 * Zero-initialised == libc.
 */
typedef struct {
    void* (*alloc)(void* ctx, size_t size, size_t align);
    void* (*realloc)(void* ctx, void* ptr, size_t old_size, size_t new_size, size_t align);
    void (*free)(void* ctx, void* ptr, size_t size, size_t align);
    void* ctx;
} wc_allocator_t;

#ifndef WC_DEFAULT_ALLOCATOR
#define WC_DEFAULT_ALLOCATOR {0} // libc
#endif

/*
 * Global allocator (thread-local). Defaults to libc.
 *   compile time: -DWC_DEFAULT_ALLOCATOR=...
 *   runtime     : WC_SET_ALLOCATOR(allocator)   (before creating containers)
 * NOTE: thread-local: other threads start with WC_DEFAULT_ALLOCATOR, not with
 * the value set on the main thread.
 *
 * Simple API (uses the global):
 *   wc_alloc(n) | wc_realloc(p, old_n, n) | wc_free(p, n)
 *   wc_alloc_aligned(n, al) | wc_realloc_aligned(p, old_n, n, al) | wc_free_aligned(p, n, al)
 *   WC_NEW(T) | WC_NEW_N(T, n) | WC_DELETE(T, p) | WC_DELETE_N(T, p, n)
 *
 * Explicit API (for types that let the user pick an allocator):
 *   wc_alloc_ex(a, n, al) | wc_realloc_ex(a, p, old_n, n, al) | wc_free_ex(a, p, n, al)
 *   WC_NEW_IN(a, T) | WC_NEW_N_IN(a, T, n) | WC_DELETE_IN(a, T, p) | WC_DELETE_N_IN(a, T, p, n)
 *   `a == NULL` means "use the global at call time".
 *   Use &wc_libc_allocator to force libc regardless of the global.
 *
 * Over-aligned T (alignof(T) > WC_MAX_ALIGN): free with WC_DELETE*, not wc_free.
 */
extern _Thread_local wc_allocator_t wc_default_allocator;
extern const wc_allocator_t         wc_libc_allocator;

#define WC_MAX_ALIGN alignof(max_align_t)

#define WC_SET_ALLOCATOR(allocator) (wc_default_allocator = (allocator))


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


/* helpers */

#define WC_RESOLVE(a) ((a) ? (a) : &wc_default_allocator)

static inline size_t wc_norm_align(size_t align)
{
    assert(align != 0 && (align & (align - 1)) == 0 && "align must be a power of two");
    return align < WC_MAX_ALIGN ? WC_MAX_ALIGN : align;
}


/* explicit allocator API (a == NULL -> global) */

static inline void* wc_alloc_ex(const wc_allocator_t* a, size_t n, size_t align)
{
    a     = WC_RESOLVE(a);
    align = wc_norm_align(align);
    return a->alloc ? a->alloc(a->ctx, n, align) : wc_libc_alloc(n, align);
}

static inline void wc_free_ex(const wc_allocator_t* a, void* p, size_t n, size_t align)
{
    if (!p) {
        return;
    }
    a     = WC_RESOLVE(a);
    align = wc_norm_align(align);

    if (!a->alloc) {
        wc_libc_free(p, align);
        return;
    }
    if (a->free) { // custom allocator with no free == no-op (e.g. arenas)
        a->free(a->ctx, p, n, align);
    }
}

static inline void* wc_realloc_ex(const wc_allocator_t* a, void* p, size_t old_n, size_t n, size_t align)
{
    a     = WC_RESOLVE(a);
    align = wc_norm_align(align);

    if (!p) {
        return wc_alloc_ex(a, n, align); // realloc(NULL, n) == alloc
    }
    if (!a->alloc) {
        return wc_libc_realloc(p, old_n, n, align);
    }
    if (a->realloc) {
        return a->realloc(a->ctx, p, old_n, n, align);
    }

    // custom allocator without a realloc callback: emulate
    void* q = a->alloc(a->ctx, n, align);
    if (!q) {
        return NULL; // p left untouched
    }
    memcpy(q, p, old_n < n ? old_n : n);
    wc_free_ex(a, p, old_n, align);
    return q;
}


/* simple API (global allocator) */

static inline void* wc_alloc_aligned(size_t n, size_t align)
{
    return wc_alloc_ex(NULL, n, align);
}

static inline void* wc_realloc_aligned(void* p, size_t old_n, size_t n, size_t align)
{
    return wc_realloc_ex(NULL, p, old_n, n, align);
}

static inline void wc_free_aligned(void* p, size_t n, size_t align)
{
    wc_free_ex(NULL, p, n, align);
}

#define wc_alloc(n)             wc_alloc_aligned((n), WC_MAX_ALIGN)
#define wc_realloc(p, old_n, n) wc_realloc_aligned((p), (old_n), (n), WC_MAX_ALIGN)
#define wc_free(p, n)           wc_free_aligned((p), (n), WC_MAX_ALIGN)


/* typed helpers */

#define WC_NEW(T)             ((T*)wc_alloc_aligned(sizeof(T), alignof(T)))
#define WC_NEW_N(T, n)        ((T*)wc_alloc_aligned(sizeof(T) * (n), alignof(T)))
#define WC_REALLOC_N(T, p, n) ((T*)wc_realloc_aligned((p), sizeof(T) * (n), alignof(T)))
#define WC_DELETE(T, p)       wc_free_aligned((p), sizeof(T), alignof(T))
#define WC_DELETE_N(T, p, n)  wc_free_aligned((p), sizeof(T) * (n), alignof(T))

#define WC_NEW_IN(a, T)                ((T*)wc_alloc_ex((a), sizeof(T), alignof(T)))
#define WC_NEW_N_IN(a, T, n)           ((T*)wc_alloc_ex((a), sizeof(T) * (n), alignof(T)))
#define WC_REALLOC_N_IN(a, T, p, o, n) ((T*)wc_realloc_ex((a), (p), (o), sizeof(T) * (n), alignof(T)))
#define WC_DELETE_IN(a, T, p)          wc_free_ex((a), (p), sizeof(T), alignof(T))
#define WC_DELETE_N_IN(a, T, p, n)     wc_free_ex((a), (p), sizeof(T) * (n), alignof(T))



/*
 * NEW ALLOCATOR API
 *
 * TRANSITIONAL NAMES: lives alongside the old global API above until
 * Phase 2 exit, then `wc2_` / `WC2_` is renamed to `wc_` / `WC_` and everything
 * above this line is deleted.
 *
 * An allocator is a (vtable, ctx) pair, 16 bytes, passed and stored BY VALUE.
 * A zero-initialised allocator is libc: `WC_LIBC`.
 *
 * Contract (the wc2_* wrappers enforce it, backends may rely on it):
 *   - callbacks are never called with p == NULL or size == 0
 *   - `align` is a power of two >= 1 and is an ADDRESS alignment
 *   - `free` / `realloc` receive the exact size and align of the block's last
 *     alloc/realloc (wc_test_alloc verifies this; arenas ignore it)
 *   - `realloc` failure returns NULL and leaves p valid
 *   - missing `realloc` is emulated (alloc + copy + free); missing `free` is a no-op
 *   - wc2_alloc(a, 0, ..) returns NULL without calling the backend
 *   - wc2_realloc(a, NULL, 0, n, ..) == wc2_alloc(a, n, ..)
 *   - wc2_realloc(a, p, old, 0, ..) frees p and returns NULL
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

static inline int wc2_is_libc(wc_allocator a)
{
    return a.vt == NULL;
}

static inline int wc2_same(wc_allocator a, wc_allocator b)
{
    return a.vt == b.vt && a.ctx == b.ctx;
}

#define WC2_ASSERT_ALIGN(align) \
    assert((align) != 0 && ((align) & ((align) - 1)) == 0 && "align must be a power of two >= 1")

static inline void* wc2_alloc(wc_allocator a, size_t n, size_t align)
{
    WC2_ASSERT_ALIGN(align);
    if (n == 0 || n > (size_t)PTRDIFF_MAX) { // > PTRDIFF_MAX: overflowed size (see wc2_mul)
        return NULL;
    }
    return a.vt ? a.vt->alloc(a.ctx, n, align) : wc_libc_alloc(n, align);
}

static inline void wc2_free(wc_allocator a, void* p, size_t n, size_t align)
{
    WC2_ASSERT_ALIGN(align);
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

static inline void* wc2_realloc(wc_allocator a, void* p, size_t old_n, size_t n, size_t align)
{
    WC2_ASSERT_ALIGN(align);
    if (!p) {
        return wc2_alloc(a, n, align);
    }
    if (n == 0) {
        wc2_free(a, p, old_n, align);
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
    wc2_free(a, p, old_n, align);
    return q;
}

// count * size, saturating to SIZE_MAX on overflow. SIZE_MAX can never be
// satisfied, so an overflowing request fails in the backend instead of wrapping
// to a small size. (Returning 0 would be wrong: realloc to 0 means free.)
static inline size_t wc2_mul(size_t count, size_t size)
{
    size_t r;
    return (int)__builtin_mul_overflow(count, size, &r) ? SIZE_MAX : r;
}

// Smallest safe alignment for elements of `elm_size` bytes: the
// largest power of two dividing elm_size, capped at WC_MAX_ALIGN. Valid because
// sizeof(T) is always a multiple of alignof(T).
static inline size_t wc2_align_for_size(size_t elm_size)
{
    if (elm_size == 0) {
        return 1;
    }
    size_t lowbit = elm_size & (~elm_size + 1);
    return lowbit < WC_MAX_ALIGN ? lowbit : WC_MAX_ALIGN;
}


/* typed helpers (allocator first) */

#define WC2_NEW(a, T)            ((T*)wc2_alloc((a), sizeof(T), alignof(T)))
#define WC2_NEW_N(a, T, n)       ((T*)wc2_alloc((a), wc2_mul((n), sizeof(T)), alignof(T)))
#define WC2_DELETE(a, T, p)      wc2_free((a), (p), sizeof(T), alignof(T))
#define WC2_DELETE_N(a, T, p, n) wc2_free((a), (p), wc2_mul((n), sizeof(T)), alignof(T))
#define WC2_REALLOC_N(a, T, p, old_n, n) \
    ((T*)wc2_realloc((a), (p), wc2_mul((old_n), sizeof(T)), wc2_mul((n), sizeof(T)), alignof(T)))



#endif // WC_ALLOCATOR_H
