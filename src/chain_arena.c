#include "chain_arena.h"
#include "common.h"
#include "wc_allocator.h"
#include "wc_errno.h"

#include <stdint.h>
#include <string.h>
#include "wc_poison.h" // must stay last: bans raw malloc/free below


#define NODE_HEADER  ((u64)sizeof(ChainArenaNode))
#define NODE_ALIGN   WC_MAX_ALIGN
#define REGULAR_CAP  ((u64)CHAIN_ARENA_NODE_SIZE - NODE_HEADER)

_Static_assert(CHAIN_ARENA_NODE_SIZE > sizeof(ChainArenaNode) + 64, "CHAIN_ARENA_NODE_SIZE too small");

#define CHAIN_CHECK_LIVE(a) \
    CHECK_FATAL((a)->self != (a), "ChainArena used after destroy, or copied/moved after create")


// Node helpers

static ChainArenaNode* node_new(const ChainArena* arena, u64 cap)
{
    if (cap > (u64)PTRDIFF_MAX - NODE_HEADER) {
        return NULL;
    }
    ChainArenaNode* n = wc_alloc(arena->backing, NODE_HEADER + cap, NODE_ALIGN);
    if (!n) {
        return NULL;
    }
    n->prev = NULL;
    n->next = NULL;
    n->cap  = cap;
    n->used = 0;
    return n;
}

static void node_free(const ChainArena* arena, ChainArenaNode* n)
{
    wc_free(arena->backing, n, NODE_HEADER + n->cap, NODE_ALIGN);
}

// Free `n` and every node after it.
static void free_from(const ChainArena* arena, ChainArenaNode* n)
{
    while (n) {
        ChainArenaNode* next = n->next;
        node_free(arena, n);
        n = next;
    }
}

// Offset in n->data where a `size`-byte block aligned (by address) to `align`
// would start, or UINT64_MAX if it does not fit.
static inline u64 node_fit(const ChainArenaNode* n, u64 size, u64 align)
{
    uintptr_t data    = (uintptr_t)n->data;
    uintptr_t aligned = (data + n->used + (align - 1)) & ~(uintptr_t)(align - 1);
    u64       off     = (u64)(aligned - data);
    if (off > n->cap || n->cap - off < size) {
        return UINT64_MAX;
    }
    return off;
}

// Is [p, p + size) the last block of the tail node, at or above the floor?
static inline b8 is_top(const ChainArena* arena, const u8* p, u64 size)
{
    const ChainArenaNode* t = arena->tail;
    if (p < t->data || p + size != t->data + t->used) {
        return 0;
    }
    // Nodes are only ever entered moving forward from floor_node, so a tail
    // other than floor_node is entirely above the floor.
    return arena->floor_node != t || p >= t->data + arena->floor_used;
}


// Was `p` allocated before the innermost scratch scope began? (debug checks only)
static __attribute__((unused)) b8 is_below_floor(const ChainArena* arena, const u8* p)
{
    const ChainArenaNode* f = arena->floor_node;
    if (!f) {
        return 0;
    }
    if (p >= f->data && p < f->data + f->cap) {
        return p < f->data + arena->floor_used;
    }
    for (const ChainArenaNode* n = f->prev; n; n = n->prev) {
        if (p >= n->data && p < n->data + n->cap) {
            return 1;
        }
    }
    return 0;
}


// Lifecycle

void ChainArena_create(ChainArena* arena, wc_allocator backing)
{
    *arena = (ChainArena){.backing = backing};

    ChainArenaNode* first = node_new(arena, REGULAR_CAP);
    if (!first) {
        FATAL("ChainArena first node allocation failed"); // D5: unconditional
    }
    arena->head = first;
    arena->tail = first;
    arena->self = arena;
}

void ChainArena_destroy(ChainArena* arena)
{
    if (arena->self == NULL) {
        return; // zeroed or already destroyed
    }
    CHAIN_CHECK_LIVE(arena);

    free_from(arena, arena->head);
    memset(arena, 0, sizeof(*arena));
}

void ChainArena_reset(ChainArena* arena)
{
    CHAIN_CHECK_LIVE(arena);

    free_from(arena, arena->head->next);
    arena->head->next = NULL;
    arena->head->used = 0;
    arena->tail       = arena->head;
    arena->used       = 0;
    arena->floor_node = NULL;
    arena->floor_used = 0;
}

void ChainArena_clear(ChainArena* arena)
{
    CHAIN_CHECK_LIVE(arena);

    for (ChainArenaNode* n = arena->head; n; n = n->next) {
        n->used = 0;
    }
    arena->tail       = arena->head;
    arena->used       = 0;
    arena->floor_node = NULL;
    arena->floor_used = 0;
}


// Allocation

u8* ChainArena_alloc_aligned(ChainArena* arena, u64 size, u64 align)
{
    CHAIN_CHECK_LIVE(arena);
    CHECK_FATAL(size == 0, "allocation size must be > 0");
    CHECK_FATAL(align == 0 || (align & (align - 1)) != 0, "alignment must be a power of two");

    ChainArenaNode* node = arena->tail;
    u64             off  = node_fit(node, size, align);

    if (off == UINT64_MAX) {
        // Reuse the next node if it is spare (after clear) and big enough,
        // otherwise insert a fresh node right after the tail.
        ChainArenaNode* spare = node->next;
        if (spare && (off = node_fit(spare, size, align)) != UINT64_MAX) {
            node = spare;
        } else {
            WC_SET_RET(WC_ERR_FULL, size > UINT64_MAX - align, NULL);
            u64 need = size + align - 1; // worst-case padding at the start of a node (A3)
            u64 cap  = need > REGULAR_CAP ? need : REGULAR_CAP;

            ChainArenaNode* fresh = node_new(arena, cap);
            WC_SET_RET(WC_ERR_FULL, !fresh, NULL);

            fresh->prev = node;
            fresh->next = node->next;
            if (node->next) {
                node->next->prev = fresh;
            }
            node->next = fresh;

            node = fresh;
            off  = node_fit(node, size, align); // cannot fail: cap >= size + align - 1
        }
        arena->tail = node;
    }

    arena->used += (off + size) - node->used;
    node->used = off + size;
    return node->data + off;
}


// Allocator backend

static void* chain_vt_alloc(void* ctx, size_t size, size_t align)
{
    return ChainArena_alloc_aligned((ChainArena*)ctx, size, align);
}

static void* chain_vt_realloc(void* ctx, void* ptr, size_t old_size, size_t new_size, size_t align)
{
    ChainArena* arena = ctx;
    u8*         p     = ptr;
    CHAIN_CHECK_LIVE(arena);

    // Same rule as Arena: growing a block that predates the innermost scratch
    // scope would leave it dangling (or truncated) at scope end.
    CHECK_FATAL(new_size > old_size && is_below_floor(arena, p),
                "growing a block allocated before the current scratch scope (it would dangle at scope end)");

    if (is_top(arena, p, old_size)) {
        ChainArenaNode* t   = arena->tail;
        u64             off = (u64)(p - t->data);
        if (t->cap - off >= new_size) {
            arena->used = arena->used - t->used + (off + new_size);
            t->used     = off + new_size;
            return p; // grown or shrunk in place
        }
    } else if (new_size <= old_size) {
        return p; // shrinking a block that is not on top: keep it
    }

    u8* q = ChainArena_alloc_aligned(arena, new_size, align);
    if (!q) {
        return NULL; // p stays valid
    }
    memcpy(q, p, old_size < new_size ? old_size : new_size);
    return q;
}

static void chain_vt_free(void* ctx, void* ptr, size_t size, size_t align)
{
    (void)align;
    ChainArena* arena = ctx;
    u8*         p     = ptr;
    CHAIN_CHECK_LIVE(arena);

    if (is_top(arena, p, size)) {
        ChainArenaNode* t   = arena->tail;
        u64             off = (u64)(p - t->data);
        arena->used -= t->used - off;
        t->used = off;
    }
}

// Positional on purpose: the field names `realloc`/`free` are poisoned in
// library sources (wc_poison.h). Order matches wc_alloc_vtable.
static const wc_alloc_vtable chain_vt = {
    chain_vt_alloc,   // alloc
    chain_vt_realloc, // realloc
    chain_vt_free,    // free
};

wc_allocator ChainArena_allocator(ChainArena* arena)
{
    CHAIN_CHECK_LIVE(arena);
    return (wc_allocator){.vt = &chain_vt, .ctx = arena};
}


// Scratch

ChainArenaScratch ChainArena_scratch_begin(ChainArena* arena)
{
    CHAIN_CHECK_LIVE(arena);

    ChainArenaScratch s = {
        .arena           = arena,
        .node            = arena->tail,
        .node_used       = arena->tail->used,
        .used            = arena->used,
        .prev_floor_node = arena->floor_node,
        .prev_floor_used = arena->floor_used,
    };
    arena->floor_node = arena->tail;
    arena->floor_used = arena->tail->used;
    return s;
}

void ChainArena_scratch_end(ChainArenaScratch scratch)
{
    ChainArena* arena = scratch.arena;
    if (!arena) {
        return;
    }
    CHAIN_CHECK_LIVE(arena);

    // Nodes after the mark were appended (or reused) inside the scope: release them.
    free_from(arena, scratch.node->next);
    scratch.node->next = NULL;
    scratch.node->used = scratch.node_used;

    arena->tail       = scratch.node;
    arena->used       = scratch.used;
    arena->floor_node = scratch.prev_floor_node;
    arena->floor_used = scratch.prev_floor_used;
}
