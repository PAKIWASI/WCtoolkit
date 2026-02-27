#ifndef HASHSET_H
#define HASHSET_H

#include "map_setup.h"



typedef struct {
    u8*   elm;
    STATE state;
} ELM;

typedef struct {
    ELM*           buckets;
    u64            size;
    u64            capacity;
    u32            elm_size;
    custom_hash_fn hash_fn;
    compare_fn     cmp_fn;

    // Shared ops vtable for elements.
    // Pass NULL for POD types (int, float, flat structs).
    const container_ops* ops;
} hashset;


// Create a new hashset.
// hash_fn and cmp_fn default to fnv1a_hash / default_compare if NULL.
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

// Get raw bucket pointer at index i.
ELM* hashset_get_bucket(hashset* set, u64 i);

// Remove all elements, keep capacity.
void hashset_clear(hashset* set);

// Remove all elements and reset to initial capacity.
void hashset_reset(hashset* set);

void hashset_print(const hashset* set, print_fn print_fn);

// Deep copy src into dest (dest must be uninitialised or already destroyed).
void hashset_copy(hashset* dest, const hashset* src);

static inline u64 hashset_size(const hashset* set)     { CHECK_FATAL(!set, "set is null"); return set->size;      }
static inline u64 hashset_capacity(const hashset* set) { CHECK_FATAL(!set, "set is null"); return set->capacity;  }
static inline b8  hashset_empty(const hashset* set)    { CHECK_FATAL(!set, "set is null"); return set->size == 0; }


#endif // HASHSET_H
