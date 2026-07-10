#include "arena.h"
#include "wc_errno.h"

/*'''python
align a 4 byte thing to 8 bytes alignment boundry:
>>> 4 + (8 - 1) & ~(8 - 1)
8
>>> 7 + (8 - 1) & ~(8 - 1)
8
>>> 9 + (8 - 1) & ~(8 - 1)
16 <- how much bytes should a 9 byte thing occupy to align to boundry
>>> 15 + (8 - 1) & ~(8 - 1)
16
>>> 18 + (8 - 1) & ~(8 - 1)
24
'''*/
// Align a value to alignment boundary
// Note: align MUST be power of 2 and >= 1
#define ALIGN_UP(val, align) \
    ((align) == 0 ? (val) : (((val) + ((align) - 1)) & ~((align) - 1)))

// align value to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_UP_DEFAULT(val) \
    ALIGN_UP((val), ARENA_DEFAULT_ALIGNMENT)


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

void arena_create_stk(Arena* arena, u64 capacity)
{
    if (capacity == 0) {
        capacity = ARENA_DEFAULT_SIZE;
    }

    arena->base = (u8*)malloc(capacity);
    CHECK_FATAL(!arena->base, "arena base malloc failed");

    arena->idx  = 0;
    arena->size = capacity;
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

u8* arena_alloc(Arena* arena, u64 size)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");

    // Align the current index first
    u64 aligned_idx = ALIGN_UP_DEFAULT(arena->idx);
    WC_SET_RET(WC_ERR_FULL, arena->size - aligned_idx < size, NULL);

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

    WC_SET_RET(WC_ERR_FULL, arena->size - aligned_idx < size, NULL);

    u8* ptr = ARENA_PTR(arena, aligned_idx);
    arena->idx = aligned_idx + size;

    return ptr;
}


