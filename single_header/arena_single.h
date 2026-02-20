#ifndef WC_ARENA_SINGLE_H
#define WC_ARENA_SINGLE_H

/*
 * arena_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "arena_single.h"
 *
 * All other files just:
 *     #include "arena_single.h"
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
#define COLOR_RESET  "\033[0m"
#define COLOR_RED    "\033[1;31m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_GREEN  "\033[1;32m"
#define COLOR_BLUE   "\033[1;34m"
#define COLOR_CYAN   "\033[1;36m"


#define WARN(fmt, ...)                                            \
    do {                                                          \
        printf(COLOR_YELLOW "[WARN]"                              \
                            " %s:%d:%s(): " fmt "\n" COLOR_RESET, \
               __FILE__, __LINE__, __func__, ##__VA_ARGS__);      \
    } while (0)

#define FATAL(fmt, ...)                                         \
    do {                                                        \
        fprintf(stderr,                                         \
                COLOR_RED "[FATAL]"                             \
                          " %s:%d:%s(): " fmt "\n" COLOR_RESET, \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__);   \
        exit(EXIT_FAILURE);                                     \
    } while (0)

#define CHECK_WARN(cond, fmt, ...)                           \
    do {                                                     \
        if (__builtin_expect(!!(cond), 0)) {                                        \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                    \
    } while (0)

#define CHECK_WARN_RET(cond, ret, fmt, ...)                  \
    do {                                                     \
        if (__builtin_expect(!!(cond), 0)) {                                        \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                                      \
        }                                                    \
    } while (0)

#define CHECK_FATAL(cond, fmt, ...)                           \
    do {                                                      \
        if (__builtin_expect(!!(cond), 0)) {                                           \
            FATAL("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                     \
    } while (0)

#define LOG(fmt, ...)                                       \
    do {                                                    \
        printf(COLOR_CYAN "[LOG]"                           \
                          " : %s(): " fmt "\n" COLOR_RESET, \
               __func__, ##__VA_ARGS__);                    \
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
typedef int (*compare_fn)(const u8* a, const u8* b, u64 size);


// CASTING

#define cast(x)    ((u8*)(&(x)))
#define castptr(x) ((u8*)(x))


// COMMON SIZES

#define KB (1 << 10)
#define MB (1 << 20)

#define nKB(n) ((u64)((n) * KB))
#define nMB(n) ((u64)((n) * MB))


// RAW BYTES TO HEX

static inline void print_hex(const u8* ptr, u64 size, u32 bytes_per_line) 
{
    if (ptr == NULL || size == 0 || bytes_per_line == 0) { return; }

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


// TEST HELPERS

// Generic print functions for primitive types
static inline void wc_print_int(const u8* elm)
{
    printf("%d ", *(int*)elm);
}
static inline void wc_print_u32(const u8* elm)
{
    printf("%u ", *(u32*)elm);
}
static inline void wc_print_u64(const u8* elm)
{
    printf("%llu ", (unsigned long long)*(u64*)elm);
}
static inline void wc_print_float(const u8* elm)
{
    printf("%.2f ", *(float*)elm);
}
static inline void wc_print_char(const u8* elm)
{
    printf("%c ", *(char*)elm);
}
static inline void wc_print_cstr(const u8* elm)
{
    printf("%s ", (const char*)elm);
}

#endif /* WC_COMMON_H */

/* ===== wc_errno.h ===== */
#ifndef WC_WC_ERRNO_H
#define WC_WC_ERRNO_H

#include <stdio.h>


/* wc_errno.h — Error reporting for WCtoolkit
 * ============================================
 *
 * Two tiers:
 *
 *   CHECK_FATAL  Programmer errors: null pointer, out of bounds, OOM.
 *                Crashes with a message. These are bugs, not conditions.
 *
 *   wc_errno     Expected conditions: pop on empty, arena full.
 *                Function returns NULL / 0 / void. wc_errno says why.
 *                Ignore it if you don't care. Check it if you do.
 *
 *
 * USAGE
 * -----
 *   // Check a single call:
 *   wc_errno = WC_OK;
 *   u8* p = arena_alloc(arena, size);
 *   if (!p && wc_errno == WC_ERR_FULL) { ... }
 *
 *   // Check a batch — wc_errno stays set if any call failed:
 *   wc_errno = WC_OK;
 *   float* a = (float*)arena_alloc(arena, 256);
 *   float* b = (float*)arena_alloc(arena, 256);
 *   if (wc_errno) { wc_perror("alloc"); }
 *
 *
 * RULES
 * -----
 *   1. Successful calls do NOT clear wc_errno — clear it yourself.
 *   2. Check the return value first. wc_errno tells you WHY, not WHETHER.
 *   3. wc_errno is thread-local. Each thread has its own copy.
 *
 *
 * WHAT SETS wc_errno
 * ------------------
 *   arena_alloc, arena_alloc_aligned      WC_ERR_FULL    arena exhausted
 *   genVec_pop, genVec_front, genVec_back WC_ERR_EMPTY   vec is empty
 *   dequeue, queue_peek, queue_peek_ptr   WC_ERR_EMPTY   queue is empty
 *   stack_pop, stack_peek                 WC_ERR_EMPTY   stack is empty
 */


typedef enum {
    WC_OK        = 0,
    WC_ERR_FULL,       // arena exhausted / container at capacity
    WC_ERR_EMPTY,      // pop or peek on empty container
} wc_err;

static inline const char* wc_strerror(wc_err e)
{
    switch (e) {
        case WC_OK:        return "ok";
        case WC_ERR_FULL:  return "full";
        case WC_ERR_EMPTY: return "empty";
        default:           return "unknown";
    }
}

/* Defined in wc_errno.c:
 *   _Thread_local wc_err wc_errno = WC_OK;
 */
extern _Thread_local wc_err wc_errno;

/* Print last error — same pattern as perror(3).
 *   wc_perror("arena_alloc");  ->  "arena_alloc: full"
 */
static inline void wc_perror(const char* prefix)
{
    if (prefix && prefix[0]) {
        fprintf(stderr, "%s: %s\n", prefix, wc_strerror(wc_errno));
    } else {
        fprintf(stderr, "%s\n", wc_strerror(wc_errno));
    }
}


/* Internal macros (library use only)
 * ------------------------------------
 * WC_SET_RET — replaces CHECK_WARN_RET at expected-condition sites.
 * Sets wc_errno silently and returns. No print.
 *
 *   WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, );     void return
 *   WC_SET_RET(WC_ERR_FULL,  cond,           NULL); pointer return
 */
#define WC_SET_RET(err_code, cond, ret) \
    do {                                \
        if (cond) {                     \
            wc_errno = (err_code);      \
            return ret;                 \
        }                               \
    } while (0)

/* WC_PROPAGATE_RET — exit immediately if a callee already set wc_errno.
 *
 *   some_internal_fn(vec);
 *   WC_PROPAGATE_RET( );   // exits if some_internal_fn set wc_errno
 */
#define WC_PROPAGATE_RET(ret)    \
    do {                         \
        if (wc_errno != WC_OK) { \
            return ret;          \
        }                        \
    } while (0)

#endif /* WC_WC_ERRNO_H */

/* ===== arena.h ===== */
#ifndef WC_ARENA_H
#define WC_ARENA_H

typedef struct {
    u8* base;
    u64 idx;
    u64 size;
} Arena;




// Tweakable settings
#define ARENA_DEFAULT_ALIGNMENT (sizeof(u64)) // 8 byte
#define ARENA_DEFAULT_SIZE      (nKB(4))      // 4 KB


/*
Allocate and return a pointer to memory to the arena
with a region with the specified size. Providing a
size = 0 results in size = ARENA_DEFAULT_SIZE (user can modify)

Parameters:
  u64 size    |    The size (in bytes) of the arena
                      memory region.
Return:
  Pointer to arena on success, NULL on failure
*/
Arena* arena_create(u64 capacity);

/*
Initialize an arena object with pointers to the arena and a
pre-allocated region(base ptr), as well as the size of the provided
region. Good for using the stack instead of the heap.
The arena and the data may be stack initialized, so no arena_release.
Note that ARENA_DEFAULT_SIZE is not used.

Parameters:
  Arena* arena    |   The arena object being initialized.
  u8*    data     |   The region to be arena-fyed.
  u64    size     |   The size of the region in bytes.
*/
void arena_create_arr_stk(Arena* arena, u8* data, u64 size);

/*
Reset the pointer to the arena region to the beginning
of the allocation. Allows reuse of the memory without
expensive frees.

Parameters:
  Arena *arena    |    The arena to be cleared.
*/
void arena_clear(Arena* arena);

/*
Free the memory allocated for the entire arena region.

Parameters:
  Arena *arena    |    The arena to be destroyed.
*/
void arena_release(Arena* arena);

/*
Return a pointer to a portion of specified size of the
specified arena's region. Nothing will restrict you
from allocating more memory than you specified, so be
mindful of your memory (as you should anyways) or you
will get some hard-to-track bugs. By default, memory is
aligned by alignof(size_t), but you can change this by
#defining ARENA_DEFAULT_ALIGNMENT before #include'ing
arena.h. Providing a size of zero results in a failure.

Parameters:
  Arena* arena    |    The arena of which the pointer
                       from the region will be
                       distributed
  u64 size        |    The size (in bytes) of
                       allocated memory planned to be
                       used.
Return:
  Pointer to arena region segment on success, NULL on
  failure.
*/
u8* arena_alloc(Arena* arena, u64 size);

/*
Same as arena_alloc, except you can specify a memory
alignment for allocations.

Return a pointer to a portion of specified size of the
specified arena's region. Nothing will restrict you
from allocating more memory than you specified, so be
mindful of your memory (as you should anyways) or you
will get some hard-to-track bugs. Providing a size of
zero results in a failure.

Parameters:
  Arena* arena              |    The arena of which the pointer
                                 from the region will be
                                 distributed
  u64 size                  |    The size (in bytes) of
                                 allocated memory planned to be
                                 used.
  u32 alignment             |    Alignment (in bytes) for each
                                 memory allocation.
Return:
  Pointer to arena region segment on success, NULL on
  failure.
*/
u8* arena_alloc_aligned(Arena* arena, u64 size, u32 alignment);


/*
Get the value of index at the current state of arena
This can be used to later clear upto that point using arena_clear_mark

Parameters:
  Arena* arena          |   The arena whose idx will be returned

Return:
  The current value of idx variable
*/
u64 arena_get_mark(Arena* arena);

/*
Clear the arena from current index back to mark

Parameters:
  Arena* arena          |   The arena you want to clear using it's mark
  u64    mark           |   The mark previosly obtained by arena_get_mark 
*/
void arena_clear_mark(Arena* arena, u64 mark);


// Get used capacity
static inline u64 arena_used(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    return arena->idx;
}

// Get remaining capacity
static inline u64 arena_remaining(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    return arena->size - arena->idx;
}



// explicit scratch arena

typedef struct {
    Arena* arena;
    u64 saved_idx;
} arena_scratch;


static inline arena_scratch arena_scratch_begin(Arena* arena) {
    CHECK_FATAL(!arena, "arena is null");
    return (arena_scratch){ .arena = arena, .saved_idx = arena->idx };
}

static inline void arena_scratch_end(arena_scratch* scratch) {
    if (scratch && scratch->arena) {
        scratch->arena->idx = scratch->saved_idx;
    }
}

// macro for automatic cleanup arena_scratch
#define ARENA_SCRATCH(name, arena_ptr) \
    for (arena_scratch name = arena_scratch_begin(arena_ptr); \
         (name).arena != NULL; \
         arena_scratch_end(&(name)), (name).arena = NULL)

/* USAGE:
// Manual:
ScratchArena scratch = arena_scratch_begin(arena);
char* tmp = ARENA_ALLOC_N(arena, char, 256);
arena_scratch_end(&scratch);

// Automatic:
ARENA_SCRATCH(scratch, arena) {
    char* tmp = ARENA_ALLOC_N(arena, char, 256);
} // auto cleanup
*/


// USEFULL MACROS

#define ARENA_CREATE_STK_ARR(arena, n) (arena_create_arr_stk((arena), (u8[nKB(n)]){0}, nKB(n)))

// typed allocation
#define ARENA_ALLOC(arena, T) ((T*)arena_alloc((arena), sizeof(T)))

#define ARENA_ALLOC_N(arena, T, n) ((T*)arena_alloc((arena), sizeof(T) * (n)))

// common for structs
#define ARENA_ALLOC_ZERO(arena, T) ((T*)memset(ARENA_ALLOC(arena, T), 0, sizeof(T)))

#define ARENA_ALLOC_ZERO_N(arena, T, n) ((T*)memset(ARENA_ALLOC_N(arena, T, n), 0, sizeof(T) * (n)))

// Allocate and copy array into arena
#define ARENA_PUSH_ARRAY(arena, T, src, count)      \
    ({                                              \
        (T)* _dst = ARENA_ALLOC_N(arena, T, count); \
        memcpy(_dst, src, sizeof(T) * (count));     \
        _dst;                                       \
    })

#endif /* WC_ARENA_H */

#ifdef WC_IMPLEMENTATION

/* ===== wc_errno.c ===== */
#ifndef WC_WC_ERRNO_IMPL
#define WC_WC_ERRNO_IMPL

/* One definition of the thread-local error variable.
 * Every translation unit that includes wc_error.h sees the extern declaration.
 * This file provides the actual storage.
 */
_Thread_local wc_err wc_errno = WC_OK;

#endif /* WC_WC_ERRNO_IMPL */

/* ===== arena.c ===== */
#ifndef WC_ARENA_IMPL
#define WC_ARENA_IMPL

/* python
align to 8 bytes
>>> 4 + 7 & ~(7)
8
align to 4 bytes
>>> 1 + 4 & ~(4)
1
*/
// Align a value to alignment boundary
#define ALIGN_UP(val, align) \
    (((val) + ((align) - 1)) & ~((align) - 1))

// align value to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_UP_DEFAULT(val) \
    ALIGN_UP((val), ARENA_DEFAULT_ALIGNMENT)

// Align a pointer to alignment boundary  
// turn ptr to a u64 val to align, then turn to ptr again
#define ALIGN_PTR(ptr, align) \
    ((u8*)ALIGN_UP((ptr), (align)))

// align a pointer to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_PTR_DEFAULT(ptr) \
    ALIGN_PTR((ptr), ARENA_DEFAULT_ALIGNMENT)


#define ARENA_CURR_IDX_PTR(arena) ((arena)->base + (arena)->idx)
#define ARENA_PTR(arena, idx) ((arena)->base + (idx))





Arena* arena_create(u64 capacity)
{
    if (capacity == 0) {
        capacity = ARENA_DEFAULT_SIZE;
    }

    Arena* arena = (Arena*)malloc(sizeof(Arena));
    CHECK_FATAL(!arena, "arena malloc failed");

    arena->base = (u8*)malloc(capacity);
    CHECK_FATAL(!arena->base, "arena base malloc failed");

    arena->idx = 0;
    arena->size = capacity;

    return arena;
}

void arena_create_arr_stk(Arena* arena, u8* data, u64 size)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(size == 0, "size can't be zero");

    arena->base = data;
    arena->idx = 0;
    arena->size = size;
}

void arena_clear(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    arena->idx = 0;
}

void arena_release(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");
    
    free(arena->base);
    free(arena);
}

u8* arena_alloc(Arena* arena, u64 size)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    
    // Align the current index first
    u64 aligned_idx = ALIGN_UP_DEFAULT(arena->idx);
    WC_SET_RET(WC_ERR_FULL, arena->size - aligned_idx < size, NULL);
    
    u8* ptr = ARENA_PTR(arena, aligned_idx);
    arena->idx = aligned_idx + size;
    
    return ptr;
}

u8* arena_alloc_aligned(Arena* arena, u64 size, u32 alignment)
{

    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    CHECK_FATAL((alignment & (alignment - 1)) != 0,
                "alignment must be power of two");


    u64 aligned_idx = ALIGN_UP(arena->idx, alignment);

    WC_SET_RET(WC_ERR_FULL, arena->size - aligned_idx < size, NULL);

    u8* ptr = ARENA_PTR(arena, aligned_idx);
    arena->idx = aligned_idx + size;

    return ptr;
}

u64 arena_get_mark(Arena* arena)
{
    CHECK_FATAL(!arena, "arena is null");

    return arena->idx;
}

void arena_clear_mark(Arena* arena, u64 mark)
{
    CHECK_FATAL(!arena, "arena is null");
    CHECK_FATAL(mark > arena->idx, "mark is out of bounds");

    if (mark == arena->idx) { return; }

    arena->idx = mark;
}

#endif /* WC_ARENA_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_ARENA_SINGLE_H */
