#ifndef CHAIN_ARENA_H
#define CHAIN_ARENA_H

#include "common.h"
#include "wc_allocator.h"

#include <stdalign.h>
#include <string.h>


#ifndef ARENA_DEFAULT_ALIGNMENT
#define ARENA_DEFAULT_ALIGNMENT (sizeof(void*)) // 8 bytes
#endif

// Total bytes per regular node, header included (one backing allocation per node).
#ifndef CHAIN_ARENA_NODE_SIZE
#define CHAIN_ARENA_NODE_SIZE (nKB(4))
#endif


/*
 * ChainArena: growing bump allocator made of nodes taken from a backing allocator.
 *
 * - Nodes form an intrusive doubly linked list; no dependency on any container.
 * - Requests larger than a regular node get a dedicated node sized to fit (A3).
 * - Alignment is by address (A2).
 * - In-place realloc / last-block free in the current node, guarded by a floor
 *   exactly like Arena (plan D10).
 * - ChainArena_clear keeps every node and reuses them in order.
 *
 * PINNED: its address is the allocator ctx. Do not copy or move after create.
 */
typedef struct ChainArenaNode {
    struct ChainArenaNode* prev;
    struct ChainArenaNode* next;
    u64                    cap;  // usable bytes in data[]
    u64                    used; // bytes consumed in data[]
    u8                     data[];
} ChainArenaNode;

typedef struct ChainArena {
    wc_allocator             backing;
    ChainArenaNode*          head;       // first node, kept by reset
    ChainArenaNode*          tail;       // current node: allocations happen here
    u64                      used;       // bytes consumed in head..tail (padding included)
    ChainArenaNode*          floor_node; // NULL: no floor
    u64                      floor_used; // floor offset inside floor_node
    const struct ChainArena* self;       // == this while live; NULL after destroy
} ChainArena;

typedef struct {
    ChainArena*     arena;
    ChainArenaNode* node;      // tail at scope start
    u64             node_used; // its `used` at scope start
    u64             used;      // arena->used at scope start
    ChainArenaNode* prev_floor_node;
    u64             prev_floor_used;
} ChainArenaScratch;


// Lifecycle (out-param: the struct is pinned)

// Creates the first node from `backing`. Fatal if that allocation fails.
void ChainArena_create(ChainArena* arena, wc_allocator backing) __attribute__((nonnull(1)));

// Frees every node through the backing allocator and zeroes the struct.
// Safe on a zeroed or already-destroyed ChainArena.
void ChainArena_destroy(ChainArena* arena) __attribute__((nonnull(1)));

// Free every node except the first, which is emptied.
void ChainArena_reset(ChainArena* arena) __attribute__((nonnull(1)));

// Empty every node but keep them all for reuse.
void ChainArena_clear(ChainArena* arena) __attribute__((nonnull(1)));


// Allocation. NULL (wc_errno = WC_ERR_FULL) only if the backing allocator fails.

u8* ChainArena_alloc_aligned(ChainArena* arena, u64 size, u64 align) __attribute__((nonnull(1), alloc_size(2)));

static inline __attribute__((nonnull(1), alloc_size(2))) u8* ChainArena_alloc(ChainArena* arena, u64 size)
{
    return ChainArena_alloc_aligned(arena, size, ARENA_DEFAULT_ALIGNMENT);
}

static inline __attribute__((nonnull(1))) u64 ChainArena_used(const ChainArena* arena)
{
    return arena->used;
}

wc_allocator ChainArena_allocator(ChainArena* arena) __attribute__((nonnull(1)));


// Scratch: save the position and raise the floor; end frees nodes appended after
// the mark. Scopes nest; end them in LIFO order. Do not reset/clear inside a scope.

ChainArenaScratch ChainArena_scratch_begin(ChainArena* arena) __attribute__((nonnull(1)));

void ChainArena_scratch_end(ChainArenaScratch scratch);

static inline void wc_chain_arena_scratch_cleanup(ChainArenaScratch* s)
{
    if (s && s->arena) {
        ChainArena_scratch_end(*s);
        s->arena = NULL;
    }
}

#define CHAIN_ARENA_SCRATCH(c_arena_ptr)                                                                              \
    for (int _cas_once = 1; _cas_once; _cas_once = 0)                                                                 \
        for (ChainArenaScratch                                                                                        \
             __attribute__((cleanup(wc_chain_arena_scratch_cleanup))) _cas_s = ChainArena_scratch_begin(c_arena_ptr); \
             _cas_once; _cas_once                                            = 0)


// Typed allocation macros
#define CHAIN_ARENA_ALLOC(arena, T)      ((T*)ChainArena_alloc_aligned((arena), sizeof(T), alignof(T)))
#define CHAIN_ARENA_ALLOC_N(arena, T, n) ((T*)ChainArena_alloc_aligned((arena), wc2_mul((n), sizeof(T)), alignof(T)))
#define CHAIN_ARENA_ALLOC_ZERO(arena, T)                  \
    ({                                                    \
        (T)* _caz = CHAIN_ARENA_ALLOC(arena, T);            \
        _caz ? (T*)memset(_caz, 0, sizeof(T)) : (T*)NULL; \
    })
#define CHAIN_ARENA_ALLOC_ZERO_N(arena, T, n)                     \
    ({                                                            \
        u64 _cazn = (u64)(n);                                     \
        (T)*  _caz  = CHAIN_ARENA_ALLOC_N(arena, T, _cazn);         \
        _caz ? (T*)memset(_caz, 0, sizeof(T) * _cazn) : (T*)NULL; \
    })

#endif // CHAIN_ARENA_H
