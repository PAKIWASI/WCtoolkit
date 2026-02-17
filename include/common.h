#ifndef COMMON_H
#define COMMON_H



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



#endif // COMMON_H
