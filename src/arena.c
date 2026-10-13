#include "arena.h"
#include "common.h"
#include "wc_allocator.h"

#include <inttypes.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>



// Carve `size` bytes, aligned by ADDRESS to `align`, out of the arena. NULL if it doesn't fit.
static void* arena_bump(Arena* a, u64 size, u64 align)
{
    WC_ASSERT(a->off <= a->cap, "arena corrupted");

    u64       room = a->cap - a->off;
    uintptr_t cur  = (uintptr_t)a->base + (uintptr_t)a->off;
    u64       pad  = (u64)((0 - cur) & (uintptr_t)(align - 1)); // bytes up to the next aligned address

    if (pad > room || size > room - pad) {
        return NULL; // arena is full
    }

    u8* p = a->base + a->off + pad;
    a->off += pad + size; // padding goes BEFORE the allocation
    return p;
}


// wc_allocator interface

// Only alloc is provided. free is a no-op and realloc is emulated (alloc + copy) by wc_realloc.
// Exhaustion returns NULL.
static void* arena_alloc(void* ctx, size_t size, size_t align)
{
    return Arena_alloc_aligned((Arena*)ctx, size, align);
}

static const wc_alloc_vtable arena_vtable = {.alloc = arena_alloc, .realloc = NULL, .free = NULL};


void Arena_create(Arena* a, const wc_allocator* backing, u64 cap)
{
    if (cap < ARENA_MIN_SIZE) {
        cap = ARENA_MIN_SIZE;
    }

    u8* base = wc_alloc(backing, cap, WC_MAX_ALIGN);
    FATAL_IF(!base, "arena backing alloc failed (cap %" PRIu64 ")", cap);

    // `self` has to be built here, in place, because its ctx is this arena's own address.
    *a = (Arena){
        .base  = base,
        .alloc = backing, // remembered so Arena_destroy can give the region back
        .off   = 0,
        .cap   = cap,
        .self  = {.vt = &arena_vtable, .ctx = a},
    };
}

void Arena_destroy(Arena* a)
{
    if (!a->self.ctx) {
        return; // zeroed or already destroyed
    }
    wc_free(a->alloc, a->base, a->cap, WC_MAX_ALIGN);
    memset(a, 0, sizeof(Arena)); // also clears self.ctx, so a stale allocator is detectable
}

void* Arena_alloc(Arena* a, u64 size)
{
    WC_ASSERT(a->self.ctx != NULL, "Arena is in the zero state (moved-from or destroyed): re-create it first");
    WC_ASSERT(size != 0, "can't allocate 0 bytes");
    WC_ASSERT(a->self.ctx == a, "arena was moved or copied after Arena_create");
    return arena_bump(a, size, WC_MAX_ALIGN);
}

void* Arena_alloc_aligned(Arena* a, u64 size, u64 align)
{
    WC_ASSERT(a->self.ctx != NULL, "Arena is in the zero state (moved-from or destroyed): re-create it first");
    WC_ASSERT(size != 0, "can't allocate 0 bytes");
    WC_ALLOC_ASSERT_ALIGN(align);
    WC_ASSERT(a->self.ctx == a, "arena was moved or copied after Arena_create");
    return arena_bump(a, size, align);
}
