#include "hashset.h"
#include "wc_macros.h"
#include <string.h>




#define GET_ELM(set, i) ((set)->buckets + (i))


/*
====================PRIVATE FUNCTIONS====================
*/

static void elm_destroy(const container_ops* ops, const ELM* elm);
static void memset_buckets(ELM* buckets, u64 size);

static u64 find_slot(const hashset* set, const u8* element, b8* found, int* tombstone);

static void hashset_resize(hashset* set, u64 new_capacity);
static void hashset_maybe_resize(hashset* set);


/*
====================PUBLIC FUNCTIONS====================
*/

hashset* hashset_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* ops)
{
    CHECK_FATAL(elm_size == 0, "elm size can't be 0");

    hashset* set = malloc(sizeof(hashset));
    CHECK_FATAL(!set, "set malloc failed");

    set->buckets = malloc(HASHMAP_INIT_CAPACITY * sizeof(ELM));
    CHECK_FATAL(!set->buckets, "set bucket init failed");

    memset_buckets(set->buckets, HASHMAP_INIT_CAPACITY);

    set->capacity = HASHMAP_INIT_CAPACITY;
    set->size     = 0;
    set->elm_size = elm_size;

    set->hash_fn = hash_fn ? hash_fn : fnv1a_hash;
    set->cmp_fn  = cmp_fn  ? cmp_fn  : default_compare;

    set->ops = ops;

    return set;
}

void hashset_destroy(hashset* set)
{
    CHECK_FATAL(!set,          "set is null");
    CHECK_FATAL(!set->buckets, "set buckets is null");

    SET_FOREACH_BUCKET(set, elm) {
        elm_destroy(set->ops, elm);
    }

    free(set->buckets);
    free(set);
}

void hashset_clear(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    for (u64 i = 0; i < set->capacity; i++) {
        ELM* elm = GET_ELM(set, i);
        if (elm->state == FILLED) {
            elm_destroy(set->ops, elm);
        }
        elm->elm   = NULL;
        elm->state = EMPTY;
    }

    set->size = 0;
}

void hashset_reset(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    hashset_clear(set);

    if (set->capacity > HASHMAP_INIT_CAPACITY) {
        free(set->buckets);
        set->buckets = malloc(HASHMAP_INIT_CAPACITY * sizeof(ELM));
        CHECK_FATAL(!set->buckets, "reset malloc failed");
        memset_buckets(set->buckets, HASHMAP_INIT_CAPACITY);
        set->capacity = HASHMAP_INIT_CAPACITY;
    }
}

// COPY semantics
b8 hashset_insert(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!elm, "elm is null");

    hashset_maybe_resize(set);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = find_slot(set, elm, &found, &tombstone);

    if (found) {
        return 1; // already exists
    }

    u8* new_elm = malloc(set->elm_size);
    CHECK_FATAL(!new_elm, "elm malloc failed");

    if (set->ops && set->ops->copy_fn) {
        set->ops->copy_fn(new_elm, elm);
    } else {
        memcpy(new_elm, elm, set->elm_size);
    }

    ELM* elem   = GET_ELM(set, slot);
    elem->elm   = new_elm;
    elem->state = FILLED;

    set->size++;

    return 0;
}

// MOVE semantics
b8 hashset_insert_move(hashset* set, u8** elm)
{
    CHECK_FATAL(!set,  "set is null");
    CHECK_FATAL(!elm,  "elm is null");
    CHECK_FATAL(!*elm, "*elm is null");

    hashset_maybe_resize(set);

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = find_slot(set, *elm, &found, &tombstone);

    if (found) {
        // Already exists — clean up the passed element
        if (set->ops && set->ops->del_fn) {
            set->ops->del_fn(*elm);
        }
        free(*elm);
        *elm = NULL;
        return 1;
    }

    u8* new_elm = malloc(set->elm_size);
    CHECK_FATAL(!new_elm, "elm malloc failed");

    if (set->ops && set->ops->move_fn) {
        set->ops->move_fn(new_elm, elm);
    } else {
        memcpy(new_elm, *elm, set->elm_size);
        *elm = NULL;
    }

    ELM* elem   = GET_ELM(set, slot);
    elem->elm   = new_elm;
    elem->state = FILLED;

    set->size++;

    return 0;
}

b8 hashset_remove(hashset* set, const u8* elm)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!elm, "elm is null");

    if (set->size == 0) {
        return 0;
    }

    b8  found     = 0;
    int tombstone = -1;
    u64 slot      = find_slot(set, elm, &found, &tombstone);

    if (found) {
        ELM* elem = GET_ELM(set, slot);
        elm_destroy(set->ops, elem);

        elem->elm   = NULL;
        elem->state = TOMBSTONE;

        set->size--;

        hashset_maybe_resize(set);
        return 1;
    }

    return 0;
}

ELM* hashset_get_bucket(hashset* set, u64 i)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(i >= set->capacity, "index out of bounds");

    return (set->buckets + i);
}

b8 hashset_has(const hashset* set, const u8* elm)
{
    CHECK_FATAL(!set, "set is null");
    CHECK_FATAL(!elm, "elm is null");

    b8  found     = 0;
    int tombstone = -1;
    find_slot(set, elm, &found, &tombstone);

    return found;
}

void hashset_print(const hashset* set, print_fn print_fn)
{
    CHECK_FATAL(!set,      "set is null");
    CHECK_FATAL(!print_fn, "print_fn is null");

    printf("\t=========\n");
    printf("\tSize: %lu / Capacity: %lu\n", set->size, set->capacity);
    printf("\t=========\n");

    SET_FOREACH_BUCKET(set, elm) {
        printf("\t   ");
        print_fn(elm->elm);
        printf("\n");
    }

    printf("\t=========\n");
}

// Deep copy src into dest (dest must be uninitialised or already destroyed).
void hashset_copy(hashset* dest, const hashset* src)
{
    CHECK_FATAL(!dest, "dest is null");
    CHECK_FATAL(!src,  "src is null");

    copy_fn e_copy = src->ops ? src->ops->copy_fn : NULL;

    // Copy all scalar fields (sizes, fn ptrs, ops), then give dest its own bucket array.
    dest->capacity = src->capacity;
    dest->size     = 0;
    dest->elm_size = src->elm_size;
    dest->hash_fn  = src->hash_fn;
    dest->cmp_fn   = src->cmp_fn;
    dest->ops      = src->ops;

    dest->buckets = malloc(src->capacity * sizeof(ELM));
    CHECK_FATAL(!dest->buckets, "dest bucket malloc failed");
    memset_buckets(dest->buckets, src->capacity);

    SET_FOREACH_BUCKET(src, kv) {
        b8  found     = 0;
        int tombstone = -1;
        u64 slot      = find_slot(dest, kv->elm, &found, &tombstone);

        u8* e = malloc(src->elm_size);
        CHECK_FATAL(!e, "elm malloc failed");

        if (e_copy) { e_copy(e, kv->elm); }
        else        { memcpy(e, kv->elm, src->elm_size); }

        ELM* new_elm   = GET_ELM(dest, slot);
        new_elm->elm   = e;
        new_elm->state = FILLED;

        dest->size++;
    }
}



/*
====================PRIVATE FUNCTION IMPLEMENTATIONS====================
*/


static void elm_destroy(const container_ops* ops, const ELM* elm)
{
    CHECK_FATAL(!elm, "ELM is null");

    if (elm->elm) {
        if (ops && ops->del_fn) {
            ops->del_fn(elm->elm);
        }
        free(elm->elm);
    }
}

// memset gives: elm = NULL, state = EMPTY (= 0)
static void memset_buckets(ELM* buckets, u64 size)
{
    memset(buckets, 0, sizeof(ELM) * size);
}

static u64 find_slot(const hashset* set, const u8* element, b8* found, int* tombstone)
{
    u64 index = set->hash_fn(element, set->elm_size) % set->capacity;

    *found     = 0;
    *tombstone = -1;

    for (u64 x = 0; x < set->capacity; x++) {
        u64        i   = (index + x) % set->capacity;
        const ELM* elm = GET_ELM(set, i);

        switch (elm->state) {
            case EMPTY:
                // Return tombstone slot if we passed one — reuse it
                return (*tombstone != -1) ? (u64)*tombstone : i;
            case FILLED:
                if (set->cmp_fn(elm->elm, element, set->elm_size) == 0) {
                    *found = 1;
                    return i;
                }
                break;
            case TOMBSTONE:
                if (*tombstone == -1) {
                    *tombstone = (int)i;
                }
                break;
        }
    }

    return (*tombstone != -1) ? (u64)*tombstone : 0;
}

static void hashset_resize(hashset* set, u64 new_capacity)
{
    if (new_capacity < HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    ELM* old_buckets = set->buckets;
    u64 old_cap     = set->capacity;

    set->buckets = malloc(new_capacity * sizeof(ELM));
    CHECK_FATAL(!set->buckets, "resize malloc failed");
    memset_buckets(set->buckets, new_capacity);

    set->capacity = new_capacity;
    set->size     = 0;

    // Rehash — pointers are moved as-is, no copy/del needed
    for (u64 i = 0; i < old_cap; i++) {
        const ELM* old_elm = old_buckets + i;

        if (old_elm->state == FILLED) {
            b8  found     = 0;
            int tombstone = -1;
            u64 slot      = find_slot(set, old_elm->elm, &found, &tombstone);

            ELM* new_elm   = GET_ELM(set, slot);
            new_elm->elm   = old_elm->elm;
            new_elm->state = FILLED;

            set->size++;
        }
    }

    free(old_buckets);
}

static void hashset_maybe_resize(hashset* set)
{
    CHECK_FATAL(!set, "set is null");

    double load = (double)set->size / (double)set->capacity;

    if (load > LOAD_FACTOR_GROW) {
        hashset_resize(set, next_prime(set->capacity));
    } else if (load < LOAD_FACTOR_SHRINK && set->capacity > HASHMAP_INIT_CAPACITY) {
        u64 new_cap = prev_prime(set->capacity);
        if (new_cap >= HASHMAP_INIT_CAPACITY) {
            hashset_resize(set, new_cap);
        }
    }
}


