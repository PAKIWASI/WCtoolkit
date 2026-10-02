#ifndef ARENA_H
#define ARENA_H

#include "common.h"
#include "wc_allocator.h"

#include <stdalign.h>
#include <string.h>


/*
 * Arena: single-block bump allocator with a fixed capacity.
 *
 * PINNED: `Arena_allocator(a)` hands out the arena's address as the allocator
 * ctx, so an Arena must not be moved or copied after Arena_create*. Every entry
 * point checks `self == a` in debug builds to catch copies and use after destroy.
 *
 * In-place ops (plan D10): realloc grows/shrinks the LAST block in place and
 * free rewinds the LAST block, but only for blocks at or above `floor`.
 * A scratch scope raises the floor to the current position, so blocks created
 * before the scope are never extended past (or rewound below) the scope's mark.
 */
typedef struct Arena {
    wc_allocator        backing; // where `base` came from (unused when !owns_base)
    u8*                 base;
    u64                 size;      // capacity in bytes
    u64                 idx;       // next free offset
    u64                 floor;     // in-place realloc/free never touch blocks below this
    const struct Arena* self;      // == this arena while live; NULL after destroy
    b8                  owns_base; // 0 for Arena_create_buf // TODO: remove this non-owning bullshit
} Arena;

_Static_assert(sizeof(Arena) == 64, "Arena is one cache line");


// Tweakable settings // TODO: isnt this the same as the alignment def in wc_allocator.h?
#ifndef ARENA_DEFAULT_ALIGNMENT
#define ARENA_DEFAULT_ALIGNMENT (sizeof(void*)) // 8 bytes
#endif
#ifndef ARENA_DEFAULT_SIZE
#define ARENA_DEFAULT_SIZE (nKB(4)) // 4 KB
#endif


// Lifecycle

// Initialise `arena` in place with `capacity` bytes taken from `backing`
// (capacity 0 -> ARENA_DEFAULT_SIZE). Fatal if the backing allocation fails.
void Arena_create(Arena* arena, wc_allocator backing, u64 capacity) __attribute__((nonnull(1)));

// Initialise `arena` over caller-owned memory (a stack array, a static buffer...).
// `buf` needs no particular alignment: allocations are aligned by address.
void Arena_create_buf(Arena* arena, void* buf, u64 size) __attribute__((nonnull(1, 2)));

// Free the region through the backing allocator (only if owned) and zero the
// struct. Safe on a zeroed or already-destroyed Arena.
void Arena_destroy(Arena* arena) __attribute__((nonnull(1)));

// Forget every allocation (idx = floor = 0). Keeps the region.
void Arena_reset(Arena* arena) __attribute__((nonnull(1)));


// Allocation. Return NULL and set wc_errno = WC_ERR_FULL when the arena is full.

// Aligned to ARENA_DEFAULT_ALIGNMENT.
void* Arena_alloc(Arena* arena, u64 size) __attribute__((nonnull(1), alloc_size(2)));

// `align` must be a power of two >= 1. Alignment is by ADDRESS (A2).
void* Arena_alloc_aligned(Arena* arena, u64 size, u64 align) __attribute__((nonnull(1), alloc_size(2)));


static inline __attribute__((nonnull(1))) u64 Arena_used(const Arena* arena)
{
    return arena->idx;
}

static inline __attribute__((nonnull(1))) u64 Arena_remaining(const Arena* arena)
{
    return arena->size - arena->idx;
}


// Allocator views

// The arena as a wc_allocator: in-place realloc of the last block, last-block
// free, everything else is bump + copy.
wc_allocator Arena_allocator(Arena* arena) __attribute__((nonnull(1)));



// Scratch scopes

typedef struct {
    Arena* arena;
    u64    mark;       // idx at scope start
    u64    prev_floor; // floor to restore at scope end
} ArenaScratch;

// Save the position and raise the floor to it (D10). Scopes nest; end them in LIFO order.
static inline __attribute__((nonnull(1))) ArenaScratch Arena_scratch_begin(Arena* arena)
{
    ArenaScratch s = {.arena = arena, .mark = arena->idx, .prev_floor = arena->floor};
    arena->floor   = arena->idx;
    return s;
}

static inline void Arena_scratch_end(ArenaScratch scratch)
{
    if (scratch.arena) {
        scratch.arena->idx   = scratch.mark;
        scratch.arena->floor = scratch.prev_floor;
    }
}

static inline void wc_arena_scratch_cleanup(ArenaScratch* s)
{
    if (s && s->arena) {
        Arena_scratch_end(*s);
        s->arena = NULL;
    }
}

// Scoped scratch: restored on scope exit, including return/break/goto.
#define ARENA_SCRATCH(arena_ptr)                                                                                     \
    for (int _as_once = 1; _as_once; _as_once = 0)                                                                   \
        for (ArenaScratch __attribute__((cleanup(wc_arena_scratch_cleanup))) _as_s = Arena_scratch_begin(arena_ptr); \
             _as_once; _as_once                                                    = 0)

/* USAGE:
// Manual:
ArenaScratch scratch = Arena_scratch_begin(&arena);
char* tmp = ARENA_ALLOC_N(&arena, char, 256);
Arena_scratch_end(scratch);

// Automatic:
ARENA_SCRATCH(&arena) {
    char* tmp = ARENA_ALLOC_N(&arena, char, 256);
} // auto cleanup
*/


// SCOPED ARENA
//
// ARENA_SCOPE(name, cap) { ... }
// Creates a libc-backed Arena of `cap` bytes, exposes it inside the block as
// `wc_allocator name`, and destroys it when the block exits (normal exit,
// break, return or goto). Everything allocated from `name` dies with the
// block: never let a container built on it escape. `break` leaves the scope.
//
//   ARENA_SCOPE(tmp, nKB(64)) {
//       GenVec v = VEC_OF_IN(tmp, int, 16);
//       ...                       // no destroy needed
//   }
#define ARENA_SCOPE(name, cap) ARENA_SCOPE_(name, (cap), WC_CAT(_asc_, __COUNTER__))
#define ARENA_SCOPE_(name, cap, id)                                                                             \
    for (int WC_CAT(id, _once) = 1; WC_CAT(id, _once); WC_CAT(id, _once) = 0)                                   \
        for (Arena __attribute__((cleanup(Arena_destroy))) WC_CAT(id, _arena) = {0}; WC_CAT(id, _once);         \
             WC_CAT(id, _once)                                                = 0)                              \
            for (wc_allocator name =                                                                            \
                     (Arena_create(&WC_CAT(id, _arena), WC_LIBC, (cap)), Arena_allocator(&WC_CAT(id, _arena))); \
                 WC_CAT(id, _once); WC_CAT(id, _once) = 0)


// USEFUL MACROS

// Arena over an anonymous stack buffer of `nbytes` (lives until the enclosing block ends).
#define ARENA_CREATE_BUF(arena_ptr, nbytes) Arena_create_buf((arena_ptr), (u8[nbytes]){0}, (nbytes))

// typed allocation
#define ARENA_ALLOC(arena, T) ((T*)Arena_alloc_aligned((arena), sizeof(T), alignof(T)))

#define ARENA_ALLOC_N(arena, T, n) ((T*)Arena_alloc_aligned((arena), wc_mul((n), sizeof(T)), alignof(T)))

// common for structs
#define ARENA_ALLOC_ZERO(arena, T)                      \
    ({                                                  \
        T* _az = ARENA_ALLOC(arena, T);                 \
        _az ? (T*)memset(_az, 0, sizeof(T)) : (T*)NULL; \
    })

#define ARENA_ALLOC_ZERO_N(arena, T, n)                        \
    ({                                                         \
        u64 _azn = (u64)(n);                                   \
        T*  _az  = ARENA_ALLOC_N(arena, T, _azn);              \
        _az ? (T*)memset(_az, 0, sizeof(T) * _azn) : (T*)NULL; \
    })

// Allocate and copy array into Arena
#define ARENA_PUSH_ARRAY(arena, T, src, count)     \
    ({                                             \
        u64 _apc = (u64)(count);                   \
        T*  _dst = ARENA_ALLOC_N(arena, T, _apc);  \
        if (_dst) {                                \
            memcpy(_dst, (src), sizeof(T) * _apc); \
        }                                          \
        _dst;                                      \
    })


#endif // ARENA_H
