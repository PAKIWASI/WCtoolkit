#include "hashset.h"
#include "common.h"
#include "map_setup.h"
#include "wc_allocator.h"

#include <stdio.h>
#include <string.h>


#define GET_ELM(set, i) ((set)->elms + ((u64)(set)->elm_size * (i)))
#define GET_PSL(set, i) ((set)->psls + (i))

// capacity is always power-of-2: use bitmask instead of %
#define SET_MASK(set)     ((set)->capacity - 1)
#define SET_IDX(set, elm) ((set)->hash_fn((elm), (set)->elm_size) & SET_MASK(set))
#define SET_NEXT(set, i)  (((i) + 1) & SET_MASK(set))

// PSL 0 == empty bucket; stored PSL is (real_psl + 1), starting at 1
#define BUCKET_EMPTY 0

// A stored PSL is a u8 where 0 means empty: a probe past this means the hash
// function is degenerate. Die loudly instead of wrapping to BUCKET_EMPTY.
#define PSL_LIMIT 250

// Single block layout (rule A9):
//   [elms: cap * elm_size][scratch: 2 * EPAD][psls: cap bytes]
// scratch: [0, EPAD) = stage, [EPAD, 2 * EPAD) = swap.
// stage: where insert puts the incoming element before set_insert
// swap:  where set_insert saves a displaced resident during Robin Hood eviction
// The two halves alternate on each eviction so the element in hand never
// aliases the buffer being written into.

static inline u64 set_align_up(u64 x, u64 a)
{
    return (x + (a - 1)) & ~(a - 1);
}

#define ELM_ALIGN(set) wc_align_for_size((set)->elm_size)
#define EPAD(set)      set_align_up((set)->elm_size, ELM_ALIGN(set))
#define STAGE_ELM(set) ((set)->scratch)
#define SWAP_ELM(set)  ((set)->scratch + EPAD(set))

typedef struct {
    u64 scratch, psls, total; // byte offsets from the block start, and its size
} set_layout;

static inline set_layout set_layout_for(u64 cap, u32 elm_size)
{
    u64        al   = wc_align_for_size(elm_size);
    u64        epad = set_align_up(elm_size, al);
    set_layout l;
    l.scratch = set_align_up(cap * elm_size, al);
    l.psls    = l.scratch + (2 * epad);
    l.total   = l.psls + cap;
    return l;
}

// One allocation for the whole table; psls zeroed (all buckets empty).
static void set_alloc_block(HashSet* set, u64 cap)
{
    set_layout l = set_layout_for(cap, set->elm_size);
    u8*        b = wc_alloc(set->alloc, l.total, ELM_ALIGN(set));
    FATAL_IF(!b, "HashSet: table allocation of %llu bytes failed", (unsigned long long)l.total);

    set->elms     = b;
    set->scratch  = b + l.scratch;
    set->psls     = b + l.psls;
    set->capacity = cap;
    memset(set->psls, 0, cap);
}

static void set_free_block(const HashSet* set, u8* block, u64 cap)
{
    wc_free(set->alloc, block, set_layout_for(cap, set->elm_size).total, ELM_ALIGN(set));
}

// Delete every live element in place.
static void set_delete_live(HashSet* set)
{
    wc_delete_fn e_del = SET_DEL(set->ops);
    if (!e_del) {
        return;
    }
    for (u64 i = 0; i < set->capacity; i++) {
        if (*GET_PSL(set, i) != BUCKET_EMPTY) {
            e_del(GET_ELM(set, i));
        }
    }
}


/*
====================PRIVATE DECLARATIONS====================
*/

static u64         set_lookup(const HashSet* set, const u8* elm, LOOKUP_RES* res, u8* out_psl);
static void        set_insert(HashSet* set, u8* elm, u8 psl, u64 idx);
static void        set_resize(HashSet* set, u64 new_capacity);
static inline void set_maybe_resize(HashSet* set);


/*
====================PUBLIC FUNCTIONS====================
*/

HashSet HashSet_create(const wc_allocator* a, u32 elm_size, custom_hash_fn hash_fn, wc_compare_fn cmp_fn,
                       const wc_container_ops* ops)
{
    FATAL_IF(elm_size == 0, "elm_size can't be 0");

    HashSet set = {
        .size     = 0,
        .elm_size = elm_size,
        .hash_fn  = hash_fn ? hash_fn : wyhash,
        .cmp_fn   = cmp_fn ? cmp_fn : default_compare,
        .ops      = ops,
        .alloc    = a,
    };
    set_alloc_block(&set, HASHMAP_INIT_CAPACITY); // the one allocation
    return set;
}


void HashSet_destroy(HashSet* set)
{
    // Safe on a zeroed / moved-from struct (capacity == 0).
    if (!set->capacity) {
        return;
    }

    set_delete_live(set);
    set_free_block(set, set->elms, set->capacity);
    memset(set, 0, sizeof(*set));
}


void HashSet_move(HashSet* dest, HashSet* src)
{
    if (dest == src) {
        return;
    }
    memcpy(dest, src, sizeof(HashSet));
    memset(src, 0, sizeof(HashSet));
}


// Deep copy src into a new set allocated from `a` (A14). Same capacity, so
// every element keeps its bucket: no rehash.
HashSet HashSet_copy(const wc_allocator* a, const HashSet* src)
{
    if (src->capacity == 0) {
        return (HashSet){0};
    }

    HashSet dest = *src; // sizes, functions, ops
    dest.alloc   = a;
    set_alloc_block(&dest, src->capacity);

    wc_copy_fn e_cp = SET_COPY(src->ops);
    if (!e_cp) {
        // identical layout: one memcpy of the whole block
        memcpy(dest.elms, src->elms, set_layout_for(src->capacity, src->elm_size).total);
        return dest;
    }

    for (u64 i = 0; i < src->capacity; i++) {
        u8 psl = *GET_PSL(src, i);
        if (psl == BUCKET_EMPTY) {
            continue;
        }
        *GET_PSL(&dest, i) = psl;
        e_cp(a, GET_ELM(&dest, i), GET_ELM(src, i));
    }

    return dest;
}


// Insert element — COPY semantics (B8).
// Returns 1 if already existed (no-op), 0 if newly inserted.
bool HashSet_insert(HashSet* set, const void* elm)
{
    FATAL_IF(set->capacity == 0, "HashSet_insert called on zero-state HashSet");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = set_lookup(set, elm, &res, &out_psl);

    if (res == FOUND) {
        return 1;
    }

    // Stage a deep copy; set_insert only relocates raw bytes (B5).
    wc_copy_fn e_cp = SET_COPY(set->ops);
    if (e_cp) {
        e_cp(set->alloc, STAGE_ELM(set), elm);
    } else {
        memcpy(STAGE_ELM(set), elm, set->elm_size);
    }

    set_insert(set, STAGE_ELM(set), out_psl, slot);
    set_maybe_resize(set);
    return 0;
}


// Insert element — MOVE semantics (*elm zeroed on insert, or destroyed if duplicate, B10).
// Returns 1 if already existed (elm destroyed), 0 if newly inserted.
bool HashSet_insert_move(HashSet* set, void* elm)
{
    FATAL_IF(set->capacity == 0, "HashSet_insert_move called on zero-state HashSet");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = set_lookup(set, elm, &res, &out_psl);

    if (res == FOUND) {
        wc_delete_fn e_del = SET_DEL(set->ops);
        if (e_del) {
            e_del(elm);
        }
        memset(elm, 0, set->elm_size);
        return 1;
    }

    memcpy(STAGE_ELM(set), elm, set->elm_size); // move: memcpy + zero (B6)
    memset(elm, 0, set->elm_size);

    set_insert(set, STAGE_ELM(set), out_psl, slot);
    set_maybe_resize(set);
    return 0;
}


// Returns 1 if found, 0 if not.
bool HashSet_has(const HashSet* set, const void* elm)
{
    if (!set->capacity) {
        return 0;
    }
    LOOKUP_RES res;
    u8         out_psl;
    set_lookup(set, elm, &res, &out_psl);
    return res == FOUND;
}

const void* HashSet_get_ptr(const HashSet* set, const void* elm)
{
    if (!set->capacity) {
        return NULL;
    }
    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = set_lookup(set, elm, &res, &out_psl);
    return (res == FOUND) ? GET_ELM(set, slot) : NULL;
}

bool HashSet_bucket_occupied(const HashSet* set, u64 i)
{
    WC_ASSERT(i < set->capacity, "index out of bounds");
    return *GET_PSL(set, i) != BUCKET_EMPTY;
}

const void* HashSet_bucket_elm_ptr(const HashSet* set, u64 i)
{
    WC_ASSERT(i < set->capacity, "index out of bounds");
    return GET_ELM(set, i);
}


// Returns 1 if found and removed, 0 if not found.
// Robin Hood backward-shift deletion keeps the probe-sequence invariant
// without tombstones: after removing a slot, shift subsequent entries back one
// position as long as they have PSL > 1 (i.e. they are not at their home slot).
bool HashSet_remove(HashSet* set, const void* elm)
{
    FATAL_IF(set->capacity == 0, "HashSet_remove called on zero-state HashSet");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = set_lookup(set, elm, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    wc_delete_fn e_del = SET_DEL(set->ops);
    if (e_del) {
        e_del(GET_ELM(set, slot));
    }

    u64 cur = slot;
    for (;;) {
        u64 next     = SET_NEXT(set, cur);
        u8  next_psl = *GET_PSL(set, next);

        if (next_psl <= 1) {
            *GET_PSL(set, cur) = BUCKET_EMPTY;
            break;
        }

        *GET_PSL(set, cur) = next_psl - 1;
        memcpy(GET_ELM(set, cur), GET_ELM(set, next), set->elm_size);

        cur = next;
    }

    set->size--;
    return 1;
}


// Print all elements.
void HashSet_print(const HashSet* set, wc_print_fn print)
{
    printf("\t=========\n");
    printf("\tSize: %llu / Capacity: %llu\n", (unsigned long long)set->size, (unsigned long long)set->capacity);
    printf("\t=========\n");

    for (u64 i = 0; i < set->capacity; i++) {
        if (*GET_PSL(set, i) == BUCKET_EMPTY) {
            continue;
        }
        putchar('\t');
        print(GET_ELM(set, i));
        putchar('\n');
    }

    printf("\t=========\n");
}


// Remove all elements, keep capacity.
void HashSet_clear(HashSet* set)
{
    FATAL_IF(set->capacity == 0, "HashSet_clear called on zero-state HashSet");

    set_delete_live(set);
    memset(set->psls, 0, set->capacity);
    set->size = 0;
}


// Make room for n elements in total without any further resize.
void HashSet_reserve(HashSet* set, u64 n)
{
    FATAL_IF(set->capacity == 0, "HashSet_reserve on zeroed/moved-from set");
    FATAL_IF(n > ((u64)1 << 56), "HashSet_reserve: n too large");

    u64 need = set->capacity;
    while (n * 4 >= need * 3) {
        need *= 2;
    }
    if (need > set->capacity) {
        set_resize(set, need);
    }
}


/*
====================PRIVATE FUNCTIONS====================
*/

static inline void set_maybe_resize(HashSet* set)
{
    // integer multiply avoids float: equivalent to load > 0.75
    if (set->size * 4 >= set->capacity * 3) {
        set_resize(set, set->capacity * 2);
    }
}


static u64 set_lookup(const HashSet* set, const u8* elm, LOOKUP_RES* res, u8* out_psl)
{
    u64 idx = SET_IDX(set, elm);
    u8  psl = 1; // stored PSL=1 means real probe distance 0 (home slot)

    for (u64 i = idx;; i = SET_NEXT(set, i)) {
        u8 slot_psl = *GET_PSL(set, i);
        *out_psl    = psl;

        if (slot_psl == BUCKET_EMPTY) {
            *res = NOT_FOUND;
            return i;
        }

        if (slot_psl < psl) {
            // The resident was inserted closer to home than we are:
            // our elm can't be further ahead (Robin Hood invariant).
            *res = ROBINHOOD_EXIT;
            return i;
        }

        if (set->cmp_fn(GET_ELM(set, i), elm, set->elm_size) == 0) {
            *res = FOUND;
            return i;
        }

        psl++;
    }
}


// Place `elm` (already owned by the set) starting at bucket idx with probe
// length psl. Only relocates raw bytes: never calls copy/del (B5).
static void set_insert(HashSet* set, u8* elm, u8 psl0, u64 idx)
{
    u8* cur = STAGE_ELM(set);
    u8* swp = SWAP_ELM(set);

    // elm may already be STAGE_ELM (called from insert/insert_move)
    if (elm != cur) {
        memcpy(cur, elm, set->elm_size);
    }

    u32 psl = psl0;
    for (u64 i = idx;; i = SET_NEXT(set, i)) {
        u8 slot_psl = *GET_PSL(set, i);

        if (slot_psl == BUCKET_EMPTY) {
            *GET_PSL(set, i) = (u8)psl;
            memcpy(GET_ELM(set, i), cur, set->elm_size);
            set->size++;
            return;
        }

        // Robin Hood: evict the "rich" resident (lower PSL = closer to home).
        if (slot_psl < psl) {
            memcpy(swp, GET_ELM(set, i), set->elm_size);
            *GET_PSL(set, i) = (u8)psl;
            memcpy(GET_ELM(set, i), cur, set->elm_size);

            // the evicted entry is now in swp: swap roles
            u8* tmp = cur;
            cur     = swp;
            swp     = tmp;
            psl     = slot_psl;
        }

        if (++psl > PSL_LIMIT) {
            FATAL("HashSet: probe length overflow (degenerate hash function)");
        }
    }
}


// Rehash into a new block. Elements are unique, so no lookup and no compare:
// each one starts at its home bucket and Robin Hood insertion does the rest.
static void set_resize(HashSet* set, u64 new_capacity)
{
    if (new_capacity < HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    u8* old_block = set->elms;
    u8* old_psls  = set->psls;
    u64 old_cap   = set->capacity;

    set_alloc_block(set, new_capacity);
    set->size = 0;

    for (u64 i = 0; i < old_cap; i++) {
        if (old_psls[i] == BUCKET_EMPTY) {
            continue;
        }
        u8* old_elm = old_block + ((u64)set->elm_size * i);
        set_insert(set, old_elm, 1, SET_IDX(set, old_elm));
    }

    set_free_block(set, old_block, old_cap);
}
