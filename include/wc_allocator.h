#ifndef WC_ALLOCATOR_H
#define WC_ALLOCATOR_H

#include <stddef.h>
#include <stdlib.h>


// 16-byte handle, passed and stored by value. Zero-init == libc.
typedef struct {
    void* (*alloc)(void* ctx, size_t size);
    void* (*realloc)(void* ctx, void* ptr, size_t old_size, size_t new_size);
    void  (*free)(void* ctx, void* ptr, size_t size);
    void* ctx;
} wc_allocator;


#ifndef WC_DEFAULT_ALLOCATOR
    #define WC_DEFAULT_ALLOCATOR ((wc_allocator){0}) // libc
#endif

_Thread_local wc_allocator wc_default_allocator = WC_DEFAULT_ALLOCATOR;

#define WC_SET_ALLOCATOR(allocator) (wc_default_allocator = (allocator))



static inline void* wc_alloc(size_t n)
{
    return wc_default_allocator.alloc ?
        wc_default_allocator.alloc(wc_default_allocator.ctx, n) : malloc(n);
}

static inline void* wc_realloc(void* p, size_t old_n, size_t n)
{
    return wc_default_allocator.realloc ?
        wc_default_allocator.realloc(wc_default_allocator.ctx, p, old_n, n) : realloc(p, n);
}

static inline void wc_free(void* p, size_t n)
{
    return wc_default_allocator.free ?
        wc_default_allocator.free(wc_default_allocator.ctx, p, n) : free(p);
}

#define WC_NEW(a, T)      ((T*)wc_alloc((a), sizeof(T)))
#define WC_NEW_N(a, T, n) ((T*)wc_alloc((a), sizeof(T) * (n)))



#endif // WC_ALLOCATOR_H
