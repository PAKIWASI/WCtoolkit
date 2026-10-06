#ifndef HASHMAP_H
#define HASHMAP_H

#include "common.h"
#include "wc_hash.h"
#include "wc_allocator.h"

#include <string.h>


/* Generic Hashmap with Ownership Semantics
  - Robin Hood Hashing
  - 3 arrays: keys, vals and psls, plus a scratch area for Robin Hood swaps.
    All four live in ONE allocation: [keys][vals][scratch][psls], each region
    aligned for its element. Create = 1 alloc, resize = 1 alloc + 1 free.
  - PSL: probe sequence length: the distance from hashing location
  - we actually store psl + 1 as psl = 0 means empty bucket
  - Robin Hood Invariant: all keys that hash to i come before keys that hash to i + 1
  - keys and vals are stored inline

  SETS: a HashMap with val_size == 0 is a set. The values region takes no
  memory, `val` arguments may be NULL, and val_ops must be NULL. Use
  HashMap_get_key_ptr to reach a stored element. SET_OF(K) in wc_macros.h
  declares one.

  Memory rules: put copies or moves, del MOVES the value into `out`,
  rehash and Robin Hood shuffles are memcpy only (B5). Zero state: capacity == 0.
*/



#ifndef HASHMAP_INIT_CAPACITY
#define HASHMAP_INIT_CAPACITY 16 // power of two; grows by doubling at load 0.75
#endif


typedef struct {
    u8*           keys; // start of the single block
    u8*           vals;
    u8*           scratch; // STAGE key/val + SWAP key/val, for Robin Hood insertion
    u8*           psls;
    u64           size;
    u64           capacity;
    u32           key_size;
    u32           val_size; // 0: a set
    wc_hash_fn    hash_fn;  // NULL: wc_hash over the key bytes
    wc_compare_fn cmp_fn;   // NULL: byte equality

    // Shared ops vtables for keys and values.
    // Pass NULL for POD types (int, float, flat structs).
    // For types with heap resources define one static ops per type:
    const wc_container_ops* key_ops;
    const wc_container_ops* val_ops;
    const wc_allocator*     alloc;
} HashMap;

_Static_assert(sizeof(HashMap) == 96, "HashMap must be 96 bytes");


// Safely extract callbacks, always NULL-safe on ops itself.
#define MAP_COPY(ops) ((ops) ? (ops)->copy_fn : NULL)
#define MAP_DEL(ops)  ((ops) ? (ops)->del_fn : NULL)


// Create a new HashMap by value.
// hash_fn NULL: wc_hash over the key bytes. cmp_fn NULL: byte equality.
// Both defaults are only correct for keys without pointers.
// key_ops / val_ops: pass NULL for POD types. val_size 0 makes a set.
HashMap HashMap_create(const wc_allocator* a, u32 key_size, u32 val_size, wc_hash_fn hash_fn, wc_compare_fn cmp_fn,
                       const wc_container_ops* key_ops, const wc_container_ops* val_ops)
    __attribute__((nonnull(1), warn_unused_result));

// Destroy all elements and free internal buffers via map->alloc.
// Safe on zeroed/moved-from maps. Leaves struct zeroed.
void HashMap_destroy(HashMap* map) __attribute__((nonnull(1)));

// Deep copy src into a new HashMap allocated from `a`.
HashMap HashMap_copy(const wc_allocator* a, const HashMap* src) __attribute__((nonnull(1, 2), warn_unused_result));

// Transfer ownership from src to dest. src is left zeroed.
void HashMap_move(HashMap* dest, HashMap* src) __attribute__((nonnull(1, 2)));

// Insert or update — COPY semantics. val may be NULL for a set.
// Returns 1 if key existed (updated), 0 if new key inserted.
bool HashMap_put(HashMap* map, const void* key, const void* val) __attribute__((nonnull(1, 2)));

// Insert or update, MOVE semantics: key and val point at the caller's elements, both left zeroed
// (a duplicate key is destroyed; the map keeps its own). val may be NULL for a set.
// Returns 1 if key existed (updated), 0 if new key inserted.
bool HashMap_put_move(HashMap* map, void* key, void* val) __attribute__((nonnull(1, 2)));

// Mixed: key copied, val moved.
bool HashMap_put_val_move(HashMap* map, const void* key, void* val) __attribute__((nonnull(1, 2)));

// Mixed: key moved, val copied.
bool HashMap_put_key_move(HashMap* map, void* key, const void* val) __attribute__((nonnull(1, 2)));

// Get value for key: deep COPY into val (the map keeps its own). Returns 1 if found.
// For owning values prefer HashMap_get_ptr: no copy, no allocation.
bool HashMap_get(const HashMap* map, const void* key, void* val) __attribute__((nonnull(1, 2, 3)));

// Pointer to the stored value, or NULL if key is absent.
const void* HashMap_get_ptr(const HashMap* map, const void* key) __attribute__((nonnull(1, 2)));

__attribute__((nonnull(1, 2))) static inline void* HashMap_get_ptr_mut(HashMap* map, const void* key)
{
    return (void*)HashMap_get_ptr(map, key);
}

// Pointer to the stored KEY equal to `key`, or NULL. For a set this is the
// element; for a map it is the canonical key (e.g. an interned String).
// Never change the bytes the hash or compare reads through it.
const void* HashMap_get_key_ptr(const HashMap* map, const void* key) __attribute__((nonnull(1, 2)));

// Bucket iteration accessors
__attribute__((nonnull(1))) static inline u64 HashMap_bucket_count(const HashMap* map)
{
    return map->capacity;
}

bool        HashMap_bucket_occupied(const HashMap* map, u64 i) __attribute__((nonnull(1)));
const void* HashMap_bucket_key_ptr(const HashMap* map, u64 i) __attribute__((nonnull(1)));
void*       HashMap_bucket_val_ptr(HashMap* map, u64 i) __attribute__((nonnull(1)));

// Delete key. out != NULL: the value is MOVED into it (caller owns it, B7).
// out == NULL: the value is deleted. The key is always deleted.
// Returns 1 if found and deleted, 0 if not found.
bool HashMap_del(HashMap* map, const void* key, void* out) __attribute__((nonnull(1, 2)));

// Check if key exists.
bool HashMap_has(const HashMap* map, const void* key) __attribute__((nonnull(1, 2)));

// Print all key-value pairs. val_print may be NULL (sets print keys only).
void HashMap_print(const HashMap* map, wc_print_fn key_print, wc_print_fn val_print) __attribute__((nonnull(1, 2)));

// Make room for n elements in total so that inserting up to n never resizes.
// No-op if the table is already large enough. Never shrinks.
void HashMap_reserve(HashMap* map, u64 n) __attribute__((nonnull(1)));

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
static inline __attribute__((nonnull(1))) bool HashMap_empty(const HashMap* map)
{
    return map->size == 0;
}


#endif // HASHMAP_H
