#ifndef HASHSET_H
#define HASHSET_H

#include "map_setup.h"


/* Generic Hashset with Ownership Semantics
  - Robin Hood Hashing
  - we have 2 arrays: elms, psls
  - PSL: probe sequence length - the distance from hashing location
  - we actually store psl + 1 as psl = 0 means empty bucket
  - Robin Hood Invariant: all elms that hash to i come before elms that hash to i + 1
  - elms stored inline
*/


typedef struct {
    u8*            elms;
    u8*            psls;
    u64            size;
    u64            capacity;
    u32            elm_size;
    u8*            scratch;  // 2 * elm_size bytes — stage (first half) + RH swap (second half)
    custom_hash_fn hash_fn;
    compare_fn     cmp_fn;

    // Shared ops vtable for elements.
    // Pass NULL for POD types (int, float, flat structs).
    const container_ops* ops;
} hashset;


// Safely extract callbacks — always NULL-safe on ops itself.
#define SET_COPY(ops) ((ops) ? (ops)->copy_fn : NULL)
#define SET_MOVE(ops) ((ops) ? (ops)->move_fn : NULL)
#define SET_DEL(ops)  ((ops) ? (ops)->del_fn  : NULL)


// Create a new hashset.
// hash_fn and cmp_fn default to wyhash / default_compare if NULL.
// ops: pass NULL for POD types.
hashset* hashset_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* ops);

void hashset_destroy(hashset* set);

// Insert element — COPY semantics.
// Returns 1 if already existed (no-op), 0 if newly inserted.
b8 hashset_insert(hashset* set, const u8* elm);

// Insert element — MOVE semantics (elm is nulled on insert, or freed if duplicate).
// Returns 1 if already existed (elm freed), 0 if newly inserted.
b8 hashset_insert_move(hashset* set, u8** elm);

// Returns 1 if found, 0 if not.
b8 hashset_has(const hashset* set, const u8* elm);

// Returns 1 if found and removed, 0 if not found.
b8 hashset_remove(hashset* set, const u8* elm);

// Print all elements.
void hashset_print(const hashset* set, print_fn print);

// Remove all elements, keep capacity.
void hashset_clear(hashset* set);

// Deep copy src into dest
// dest should be pre-inited
void hashset_copy(hashset* dest, const hashset* src);


static inline u64 hashset_size(const hashset* set)
{
    CHECK_FATAL(!set, "set is null");
    return set->size;
}

static inline u64 hashset_capacity(const hashset* set)
{
    CHECK_FATAL(!set, "set is null");
    return set->capacity;
}

static inline b8 hashset_empty(const hashset* set)
{
    CHECK_FATAL(!set, "set is null");
    return set->size == 0;
}


#endif // HASHSET_H
