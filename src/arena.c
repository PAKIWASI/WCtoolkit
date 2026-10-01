#include "arena.h"
#include "common.h"
#include "wc_allocator.h"
#include "wc_errno.h"

#include <stdint.h>
#include <string.h>


// Region alignment requested from the backing allocator.
#define ARENA_BASE_ALIGN WC_MAX_ALIGN

// Debug-only liveness check: catches use after destroy and use of a copied Arena
// (a copy's `self` still points at the original).
#define ARENA_CHECK_LIVE(a) CHECK_FATAL((a)->self != (a), "Arena used after destroy, or copied/moved after create")


static inline b8 is_pow2(u64 x)
{
    return x != 0 && (x & (x - 1)) == 0;
}


// Lifecycle

void Arena_create(Arena* arena, wc_allocator backing, u64 capacity)
{
    if (capacity == 0) {
        capacity = ARENA_DEFAULT_SIZE;
    }

    u8* base = wc_alloc(backing, capacity, ARENA_BASE_ALIGN);
    CHECK_FATAL(!base, "Arena base allocation of %llu bytes failed", (unsigned long long)capacity); // D5: unconditional

    *arena = (Arena){
        .backing   = backing,
        .base      = base,
        .size      = capacity,
        .idx       = 0,
        .floor     = 0,
        .self      = arena,
        .owns_base = 1,
    };
}

void Arena_create_buf(Arena* arena, u8* buf, u64 size)
{
    CHECK_FATAL(size == 0, "size can't be zero");

    *arena = (Arena){
        .backing   = wc_borrowed,
        .base      = buf,
        .size      = size,
        .idx       = 0,
        .floor     = 0,
        .self      = arena,
        .owns_base = 0,
    };
}

void Arena_destroy(Arena* arena)
{
    if (arena->self == NULL) {
        return; // zeroed or already destroyed
    }
    ARENA_CHECK_LIVE(arena);

    if (arena->owns_base) {
        wc_allocator backing = arena->backing; // read before zeroing
        wc_free(backing, arena->base, arena->size, ARENA_BASE_ALIGN);
    }
    memset(arena, 0, sizeof(*arena));
}

void Arena_reset(Arena* arena)
{
    ARENA_CHECK_LIVE(arena);
    arena->idx   = 0;
    arena->floor = 0;
}


// Allocation

u8* Arena_alloc(Arena* arena, u64 size)
{
    return Arena_alloc_aligned(arena, size, ARENA_DEFAULT_ALIGNMENT);
}

u8* Arena_alloc_aligned(Arena* arena, u64 size, u64 align)
{
    ARENA_CHECK_LIVE(arena);
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    CHECK_FATAL(!is_pow2(align), "alignment must be a power of two");

    // Align the ADDRESS, not the offset (A2): the base itself may be unaligned.
    uintptr_t base    = (uintptr_t)arena->base;
    uintptr_t cur     = base + arena->idx;
    uintptr_t aligned = (cur + (align - 1)) & ~(uintptr_t)(align - 1);
    u64       off     = (u64)(aligned - base);

    // Two-step check so nothing underflows (A1): the aligned start itself may be past the end.
    WC_SET_RET(WC_ERR_FULL, off > arena->size || arena->size - off < size, NULL);

    arena->idx = off + size;
    return arena->base + off;
}


// Allocator backend

// Is [p, p + size) the last block, at or above the floor?
static inline b8 arena_is_top(const Arena* arena, const u8* p, u64 size)
{
    return p >= arena->base + arena->floor && p + size == arena->base + arena->idx;
}

static void* arena_vt_alloc(void* ctx, size_t size, size_t align)
{
    return Arena_alloc_aligned((Arena*)ctx, size, align);
}

static void* arena_vt_realloc(void* ctx, void* ptr, size_t old_size, size_t new_size, size_t align)
{
    Arena* arena = ctx;
    u8*    p     = ptr;
    ARENA_CHECK_LIVE(arena);

    // Growing a block that predates the innermost scratch scope is a lifetime
    // bug either way: in place, scratch_end would truncate it; copied, it would
    // live in scratch memory and dangle at scope end. The floor (D10) keeps the
    // arena's own invariant (nothing below the mark moves or resizes in place);
    // this check makes the caller's bug loud in debug builds.
    CHECK_FATAL(new_size > old_size && p < arena->base + arena->floor,
                "growing a block allocated before the current scratch scope (it would dangle at scope end)");

    // p keeps its alignment when it stays in place: no re-alignment needed.
    if (arena_is_top(arena, p, old_size)) {
        u64 off = (u64)(p - arena->base);
        WC_SET_RET(WC_ERR_FULL, arena->size - off < new_size, NULL); // nothing past the top can fit either
        arena->idx = off + new_size;                                 // grow or shrink in place (A4)
        return p;
    }

    if (new_size <= old_size) {
        return p; // shrinking a block that is not on top: keep it, the tail is simply unused
    }

    u8* q = Arena_alloc_aligned(arena, new_size, align);
    if (!q) {
        return NULL; // p stays valid
    }
    memcpy(q, p, old_size);
    return q; // the old block is abandoned until reset/scratch end
}

static void arena_vt_free(void* ctx, void* ptr, size_t size, size_t align)
{
    (void)align;
    Arena* arena = ctx;
    u8*    p     = ptr;
    ARENA_CHECK_LIVE(arena);

    if (arena_is_top(arena, p, size)) {
        arena->idx = (u64)(p - arena->base); // rewind; padding before p stays used
    }
}

static const wc_alloc_vtable arena_vt = {
    .alloc   = arena_vt_alloc,
    .realloc = arena_vt_realloc,
    .free    = arena_vt_free,
};

wc_allocator Arena_allocator(Arena* arena)
{
    ARENA_CHECK_LIVE(arena);
    return (wc_allocator){.vt = &arena_vt, .ctx = arena};
}
