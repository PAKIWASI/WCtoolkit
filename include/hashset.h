#ifndef HASHSET_H
#define HASHSET_H

#include "map_setup.h"
#include "gen_vector.h"


// TODO: make changes according to hashmap changes


typedef struct {
    u8*            buckets;
    u64            size;
    u64            capacity;
    u32            elm_size;
    custom_hash_fn hash_fn;
    compare_fn     cmp_fn;

    // Shared ops vtable for elements.
    // Pass NULL for POD types (int, float, flat structs).
    const genVec_ops* ops;
} hashset;


// Create a new hashset.
// hash_fn and cmp_fn default to fnv1a_hash / default_compare if NULL.
// ops: pass NULL for POD types.
hashset* hashset_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const genVec_ops* ops);

void hashset_destroy(hashset* set);

// Insert element — COPY semantics.
// Returns 1 if already existed (no-op), 0 if newly inserted.
b8 hashset_insert(hashset* set, const u8* elm);

// Insert element — MOVE semantics (elm is nulled).
// Returns 1 if already existed (elm freed), 0 if newly inserted.
b8 hashset_insert_move(hashset* set, u8** elm);

// Returns 1 if found, 0 if not.
b8 hashset_has(const hashset* set, const u8* elm);

// Returns 1 if found and removed, 0 if not found.
b8 hashset_remove(hashset* set, const u8* elm);

// Remove all elements, keep capacity.
void hashset_clear(hashset* set);

// Remove all elements and reset to initial capacity.
void hashset_reset(hashset* set);

void hashset_print(const hashset* set, print_fn print_fn);

static inline u64 hashset_size(const hashset* set)     { CHECK_FATAL(!set, "set is null"); return set->size;      }
static inline u64 hashset_capacity(const hashset* set) { CHECK_FATAL(!set, "set is null"); return set->capacity;  }
static inline b8  hashset_empty(const hashset* set)    { CHECK_FATAL(!set, "set is null"); return set->size == 0; }


#endif // HASHSET_H
