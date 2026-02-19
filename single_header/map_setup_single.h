#ifndef WC_MAP_SETUP_SINGLE_H
#define WC_MAP_SETUP_SINGLE_H

/*
 * map_setup_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "map_setup_single.h"
 *
 * All other files just:
 *     #include "map_setup_single.h"
 */

/* ===== common.h ===== */
#ifndef WC_COMMON_H
#define WC_COMMON_H

/*
 * C Data Structures Library
 * Copyright (c) 2026 Wasi Ullah (PAKIWASI)
 * Licensed under the MIT License. See LICENSE file for details.
 */



// LOGGING/ERRORS

#include <stdio.h>
#include <stdlib.h>

// ANSI Color Codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_CYAN    "\033[1;36m"

#define WARN(fmt, ...)                                                                       \
    do {                                                                                     \
        printf(COLOR_YELLOW "[WARN]" " %s:%d:%s(): " fmt "\n" COLOR_RESET, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    } while (0)

#define FATAL(fmt, ...)                                                                                \
    do {                                                                                               \
        fprintf(stderr, COLOR_RED "[FATAL]" " %s:%d:%s(): " fmt "\n" COLOR_RESET, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        exit(EXIT_FAILURE);                                                                            \
    } while (0)

#define ASSERT_WARN(cond, fmt, ...)                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            WARN("Assertion failed: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                               \
    } while (0)

#define ASSERT_WARN_RET(cond, ret, fmt, ...)                            \
    do {                                                                \
        if (!(cond)) {                                                  \
            WARN("Assertion failed: (%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                                                 \
        }                                                               \
    } while (0)

#define ASSERT_FATAL(cond, fmt, ...)                                     \
    do {                                                                 \
        if (!(cond)) {                                                   \
            FATAL("Assertion failed: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                                \
    } while (0)

#define CHECK_WARN(cond, fmt, ...)                           \
    do {                                                     \
        if ((cond)) {                                        \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                    \
    } while (0)

#define CHECK_WARN_RET(cond, ret, fmt, ...)                  \
    do {                                                     \
        if ((cond)) {                                        \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                                      \
        }                                                    \
    } while (0)

#define CHECK_FATAL(cond, fmt, ...)                           \
    do {                                                      \
        if (cond) {                                           \
            FATAL("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                     \
    } while (0)

#define LOG(fmt, ...)                               \
    do {                                            \
        printf(COLOR_CYAN "[LOG]" " : %s(): " fmt "\n" COLOR_RESET, __func__, ##__VA_ARGS__); \
    } while (0)

// TYPES

#include <stdint.h>

typedef uint8_t  u8;
typedef uint8_t  b8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define false ((b8)0)
#define true  ((b8)1)


// GENERIC FUNCTIONS
typedef void (*copy_fn)(u8* dest, const u8* src);
typedef void (*move_fn)(u8* dest, u8** src);
typedef void (*delete_fn)(u8* key);
typedef void (*print_fn)(const u8* elm);
typedef int  (*compare_fn)(const u8* a, const u8* b, u64 size);
typedef void (*for_each_fn)(u8* elm); 


// CASTING

#define cast(x)    ((u8*)(&(x)))
#define castptr(x) ((u8*)(x))


// COMMON SIZES

#define KB (1 << 10)
#define MB (1 << 20)

#define nKB(n) ((u64)((n) * KB))
#define nMB(n) ((u64)((n) * MB))


// RAW BYTES TO HEX

void print_hex(const u8* ptr, u64 size, u32 bytes_per_line);

#endif /* WC_COMMON_H */

/* ===== map_setup.h ===== */
#ifndef WC_MAP_SETUP_H
#define WC_MAP_SETUP_H

#ifndef MAP_SETUP
#define MAP_SETUP

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

#endif /* WC_MAP_SETUP_H */

#ifdef WC_IMPLEMENTATION

/* ===== common.c ===== */
#ifndef WC_COMMON_IMPL
#define WC_COMMON_IMPL

void print_hex(const u8* ptr, u64 size, u32 bytes_per_line) 
{
    if (ptr == NULL | size == 0 | bytes_per_line == 0) { return; }

    // hex rep 0-15
    const char* hex = "0123456789ABCDEF";
    
    for (u64 i = 0; i < size; i++) 
    {
        u8 val1 = ptr[i] >> 4;      // get upper 4 bits as num b/w 0-15
        u8 val2 = ptr[i] & 0x0F;    // get lower 4 bits as num b/w 0-15
        
        printf("%c%c", hex[val1], hex[val2]);
        
        // Add space or newline appropriately
        if ((i + 1) % bytes_per_line == 0) {
            printf("\n");
        } else if (i < size - 1) {
            printf(" ");
        }
    }

    // Add final newline if we didn't just print one
    if (size % bytes_per_line != 0) {
        printf("\n");
    }
}

#endif /* WC_COMMON_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_MAP_SETUP_SINGLE_H */
