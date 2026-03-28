#include "hashset.h"
#include <string.h>


#define GET_ELM(set, i) ((set)->elms + ((u64)(set)->elm_size * (i)))
#define GET_PSL(set, i) ((set)->psls + (i))

// capacity is always power-of-2 — use bitmask instead of %
#define SET_MASK(set)     ((set)->capacity - 1)
#define SET_IDX(set, elm) ((set)->hash_fn((elm), (set)->elm_size) & SET_MASK(set))
#define SET_NEXT(set, i)  (((i) + 1) & SET_MASK(set))

// PSL 0 == empty bucket; stored PSL is (real_psl + 1), starting at 1
#define BUCKET_EMPTY 0

// scratch layout: [0 .. elm_size) = stage,  [elm_size .. 2*elm_size) = swap
// stage: where hashset_insert copies the incoming elm before calling set_insert
// swap:  where set_insert saves a displaced resident during Robin Hood eviction
// The two halves are alternated each eviction to avoid aliasing (elm pointer
// is always in the half that set_insert is NOT currently writing into).
#define STAGE_ELM(set) ((set)->scratch)
#define SWAP_ELM(set)  ((set)->scratch + (set)->elm_size)


/*
====================PRIVATE DECLARATIONS====================
*/

static u64         set_lookup(const hashset* set, const u8* elm, LOOKUP_RES* res, u8* out_psl);
static void        set_insert(hashset* set, u8* elm, u8 psl, u64 idx);
static void        set_resize(hashset* set, u64 new_capacity);
static inline void set_maybe_resize(hashset* set);


/*
====================PUBLIC FUNCTIONS====================
*/

hashset* hashset_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* ops)
{
    CHECK_FATAL(elm_size == 0, "elm_size can't be 0");

    hashset* set = malloc(sizeof(hashset));
    CHECK_FATAL(!set, "set malloc failed");

    set->elms = malloc((u64)HASHMAP_INIT_CAPACITY * elm_size);
    CHECK_FATAL(!set->elms, "elms calloc failed");
    set->psls = calloc(HASHMAP_INIT_CAPACITY, sizeof(u8));
    CHECK_FATAL(!set->psls, "psls calloc failed");

    // 2 * elm_size: first half = staging, second half = RH swap buffer
    set->scratch = malloc(2 * (u64)elm_size);
    CHECK_FATAL(!set->scratch, "scratch malloc failed");

    set->size     = 0;
    set->capacity = HASHMAP_INIT_CAPACITY;
    set->elm_size = elm_size;

    set->hash_fn = hash_fn ? hash_fn : wyhash;
    set->cmp_fn  = cmp_fn  ? cmp_fn  : default_compare;

    set->ops = ops;

    return set;
}


void hashset_destroy(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    delete_fn e_del = SET_DEL(set->ops);

    if (e_del) {
        for (u64 i = 0; i < set->capacity; i++) {
            if (*GET_PSL(set, i) == BUCKET_EMPTY) {
                continue;
            }
            e_del(GET_ELM(set, i));
        }
    }

    free(set->elms);
    free(set->psls);
    free(set->scratch);
    free(set);
}

void hashset_destroy_stk(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    delete_fn e_del = SET_DEL(set->ops);

    if (e_del) {
        for (u64 i = 0; i < set->capacity; i++) {
            if (*GET_PSL(set, i) == BUCKET_EMPTY) {
                continue;
            }
            e_del(GET_ELM(set, i));
        }
    }

    free(set->elms);
    free(set->psls);
    free(set->scratch);
}


// Insert element — COPY semantics.
// Returns 1 if already existed (no-op), 0 if newly inserted.
b8 hashset_insert(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set || !elm, "args null");

    copy_fn e_cp = SET_COPY(set->ops);

    LOOKUP_RES res;
    u8             out_psl;
    u64            slot = set_lookup(set, elm, &res, &out_psl);

    if (res == FOUND) {
        return 1;
    }

    // Stage a deep copy into scratch before calling set_insert.
    // set_insert only does raw memcpy moves between slots — it never calls copy/del.
    if (e_cp) {
        e_cp(STAGE_ELM(set), elm);
    } else {
        memcpy(STAGE_ELM(set), elm, set->elm_size);
    }

    set_insert(set, STAGE_ELM(set), out_psl, slot);
    set_maybe_resize(set);
    return 0;
}


// Insert element — MOVE semantics (elm is nulled on insert, or freed if duplicate).
// Returns 1 if already existed (elm freed), 0 if newly inserted.
b8 hashset_insert_move(hashset* set, u8** elm)
{
    CHECK_FATAL(!set || !elm || !*elm, "args null");

    move_fn   e_mv  = SET_MOVE(set->ops);
    delete_fn e_del = SET_DEL(set->ops);

    CHECK_FATAL(!e_mv, "elm move func required");

    LOOKUP_RES res;
    u8             out_psl;
    u64            slot = set_lookup(set, *elm, &res, &out_psl);

    if (res == FOUND) {
        // Already exists — consume (destroy) the incoming duplicate.
        if (e_del) {
            e_del(*elm);
        }
        free(*elm);
        *elm = NULL;
        return 1;
    }

    // Stage: move elm into STAGE_ELM — transfers heap resource, nulls *elm.
    e_mv(STAGE_ELM(set), elm);

    set_insert(set, STAGE_ELM(set), out_psl, slot);
    set_maybe_resize(set);
    return 0;
}


// Returns 1 if found, 0 if not.
b8 hashset_has(const hashset* set, const u8* elm)
{
    CHECK_FATAL(!set || !elm, "null arg");

    LOOKUP_RES res;
    u8             out_psl;
    set_lookup(set, elm, &res, &out_psl);
    return res == FOUND;
}


// Returns 1 if found and removed, 0 if not found.
// Uses Robin Hood backward-shift deletion to maintain the probe-sequence invariant
// without tombstones: after removing a slot, shift subsequent entries back one
// position as long as they have PSL > 1 (i.e. they are not at their home slot).
b8 hashset_remove(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set || !elm, "null arg");

    LOOKUP_RES res;
    u8             out_psl;
    u64            slot = set_lookup(set, elm, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    delete_fn e_del = SET_DEL(set->ops);

    if (e_del) {
        e_del(GET_ELM(set, slot));
    }

    // Backward-shift: pull subsequent entries one slot back as long as
    // they have PSL > 1. Entries at their home slot (PSL == 1) must not move.
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
void hashset_print(const hashset* set, print_fn print)
{
    CHECK_FATAL(!set || !print, "null arg");

    printf("\t=========\n");
    printf("\tSize: %lu / Capacity: %lu\n", set->size, set->capacity);
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
void hashset_clear(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    delete_fn e_del = SET_DEL(set->ops);

    for (u64 i = 0; i < set->capacity; i++) {
        if (*GET_PSL(set, i) == BUCKET_EMPTY) {
            continue;
        }
        if (e_del) {
            e_del(GET_ELM(set, i));
        }
    }

    memset(set->psls, 0, set->capacity * sizeof(u8));
    set->size = 0;
}


// Deep copy src into dest
// Ownership: dest gets independently owned copies of all elements.
void hashset_copy(hashset* dest, const hashset* src)
{
    CHECK_FATAL(!dest || !src, "null arg");

    hashset_destroy_stk(dest);

    dest->elms = calloc(src->capacity, src->elm_size);
    CHECK_FATAL(!dest->elms, "copy elms calloc failed");
    dest->psls = calloc(src->capacity, sizeof(u8));
    CHECK_FATAL(!dest->psls, "copy psls calloc failed");
    dest->scratch = malloc(2 * (u64)src->elm_size);
    CHECK_FATAL(!dest->scratch, "copy scratch malloc failed");

    dest->size     = src->size;
    dest->capacity = src->capacity;
    dest->elm_size = src->elm_size;
    dest->hash_fn  = src->hash_fn;
    dest->cmp_fn   = src->cmp_fn;
    dest->ops      = src->ops;

    copy_fn e_cp = SET_COPY(src->ops);

    for (u64 i = 0; i < src->capacity; i++) {
        u8 psl = *GET_PSL(src, i);
        if (psl == BUCKET_EMPTY) {
            continue;
        }

        *GET_PSL(dest, i) = psl;

        if (e_cp) {
            e_cp(GET_ELM(dest, i), GET_ELM(src, i));
        } else {
            memcpy(GET_ELM(dest, i), GET_ELM(src, i), src->elm_size);
        }
    }
}


/*
====================PRIVATE FUNCTIONS====================
*/

static inline void set_maybe_resize(hashset* set)
{
    // integer multiply avoids float — equivalent to load > 0.75
    if (set->size * 4 >= set->capacity * 3) {
        set_resize(set, set->capacity * 2);
    }
}


static u64 set_lookup(const hashset* set, const u8* elm, LOOKUP_RES* res, u8* out_psl)
{
    u64 idx = SET_IDX(set, elm);
    u8  psl = 1; // stored PSL=1 means real probe distance 0 (home slot)

    for (u64 i = idx;; i = SET_NEXT(set, i))
    {
        u8 slot_psl = *GET_PSL(set, i);
        *out_psl    = psl;

        if (slot_psl == BUCKET_EMPTY) {
            *res = NOT_FOUND;
            return i;
        }

        if (slot_psl < psl) {
            // The resident was inserted closer to home than we are —
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


static void set_insert(hashset* set, u8* elm, u8 psl, u64 idx)
{
    // elm is already owned (either staged copy or moved pointer).
    // Alternates between the two scratch halves on each Robin Hood eviction
    // so that elm never aliases the buffer being written into.
    u8* cur = STAGE_ELM(set);
    u8* swp = SWAP_ELM(set);

    // elm may already be STAGE_ELM (called from hashset_insert/insert_move);
    // only copy if it isn't already there.
    if (elm != cur) {
        memcpy(cur, elm, set->elm_size);
    }

    for (u64 i = idx;; i = SET_NEXT(set, i))
    {
        u8 slot_psl = *GET_PSL(set, i);

        if (slot_psl == BUCKET_EMPTY) {
            *GET_PSL(set, i) = psl;
            memcpy(GET_ELM(set, i), cur, set->elm_size);
            set->size++;
            return;
        }

        // Robin Hood: evict the "rich" resident (lower PSL = closer to home).
        if (slot_psl < psl) {
            u8 tmp_psl = slot_psl;

            // Save displaced resident into swp (disjoint from cur).
            memcpy(swp, GET_ELM(set, i), set->elm_size);

            // Place incoming element into slot.
            *GET_PSL(set, i) = psl;
            memcpy(GET_ELM(set, i), cur, set->elm_size);

            // The evicted entry is now in swp; swap roles so cur always
            // points to the element being placed and swp is the free buffer.
            u8* tmp = cur; cur = swp; swp = tmp;
            psl = tmp_psl + 1; // +1: evicted entry moves one slot further from home
            continue;          // skip the unconditional psl++ below
        }

        psl++;
    }
}


static void set_resize(hashset* set, u64 new_capacity)
{
    if (new_capacity < HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    u8* old_elms = set->elms;
    u8* old_psls = set->psls;
    u64 old_cap  = set->capacity;

    set->elms = calloc(new_capacity, set->elm_size);
    CHECK_FATAL(!set->elms, "resize elms calloc failed");
    set->psls = calloc(new_capacity, sizeof(u8));
    CHECK_FATAL(!set->psls, "resize psls calloc failed");

    set->capacity = new_capacity;
    set->size     = 0;

    for (u64 i = 0; i < old_cap; i++) {
        if (old_psls[i] == BUCKET_EMPTY) {
            continue;
        }

        u8* old_elm = old_elms + ((u64)set->elm_size * i);

        // Stage each entry before inserting — set_insert uses SWAP_ELM (second
        // half of scratch) as its eviction buffer, so old_elm must not alias it.
        memcpy(STAGE_ELM(set), old_elm, set->elm_size);

        LOOKUP_RES res;
        u8             out_psl;
        u64            slot = set_lookup(set, STAGE_ELM(set), &res, &out_psl);
        set_insert(set, STAGE_ELM(set), out_psl, slot);
    }

    free(old_elms);
    free(old_psls);
}


