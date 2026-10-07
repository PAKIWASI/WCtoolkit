#include "arena.h"
#include "common.h"
#include "wc_allocator.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>


#define ALIGN_UP(p, align) ((u8*)(((uintptr_t)(p) + ((uintptr_t)(align) - 1)) & ~((uintptr_t)(align) - 1)))

#define IS_POW_2(align) (((align) & ((align) - 1)) == 0)


Arena Arena_create(const wc_allocator* alloc, u64 cap)
{
    if (cap < VARENA_MIN_SIZE) {
        cap = VARENA_MIN_SIZE;
    }

    u8* base = wc_alloc(alloc, cap, WC_MAX_ALIGN);
    FATAL_IF(!base, "alloc failed for cap %lu, alignment %lu", cap, WC_MAX_ALIGN);

    return (Arena){
        .base = base,
        .cap  = cap,
        .off  = 0,
    };
}



void* Arena_alloc(Arena* va, u64 size)
{
    WC_ASSERT(size != 0, "can't allocate 0 bytes");
    WC_ASSERT(va->off <= va->cap, "arena corrupted");
    WC_ASSERT(size <= va->cap - va->off, "arena full");

    u8* alloc   = va->base + va->off;
    u8* aligned = ALIGN_UP(alloc, WC_MAX_ALIGN); // TODO: should we default align to 16 bytes or 8 bytes?
    u64 pad     = (u64)(aligned - alloc);

    WC_ASSERT(pad <= va->cap - va->off - size, "arena full (alignment padding)");

    va->off += pad + size;
    return aligned; //  return the aligned address (padding inserted BEFORE each allocation)
}

void* Arena_alloc_aligned(Arena* va, u64 size, u64 align)
{
    WC_ASSERT(size != 0, "can't allocate 0 bytes");
    WC_ALLOC_ASSERT_ALIGN(align);
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
    return Arena_alloc_aligned(((Arena*)ctx), size, align);
}

// only alloc is needed for this. realloc is trivial and no free
static const wc_alloc_vtable varena_alloc_vtable_base = {.alloc = varena_alloc, .realloc = NULL, .free = NULL};
const wc_alloc_vtable*       varena_alloc_vtable      = &varena_alloc_vtable_base;


wc_allocator Arena_create_allocator(Arena* va)
{
    return (wc_allocator){
        .ctx = va,
        .vt  = varena_alloc_vtable,
    };
}


