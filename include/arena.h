#ifndef ARENA_H
#define ARENA_H

#include "common.h"
#include <stdlib.h>


typedef struct {
    u8* base;
    u64 idx;
    u64 size;
} Arena;


// Tweakable settings
#ifndef ARENA_DEFAULT_ALIGNMENT
    #define ARENA_DEFAULT_ALIGNMENT (sizeof(void*)) // 8 bytes
#endif
#ifndef ARENA_DEFAULT_SIZE
    #define ARENA_DEFAULT_SIZE      (nKB(4))      // 4 KB
#endif


/*
Allocate and return a pointer to memory to the Arena
with a region with the specified size. Providing a
size = 0 results in size = ARENA_DEFAULT_SIZE (user can modify)

Parameters:
  u64 size    |    The size (in bytes) of the Arena
                      memory region.
Return:
  Pointer to Arena on success, NULL on failure
*/
Arena* Arena_create(u64 capacity) __attribute__((warn_unused_result));

/*
Initialize an Arena object with pointers to the Arena and a
pre-allocated region(base ptr), as well as the size of the provided
region. Good for using the Stack instead of the heap.
The Arena and the data may be Stack initialized, so no Arena_destroy.
Note that ARENA_DEFAULT_SIZE is not used.

Parameters:
  Arena* Arena    |   The Arena object being initialized.
  u8*    data     |   The region to be Arena-fyed.
  u64    size     |   The size of the region in bytes.
*/
void Arena_create_arr_stk(Arena* arena, u64 size, u8* data) __attribute__((nonnull(1, 3)));



void Arena_create_stk(Arena* arena, u64 capacity) __attribute__((nonnull(1)));

/*
Reset the pointer to the Arena region to the beginning
of the allocation. Allows reuse of the memory without
expensive frees.

Parameters:
  Arena *Arena    |    The Arena to be cleared.
*/
static inline __attribute__((nonnull(1))) void Arena_clear(Arena* Arena)
{
    Arena->idx = 0;
}

/*
Free the memory allocated for the entire Arena region.

Parameters:
  Arena *Arena    |    The Arena to be destroyed.
*/
static inline __attribute__((nonnull(1))) void Arena_destroy(Arena* Arena)
{
    free(Arena->base);
    free(Arena);
}

/*
Return a pointer to a portion of specified size of the
specified Arena's region. By default, memory is
aligned by alignof(size_t), but you can change this by
#defining ARENA_DEFAULT_ALIGNMENT before #include'ing
Arena.h. Providing a size of zero results in a failure.

Parameters:
  Arena* Arena    |    The Arena of which the pointer
                       from the region will be
                       distributed
  u64 size        |    The size (in bytes) of
                       allocated memory planned to be
                       used.
Return:
  Pointer to Arena region segment on success, NULL on
  failure.
*/
u8* Arena_alloc(Arena* Arena, u64 size) __attribute__((nonnull(1), alloc_size(2)));

/*
Same as Arena_alloc, except you can specify a memory
alignment for allocations.

Return a pointer to a portion of specified size of the
specified Arena's region. Providing a size of
zero results in a failure.

Parameters:
  Arena* Arena              |    The Arena of which the pointer
                                 from the region will be
                                 distributed
  u64 size                  |    The size (in bytes) of
                                 allocated memory planned to be
                                 used.
  u32 alignment             |    Alignment (in bytes) for each
                                 memory allocation.
Return:
  Pointer to Arena region segment on success, NULL on
  failure.
*/
u8* Arena_alloc_aligned(Arena* Arena, u64 size, u32 alignment) __attribute__((nonnull(1), alloc_size(2)));


// Get used capacity
static inline __attribute__((nonnull(1))) u64 Arena_used(Arena* Arena)
{
    return Arena->idx;
}

// Get remaining capacity
static inline __attribute__((nonnull(1))) u64 Arena_remaining(Arena* Arena)
{
    return Arena->size - Arena->idx;
}



// explicit scratch Arena

typedef struct {
    Arena* Arena;
    u64 mark;
} ArenaScratch;


static inline __attribute__((nonnull(1))) ArenaScratch Arena_scratch_begin(Arena* Arena)
{
    return (ArenaScratch){ .Arena = Arena, .mark = Arena->idx };
}

static inline void Arena_scratch_end(ArenaScratch scratch)
{
    if (scratch.Arena) {
        scratch.Arena->idx = scratch.mark;
        scratch.Arena = NULL;
    }
}

static inline void wc_Arena_scratch_cleanup(ArenaScratch* s)
{
    if (s && s->Arena) {
        s->Arena->idx = s->mark;
        s->Arena = NULL;
    }
}

// macro for automatic cleanup Arena_scratch — safe with return/break/goto
#define ARENA_SCRATCH(Arena_ptr)                                                                             \
    for (int _as_once = 1; _as_once; _as_once = 0)                                                          \
        for (ArenaScratch __attribute__((cleanup(wc_Arena_scratch_cleanup))) _as_s = Arena_scratch_begin(Arena_ptr); \
             _as_once; _as_once = 0)

/* USAGE:
// Manual:
ScratchArena scratch = Arena_scratch_begin(Arena);
char* tmp = ARENA_ALLOC_N(Arena, char, 256);
Arena_scratch_end(scratch);

// Automatic:
ARENA_SCRATCH(Arena) {
    char* tmp = ARENA_ALLOC_N(Arena, char, 256);
} // auto cleanup
*/


// USEFULL MACROS

#define ARENA_CREATE_STK_ARR(Arena, n) (Arena_create_arr_stk((u8[nKB(n)]){0}, nKB(n), (Arena)))

// typed allocation
#define ARENA_ALLOC(Arena, T) ((T*)Arena_alloc((Arena), sizeof(T)))

#define ARENA_ALLOC_N(Arena, T, n) ((T*)Arena_alloc((Arena), sizeof(T) * (n)))

// common for structs
#define ARENA_ALLOC_ZERO(Arena, T) ((T*)memset(ARENA_ALLOC(Arena, T), 0, sizeof(T)))

#define ARENA_ALLOC_ZERO_N(Arena, T, n) ((T*)memset(ARENA_ALLOC_N(Arena, T, n), 0, sizeof(T) * (n)))

// Allocate and copy array into Arena
#define ARENA_PUSH_ARRAY(Arena, T, src, count)      \
    ({                                              \
        (T)* _dst = ARENA_ALLOC_N(Arena, T, count); \
        memcpy(_dst, src, sizeof(T) * (count));     \
        _dst;                                       \
    })



#endif // ARENA_H
