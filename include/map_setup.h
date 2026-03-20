#ifndef MAP_SETUP_H
#define MAP_SETUP_H


#include "common.h"
#include <string.h>


typedef u64 (*custom_hash_fn)(const u8* key, u64 size);

#define LOAD_FACTOR_GROW      0.75 // Robin Hood sweet spot
#define HASHMAP_INIT_CAPACITY 16   // power-of-2 to avoid modulo


/*
====================WYHASH====================
*/
// wyhash v4 — public domain, Wang Yi
// Best default for hashmaps: fast, excellent avalanche, low collision rate.
// Beats FNV1a and MurmurHash3 on all key sizes.

static inline u64 wyr8(const u8* p)
{
    u64 v;
    memcpy(&v, p, 8);
    return v;
}
static inline u64 wyr4(const u8* p)
{
    u32 v;
    memcpy(&v, p, 4);
    return v;
}

static inline u64 wymix(u64 a, u64 b)
{
    __uint128_t r = (__uint128_t)a * b;
    return (u64)(r) ^ (u64)(r >> 64);
}

static u64 wyhash(const u8* key, u64 len)
{
    const u64 seed = 0x517cc1b727220a95ULL;
    const u64 s0   = 0x2d358dccaa6c78a5ULL;
    const u64 s1   = 0x8bb84b93962eacc9ULL;
    const u64 s2   = 0x4b33a62ed433d4a3ULL;

    u64 a, b;
    u64 h = seed ^ wymix(seed ^ s0, s1) ^ len;

    const u8* p = key;
    u64       i = len;

    // bulk: 16 bytes at a time
    for (; i >= 16; i -= 16, p += 16) {
        h = wymix(wyr8(p) ^ s1, wyr8(p + 8) ^ h);
    }

    // tail
    if (i >= 8) {
        a = wyr8(p);
        b = wyr8(p + i - 8);
    } else if (i >= 4) {
        a = wyr4(p);
        b = wyr4(p + i - 4);
    } else if (i > 0) {
        a = ((u64)p[0] << 16) | ((u64)p[i >> 1] << 8) | p[i - 1];
        b = 0;
    } else {
        a = 0;
        b = 0;
    }

    return wymix(a ^ s2 ^ h, b ^ s2);
}

/*
====================DEFAULT FUNCTIONS====================
*/

static inline u64 fnv1a_hash(const u8* bytes, u64 size)
{
    u64 hash = 14695981039346656037ULL; // 64-bit FNV offset basis
    for (u64 i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL; // 64-bit FNV prime
    }
    return hash;
}

static inline int default_compare(const u8* a, const u8* b, u64 size)
{
    return memcmp(a, b, size);
}


/*
====================STRING HASHING====================
*/

#include "String.h"

// wyhash variants for String

static u64 wyhash_str(const u8* key, u64 size)
{
    (void)size;
    String* str = (String*)key;
    return wyhash((const u8*)string_data_ptr(str), string_len(str));
}

static u64 wyhash_str_ptr(const u8* key, u64 size)
{
    (void)size;
    String* str = *(String**)key;
    return wyhash((const u8*)string_data_ptr(str), string_len(str));
}

#define ALIGN8(size) (((u64)(size) + 7u) & ~7u)


#endif // MAP_SETUP_H
