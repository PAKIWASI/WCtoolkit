#include "arena.h"


/* python
align to 8 bytes
>>> 4 + 7 & ~(7)
8
align to 4 bytes
>>> 1 + 4 & ~(4)
1
*/
// Align a value to alignment boundary
#define ALIGN_UP(val, align) \
    (((val) + ((align) - 1)) & ~((align) - 1))

// align value to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_UP_DEFAULT(val) \
    ALIGN_UP((val), ARENA_DEFAULT_ALIGNMENT)

// Align a pointer to alignment boundary  
// turn ptr to a u64 val to align, then turn to ptr again
#define ALIGN_PTR(ptr, align) \
    ((u8*)ALIGN_UP((ptr), (align)))

// align a pointer to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_PTR_DEFAULT(ptr) \
    ALIGN_PTR((ptr), ARENA_DEFAULT_ALIGNMENT)


#define ARENA_CURR_IDX_PTR(arena) ((arena)->base + (arena)->idx)
#define ARENA_PTR(arena, idx) ((arena)->base + (idx))





Arena* arena_create(u64 capacity)
{
    if (capacity == 0) {
        capacity = ARENA_DEFAULT_SIZE;
    }

    Arena* arena = (Arena*)malloc(sizeof(Arena));
    CHECK_FATAL(!arena, "arena malloc failed");

    arena->base = (u8*)malloc(capacity);
    CHECK_FATAL(!arena->base, "arena base malloc failed");

    arena->idx = 0;
    arena->size = capacity;

    return arena;
}

void arena_create_arr_stk(Arena* arena, u8* data, u64 size)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(size == 0, "size can't be zero");

    arena->base = data;
    arena->idx = 0;
    arena->size = size;
}

void arena_clear(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    arena->idx = 0;
}

void arena_release(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    
    free(arena->base);
    free(arena);
}

u8* arena_alloc(Arena* arena, u64 size)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    
    // Align the current index first
    u64 aligned_idx = ALIGN_UP_DEFAULT(arena->idx);
    
    CHECK_WARN_RET(arena->size - aligned_idx < size,
                   NULL, "not enough space in arena for SIZE");
    
    u8* ptr = ARENA_PTR(arena, aligned_idx);
    arena->idx = aligned_idx + size;
    
    return ptr;
}

u8* arena_alloc_aligned(Arena* arena, u64 size, u32 alignment)
{

    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    CHECK_FATAL((alignment & (alignment - 1)) != 0,
                "alignment must be power of two");


    u64 aligned_idx = ALIGN_UP(arena->idx, alignment);

    CHECK_WARN_RET(arena->size - aligned_idx < size,
                   NULL, "not enough space in arena for SIZE");

    u8* ptr = ARENA_PTR(arena, aligned_idx);
    arena->idx = aligned_idx + size;

    return ptr;
}

u64 arena_get_mark(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    return arena->idx;
}

void arena_clear_mark(Arena* arena, u64 mark)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(mark > arena->idx, "mark is out of bounds");

    if (mark == arena->idx) { return; }

    arena->idx = mark;
}


