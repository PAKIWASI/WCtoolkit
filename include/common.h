#ifndef COMMON_H
#define COMMON_H



/*
 * WCtoolkit
 * Copyright (c) 2026 Wasi Ullah (PAKIWASI)
 * Licensed under the MIT License. See LICENSE file for details.
 */



// LOGGING/ERRORS

#include <stdarg.h>
#include <stdio.h>
#include <string.h>


// ANSI Color Codes
#define WC_COLOR_RESET  "\033[0m"
#define WC_COLOR_RED    "\033[1;31m"
#define WC_COLOR_YELLOW "\033[1;33m"
#define WC_COLOR_GREEN  "\033[1;32m"
#define WC_COLOR_BLUE   "\033[1;34m"
#define WC_COLOR_CYAN   "\033[1;36m"


/*
 * DIAGNOSTICS
 *
 *   SURVIVES EVERY BUILD (Debug and Release)
 *     FATAL(fmt, ...)                 abort with a message
 *
 *     FATAL_IF(cond, fmt, ...)        abort if cond is TRUE. Absolute failures
 *                                     only: allocation failed, or a violation
 *                                     that can't be treated as a caller bug
 *                                     (HashMap_reserve range check).
 *
 *     WARN(fmt, ...)                  print a warning to stderr, continue
 *
 *     WARN_IF(cond, fmt, ...)         warn if cond is TRUE, continue
 *
 *     WARN_IF_RET(cond, ret, fmt, ...) warn and `return ret` if cond is TRUE
 *                                     (GNU statement expression can return)
 *
 *     LOG(fmt, ...)                   print to stderr
 *
 *     LOG_IF(cond, fmt, ...)          log if cond is TRUE
 *
 *   STRIPPED UNDER NDEBUG (Release)
 *
 *     WC_ASSERT(cond, fmt, ...)       abort if cond is FALSE. cond is the
 *                                     INVARIANT (`i < size`), not the failure.
 *                                     For programmer errors: bounds, API misuse.
 *                                     Release: cond is NOT evaluated (no side
 *                                     effects run) but still type-checked.
 *
 * The rule: if continuing past the failure would corrupt memory even in a
 * correct program (an allocator returned NULL), use FATAL_IF. If the failure
 * means the CALLER has a bug, use WC_ASSERT. That includes use of a zeroed,
 * moved-from or destroyed container: caught in Debug only, undefined behavior
 * in Release.
 *
 * Branch prediction: failure paths are wc_unlikely, and the reporters are
 * `cold, noinline` functions defined once in wc_errno.c. The compiler moves
 * every failure branch (argument setup + call) out of the hot path into
 * .text.unlikely, so a check costs one compare and one not-taken branch.
*/

/* ── Compiler extensions (wc_nonull, wc_typeof, wc_likely, ...) ────────────
 * One wrapper block for all non-standard constructs, kept in its own tiny
 * header so the allocator layer can stay kit-independent.
 */
#include "wc_comp.h"

wc_cold wc_noinline wc_noreturn wc_format(printf, 4, 5) void
wc_fatal_report(const char* file, int line, const char* func, const char* fmt, ...);

// TODO:
/* Fatal handler. Every FATAL / FATAL_IF formats its message and calls the
 * installed handler. The handler must not return: abort, exit, or longjmp out
 * (tests can verify that something FATALs). If it does return, the program
 * exits. NULL restores the default (print to stderr, exit(EXIT_FAILURE)).
 * Returns the previous handler. Set it once at startup: it is not thread-local. */
typedef void (*wc_fatal_fn)(const char* file, int line, const char* func, const char* msg);
wc_fatal_fn wc_set_fatal_handler(wc_fatal_fn fn);

wc_cold wc_noinline wc_format(printf, 4, 5) void wc_warn_report(const char* file, int line, const char* func,
                                                                const char* fmt, ...);


/* SURVIVES EVERY BUILD */

#define FATAL(fmt, ...) wc_fatal_report(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define FATAL_IF(cond, fmt, ...) ((void)(wc_unlikely(cond) && (FATAL("(%s): " fmt, #cond, ##__VA_ARGS__), 0)))

#define WARN(fmt, ...) wc_warn_report(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define WARN_IF(cond, fmt, ...) ((void)(wc_unlikely(cond) && (WARN("(%s): " fmt, #cond, ##__VA_ARGS__), 0)))

#define WARN_IF_RET(cond, ret, fmt, ...)              \
    ({                                                \
        if (wc_unlikely(cond)) {                      \
            WARN("(%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                               \
        }                                             \
        (void)0;                                      \
    })

// stderr, like WARN/FATAL: diagnostics never mix into program output.
#define LOG(fmt, ...) \
    ((void)fprintf(stderr, WC_COLOR_CYAN "[LOG] %s(): " fmt "\n" WC_COLOR_RESET, __func__, ##__VA_ARGS__))

#define LOG_IF(cond, fmt, ...) ((void)((cond) && (LOG(fmt, ##__VA_ARGS__), 0)))


/* STRIPPED UNDER NDEBUG */

#ifdef NDEBUG
// Not evaluated (sizeof of an int expression), still compiled: a stale field
// name or type error inside an assert breaks the Release build too, and
// variables used only in asserts don't trigger unused warnings.
#define WC_ASSERT(cond, fmt, ...) ((void)sizeof(!(cond)))
#else
#define WC_ASSERT(cond, fmt, ...) \
    ((void)(wc_likely(cond) || (FATAL("assertion (%s) failed: " fmt, #cond, ##__VA_ARGS__), 0)))
#endif


// Token pasting that expands its arguments first. `a##b` pastes BEFORE expansion,
// so WC_CAT_(x, __LINE__) gives `x__LINE__`; the extra level expands __LINE__ to
// 42 first and then pastes, giving `x42`. Used for __COUNTER__/__LINE__ names.
#define WC_CAT_(a, b) a##b
#define WC_CAT(a, b)  WC_CAT_(a, b)


// TYPES

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// we are using unsigned indices/sizes
#define WC_NOT_FOUND ((u64) - 1)


// FIXED-SIZE COPY / COMPARE
//
// Containers store elements as raw bytes and learn the element size at runtime. These two helpers are
// drop-ins for memcpy / (memcmp == 0): common element sizes take a fixed-size move or integer compare
// instead of a call into libc, other sizes fall back to libc.

// Drop-in for memcpy(dst, src, n), including the restrict contract: dst and src must not overlap.
// If they may overlap, use memmove.
//
// n == 0 is a no-op on purpose. A set (HashMap with val_size 0) stores no values and may pass a NULL
// src for its value copies. memcpy(dst, NULL, 0) is UB, and glibc declares memcpy nonnull, so the
// compiler may assume src != NULL afterwards. The explicit 0 case avoids the call entirely.
static wc_always_inline void wc_copy_n(void* restrict dst, const void* restrict src, u64 n)
{
    switch (n) {
    case 0:
        break; // a set's value: src may be NULL, so no memcpy at all
    case 1:
        memcpy(dst, src, 1);
        break;
    case 2:
        memcpy(dst, src, 2);
        break;
    case 4:
        memcpy(dst, src, 4);
        break;
    case 8:
        memcpy(dst, src, 8);
        break;
    case 16:
        memcpy(dst, src, 16);
        break;
    case 24:
        memcpy(dst, src, 24);
        break;
    case 32:
        memcpy(dst, src, 32);
        break;
    case 48:
        memcpy(dst, src, 48);
        break;
    case 64:
        memcpy(dst, src, 64);
        break;
    default:
        memcpy(dst, src, n);
        break;
    }
}

// Drop-in for (memcmp(a, b, n) == 0), returning true/false instead of the memcmp tri-state.
//
// 4 and 8 bytes compare as one integer (the common hash key: an int, a u64, a pointer), 16 bytes as two
// 8-byte compares (a string view, a pair of u64). The memcpy into a local is the standard type-punning
// idiom: it compiles to a plain load and does not violate strict aliasing. Other sizes use memcmp.
//
// n == 0 is true without touching either pointer, so both may be NULL.
static wc_always_inline bool wc_equal_n(const void* a, const void* b, u64 n)
{
    switch (n) {
    case 0:
        return true;
    case 4: {
        u32 x;
        u32 y;
        memcpy(&x, a, 4);
        memcpy(&y, b, 4);
        return x == y;
    }
    case 8: {
        u64 x;
        u64 y;
        memcpy(&x, a, 8);
        memcpy(&y, b, 8);
        return x == y;
    }
    case 16: {
        u64 x0;
        u64 x1;
        u64 y0;
        u64 y1;
        memcpy(&x0, a, 8);
        memcpy(&x1, (const u8*)a + 8, 8);
        memcpy(&y0, b, 8);
        memcpy(&y1, (const u8*)b + 8, 8);
        return (x0 == y0 && x1 == y1) != 0;
    }
    default:
        return memcmp(a, b, n) == 0;
    }
}


// GENERIC FUNCTIONS
#include "wc_allocator.h"

/* ELEMENT OPS
 *
 * Every element is TRIVIALLY RELOCATABLE. A container may move
 * an element to another address with a raw memcpy (growth, insert/remove shifts,
 * hash table shuffles, queue compaction, taking an element out) and never asks
 * the element first. A type that holds a pointer to itself, or that something
 * outside points into, cannot be stored by value: store it by pointer.
 *
 */

// Deep-copy `src` INTO `dest`, allocating any owned resources from `dst`.
// `dest` is uninitialised raw slot memory: never read or free it (B3).
// Called ONLY when both sides keep the value (B8).
typedef void (*wc_copy_fn)(const wc_allocator* dst, void* dest, const void* src);
// Release owned resources of the element (not the slot). Elements that own
// memory store their own allocator, so no allocator argument is needed (B2).
typedef void (*wc_delete_fn)(void* elm);
typedef void (*wc_print_fn)(const void* elm);
typedef int (*wc_compare_fn)(const void* a, const void* b, u64 size);


// vtable: one instance shared across all objects of the same type.
// Pass NULL for any callback not needed (no copy_fn: copies are memcpy;
// no del_fn: nothing is freed per element). For plain data pass NULL ops.
typedef struct {
    wc_copy_fn   copy_fn; // Deep copy function for owned resources (or NULL)
    wc_delete_fn del_fn;  // Cleanup function for owned resources (or NULL)
} wc_container_ops;


#include <stdalign.h>

// Heap-box a value-returning constructor through allocator A:
//   GenVec* v = WC_BOX(A, GenVec, GenVec_create, 8, sizeof(int), NULL);
// Invariant: the shell comes from the same allocator the child stores, so a
// by-pointer delete can free the shell with the child's own allocator.
#define WC_BOX(A, T, init_fn, ...)                              \
    ({                                                          \
        T* _wbx_p = wc_alloc(A, sizeof(T), alignof(T));         \
        FATAL_IF(!_wbx_p, "WC_BOX(" #T "): allocation failed"); \
        *_wbx_p = init_fn(A, __VA_ARGS__);                      \
        _wbx_p;                                                 \
    })


// COMMON SIZES

#define KB (1 << 10)
#define MB (1 << 20)

#define nKB(n) ((u64)((n) * KB))
#define nMB(n) ((u64)((n) * MB))


// RAW BYTES TO HEX

static inline void print_hex(const void* ptr, u64 size, u32 bytes_per_line)
{
    if (ptr == NULL || size == 0 || bytes_per_line == 0) {
        return;
    }

    // hex rep 0-15
    const char* hex = "0123456789ABCDEF";

    for (u64 i = 0; i < size; i++) {
        u8 val1 = *((u8*)ptr + i) >> 4;   // get upper 4 bits as num b/w 0-15
        u8 val2 = *((u8*)ptr + i) & 0x0F; // get lower 4 bits as num b/w 0-15

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


// TEST HELPERS

// Generic print functions for primitive types
static inline void wc_print_int(const void* elm)
{
    printf("%d ", *(int*)elm);
}
static inline void wc_print_u32(const void* elm)
{
    printf("%u ", *(u32*)elm);
}
static inline void wc_print_u64(const void* elm)
{
    printf("%llu ", (unsigned long long)*(u64*)elm);
}
static inline void wc_print_float(const void* elm)
{
    printf("%.2f ", (double)*(float*)elm);
}
static inline void wc_print_char(const void* elm)
{
    printf("%c ", *(char*)elm);
}
static inline void wc_print_cstr(const void* elm)
{
    printf("%s ", (const char*)elm);
}


#endif // COMMON_H
