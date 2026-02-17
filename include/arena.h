#ifndef ARENA_H
#define ARENA_H

#include "common.h"



typedef struct {
    u8* base;
    u64 idx;
    u64 size;
} Arena;




// Tweakable settings
#define ARENA_DEFAULT_ALIGNMENT (sizeof(u64)) // 8 byte
#define ARENA_DEFAULT_SIZE      (nKB(4))      // 4 KB


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

/*
Reset the pointer to the arena region to the beginning
of the allocation. Allows reuse of the memory without
expensive frees.

Parameters:
  Arena *arena    |    The arena to be cleared.
*/
void arena_clear(Arena* arena);

/*
Free the memory allocated for the entire arena region.

Parameters:
  Arena *arena    |    The arena to be destroyed.
*/
void arena_release(Arena* arena);

/*
Return a pointer to a portion of specified size of the
specified arena's region. Nothing will restrict you
from allocating more memory than you specified, so be
mindful of your memory (as you should anyways) or you
will get some hard-to-track bugs. By default, memory is
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
specified arena's region. Nothing will restrict you
from allocating more memory than you specified, so be
mindful of your memory (as you should anyways) or you
will get some hard-to-track bugs. Providing a size of
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


/*
Get the value of index at the current state of arena
This can be used to later clear upto that point using arena_clear_mark

Parameters:
  Arena* arena          |   The arena whose idx will be returned

Return:
  The current value of idx variable
*/
u64 arena_get_mark(Arena* arena);

/*
Clear the arena from current index back to mark

Parameters:
  Arena* arena          |   The arena you want to clear using it's mark
  u64    mark           |   The mark previosly obtained by arena_get_mark 
*/
void arena_clear_mark(Arena* arena, u64 mark);


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
    u64 saved_idx;
} arena_scratch;


static inline arena_scratch arena_scratch_begin(Arena* arena) {
    CHECK_FATAL(!arena, "arena is null");
    return (arena_scratch){ .arena = arena, .saved_idx = arena->idx };
}

static inline void arena_scratch_end(arena_scratch* scratch) {
    if (scratch && scratch->arena) {
        scratch->arena->idx = scratch->saved_idx;
    }
}

// macro for automatic cleanup arena_scratch
#define ARENA_SCRATCH(name, arena_ptr) \
    for (arena_scratch name = arena_scratch_begin(arena_ptr); \
         (name).arena != NULL; \
         arena_scratch_end(&(name)), (name).arena = NULL)

/* USAGE:
// Manual:
ScratchArena scratch = arena_scratch_begin(arena);
char* tmp = ARENA_ALLOC_N(arena, char, 256);
arena_scratch_end(&scratch);

// Automatic:
ARENA_SCRATCH(scratch, arena) {
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
