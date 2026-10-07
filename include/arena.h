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
    u8* base;
    const wc_allocator* alloc;
    u64 off;
    u64 cap;
} Varena;

[[nodiscard]] Varena Varena_create(const wc_allocator* alloc, u64 cap) WC_NONULL(1);

[[nodiscard]] void* Varena_alloc(Varena* va, u64 size) WC_NONULL(1);

[[nodiscard]] void* Varena_alloc_aligned(Varena* va, u64 size, u64 align) WC_NONULL(1);


// wc_allocator interface
const extern wc_alloc_vtable* varena_alloc_vtable;

[[nodiscard]] wc_allocator Varena_create_allocator(Varena* va) WC_NONULL(1);


// TYPED ALLOCATION MACROS

#define VARENA_ALLOC(va, T) Varena_alloc_aligned(va, sizeof(T), alignof(T))

#define VARENA_ALLOC_N(va, T, n) Varena_alloc_aligned(va, sizeof(T) * (n), alignof(T))



// SCRATCH STUFF

// When doing scratch allocations per iteration that are discarded afterwards
typedef struct {
    const Varena* arena;
    const u64     mark; // the offset value to go back to
} VarenaScratch;


[[nodiscard]] static inline WC_NONULL(1) // TODO: fix clang format for this
    VarenaScratch VarenaScratch_create(Varena* va)
{
    return (VarenaScratch){
        .arena = va,
        .mark  = va->off,
    };
}

static inline void VarenaScratch_scratch_off(VarenaScratch scratch)
{
    WC_ASSERT(scratch.arena, "arena is null");
    ((Varena*)scratch.arena)->off = scratch.mark;
}

static inline void VarenaScratch_destroy(VarenaScratch scratch)
{
    VarenaScratch_scratch_off(scratch);
    scratch.arena = NULL;
}


// TODO: use cleanup attribute for auto cleanup at scope end. usage:
// VARENA_SCRATCH(va) {
//  temp allocations...
// }
#define VARENA_SCRATCH(va) for ()


#endif // ARENA_H
