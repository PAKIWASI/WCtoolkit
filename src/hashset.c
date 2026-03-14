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
#define STAGE_ELM(set) ((set)->scratch)
#define SWAP_ELM(set)  ((set)->scratch + (set)->elm_size)

typedef enum {
    NOT_FOUND = 0,
    FOUND,
    ROBINHOOD_EXIT,
} SET_LOOKUP_RES;


/*
====================PRIVATE DECLARATIONS====================
*/

static u64         set_lookup(const hashset* set, const u8* elm, SET_LOOKUP_RES* res, u8* out_psl);
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

    set->elms = calloc(HASHMAP_INIT_CAPACITY, elm_size);
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


// Insert element — COPY semantics.
// Returns 1 if already existed (no-op), 0 if newly inserted.
b8 hashset_insert(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set || !elm, "null arg");

    set_maybe_resize(set);

    SET_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = set_lookup(set, elm, &res, &out_psl);

    if (res == FOUND) {
        return 1;
    }

    // New elm — stage a deep copy into the first half of scratch, then hand off
    // to set_insert which uses the second half (SWAP_ELM) for Robin Hood evictions.
    u8*     e_buf  = STAGE_ELM(set);
    copy_fn e_copy = SET_COPY(set->ops);

    if (e_copy) {
        e_copy(e_buf, elm);
    } else {
        memcpy(e_buf, elm, set->elm_size);
    }

    set_insert(set, e_buf, out_psl, slot);
    return 0;
}


// Insert element — MOVE semantics (elm is nulled on insert, or freed if duplicate).
// Returns 1 if already existed (elm freed), 0 if newly inserted.
b8 hashset_insert_move(hashset* set, u8** elm)
{
    CHECK_FATAL(!set || !elm, "null arg");

    set_maybe_resize(set);

    SET_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = set_lookup(set, *elm, &res, &out_psl);

    if (res == FOUND) {
        return 1;
    }

    // set_insert memcpy's these into slots — ownership transfers in
    set_insert(set, *elm, out_psl, slot);
    *elm = NULL;
    return 0;
}


// Returns 1 if found, 0 if not.
b8 hashset_has(const hashset* set, const u8* elm)
{
    CHECK_FATAL(!set || !elm, "null arg");

    SET_LOOKUP_RES res;
    u8             out_psl;
    set_lookup(set, elm, &res, &out_psl);
    return res == FOUND;
}


// Returns 1 if found and removed, 0 if not found.
b8 hashset_remove(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set || !elm, "null arg");

    if (set->size == 0) {
        return 0;
    }

    SET_LOOKUP_RES res;
    u8             out_psl;
    u64            slot = set_lookup(set, elm, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    delete_fn e_del = SET_DEL(set->ops);
    if (e_del) {
        e_del(GET_ELM(set, slot));
    }

    // Robin Hood backward shift deletion — no tombstones needed.
    // Walk forward and pull each subsequent element back one slot
    // as long as its PSL > 1 (it's not in its ideal slot).
    for (;;) {
        u64 next     = SET_NEXT(set, slot);
        u8  next_psl = *GET_PSL(set, next);

        // Stop if next slot is empty or already at its ideal position
        if (next_psl <= 1) {
            *GET_PSL(set, slot) = BUCKET_EMPTY;
            break;
        }

        // Pull neighbour back one slot, reducing its PSL by 1
        memcpy(GET_ELM(set, slot), GET_ELM(set, next), set->elm_size);
        *GET_PSL(set, slot) = next_psl - 1;

        slot = next;
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
        printf("\t   ");
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

    memset(set->psls, 0, set->capacity);
    memset(set->elms, 0, (u64)set->elm_size * set->capacity);

    set->size = 0;
}


// Deep copy src into dest (dest must be uninitialised or already destroyed).
void hashset_copy(hashset* dest, const hashset* src)
{
    CHECK_FATAL(!dest || !src, "null arg");

    copy_fn e_copy = SET_COPY(src->ops);

    dest->elms = calloc(src->capacity, src->elm_size);
    CHECK_FATAL(!dest->elms, "elms calloc failed");
    dest->psls = calloc(src->capacity, sizeof(u8));
    CHECK_FATAL(!dest->psls, "psls calloc failed");
    dest->scratch = malloc(2 * (u64)src->elm_size);
    CHECK_FATAL(!dest->scratch, "scratch malloc failed");

    dest->size     = 0;
    dest->capacity = src->capacity;
    dest->elm_size = src->elm_size;
    dest->hash_fn  = src->hash_fn;
    dest->cmp_fn   = src->cmp_fn;
    dest->ops      = src->ops;

    for (u64 i = 0; i < src->capacity; i++) {
        if (*GET_PSL(src, i) == BUCKET_EMPTY) {
            continue;
        }

        // Deep-copy elm into the staging region, then insert
        u8* e_buf = STAGE_ELM(dest);

        if (e_copy) {
            e_copy(e_buf, GET_ELM(src, i));
        } else {
            memcpy(e_buf, GET_ELM(src, i), src->elm_size);
        }

        SET_LOOKUP_RES res;
        u8             out_psl;
        u64            slot = set_lookup(dest, e_buf, &res, &out_psl);
        set_insert(dest, e_buf, out_psl, slot);
    }
}


/*
====================PRIVATE FUNCTIONS====================
*/

static inline void set_maybe_resize(hashset* set)
{
    // integer multiply avoids float — equivalent to load > 0.75
    if (set->size * 4 >= set->capacity * 3) {
        set_resize(set, set->capacity * 2); // power-of-2 doubles stay power-of-2
    }
}


static u64 set_lookup(const hashset* set, const u8* elm, SET_LOOKUP_RES* res, u8* out_psl)
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
    // This loop only shuffles ownership between slots — no copy_fn ever.
    // Uses SWAP_ELM (second half of scratch) to avoid aliasing the staged data.

    for (u64 i = idx;; i = SET_NEXT(set, i))
    {
        u8 slot_psl = *GET_PSL(set, i);

        if (slot_psl == BUCKET_EMPTY) {
            *GET_PSL(set, i) = psl;
            memcpy(GET_ELM(set, i), elm, set->elm_size);
            set->size++;
            return;
        }

        // Robin Hood: evict the "rich" resident (lower PSL = closer to home)
        if (slot_psl < psl) {
            // Save displaced resident into swap buffer
            memcpy(SWAP_ELM(set), GET_ELM(set, i), set->elm_size);
            u8 tmp_psl = slot_psl;

            // Place incoming element
            *GET_PSL(set, i) = psl;
            memcpy(GET_ELM(set, i), elm, set->elm_size);

            // Continue inserting the displaced element
            elm = SWAP_ELM(set);
            psl = tmp_psl;
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

    // Scratch size scales with elm_size only — no realloc needed.

    set->capacity = new_capacity;
    set->size     = 0;

    // Rehash — ownership transfers as-is, no copy/del callbacks
    for (u64 i = 0; i < old_cap; i++) {
        if (old_psls[i] == BUCKET_EMPTY) {
            continue;
        }

        u8* old_elm = old_elms + ((u64)set->elm_size * i);

        SET_LOOKUP_RES res;
        u8             out_psl;
        u64            slot = set_lookup(set, old_elm, &res, &out_psl);
        set_insert(set, old_elm, out_psl, slot);
    }

    free(old_elms);
    free(old_psls);
}


