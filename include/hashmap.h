#ifndef HASHMAP_H
#define HASHMAP_H

#include "common.h"
#include "map_setup.h"


/* Generic Hashmap with Ownership Semantics
  - Robin Hood Hashing
  - we have 3 arrays: keys, psls, and vals
  - PSL: probe sequence length - the distance from hashing location
  - we actuall store psl + 1 as psl = 0 means empty bucket
  - Robin Hood Invarient: all keys that hash to i come before keys that hash to i + 1
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
    u8*            scratch;  // key_size + val_size bytes + alignment - temp buffer for robin hood swaps
    custom_hash_fn hash_fn;
    compare_fn     cmp_fn;

    // Shared ops vtables for keys and values.
    // Pass NULL for POD types (int, float, flat structs).
    // For types with heap resources define one static ops per type:
    const container_ops* key_ops;
    const container_ops* val_ops;
} hashmap;


// Safely extract callbacks — always NULL-safe on ops itself.
#define MAP_COPY(ops) ((ops) ? (ops)->copy_fn : NULL)
#define MAP_MOVE(ops) ((ops) ? (ops)->move_fn : NULL)
#define MAP_DEL(ops)  ((ops) ? (ops)->del_fn  : NULL)

/* TODO: 
    reserve one extra slot at the end of the key/val arrays that never holds a real entry.
    During insert, you keep the “current” key/value in registers or local variables
    and only write them into the array when the final empty slot is found.
    This requires a small rewrite of map_insert, but it saves two memcpy calls per eviction
    and eliminates scratch
*/

// Create a new hashmap.
// hash_fn and cmp_fn default to fnv1a_hash / default_compare if NULL.
// key_ops / val_ops: pass NULL for POD types.
hashmap* hashmap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* key_ops, const container_ops* val_ops);
void     hashmap_create_stk(u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                            const container_ops* key_ops, const container_ops* val_ops, hashmap* map) __attribute__((nonnull(7)));

void hashmap_destroy(hashmap* map) __attribute__((nonnull(1)));
void hashmap_destroy_stk(hashmap* map) __attribute__((nonnull(1)));

// Insert or update — COPY semantics.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put(hashmap* map, const u8* key, const u8* val) __attribute__((nonnull(1, 2, 3)));

// Insert or update — MOVE semantics (key and val are u8**, both nulled).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put_move(hashmap* map, u8** key, u8** val) __attribute__((nonnull(1, 2, 3)));

// Mixed: key copied, val moved.
b8 hashmap_put_val_move(hashmap* map, const u8* key, u8** val) __attribute__((nonnull(1, 2, 3)));

// Mixed: key moved, val copied.
b8 hashmap_put_key_move(hashmap* map, u8** key, const u8* val) __attribute__((nonnull(1, 2, 3)));

// Get value for key — copies into val. Returns 1 if found, 0 if not.
b8 hashmap_get(const hashmap* map, const u8* key, u8* val) __attribute__((nonnull(1, 2, 3)));

// Get pointer to value
const u8* hashmap_get_ptr(const hashmap* map, const u8* key) __attribute__((nonnull(1, 2)));
u8*       hashmap_get_ptr_mut(hashmap* map, const u8* key) __attribute__((nonnull(1, 2)));

// Bucket iteration accessors
u64       hashmap_bucket_count(const hashmap* map) __attribute__((nonnull(1)));
b8        hashmap_bucket_occupied(const hashmap* map, u64 i) __attribute__((nonnull(1)));
const u8* hashmap_bucket_key_ptr(const hashmap* map, u64 i) __attribute__((nonnull(1)));
u8*       hashmap_bucket_val_ptr(hashmap* map, u64 i) __attribute__((nonnull(1)));

// Delete key. If out is provided, value is copied to it before deletion.
// Returns 1 if found and deleted, 0 if not found.
b8 hashmap_del(hashmap* map, const u8* key, u8* out) __attribute__((nonnull(1, 2)));

// Check if key exists.
b8 hashmap_has(const hashmap* map, const u8* key) __attribute__((nonnull(1, 2)));

// Print all key-value pairs.
void hashmap_print(const hashmap* map, print_fn key_print, print_fn val_print) __attribute__((nonnull(1, 2, 3)));

// Remove all elements, keep capacity.
void hashmap_clear(hashmap* map) __attribute__((nonnull(1)));

// Deep copy src into dest
// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void hashmap_copy(hashmap* dest, const hashmap* src) __attribute__((nonnull(1, 2)));


static inline __attribute__((nonnull(1))) u64 hashmap_size(const hashmap* map)
{
    CHECK_FATAL(!map, "map is null");
    return map->size;
}
static inline __attribute__((nonnull(1))) u64 hashmap_capacity(const hashmap* map)
{
    CHECK_FATAL(!map, "map is null");
    return map->capacity;
}
static inline __attribute__((nonnull(1))) b8 hashmap_empty(const hashmap* map)
{
    CHECK_FATAL(!map, "map is null");
    return map->size == 0;
}


#endif // HASHMAP_H
