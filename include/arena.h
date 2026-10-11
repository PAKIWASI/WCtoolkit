#ifndef ARENA_H
#define ARENA_H

#include "common.h"
#include "wc_allocator.h"

#include <stdalign.h>

#ifndef ARENA_MIN_SIZE
#define ARENA_MIN_SIZE (nKB(4)) // 4 KB
#endif


// arena backed by another allocator (libc, WC_MMAP, ...)
// use this for allocations that persist for the entire program and you know the upper bound
typedef struct {
    u8*                 base;
    const wc_allocator* alloc; // backing allocator the region came from; Arena_destroy gives it back here
    u64                 off;
    u64                 cap;
    wc_allocator        self; // what containers store. ctx == this Arena. lives as long as the arena, so allocator
    // becomes invalidatd when arena is destroyed.
} Arena;

// Takes a `cap`-byte region (at least VARENA_MIN_SIZE) from `backing`. Aborts if backing fails.
// `a` is initialised in place and must not be moved or copied afterwards.
void Arena_create(Arena* a, const wc_allocator* backing, u64 cap) WC_NONULL(1, 2);

// Gives the region back to the backing allocator and zeroes the arena. Safe on a zeroed
// arena and safe to call twice.
void Arena_destroy(Arena* a) WC_NONULL(1);

// Both return NULL when the arena can't fit the request. Arena_alloc aligns to WC_MAX_ALIGN
// (like malloc). Arena_alloc_aligned aligns the ADDRESS to `align` (a power of two).
[[nodiscard]] void* Arena_alloc(Arena* a, u64 size) WC_NONULL(1);

[[nodiscard]] void* Arena_alloc_aligned(Arena* a, u64 size, u64 align) WC_NONULL(1);

// The allocator to give to containers. Points into the arena, so it lives as long as the arena
[[nodiscard]] static inline const wc_allocator* Arena_allocator(Arena* a) WC_NONULL(1);

static inline const wc_allocator* Arena_allocator(Arena* a)
{
    // zeroed self has vt == NULL, which would silently read as WC_LIBC
    FATAL_IF(!a->self.ctx, "Arena_allocator on zeroed/destroyed Arena");
    return &a->self;
}


// TYPED ALLOCATION MACROS

#define ARENA_ALLOC(a, T) Arena_alloc_aligned(a, sizeof(T), alignof(T))

#define ARENA_ALLOC_N(a, T, n) Arena_alloc_aligned(a, sizeof(T) * (n), alignof(T))



// SCRATCH STUFF

// When doing scratch allocations per iteration that are discarded afterwards
typedef struct {
    Arena* arena;
    u64    mark; // the offset value to go back to
} ArenaScratch;


[[nodiscard]] static inline WC_NONULL(1) // TODO: fix clang format for this
    ArenaScratch ArenaScratch_create(Arena* a)
{
    return (ArenaScratch){
        .arena = a,
        .mark  = a->off,
    };
}

static inline void ArenaScratch_scratch_off(ArenaScratch scratch)
{
    WC_ASSERT(scratch.arena, "arena is null");
    scratch.arena->off = scratch.mark;
}

static inline void ArenaScratch_destroy(ArenaScratch scratch)
{
    ArenaScratch_scratch_off(scratch);
    scratch.arena = NULL;
}


/* ARENA_SCRATCH(&arena) { ... }
 *
 * Everything allocated from `arena` inside the block is rolled back when the block exits.
 * The arena itself survives, and so does everything allocated before the block.
 *
 *     Arena arena;
 *     Arena_create(&arena, WC_LIBC, nKB(4));
 *     ARENA_SCRATCH(&arena) {
 *         char* tmp = Arena_alloc(&arena, 1024);   // rolled back at the closing brace
 *     }
 *
 * It uses __attribute__((cleanup)), so the rollback runs on EVERY way out of the block:
 * falling off the end, `break`, `continue`, `return` and `goto`. (It does not run on
 * longjmp, exit or abort.) GCC and Clang only: elsewhere, use ArenaScratch_create and
 * ArenaScratch_destroy by hand.
 *
*/

static inline void ArenaScratch_cleanup(ArenaScratch* s)
{
    ArenaScratch_scratch_off(*s);
}
#define ARENA_SCRATCH(a) ARENA_SCRATCH_((a), WC_CAT(_wsc_once_, __LINE__), WC_CAT(_wsc_, __LINE__))
#define ARENA_SCRATCH_(a, once, s)        \
    for (int(once) = 1; once; (once) = 0) \
        for (ArenaScratch s __attribute__((cleanup(ArenaScratch_cleanup))) = ArenaScratch_create(a); once; (once) = 0)



#endif // ARENA_H
