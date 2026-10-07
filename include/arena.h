#ifndef ARENA_H
#define ARENA_H

#include "common.h"
#include "wc_allocator.h"

#include <stdalign.h>
#include <string.h>

#ifndef VARENA_MIN_SIZE
#define VARENA_MIN_SIZE (nKB(4)) // 4 KB
#endif


// arena backed by mmap/VirtualAlloc
// use this for allocations that persist for the entire program
//  and you know the upper bound
typedef struct {
    u8*                 base;
    const wc_allocator* alloc;
    u64                 off;
    u64                 cap;
} Arena;

[[nodiscard]] Arena Arena_create(const wc_allocator* alloc, u64 cap) WC_NONULL(1);

void Arena_destroy(Arena* a) WC_NONULL(1);

[[nodiscard]] void* Arena_alloc(Arena* a, u64 size) WC_NONULL(1);

[[nodiscard]] void* Arena_alloc_aligned(Arena* a, u64 size, u64 align) WC_NONULL(1);


// wc_allocator interface
const extern wc_alloc_vtable* varena_alloc_vtable;

[[nodiscard]] wc_allocator Arena_create_allocator(Arena* a) WC_NONULL(1);


// TYPED ALLOCATION MACROS

#define VARENA_ALLOC(a, T) Arena_alloc_aligned(a, sizeof(T), alignof(T))

#define VARENA_ALLOC_N(a, T, n) Arena_alloc_aligned(a, sizeof(T) * (n), alignof(T))



// SCRATCH STUFF

// When doing scratch allocations per iteration that are discarded afterwards
typedef struct {
    const Arena* arena;
    const u64    mark; // the offset value to go back to
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
    ((Arena*)scratch.arena)->off = scratch.mark;
}

static inline void ArenaScratch_destroy(ArenaScratch scratch)
{
    ArenaScratch_scratch_off(scratch);
    scratch.arena = NULL;
}


// TODO: use cleanup attribute for auto cleanup at scope end. usage:
// VARENA_SCRATCH(va) {
//  temp allocations...
// }
#define VARENA_SCRATCH(a)


#endif // ARENA_H
