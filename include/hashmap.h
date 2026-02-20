#ifndef HASHMAP_H
#define HASHMAP_H

#include "map_setup.h"

// TODO: make ergonomic macros


typedef struct {
    u8*             buckets;
    u64             size;
    u64             capacity;
    u32             key_size;
    u32             val_size;
    custom_hash_fn  hash_fn;
    compare_fn      cmp_fn;
    copy_fn         key_copy_fn;
    move_fn         key_move_fn;
    delete_fn       key_del_fn;
    copy_fn         val_copy_fn;
    move_fn         val_move_fn;
    delete_fn       val_del_fn;
} hashmap;

/**
 * Create a new hashmap
 */
hashmap* hashmap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn,
                        compare_fn cmp_fn, copy_fn key_copy, copy_fn val_copy,
                        move_fn key_move, move_fn val_move,
                        delete_fn key_del, delete_fn val_del);

void hashmap_destroy(hashmap* map);

/**
 * Insert or update key-value pair (COPY semantics)
 * Both key and val are passed as const u8*
 * 
 * @return 1 if key existed (updated), 0 if new key inserted
 */
b8 hashmap_put(hashmap* map, const u8* key, const u8* val);

/**
 * Insert or update key-value pair (MOVE semantics)
 * Both key and val are passed as u8** and will be nulled
 * 
 * @return 1 if key existed (updated), 0 if new key inserted
 */
b8 hashmap_put_move(hashmap* map, u8** key, u8** val);

/**
 * Insert with mixed semantics
 */
b8 hashmap_put_val_move(hashmap* map, const u8* key, u8** val);
b8 hashmap_put_key_move(hashmap* map, u8** key, const u8* val);

/**
 * Get value for key (copy semantics)
 */
b8 hashmap_get(const hashmap* map, const u8* key, u8* val);

/**
 * Get pointer to value (no copy)
 * WARNING: Pointer invalidated by put/del operations
 */
u8* hashmap_get_ptr(hashmap* map, const u8* key);

/**
 * Delete key-value pair
 * If out is provided, value is copied to it before deletion
 */
b8 hashmap_del(hashmap* map, const u8* key, u8* out);



/**
 * Check if key exists
 */
b8 hashmap_has(const hashmap* map, const u8* key);

/**
 * Print all key-value pairs
 */
void hashmap_print(const hashmap* map, print_fn key_print, print_fn val_print);



static inline u64 hashmap_size(const hashmap* map)
{
    CHECK_FATAL(!map, "map is null");
    return map->size;
}

static inline u64 hashmap_capacity(const hashmap* map)
{
    CHECK_FATAL(!map, "map is null");
    return map->capacity;
}

static inline b8 hashmap_empty(const hashmap* map)
{
    CHECK_FATAL(!map, "map is null");
    return map->size == 0;
}


#endif // HASHMAP_H
