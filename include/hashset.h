#ifndef HASHSET_H
#define HASHSET_H

#include "common.h"
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
    u8*            scratch; // 2 * elm_size bytes — stage (first half) + RH swap (second half)
    custom_hash_fn hash_fn;
    compare_fn     cmp_fn;

    // Shared ops vtable for elements.
    // Pass NULL for POD types (int, float, flat structs).
    const container_ops* ops;
} HashSet;


// Safely extract callbacks — always NULL-safe on ops itself.
#define SET_COPY(ops) ((ops) ? (ops)->copy_fn : NULL)
#define SET_MOVE(ops) ((ops) ? (ops)->move_fn : NULL)
#define SET_DEL(ops)  ((ops) ? (ops)->del_fn : NULL)


// Create a new HashSet.
// hash_fn and cmp_fn default to wyhash / default_compare if NULL.
// ops: pass NULL for POD types.
HashSet* HashSet_create(u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn, const container_ops* ops)
    __attribute__((warn_unused_result));
void HashSet_create_stk(HashSet* set, u32 elm_size, custom_hash_fn hash_fn, compare_fn cmp_fn, const container_ops* ops)
    __attribute__((nonnull(1)));

void HashSet_destroy(HashSet* set) __attribute__((nonnull(1)));
void HashSet_destroy_stk(HashSet* set) __attribute__((nonnull(1)));

// Insert element — COPY semantics.
// Returns 1 if already existed (no-op), 0 if newly inserted.
b8 HashSet_insert(HashSet* set, const u8* elm) __attribute__((nonnull(1, 2)));

// Insert element — MOVE semantics (elm is nulled on insert, or freed if duplicate).
// Returns 1 if already existed (elm freed), 0 if newly inserted.
b8 HashSet_insert_move(HashSet* set, u8** elm) __attribute__((nonnull(1, 2)));

// Returns 1 if found, 0 if not.
b8 HashSet_has(const HashSet* set, const u8* elm) __attribute__((nonnull(1, 2)));

// Get pointer to element in-place. Returns NULL if not found.
const u8* HashSet_get_ptr(const HashSet* set, const u8* elm) __attribute__((nonnull(1, 2)));

__attribute__((nonnull(1, 2))) static inline u8* HashSet_get_ptr_mut(HashSet* set, const u8* elm)
{
    return (u8*)HashSet_get_ptr(set, elm);
}

// Bucket iteration accessors
__attribute__((nonnull(1))) static inline u64 HashSet_bucket_count(const HashSet* set)
{
    return set->capacity;
}

b8        HashSet_bucket_occupied(const HashSet* set, u64 i) __attribute__((nonnull(1)));
const u8* HashSet_bucket_elm_ptr(const HashSet* set, u64 i) __attribute__((nonnull(1)));

// Returns 1 if found and removed, 0 if not found.
b8 HashSet_remove(HashSet* set, const u8* elm) __attribute__((nonnull(1, 2)));

// Print all elements.
void HashSet_print(const HashSet* set, print_fn print) __attribute__((nonnull(1, 2)));

// Remove all elements, keep capacity.
void HashSet_clear(HashSet* set) __attribute__((nonnull(1)));

// Deep copy src into dest
// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void HashSet_copy(HashSet* dest, const HashSet* src) __attribute__((nonnull(1, 2)));


static inline __attribute__((nonnull(1))) u64 HashSet_size(const HashSet* set)
{
    return set->size;
}

static inline __attribute__((nonnull(1))) u64 HashSet_capacity(const HashSet* set)
{
    return set->capacity;
}

static inline __attribute__((nonnull(1))) b8 HashSet_empty(const HashSet* set)
{
    return set->size == 0;
}


#endif // HASHSET_H
