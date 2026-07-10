#ifndef CHAIN_ARENA_H
#define CHAIN_ARENA_H

#include "common.h"


#ifndef ARENA_DEFAULT_ALIGNMENT
    #define ARENA_DEFAULT_ALIGNMENT (sizeof(void*)) // 8 bytes
#endif

#ifndef ARENA_NODE_INLINE_SIZE
    #define ARENA_NODE_INLINE_SIZE (nKB(4))     // 4096 bytes
#endif


typedef struct ArenaNode
{
    // TODO: do we ever need dynamic size for base in this type of arena?
    u8  base[ARENA_NODE_INLINE_SIZE];
    u64 used;    // per-node index - when new allocation needs more bytes than left in curr node, we create a new node and allocate there
    struct ArenaNode* next;
} ArenaNode;

// TODO: should we switch to dynamic array of nodes? then we can easily find which node holds a certain global index
typedef struct {
    ArenaNode** nodes;  // TODO: compete dynmaic arry imp
    u32 size;
    u32 cap;
    u64 idx;    // per-arena index - tracks total bytes used. Also used for marking save points (scratch)
    // TODO: instead of having idx in each node, we can have one here, when idx=ARENA_NODE_INLINE_SIZE,
    // then we create a new node and just continue. if user wants to read a particular offset, we can
    // calculate which node it resides in and hand it over
} ChainArena;

// scratch space with partial clearing arena
typedef struct {
    ChainArena* arena;
    u64 mark;       // ending scratch space clears arena upto mark
} ChainArenaScratch;
// the idea is to have a `save point` where you can go back to after doing scratch work
// you save a mark, do some work (eg per frame work in a game loop) then reset back to mark at the end of loop


ChainArena* chain_arena_create(void);

void chain_arena_release(ChainArena* arena);

// void chain_arena_create_stk(ChainArena* arena);
// void chain_arena_release_stk(ChainArena* arena);


// allocate `size` bytes with default alignment
u8* chain_arena_alloc(ChainArena* arena, u64 size);

// allocate `size` bytes with custom alignment `align`
void chain_arena_alloc_aligned(ChainArena* arena, u64 size, u32 align);

// reset arena back to inital state. with a single node
void chain_arena_reset(ChainArena* arena);

// clear all nodes but keep allocated nodes
static inline void chain_arena_clear(ChainArena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    arena->idx = 0;
}

static inline ChainArenaScratch chain_arena_scratch_begin(ChainArena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    return (ChainArenaScratch){ .arena = arena, .mark = arena->idx };

}

static inline void chain_arena_scratch_end(ChainArenaScratch scratch)
{
    if (scratch.arena) {
        scratch.arena->idx = scratch.mark;
        scratch.arena = NULL;
    }
}

// macro for automatic cleanup after scope end
#define CHAIN_ARENA_SCRATCH(c_arena_ptr) \
    for (ChainArenaScratch __nme__ = chain_arena_scratch_begin(c_arena_ptr); \
         (__nme__ ).arena != NULL; \
         chain_arena_scratch_end((__nme__ )), (__nme__).arena = NULL)

/* USAGE:
// Manual:
ChainScratchArena scratch = chain_arena_scratch_begin(arena);
char* tmp = ARENA_ALLOC_N(arena, char, 256);
chain_arena_scratch_end(scratch);

// Automatic:
CHAIN_ARENA_SCRATCH(arena) {
    char* tmp = ARENA_ALLOC_N(arena, char, 256);
} // auto cleanup at scope end
*/


// USEFULL MACROS

// typed allocation
#define CHAIN_ARENA_ALLOC(arena, T) ((T*)chain_arena_alloc((arena), sizeof(T)))

#define CHAIN_ARENA_ALLOC_N(arena, T, n) ((T*)chain_arena_alloc((arena), sizeof(T) * (n)))

// common for structs
#define CHAIN_ARENA_ALLOC_ZERO(arena, T)\
((T*)memset(CHAIN_ARENA_ALLOC(arena, T), 0, sizeof(T)))

#define CHAIN_ARENA_ALLOC_ZERO_N(arena, T, n)\
((T*)memset(CHAIN_ARENA_ALLOC_N(arena, T, n), 0, sizeof(T) * (n)))



#endif // CHAIN_ARENA_H
