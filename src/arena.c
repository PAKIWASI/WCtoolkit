#include "arena.h"
#include "common.h"
#include "wc_allocator.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>



#define ARENA_DEFAULT_ALIGNMENT ((u64)alignof(max_align_t))

#define ALIGN_UP(p, align) ((u8*)(((uintptr_t)(p) + ((uintptr_t)(align) - 1)) & ~((uintptr_t)(align) - 1)))

#define IS_POW_2(align) (((align) & ((align) - 1)) == 0)


Varena Varena_create(u64 cap)
{
    if (cap < VARENA_MIN_SIZE) {
        cap = VARENA_MIN_SIZE;
    }

    u8* base =
        mmap(NULL,                        // let kernel choose the virtual address
             cap,                         // the allocation size
             PROT_READ | PROT_WRITE,      // mapping is readable and writable
             MAP_PRIVATE | MAP_ANONYMOUS, // MAP_PRIVATE: writes are copy-on-write, MAP_ANONYMOUS: not backed by a file
             -1,                          // -1 for MAP_ANONYMOUS
             0);                          // ignored for MAP_ANONYMOUS
    FATAL_IF(base == MAP_FAILED, "mmap call failed for cap: %lu", cap);

    return (Varena){
        .base = base,
        .cap  = cap,
        .off  = 0,
    };
}



void* Varena_alloc(Varena* va, u64 size)
{
    WC_ASSERT(size != 0, "can't allocate 0 bytes");
    WC_ASSERT(va->off <= va->cap, "arena corrupted");
    WC_ASSERT(size <= va->cap - va->off, "arena full");

    u8* alloc   = va->base + va->off;
    u8* aligned = ALIGN_UP(alloc, ARENA_DEFAULT_ALIGNMENT);
    u64 pad     = (u64)(aligned - alloc);

    WC_ASSERT(pad <= va->cap - va->off - size, "arena full (alignment padding)");

    va->off += pad + size;
    return aligned; //  return the aligned address (padding inserted BEFORE each allocation)
}

void* Varena_alloc_aligned(Varena* va, u64 size, u64 align)
{
    WC_ASSERT(size != 0, "can't allocate 0 bytes");
    WC_ASSERT(align != 0 && IS_POW_2(align), "alignment must be a power of two");
    WC_ASSERT(va->off <= va->cap, "arena corrupted");
    WC_ASSERT(size <= va->cap - va->off, "arena full");

    u8* alloc   = va->base + va->off;
    u8* aligned = ALIGN_UP(alloc, align);
    u64 pad     = (u64)(aligned - alloc);

    WC_ASSERT(pad <= va->cap - va->off - size, "arena full (alignment padding)");

    va->off += pad + size;
    return aligned;
}


// wc_allocator interface

static inline void* varena_alloc(void* ctx, size_t size, size_t align)
{
    return Varena_alloc_aligned(((Varena*)ctx), size, align);
}

// only alloc is needed for this. realloc is trivial and no free
static const wc_alloc_vtable varena_alloc_vtable_base = {.alloc = varena_alloc, .realloc = NULL, .free = NULL};
const wc_alloc_vtable*       varena_alloc_vtable      = &varena_alloc_vtable_base;


wc_allocator Varena_create_allocator(Varena* va)
{
    return (wc_allocator){
        .ctx = va,
        .vt  = varena_alloc_vtable,
    };
}


