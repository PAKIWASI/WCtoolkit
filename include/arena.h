#ifndef ARENA_H
#define ARENA_H

#include "common.h"


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
Allocate and return a pointer to memory to the arena
with a region with the specified size. Providing a
size = 0 results in size = ARENA_DEFAULT_SIZE (user can modify)

Parameters:
  u64 size    |    The size (in bytes) of the arena
                      memory region.
Return:
  Pointer to arena on success, NULL on failure
*/
Arena* arena_create(u64 capacity);

/*
Initialize an arena object with pointers to the arena and a
pre-allocated region(base ptr), as well as the size of the provided
region. Good for using the stack instead of the heap.
The arena and the data may be stack initialized, so no arena_release.
Note that ARENA_DEFAULT_SIZE is not used.

Parameters:
  Arena* arena    |   The arena object being initialized.
  u8*    data     |   The region to be arena-fyed.
  u64    size     |   The size of the region in bytes.
*/
void arena_create_arr_stk(Arena* arena, u8* data, u64 size);


void arena_create_stk(Arena* arena, u64 capacity);

/*
Reset the pointer to the arena region to the beginning
of the allocation. Allows reuse of the memory without
expensive frees.

Parameters:
  Arena *arena    |    The arena to be cleared.
*/
static inline void arena_clear(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    arena->idx = 0;
}

/*
Free the memory allocated for the entire arena region.

Parameters:
  Arena *arena    |    The arena to be destroyed.
*/
static inline void arena_release(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    free(arena->base);
    free(arena);
}

/*
Return a pointer to a portion of specified size of the
specified arena's region. By default, memory is
aligned by alignof(size_t), but you can change this by
#defining ARENA_DEFAULT_ALIGNMENT before #include'ing
arena.h. Providing a size of zero results in a failure.

Parameters:
  Arena* arena    |    The arena of which the pointer
                       from the region will be
                       distributed
  u64 size        |    The size (in bytes) of
                       allocated memory planned to be
                       used.
Return:
  Pointer to arena region segment on success, NULL on
  failure.
*/
u8* arena_alloc(Arena* arena, u64 size);

/*
Same as arena_alloc, except you can specify a memory
alignment for allocations.

Return a pointer to a portion of specified size of the
specified arena's region. Providing a size of
zero results in a failure.

Parameters:
  Arena* arena              |    The arena of which the pointer
                                 from the region will be
                                 distributed
  u64 size                  |    The size (in bytes) of
                                 allocated memory planned to be
                                 used.
  u32 alignment             |    Alignment (in bytes) for each
                                 memory allocation.
Return:
  Pointer to arena region segment on success, NULL on
  failure.
*/
u8* arena_alloc_aligned(Arena* arena, u64 size, u32 alignment);


// Get used capacity
static inline u64 arena_used(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    return arena->idx;
}

// Get remaining capacity
static inline u64 arena_remaining(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    return arena->size - arena->idx;
}



// explicit scratch arena

typedef struct {
    Arena* arena;
    u64 mark;
} ArenaScratch;


static inline ArenaScratch arena_scratch_begin(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    return (ArenaScratch){ .arena = arena, .mark = arena->idx };
}

static inline void arena_scratch_end(ArenaScratch scratch)
{
    if (scratch.arena) {
        scratch.arena->idx = scratch.mark;
        scratch.arena = NULL;
    }
}

// macro for automatic cleanup arena_scratch
#define ARENA_SCRATCH(arena_ptr) \
    for (ArenaScratch __nme__ = arena_scratch_begin(arena_ptr); \
         (__nme__ ).arena != NULL; \
         arena_scratch_end((__nme__ )), (__nme__).arena = NULL)

/* USAGE:
// Manual:
ScratchArena scratch = arena_scratch_begin(arena);
char* tmp = ARENA_ALLOC_N(arena, char, 256);
arena_scratch_end(scratch);

// Automatic:
ARENA_SCRATCH(arena) {
    char* tmp = ARENA_ALLOC_N(arena, char, 256);
} // auto cleanup
*/


// USEFULL MACROS

#define ARENA_CREATE_STK_ARR(arena, n) (arena_create_arr_stk((arena), (u8[nKB(n)]){0}, nKB(n)))

// typed allocation
#define ARENA_ALLOC(arena, T) ((T*)arena_alloc((arena), sizeof(T)))

#define ARENA_ALLOC_N(arena, T, n) ((T*)arena_alloc((arena), sizeof(T) * (n)))

// common for structs
#define ARENA_ALLOC_ZERO(arena, T) ((T*)memset(ARENA_ALLOC(arena, T), 0, sizeof(T)))

#define ARENA_ALLOC_ZERO_N(arena, T, n) ((T*)memset(ARENA_ALLOC_N(arena, T, n), 0, sizeof(T) * (n)))

// Allocate and copy array into arena
#define ARENA_PUSH_ARRAY(arena, T, src, count)      \
    ({                                              \
        (T)* _dst = ARENA_ALLOC_N(arena, T, count); \
        memcpy(_dst, src, sizeof(T) * (count));     \
        _dst;                                       \
    })



#endif // ARENA_H
