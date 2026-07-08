#ifndef CHAIN_ARENA_H
#define CHAIN_ARENA_H

#include "common.h"


#ifndef ARENA_DEFAULT_ALIGNMENT
    #define ARENA_DEFAULT_ALIGNMENT (sizeof(void*)) // 8 bytes
#endif

#ifndef ARENA_DEFAULT_SIZE
    #define ARENA_DEFAULT_SIZE (nKB(4))      // 4 KB
#endif

#ifndef ARENA_NODE_INLINE_SIZE
    #define ARENA_NODE_INLINE_SIZE (nKB(1))
#endif


typedef struct ArenaNode
{
    // TODO: do we ever need dynamic size for base in this type of arena?
    u8 base[ARENA_NODE_INLINE_SIZE];
    u64 idx;
    struct ArenaNode* next;
} ArenaNode;

typedef struct {
    ArenaNode* head;
    ArenaNode* curr;
    u64 num_nodes;
} ChainArena;



ChainArena* chain_arena_create(void);

void chain_arena_release(ChainArena* arena);

void chain_arena_alloc(ChainArena* arena, u64 size);



#endif // CHAIN_ARENA_H
