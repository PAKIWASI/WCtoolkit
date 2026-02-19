#ifndef MAP_SETUP
#define MAP_SETUP

#include "common.h"
#include <string.h>


typedef enum { 
    EMPTY     = 0,
    FILLED    = 1,
    TOMBSTONE = 2
} STATE;


typedef u64 (*custom_hash_fn)(const u8* key, u64 size);



#define LOAD_FACTOR_GROW      0.70
#define LOAD_FACTOR_SHRINK    0.20
#define HASHMAP_INIT_CAPACITY 17 //prime no (index = hash % capacity)


/*
====================DEFAULT FUNCTIONS====================
*/
// 32-bit FNV-1a (default hash)
static u64 fnv1a_hash(const u8* bytes, u64 size)
{
    u32 hash = 2166136261U; // FNV offset basis

    for (u64 i = 0; i < size; i++) {
        hash ^= bytes[i];  // XOR with current byte
        hash *= 16777619U; // Multiply by FNV prime
    }

    return hash;
}


// Default compare function
static int default_compare(const u8* a, const u8* b, u64 size)
{
    return memcmp(a, b, size);
}

static const u32 PRIMES[] = {
    17,      37,      79,      163,      331,      673,      1361,    2729,
    5471,    10949,   21911,   43853,    87719,    175447,   350899,  701819,
    1403641, 2807303, 5614657, 11229331, 22458671, 44917381, 89834777
};

static const u32 PRIMES_COUNT = sizeof(PRIMES) / sizeof(PRIMES[0]);

// Find the next prime number larger than current
static u64 next_prime(u64 current)
{
    for (u64 i = 0; i < PRIMES_COUNT; i++) {
        if (PRIMES[i] > current) {
            return PRIMES[i];
        }
    }

    // If we've exceeded our prime table, fall back to approximate prime
    // Using formula: next ≈ current * 2 + 1 (often prime or close to it)
    LOG("Warning: exceeded prime table, using approximation\n");
    return (current * 2) + 1;
}

// Find the previous prime number smaller than current
static u64 prev_prime(u64 current)
{
    // Search backwards through prime table
    for (u64 i = PRIMES_COUNT - 1; i >= 0; i--) {
        if (PRIMES[i] < current) {
            return PRIMES[i];
        }
    }

    // Should never happen if HASHMAP_INIT_CAPACITY is in our table
    LOG("Warning: no smaller prime found");
    return HASHMAP_INIT_CAPACITY;
}


// custom hash function for "String"

#include "String.h"


static u64 murmurhash3_str(const u8* key, u64 size) 
{
    (void)size;
    
    String* str= (String*)key;
    const char* data = string_data_ptr(str);
    u64 len = string_len(str);
    
    const u32 c1 = 0xcc9e2d51;
    const u32 c2 = 0x1b873593;
    const u32 seed = 0x9747b28c;
    
    u32 h1 = seed;
    
    // Body - process 4-byte chunks
    const u32* blocks = (const u32*)data;
    const u64 nblocks = len / 4;
    
    for (u64 i = 0; i < nblocks; i++) {
        u32 k1 = blocks[i];
        
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        
        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = (h1 * 5) + 0xe6546b64;
    }
    
    // Tail - handle remaining bytes
    const u8* tail = (const uint8_t*)(data + ((size_t)nblocks * 4));
    u32 k1 = 0;
    
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1;
                k1 = (k1 << 15) | (k1 >> 17);
                k1 *= c2;
                h1 ^= k1;
        default:
           break; 
    }
    
    // Finalization
    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    
    return h1;
}


#endif // MAP_SETUP
