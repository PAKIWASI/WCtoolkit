#ifndef HASHMAP_H
#define HASHMAP_H

#include "common.h"
#include "wc_allocator.h"
#include "wc_string.h"

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

  Memory rules: put copies or moves, del MOVES the value into `out` (B7),
  rehash and Robin Hood shuffles are memcpy only (B5). Zero state: capacity == 0.
*/


// Hash `size` bytes at `key`. Only the top bits pick the bucket, and the map
// mixes them first, so a weak custom hash still spreads (see map_home).
typedef u64 (*wc_hash_fn)(const void* key, u64 size);

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


// Safely extract callbacks — always NULL-safe on ops itself.
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


/*
====================HASH FUNCTIONS====================
*/

/* wc_hash: rapidhash V3, "nano" variant (the one tuned for short inputs).
 *
 * Based on wyhash by Wang Yi. Source: https://github.com/Nicoshev/rapidhash
 *
 * Copyright (C) 2025 Nicolas De Carli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Changes from upstream: C only, default seed and secrets folded in, reads in
 * native byte order (hash values are for in-memory tables, never stored or
 * sent, so big-endian hosts need no byte swap). Output matches upstream
 * rapidhashNano() on little-endian hosts; tests/test_hashmap.c checks it.
 */

// 64 x 64 -> 128 bit multiply. *a gets the low half, *b the high half.
static inline __attribute__((always_inline)) void wc_rapid_mum(u64* a, u64* b)
{
    __uint128_t r = (__uint128_t)*a * *b;
    *a            = (u64)r;
    *b            = (u64)(r >> 64);
}

// Multiply, then fold the 128-bit product to 64 bits.
static inline __attribute__((always_inline)) u64 wc_rapid_mix(u64 a, u64 b)
{
    wc_rapid_mum(&a, &b);
    return a ^ b;
}

static inline __attribute__((always_inline)) u64 wc_rapid_read64(const u8* p)
{
    u64 v;
    memcpy(&v, p, 8);
    return v;
}

static inline __attribute__((always_inline)) u64 wc_rapid_read32(const u8* p)
{
    u32 v;
    memcpy(&v, p, 4);
    return v;
}

#define WC_RAPID_S0   0x2d358dccaa6c78a5ULL
#define WC_RAPID_S1   0x8bb84b93962eacc9ULL
#define WC_RAPID_S2   0x4b33a62ed433d4a3ULL
#define WC_RAPID_S7   0xaaaaaaaaaaaaaaaaULL
#define WC_RAPID_SEED 0ULL

// Hash `len` bytes. With a constant len (keys of a known size) every length
// branch folds away: a 4 or 8 byte key costs two 128-bit multiplies.
static inline __attribute__((always_inline)) u64 wc_hash(const void* key, u64 len)
{
    const u8* p    = key;
    u64       seed = WC_RAPID_SEED ^ wc_rapid_mix(WC_RAPID_SEED ^ WC_RAPID_S2, WC_RAPID_S1);
    u64       a    = 0;
    u64       b    = 0;
    u64       i    = len;

    if (WC_LIKELY(len <= 16)) {
        if (len >= 4) {
            seed ^= len;
            if (len >= 8) {
                a = wc_rapid_read64(p);
                b = wc_rapid_read64(p + len - 8);
            } else {
                a = wc_rapid_read32(p);
                b = wc_rapid_read32(p + len - 4);
            }
        } else if (len > 0) {
            a = ((u64)p[0] << 45) | p[len - 1];
            b = p[len >> 1];
        }
    } else {
        if (i > 48) {
            u64 see1 = seed;
            u64 see2 = seed;
            do {
                seed = wc_rapid_mix(wc_rapid_read64(p) ^ WC_RAPID_S0, wc_rapid_read64(p + 8) ^ seed);
                see1 = wc_rapid_mix(wc_rapid_read64(p + 16) ^ WC_RAPID_S1, wc_rapid_read64(p + 24) ^ see1);
                see2 = wc_rapid_mix(wc_rapid_read64(p + 32) ^ WC_RAPID_S2, wc_rapid_read64(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (i > 48);
            seed ^= see1;
            seed ^= see2;
        }
        if (i > 16) {
            seed = wc_rapid_mix(wc_rapid_read64(p) ^ WC_RAPID_S2, wc_rapid_read64(p + 8) ^ seed);
            if (i > 32) {
                seed = wc_rapid_mix(wc_rapid_read64(p + 16) ^ WC_RAPID_S2, wc_rapid_read64(p + 24) ^ seed);
            }
        }
        a = wc_rapid_read64(p + i - 16) ^ i;
        b = wc_rapid_read64(p + i - 8);
    }

    a ^= WC_RAPID_S1;
    b ^= seed;
    wc_rapid_mum(&a, &b);
    return wc_rapid_mix(a ^ WC_RAPID_S7, b ^ WC_RAPID_S1 ^ i);
}

// TODO: move to wc_helpers

// String keys: hash the characters, not the struct (which holds a pointer).
// Pair with str_cmp / str_cmp_ptr from wc_helpers.h. MAP_OF picks both for you.
static inline u64 wc_hash_str(const void* key, u64 size)
{
    (void)size;
    const String* str = key;
    return wc_hash(String_data_ptr(str), String_len(str));
}

static inline u64 wc_hash_str_ptr(const void* key, u64 size)
{
    (void)size;
    const String* str = *(String* const*)key;
    return wc_hash(String_data_ptr(str), String_len(str));
}


#endif // HASHMAP_H
