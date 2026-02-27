#ifndef HASHMAP_H
#define HASHMAP_H

#include "map_setup.h"


typedef struct {
    u8*   key;
    u8*   val;
    STATE state;
} KV;

typedef struct {
    KV*            buckets;
    u64            size;
    u64            capacity;
    u32            key_size;
    u32            val_size;
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


// Create a new hashmap.
// hash_fn and cmp_fn default to fnv1a_hash / default_compare if NULL.
// key_ops / val_ops: pass NULL for POD types.
hashmap* hashmap_create(u32 key_size, u32 val_size,
                        custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* key_ops, const container_ops* val_ops);

void hashmap_destroy(hashmap* map);

// Insert or update — COPY semantics.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put(hashmap* map, const u8* key, const u8* val);

// Insert or update — MOVE semantics (key and val are u8**, both nulled).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 hashmap_put_move(hashmap* map, u8** key, u8** val);

// Mixed: key copied, val moved.
b8 hashmap_put_val_move(hashmap* map, const u8* key, u8** val);

// Mixed: key moved, val copied.
b8 hashmap_put_key_move(hashmap* map, u8** key, const u8* val);

// Get value for key — copies into val. Returns 1 if found, 0 if not.
b8 hashmap_get(const hashmap* map, const u8* key, u8* val);

// Get pointer to value — no copy.
// WARNING: invalidated by put/del operations.
u8* hashmap_get_ptr(hashmap* map, const u8* key);

// Get raw bucket pointer at index i.
KV* hashmap_get_bucket(hashmap* map, u64 i);

// Delete key. If out is provided, value is copied to it before deletion.
// Returns 1 if found and deleted, 0 if not found.
b8 hashmap_del(hashmap* map, const u8* key, u8* out);

// Check if key exists.
b8 hashmap_has(const hashmap* map, const u8* key);

// Print all key-value pairs.
void hashmap_print(const hashmap* map, print_fn key_print, print_fn val_print);

// TODO: test
// Remove all elements, keep capacity.
void hashmap_clear(hashmap* map);

// TODO: test
// Deep copy src into dest (dest must be uninitialised or already destroyed).
void hashmap_copy(hashmap* dest, const hashmap* src);


static inline u64 hashmap_size(const hashmap* map)     { CHECK_FATAL(!map, "map is null"); return map->size;      }
static inline u64 hashmap_capacity(const hashmap* map) { CHECK_FATAL(!map, "map is null"); return map->capacity;  }
static inline b8  hashmap_empty(const hashmap* map)    { CHECK_FATAL(!map, "map is null"); return map->size == 0; }


#endif // HASHMAP_H
