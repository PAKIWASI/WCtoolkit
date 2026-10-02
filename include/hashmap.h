#ifndef HASHMAP_H
#define HASHMAP_H

#include "common.h"
#include "map_setup.h"
#include "wc_allocator.h"


/* Generic Hashmap with Ownership Semantics
  - Robin Hood Hashing
  - we have 3 arrays: keys, psls, and vals
  - PSL: probe sequence length: the distance from hashing location
  - we actually store psl + 1 as psl = 0 means empty bucket
  - Robin Hood Invariant: all keys that hash to i come before keys that hash to i + 1
  - vals store [val] inline
*/


typedef struct {
    u8*            keys;
    u8*            psls;
    u8*            vals;
    u64            size;
    u64            capacity;
    u32            key_size;
    u32            val_size;
    u8*            scratch; // key_size + val_size bytes + alignment: temp buffer for robin hood swaps
    custom_hash_fn hash_fn;
    wc_compare_fn  cmp_fn;

    // Shared ops vtables for keys and values.
    // Pass NULL for POD types (int, float, flat structs).
    // For types with heap resources define one static ops per type:
    const wc_container_ops* key_ops;
    const wc_container_ops* val_ops;
    wc_allocator            alloc;
} HashMap;

_Static_assert(sizeof(HashMap) == 104, "HashMap must be 104 bytes");


// Safely extract callbacks — always NULL-safe on ops itself.
#define MAP_COPY(ops) ((ops) ? (ops)->copy_fn : NULL)
#define MAP_MOVE(ops) ((ops) ? (ops)->move_fn : NULL)
#define MAP_DEL(ops)  ((ops) ? (ops)->del_fn : NULL)


// Create a new HashMap by value.
// hash_fn and cmp_fn default to wyhash / default_compare if NULL.
// key_ops / val_ops: pass NULL for POD types.
HashMap HashMap_create(wc_allocator a, u32 key_size, u32 val_size, custom_hash_fn hash_fn, wc_compare_fn cmp_fn,
                       const wc_container_ops* key_ops, const wc_container_ops* val_ops)
    __attribute__((warn_unused_result));

// Destroy all elements and free internal buffers via map->alloc.
// Safe on zeroed/moved-from maps. Leaves struct zeroed.
void HashMap_destroy(HashMap* map) __attribute__((nonnull(1)));

// Deep copy src into a new HashMap allocated from `a`.
HashMap HashMap_copy(wc_allocator a, const HashMap* src) __attribute__((nonnull(2), warn_unused_result));

// Transfer ownership from src to dest. src is left zeroed.
void HashMap_move(HashMap* dest, HashMap* src) __attribute__((nonnull(1, 2)));

// Insert or update — COPY semantics.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put(HashMap* map, const void* key, const void* val) __attribute__((nonnull(1, 2, 3)));

// Insert or update, MOVE semantics: key and val point at the caller's elements, both left zeroed
// (a duplicate key is destroyed; the map keeps its own). move_fn optional (memcpy + zero).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put_move(HashMap* map, void* key, void* val) __attribute__((nonnull(1, 2, 3)));

// Mixed: key copied, val moved.
b8 HashMap_put_val_move(HashMap* map, const void* key, void* val) __attribute__((nonnull(1, 2, 3)));

// Mixed: key moved, val copied.
b8 HashMap_put_key_move(HashMap* map, void* key, const void* val) __attribute__((nonnull(1, 2, 3)));

// Get value for key — copies into val. Returns 1 if found, 0 if not.
b8 HashMap_get(const HashMap* map, const void* key, void* val) __attribute__((nonnull(1, 2, 3)));

// Get pointer to value
const void* HashMap_get_ptr(const HashMap* map, const void* key) __attribute__((nonnull(1, 2)));

__attribute__((nonnull(1, 2))) static inline void* HashMap_get_ptr_mut(HashMap* map, const void* key)
{
    return (void*)HashMap_get_ptr(map, key);
}

// Bucket iteration accessors
__attribute__((nonnull(1))) static inline u64 HashMap_bucket_count(const HashMap* map)
{
    return map->capacity;
}

b8          HashMap_bucket_occupied(const HashMap* map, u64 i) __attribute__((nonnull(1)));
const void* HashMap_bucket_key_ptr(const HashMap* map, u64 i) __attribute__((nonnull(1)));
void*       HashMap_bucket_val_ptr(HashMap* map, u64 i) __attribute__((nonnull(1)));

// Delete key. If out is provided, value is copied to it before deletion.
// Returns 1 if found and deleted, 0 if not found.
b8 HashMap_del(HashMap* map, const void* key, void* out) __attribute__((nonnull(1, 2)));

// Check if key exists.
b8 HashMap_has(const HashMap* map, const void* key) __attribute__((nonnull(1, 2)));

// Print all key-value pairs.
void HashMap_print(const HashMap* map, wc_print_fn key_print, wc_print_fn val_print) __attribute__((nonnull(1, 2, 3)));

// Remove all elements, keep capacity.
void HashMap_clear(HashMap* map) __attribute__((nonnull(1)));

static inline __attribute__((nonnull(1))) u64 HashMap_size(const HashMap* map)
{
    return map->size;
}
static inline __attribute__((nonnull(1))) u64 HashMap_capacity(const HashMap* map)
{
    return map->capacity;
}
static inline __attribute__((nonnull(1))) b8 HashMap_empty(const HashMap* map)
{
    return map->size == 0;
}


#endif // HASHMAP_H
