#ifndef WC_VIEWS_SINGLE_H
#define WC_VIEWS_SINGLE_H

/*
 * views_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "views_single.h"
 *
 * All other files just:
 *     #include "views_single.h"
 */

/* ===== common.h ===== */
#ifndef WC_COMMON_H
#define WC_COMMON_H

/*
 * WCtoolkit
 * Copyright (c) 2026 Wasi Ullah (PAKIWASI)
 * Licensed under the MIT License. See LICENSE file for details.
 */



// LOGGING/ERRORS

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// ANSI Color Codes
#define WC_COLOR_RESET  "\033[0m"
#define WC_COLOR_RED    "\033[1;31m"
#define WC_COLOR_YELLOW "\033[1;33m"
#define WC_COLOR_GREEN  "\033[1;32m"
#define WC_COLOR_BLUE   "\033[1;34m"
#define WC_COLOR_CYAN   "\033[1;36m"



// TODO: warm paths ?

#define WARN(fmt, ...)                                                  \
    do {                                                                \
        printf(WC_COLOR_YELLOW "[WARN]"                                 \
                               " %s:%d:%s(): " fmt "\n" WC_COLOR_RESET, \
               __FILE__, __LINE__, __func__, ##__VA_ARGS__);            \
    } while (0)

__attribute__((noreturn, format(printf, 4, 5))) static inline void
wc_fatal_report(const char* file, int line, const char* func, const char* fmt, ...)
{
    fprintf(stderr, WC_COLOR_RED "[FATAL] %s:%d:%s(): ", file, line, func);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n" WC_COLOR_RESET);
    exit(EXIT_FAILURE);
}

#define FATAL(fmt, ...) wc_fatal_report(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define CHECK_WARN(cond, fmt, ...)                           \
    do {                                                     \
        if (__builtin_expect(!!(cond), 0)) {                 \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                    \
    } while (0)

#define CHECK_WARN_RET(cond, ret, fmt, ...)                  \
    do {                                                     \
        if (__builtin_expect(!!(cond), 0)) {                 \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                                      \
        }                                                    \
    } while (0)

#ifdef NDEBUG
#define CHECK_FATAL(cond, fmt, ...) ((void)0)
#else
#define CHECK_FATAL(cond, fmt, ...)                           \
    do {                                                      \
        if (__builtin_expect(!!(cond), 0)) {                  \
            FATAL("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                     \
    } while (0)
#endif

#define LOG(fmt, ...)                                             \
    do {                                                          \
        printf(WC_COLOR_CYAN "[LOG]"                              \
                             " : %s(): " fmt "\n" WC_COLOR_RESET, \
               __func__, ##__VA_ARGS__);                          \
    } while (0)


#define MALLOC(size, cap, name)                \
    ({                                         \
        void* _mlcd = malloc(size * cap);      \
        CHECK_FATAL(!_mlcd, "\"" #name "\""    \
                            " malloc failed"); \
        _mlcd;                                 \
    })


// TYPES

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t  u8;
typedef uint8_t  b8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define WC_NOT_FOUND ((u64) - 1)

// #define false ((b8)0)
// #define true  ((b8)1)


// GENERIC FUNCTIONS
typedef void (*copy_fn)(u8* dest, const u8* src);
typedef void (*move_fn)(u8* dest, u8** src);
typedef void (*delete_fn)(u8* key);
typedef void (*print_fn)(const u8* elm);
typedef int (*compare_fn)(const u8* a, const u8* b, u64 size);


// Vtable: one instance shared across all vectors of the same type.
// Pass NULL for any callback not needed.
// For POD types, pass NULL for the whole ops pointer.
typedef struct {
    copy_fn   copy_fn; // Deep copy function for owned resources (or NULL)
    move_fn   move_fn; // Transfer ownership and null original (or NULL)
    delete_fn del_fn;  // Cleanup function for owned resources (or NULL)
} container_ops;


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
    if (ptr == NULL || size == 0 || bytes_per_line == 0) {
        return;
    }

    // hex rep 0-15
    const char* hex = "0123456789ABCDEF";

    for (u64 i = 0; i < size; i++) {
        u8 val1 = ptr[i] >> 4;   // get upper 4 bits as num b/w 0-15
        u8 val2 = ptr[i] & 0x0F; // get lower 4 bits as num b/w 0-15

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
    printf("%.2f ", (double)*(float*)elm);
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

/* ===== wc_string.h ===== */
#ifndef WC_WC_STRING_H
#define WC_WC_STRING_H

#ifndef STRING_GROWTH
#define STRING_GROWTH 1.5F // capacity multiplier on grow
#endif

#define STR_SSO_SIZE 24


typedef struct {
    union {
        char* heap;
        char  stk[STR_SSO_SIZE];
    };
    u64 size;
    u64 capacity;
} String;

_Static_assert(sizeof(String) == 40, "String must be 40 bytes");



//  Construction / Destruction

// Create an empty String on the heap.
String* String_create(void) __attribute__((warn_unused_result));

// Create a String on the heap from a cstr.
String* String_from_cstr(const char* cstr) __attribute__((warn_unused_result));

// Create a copy of another heap-allocated String.
String* String_from_String(const String* other) __attribute__((nonnull(1), warn_unused_result));

// Initialise a String whose struct lives on the Stack (data may be on heap).
void String_create_stk(String* str, const char* cstr) __attribute__((nonnull(1)));

// Destroy a heap-allocated String (frees struct + data).
void String_destroy(String* str) __attribute__((nonnull(1)));

// Destroy only the internal data of a Stack-allocated String.
void String_destroy_stk(String* str) __attribute__((nonnull(1)));

// Move: transfer ownership from *src to dest, nulling *src.
// *src must be heap-allocated.
void String_move(String* dest, String** src) __attribute__((nonnull(1, 2)));

// Deep copy src into dest (dest is re-initialised).
// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void String_copy(String* dest, const String* src) __attribute__((nonnull(1, 2)));


//  Capacity

// Ensure capacity >= new_cap (never shrinks).
void String_reserve(String* str, u64 new_cap) __attribute__((nonnull(1)));

// Reserve capacity and fill new slots with c.
void String_reserve_char(String* str, u64 new_cap, char c) __attribute__((nonnull(1)));

// Shrink allocation to exactly fit current size.
void String_shrink_to_fit(String* str) __attribute__((nonnull(1)));


//  Conversion

// Return a malloc'd NUL-terminated copy — caller must free().
char* String_to_cstr(const String* str) __attribute__((nonnull(1), warn_unused_result));

void String_to_cstr_buf(const String* str, char* buff, u64 n) __attribute__((nonnull(1, 2)));

// Return a raw pointer into the internal buffer (no NUL terminator).
char* String_data_ptr(const String* str) __attribute__((nonnull(1)));

// Guarantee a '\0' sits one byte past the last real character, WITHOUT
// touching str->size (str->size is not a "logical length excluding the
// NUL" convention anywhere else in this API, and this function keeps it
// that way). Grows exactly like String_append_char would if the String
// is already full (SSO->heap conversion, or a heap realloc) so the NUL
// always lands in real, owned memory rather than the SSO mode-flag byte.
// See TEMP_CSTR_READ below for the typical use case.
void String_ensure_null_term(String* str) __attribute__((nonnull(1)));


//  Modification

void String_append_char(String* str, char c) __attribute__((nonnull(1)));
void String_append_cstr(String* str, const char* cstr) __attribute__((nonnull(1, 2)));
void String_append_String(String* str, const String* other) __attribute__((nonnull(1, 2)));
// Append other then destroy it (nulls *other).
void String_append_String_move(String* str, String** other) __attribute__((nonnull(1, 2)));

char String_pop_char(String* str) __attribute__((nonnull(1)));

void String_insert_char(String* str, u64 i, char c) __attribute__((nonnull(1)));
void String_insert_cstr(String* str, u64 i, const char* cstr) __attribute__((nonnull(1, 3)));
void String_insert_String(String* str, u64 i, const String* other) __attribute__((nonnull(1, 3)));

void String_remove_char(String* str, u64 i) __attribute__((nonnull(1)));

// Remove chars in range [start, start + len)
void String_remove_range(String* str, u64 start, u64 len) __attribute__((nonnull(1)));

// Remove all chars (keep allocation).
__attribute__((nonnull(1))) static inline void String_clear(String* str)
{
    str->size = 0;
}


//  Access

__attribute__((nonnull(1))) static inline char String_char_at(const String* str, u64 i)
{
    CHECK_FATAL(i >= str->size, "index out of bounds");
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? (str)->stk : (str)->heap)[i];
}

__attribute__((nonnull(1))) static inline char String_char_at_unsafe(const String* str, u64 i)
{
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? (str)->stk : (str)->heap)[i];
}

__attribute__((nonnull(1))) static inline void String_set_char(String* str, u64 i, char c)
{
    CHECK_FATAL(i >= str->size, "index out of bounds");
    ((str->stk[STR_SSO_SIZE - 1] != '\0') ? str->stk : str->heap)[i] = c;
}


//  Comparison

// 0 == equal, <0 == str1 < str2, >0 == str1 > str2
int String_compare(const String* s1, const String* s2) __attribute__((nonnull(1, 2)));
__attribute__((nonnull(1, 2))) static inline b8 String_equals(const String* s1, const String* s2)
{
    return String_compare(s1, s2) == 0;
}
b8 String_equals_cstr(const String* str, const char* cstr) __attribute__((nonnull(1, 2)));


//  Search

// Returns index, or WC_NOT_FOUND if not found.
u64 String_find_char(const String* str, char c) __attribute__((nonnull(1)));
u64 String_find_cstr(const String* str, const char* substr) __attribute__((nonnull(1, 2)));

// Return a heap-allocated subString starting at `start` of `length` chars.
String* String_substr(const String* str, u64 start, u64 length) __attribute__((nonnull(1), warn_unused_result));


//  I/O

void String_print(const String* str) __attribute__((nonnull(1)));


//  Inline helpers

__attribute__((nonnull(1))) static inline u64 String_len(const String* str)
{
    return str->size;
}

__attribute__((nonnull(1))) static inline u64 String_capacity(const String* str)
{
    return str->capacity;
}

__attribute__((nonnull(1))) static inline b8 String_empty(const String* str)
{
    return str->size == 0;
}

__attribute__((nonnull(1))) static inline b8 String_is_sso(const String* str)
{
    return str->stk[STR_SSO_SIZE - 1] != '\0';
}

// Read-only pointer into the buffer. Unlike String_data_ptr, this never
// returns NULL for an empty String. It's meant to be used AFTER
// String_ensure_null_term, where index 0 is guaranteed to hold at least a '\0',
// even when size == 0. Calling this without a prior String_ensure_null_term on 
// a fresh/empty String reads uninitialised memory.
__attribute__((nonnull(1))) static inline const char* String_cstr_view(const String* str)
{
    return String_is_sso(str) ? str->stk : str->heap;
}

#endif /* WC_WC_STRING_H */

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
 *   wc_errno     Expected conditions: pop on empty, Arena full.
 *                Function returns NULL / 0 / void. wc_errno says why.
 *                Ignore it if you don't care. Check it if you do.
 *
 *
 * USAGE
 * -----
 *   // Check a single call:
 *   wc_errno = WC_OK;
 *   u8* p = Arena_alloc(Arena, size);
 *   if (!p && wc_errno == WC_ERR_FULL) { ... }
 *
 *   // Check a batch — wc_errno stays set if any call failed:
 *   wc_errno = WC_OK;
 *   float* a = (float*)Arena_alloc(Arena, 256);
 *   float* b = (float*)Arena_alloc(Arena, 256);
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
 *   Arena_alloc, Arena_alloc_aligned      WC_ERR_FULL    Arena exhausted
 *   GenVec_pop, GenVec_front, GenVec_back WC_ERR_EMPTY   vec is empty
 *   Queue_pop, Queue_peek, Queue_peek_ptr WC_ERR_EMPTY   Queue is empty
 *   Stack_pop, Stack_peek                 WC_ERR_EMPTY   Stack is empty
 */


typedef enum {
    WC_OK        = 0,
    WC_ERR_FULL,       // Arena exhausted / container at capacity
    WC_ERR_EMPTY,      // pop or peek on empty container
    WC_ERR_INVALID_OP, // call to a function with preconditions not met
} wc_err;

static inline const char* wc_strerror(wc_err e)
{
    switch (e) {
        case WC_OK:             return "ok";
        case WC_ERR_FULL:       return "full";
        case WC_ERR_EMPTY:      return "empty";
        case WC_ERR_INVALID_OP: return "invalid op";
        default:                return "unknown";
    }
}

/* Defined in wc_errno.c:
 *   _Thread_local wc_err wc_errno = WC_OK;
 */
extern _Thread_local wc_err wc_errno;

/* Print last error — same pattern as perror(3).
 *   wc_perror("Arena_alloc");  ->  "Arena_alloc: full"
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

#include <stdlib.h>


typedef struct {
    u8* base;
    u64 idx;
    u64 size;
} Arena;


// Tweakable settings
#ifndef ARENA_DEFAULT_ALIGNMENT
    #define ARENA_DEFAULT_ALIGNMENT (sizeof(void*)) // 8 bytes
#endif
#ifndef ARENA_DEFAULT_SIZE
    #define ARENA_DEFAULT_SIZE      (nKB(4))      // 4 KB
#endif


/*
Allocate and return a pointer to memory to the Arena
with a region with the specified size. Providing a
size = 0 results in size = ARENA_DEFAULT_SIZE (user can modify)

Parameters:
  u64 size    |    The size (in bytes) of the Arena
                      memory region.
Return:
  Pointer to Arena on success, NULL on failure
*/
Arena* Arena_create(u64 capacity) __attribute__((warn_unused_result));

/*
Initialize an Arena object with pointers to the Arena and a
pre-allocated region(base ptr), as well as the size of the provided
region. Good for using the Stack instead of the heap.
The Arena and the data may be Stack initialized, so no Arena_destroy.
Note that ARENA_DEFAULT_SIZE is not used.

Parameters:
  Arena* Arena    |   The Arena object being initialized.
  u8*    data     |   The region to be Arena-fyed.
  u64    size     |   The size of the region in bytes.
*/
void Arena_create_arr_stk(Arena* arena, u64 size, u8* data) __attribute__((nonnull(1, 3)));



void Arena_create_stk(Arena* arena, u64 capacity) __attribute__((nonnull(1)));

/*
Reset the pointer to the Arena region to the beginning
of the allocation. Allows reuse of the memory without
expensive frees.

Parameters:
  Arena *Arena    |    The Arena to be cleared.
*/
static inline __attribute__((nonnull(1))) void Arena_clear(Arena* Arena)
{
    Arena->idx = 0;
}

/*
Free the memory allocated for the entire Arena region.

Parameters:
  Arena *Arena    |    The Arena to be destroyed.
*/
static inline __attribute__((nonnull(1))) void Arena_destroy(Arena* Arena)
{
    free(Arena->base);
    free(Arena);
}

/*
Return a pointer to a portion of specified size of the
specified Arena's region. By default, memory is
aligned by alignof(size_t), but you can change this by
#defining ARENA_DEFAULT_ALIGNMENT before #include'ing
Arena.h. Providing a size of zero results in a failure.

Parameters:
  Arena* Arena    |    The Arena of which the pointer
                       from the region will be
                       distributed
  u64 size        |    The size (in bytes) of
                       allocated memory planned to be
                       used.
Return:
  Pointer to Arena region segment on success, NULL on
  failure.
*/
u8* Arena_alloc(Arena* Arena, u64 size) __attribute__((nonnull(1), alloc_size(2)));

/*
Same as Arena_alloc, except you can specify a memory
alignment for allocations.

Return a pointer to a portion of specified size of the
specified Arena's region. Providing a size of
zero results in a failure.

Parameters:
  Arena* Arena              |    The Arena of which the pointer
                                 from the region will be
                                 distributed
  u64 size                  |    The size (in bytes) of
                                 allocated memory planned to be
                                 used.
  u32 alignment             |    Alignment (in bytes) for each
                                 memory allocation.
Return:
  Pointer to Arena region segment on success, NULL on
  failure.
*/
u8* Arena_alloc_aligned(Arena* Arena, u64 size, u32 alignment) __attribute__((nonnull(1), alloc_size(2)));


// Get used capacity
static inline __attribute__((nonnull(1))) u64 Arena_used(Arena* Arena)
{
    return Arena->idx;
}

// Get remaining capacity
static inline __attribute__((nonnull(1))) u64 Arena_remaining(Arena* Arena)
{
    return Arena->size - Arena->idx;
}



// explicit scratch Arena

typedef struct {
    Arena* Arena;
    u64 mark;
} ArenaScratch;


static inline __attribute__((nonnull(1))) ArenaScratch Arena_scratch_begin(Arena* Arena)
{
    return (ArenaScratch){ .Arena = Arena, .mark = Arena->idx };
}

static inline void Arena_scratch_end(ArenaScratch scratch)
{
    if (scratch.Arena) {
        scratch.Arena->idx = scratch.mark;
        scratch.Arena = NULL;
    }
}

static inline void wc_Arena_scratch_cleanup(ArenaScratch* s)
{
    if (s && s->Arena) {
        s->Arena->idx = s->mark;
        s->Arena = NULL;
    }
}

// macro for automatic cleanup Arena_scratch — safe with return/break/goto
#define ARENA_SCRATCH(Arena_ptr)                                                                             \
    for (int _as_once = 1; _as_once; _as_once = 0)                                                          \
        for (ArenaScratch __attribute__((cleanup(wc_Arena_scratch_cleanup))) _as_s = Arena_scratch_begin(Arena_ptr); \
             _as_once; _as_once = 0)

/* USAGE:
// Manual:
ScratchArena scratch = Arena_scratch_begin(Arena);
char* tmp = ARENA_ALLOC_N(Arena, char, 256);
Arena_scratch_end(scratch);

// Automatic:
ARENA_SCRATCH(Arena) {
    char* tmp = ARENA_ALLOC_N(Arena, char, 256);
} // auto cleanup
*/


// USEFULL MACROS

#define ARENA_CREATE_STK_ARR(Arena, n) (Arena_create_arr_stk((Arena), nKB(n), (u8[nKB(n)]){0}))

// typed allocation
#define ARENA_ALLOC(Arena, T) ((T*)Arena_alloc((Arena), sizeof(T)))

#define ARENA_ALLOC_N(Arena, T, n) ((T*)Arena_alloc((Arena), sizeof(T) * (n)))

// common for structs
#define ARENA_ALLOC_ZERO(Arena, T) ((T*)memset(ARENA_ALLOC(Arena, T), 0, sizeof(T)))

#define ARENA_ALLOC_ZERO_N(Arena, T, n) ((T*)memset(ARENA_ALLOC_N(Arena, T, n), 0, sizeof(T) * (n)))

// Allocate and copy array into Arena
#define ARENA_PUSH_ARRAY(Arena, T, src, count)      \
    ({                                              \
        (T)* _dst = ARENA_ALLOC_N(Arena, T, count); \
        memcpy(_dst, src, sizeof(T) * (count));     \
        _dst;                                       \
    })

#endif /* WC_ARENA_H */

/* ===== views.h ===== */
#ifndef WC_VIEWS_H
#define WC_VIEWS_H

// NOT COPYABLE: non-owning view into an Arena or StringStore.
typedef struct {
    const char* ptr;
    u64         len;
} StrView;

StrView StrView_from_String(String* str) __attribute__((nonnull(1)));

StrView StrView_from_String_explicit(String* str, u64 off, u64 len) __attribute__((nonnull(1)));

// allocate a cstr to an Arena and return a view over it
// kinda like an append only store
StrView StrView_cstr_Arena(Arena* a, const char* cstr, u64 clen) __attribute__((nonnull(1, 2)));

void StrView_print(StrView sv);



#define StringStore_NODE_SIZE 1024


typedef struct StringStore_node {
    union {
        char  buf[StringStore_NODE_SIZE];
        char* heap;
    };
    struct StringStore_node* next;
    // 1 → `heap` is live (overflow node), 0 → `buf` is live. Without this flag
    // StringStore_destroy cannot tell which union member to free.
    int owns_heap;
} StringStore_node;

// append-only, immutable String storage with a chain Arena-like backing
// you get StrViews over the immutable Strings
typedef struct {
    StringStore_node* tail;
    StringStore_node* head;
    u32                tail_off; // how much of th tail node is used
    u32                num;      // total number of nodes
} StringStore;

void StringStore_create(StringStore* ss) __attribute__((nonnull(1)));

void StringStore_destroy(StringStore* ss);

StrView StringStore_cstr(StringStore* ss, const char* cstr, u64 clen) __attribute__((nonnull(1, 2)));

// Free a single node (and its heap buffer, if it owns one). Exported so the
// overflow nodes created inside StringStore_cstr can be destroyed explicitly.
void StringStore_destroy_node(StringStore_node* node);

#endif /* WC_VIEWS_H */

#ifdef WC_IMPLEMENTATION

/* ===== wc_string.c ===== */
#ifndef WC_WC_STRING_IMPL
#define WC_WC_STRING_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//  Internal macros

#define GET_STR_PTR(s, i)  (GET_STR(s) + i)
#define GET_STR_CHAR(s, i) (GET_STR(s)[i])
#define STR_REMAINING(s)   ((s)->capacity - (s)->size)
#define IS_SSO(s)          (s->stk[STR_SSO_SIZE - 1] != '\0')
#define GET_STR(s)         (IS_SSO(s) ? (s)->stk : (s)->heap)

// Grow if full.
#define MAYBE_GROW_STR(s)                        \
    do {                                         \
        if ((s)->size >= (s)->capacity) {        \
            if (IS_SSO(s)) {                     \
                s->stk[STR_SSO_SIZE - 1] = '\0'; \
                stk_to_heap(s);                  \
            } else {                             \
                String_grow(s);                  \
            }                                    \
        }                                        \
    } while (0)



//  Private helpers

static inline u64  cstr_len(const char* cstr);
static inline void stk_to_heap(String* s);
static inline void heap_to_stk(String* s);
static inline void String_grow(String* s);
static inline void ensure_capacity(String* s, u64 needed);



//  Construction / Destruction

String* String_create(void)
{
    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    s->size                  = 0;
    s->capacity              = STR_SSO_SIZE - 1; // 0-22 bit is usable
    s->stk[STR_SSO_SIZE - 1] = 1;                // when last bit (23) is '\0' (NULL) then we have moved to the heap

    return s;
}

String* String_from_cstr(const char* cstr)
{
    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    String_create_stk(s, cstr);
    return s;
}

String* String_from_String(const String* other)
{
    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    s->size                  = 0;
    s->capacity              = STR_SSO_SIZE - 1;
    s->stk[STR_SSO_SIZE - 1] = 1; // mark SSO mode before GET_STR() is used below

    if (other->size > 0) {
        ensure_capacity(s, other->size);
        memcpy(GET_STR(s), GET_STR(other), other->size);
        s->size = other->size;
    }

    return s;
}

void String_create_stk(String* s, const char* cstr)
{
    s->size                  = 0;
    s->stk[STR_SSO_SIZE - 1] = 1;                // mark SSO mode
    s->capacity              = STR_SSO_SIZE - 1; // last byte reserved for the SSO flag

    if (!cstr) {
        return;
    }

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, len);
    memcpy(GET_STR(s), cstr, len);
    s->size = len;
}

void String_destroy(String* s)
{
    String_destroy_stk(s);
    free(s);
}

void String_destroy_stk(String* s)
{
    if (!IS_SSO(s)) {
        free(s->heap);
    }

    s->size                  = 0;
    s->stk[STR_SSO_SIZE - 1] = 1;                // mark SSO mode; NOT preserved from heap mode
    s->capacity              = STR_SSO_SIZE - 1; // leave in valid, reusable SSO state
}

void String_move(String* dest, String** src)
{
    CHECK_FATAL(!*src, "*src is null");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    String_destroy_stk(dest);
    memcpy(dest, *src, sizeof(String));

    // Zero out src so its destructor is harmless, then free the struct
    (*src)->size     = 0;
    (*src)->capacity = STR_SSO_SIZE - 1;
    free(*src);
    *src = NULL;
}

void String_copy(String* dest, const String* src)
{
    if (src == dest) {
        return;
    }

    // dest is documented as "re-initialised": callers may pass raw/uninitialised
    // memory , so we must not read dest's old state before it has ever been initialised.
    dest->size                  = 0;
    dest->capacity              = STR_SSO_SIZE - 1;
    dest->stk[STR_SSO_SIZE - 1] = 1; // mark SSO mode before GET_STR() is used below

    if (src->size > 0) {
        ensure_capacity(dest, src->size);
        memcpy(GET_STR(dest), GET_STR(src), src->size);
        dest->size = src->size;
    }
}


//  Capacity

void String_reserve(String* s, u64 new_cap)
{
    if (new_cap <= s->capacity) {
        return;
    }
    ensure_capacity(s, new_cap);
}

void String_reserve_char(String* s, u64 new_cap, char c)
{
    if (new_cap <= s->capacity) {
        // Fill from current size up to new_cap within existing allocation.
        char* buf = GET_STR(s);
        for (u64 i = s->size; i < new_cap; i++) {
            buf[i] = c;
        }
        s->size = new_cap;
        return;
    }

    u64 old_size = s->size;
    ensure_capacity(s, new_cap);

    char* buf = GET_STR(s);
    for (u64 i = old_size; i < new_cap; i++) {
        buf[i] = c;
    }
    s->size = new_cap;
}

void String_shrink_to_fit(String* s)
{
    if (IS_SSO(s)) {
        return;
    } // already optimal

    if (s->size == 0) {
        free(s->heap);
        s->heap     = NULL;
        s->capacity = STR_SSO_SIZE - 1;
        return;
    }

    if (s->size <= STR_SSO_SIZE - 1) {
        // Bring back to SSO (only place this happens)
        heap_to_stk(s);
        return;
    }

    char* new_data = realloc(s->heap, s->size);
    if (!new_data) {
        WARN("shrink_to_fit realloc failed");
        return;
    }
    s->heap     = new_data;
    s->capacity = s->size;
}


//  Conversion

char* String_to_cstr(const String* s)
{
    char* out = malloc(s->size + 1);
    CHECK_FATAL(!out, "malloc failed");

    if (s->size > 0) {
        memcpy(out, GET_STR(s), s->size);
    }
    out[s->size] = '\0';

    return out;
}

void String_to_cstr_buf(const String* str, char* buff, u64 n)
{
    CHECK_FATAL(n < str->size + 1, "buffer not enough");

    if (str->size > 0) {
        memcpy(buff, GET_STR(str), str->size);
    }
    buff[str->size] = '\0';
}

char* String_data_ptr(const String* s)
{
    if (s->size == 0) {
        return NULL;
    }
    // Cast away const intentionally: caller may mutate via this pointer.
    return (char*)(IS_SSO(s) ? s->stk : s->heap);
}

// Same growth path as String_append_char, minus the size++: writes '\0'
// at index s->size and leaves size untouched. Safe against the SSO
// mode-flag byte because MAYBE_GROW_STR converts to heap (or reallocs
// the heap buffer) whenever size == capacity, before we ever write —
// so the write always lands one past the last real char, never on the
// flag byte at stk[STR_SSO_SIZE - 1].
void String_ensure_null_term(String* s)
{
    MAYBE_GROW_STR(s);
    GET_STR_CHAR(s, s->size) = '\0';
}


//  Modification

void String_append_char(String* s, char c)
{
    MAYBE_GROW_STR(s);
    GET_STR_CHAR(s, s->size++) = c;
}

void String_append_cstr(String* s, const char* cstr)
{
    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, s->size + len);
    memcpy(GET_STR(s) + s->size, cstr, len);
    s->size += len;
}

void String_append_String(String* s, const String* other)
{
    if (other->size == 0) {
        return;
    }

    ensure_capacity(s, s->size + other->size);
    memcpy(GET_STR(s) + s->size, GET_STR(other), other->size);
    s->size += other->size;
}

void String_append_String_move(String* s, String** other)
{
    CHECK_FATAL(!*other, "*other is null");

    if ((*other)->size > 0) {
        String_append_String(s, *other);
    }

    String_destroy(*other);
    *other = NULL;
}

char String_pop_char(String* s)
{
    CHECK_FATAL(s->size == 0, "cannot pop from empty String");

    char c = GET_STR_CHAR(s, --s->size);
    return c;
}

void String_insert_char(String* s, u64 i, char c)
{
    CHECK_FATAL(i > s->size, "index out of bounds");

    MAYBE_GROW_STR(s);

    char* buf = GET_STR(s);
    // Shift right.
    for (u64 j = s->size; j > i; j--) {
        buf[j] = buf[j - 1];
    }
    buf[i] = c;
    s->size++;
}

void String_insert_cstr(String* s, u64 i, const char* cstr)
{
    CHECK_FATAL(i > s->size, "index out of bounds");

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, s->size + len);

    char* buf = GET_STR(s);
    // Shift existing chars right by len positions.
    for (u64 j = s->size; j > i; j--) {
        buf[j + len - 1] = buf[j - 1];
    }
    memcpy(buf + i, cstr, len);
    s->size += len;
}

void String_insert_String(String* s, u64 i, const String* other)
{
    CHECK_FATAL(i > s->size, "index out of bounds");

    if (other->size == 0) {
        return;
    }

    CHECK_WARN_RET(s == other, , "can't insert aliasing(same) Strings");

    u64 len = other->size;
    ensure_capacity(s, s->size + len);

    char* buf = GET_STR(s);
    for (u64 j = s->size; j > i; j--) {
        buf[j + len - 1] = buf[j - 1];
    }
    memcpy(buf + i, GET_STR(other), len);
    s->size += len;
}

void String_remove_char(String* s, u64 i)
{
    CHECK_FATAL(i >= s->size, "index out of bounds");

    char* buf = GET_STR(s);
    for (u64 j = i; j < s->size - 1; j++) {
        buf[j] = buf[j + 1];
    }
    s->size--;
}

/*
    0 1 2 3 4 5  (1, 2)
      ^ ^ 
    start = 1
    len = 2
    end = 2

*/

void String_remove_range(String* s, u64 start, u64 len)
{
    CHECK_FATAL(start >= s->size, "start out of bounds");

    if (len == 0) {
        return;
    }

    if (start + len >= s->size) {
        len = s->size - start;
    }

    memmove(GET_STR_PTR(s, start), GET_STR_PTR(s, start + len), s->size - start - len);

    s->size -= len;
}


//  Access


//  Comparison

int String_compare(const String* s1, const String* s2)
{
    u64 min_len = s1->size < s2->size ? s1->size : s2->size;

    if (min_len > 0) {
        int cmp = memcmp(GET_STR(s1), GET_STR(s2), min_len);
        if (cmp != 0) {
            return cmp;
        }
    }

    if (s1->size < s2->size) {
        return -1;
    }
    if (s1->size > s2->size) {
        return 1;
    }
    return 0;
}

b8 String_equals_cstr(const String* s, const char* cstr)
{
    u64 len = cstr_len(cstr);

    if (s->size != len) {
        return false;
    }
    if (len == 0) {
        return true;
    }

    return memcmp(GET_STR(s), cstr, len) == 0;
}


//  Search

u64 String_find_char(const String* s, char c)
{
    if (s->size == 0) {
        return WC_NOT_FOUND;
    }
    const char* buf = GET_STR(s);
    const char* p   = memchr(buf, (unsigned char)c, s->size);
    return p ? (u64)(p - buf) : WC_NOT_FOUND;
}

u64 String_find_cstr(const String* s, const char* substr)
{
    u64 len = cstr_len(substr);
    if (len == 0) {
        return 0;
    }
    if (len > s->size) {
        return WC_NOT_FOUND;
    }

    const char* buf = GET_STR(s);
    for (u64 i = 0; i <= s->size - len; i++) {
        if (memcmp(buf + i, substr, len) == 0) {
            return i;
        }
    }
    return WC_NOT_FOUND;
}

String* String_substr(const String* s, u64 start, u64 length)
{
    CHECK_FATAL(start >= s->size, "start out of bounds");

    if (start + length > s->size) {
        length = s->size - start;
    }

    String* result = String_create();

    if (length > 0) {
        ensure_capacity(result, length);
        memcpy(GET_STR(result), GET_STR(s) + start, length);
        result->size = length;
    }

    return result;
}


//  I/O

void String_print(const String* s)
{
    putchar('"');
    const char* buf = GET_STR(s);
    for (u64 i = 0; i < s->size; i++) {
        putchar(buf[i]);
    }
    putchar('"');
}


static inline u64 cstr_len(const char* cstr)
{
    return (u64)strlen(cstr);
}


// Promote SSO buffer to heap allocation.
static inline void stk_to_heap(String* s)
{
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);

    char* new_data = malloc(new_cap);
    CHECK_FATAL(!new_data, "malloc failed");

    memcpy(new_data, s->stk, s->size);

    s->heap     = new_data;
    s->capacity = new_cap;
}

static inline void heap_to_stk(String* s)
{
    // save the ptr as memcpy on stk will overwrite
    char* heap = s->heap;
    memcpy(s->stk, heap, s->size);
    free(heap);
    s->stk[STR_SSO_SIZE - 1] = 1; // mark SSO mode; NOT preserved from heap mode
    s->capacity              = STR_SSO_SIZE - 1;
}

static inline void String_grow(String* s)
{
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);

    char* new_data = realloc(s->heap, new_cap);
    CHECK_FATAL(!new_data, "realloc failed");

    s->heap     = new_data;
    s->capacity = new_cap;
}

static inline void ensure_capacity(String* s, u64 needed)
{
    if (needed <= s->capacity) {
        return;
    }

    // Grow by at least STRING_GROWTH factor
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);
    if (new_cap < needed) {
        new_cap = needed;
    }

    // currently in sso but sso_cap is not enough
    if (IS_SSO(s)) {
        s->stk[STR_SSO_SIZE - 1] = '\0';
        char* new_data           = malloc(new_cap);
        CHECK_FATAL(!new_data, "malloc failed");
        memcpy(new_data, s->stk, s->size);
        s->heap     = new_data;
        s->capacity = new_cap;
    } else {
        char* new_data = realloc(s->heap, new_cap);
        CHECK_FATAL(!new_data, "realloc failed");
        s->heap     = new_data;
        s->capacity = new_cap;
    }
}

#endif /* WC_WC_STRING_IMPL */

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

#include <stdlib.h>

/*'''python
align a 4 byte thing to 8 bytes alignment boundry:
>>> 4 + (8 - 1) & ~(8 - 1)
8
>>> 7 + (8 - 1) & ~(8 - 1)
8
>>> 9 + (8 - 1) & ~(8 - 1)
16 <- how much bytes should a 9 byte thing occupy to align to boundry
>>> 15 + (8 - 1) & ~(8 - 1)
16
>>> 18 + (8 - 1) & ~(8 - 1)
24
'''*/
// Align a value to alignment boundary
// Note: align MUST be power of 2 and >= 1
#define ALIGN_UP(val, align) \
    ((align) == 0 ? (val) : (((val) + ((align) - 1)) & ~((align) - 1)))

// align value to ARENA_DEFAULT_ALIGNMENT
#define ALIGN_UP_DEFAULT(val) \
    ALIGN_UP((val), ARENA_DEFAULT_ALIGNMENT)


#define ARENA_PTR(Arena, idx) ((Arena)->base + (idx))





Arena* Arena_create(u64 capacity)
{
    if (capacity == 0) {
        capacity = ARENA_DEFAULT_SIZE;
    }

    Arena* arena = (Arena*)malloc(sizeof(Arena));
    CHECK_FATAL(!arena, "Arena malloc failed");

    arena->base = (u8*)malloc(capacity);
    CHECK_FATAL(!arena->base, "Arena base malloc failed");

    arena->idx = 0;
    arena->size = capacity;

    return arena;
}

void Arena_create_stk(Arena* arena, u64 capacity)
{
    if (capacity == 0) {
        capacity = ARENA_DEFAULT_SIZE;
    }

    arena->base = (u8*)malloc(capacity);
    CHECK_FATAL(!arena->base, "Arena base malloc failed");

    arena->idx  = 0;
    arena->size = capacity;
}

void Arena_create_arr_stk(Arena* arena, u64 size, u8* data)
{
    CHECK_FATAL(size == 0, "size can't be zero");

    arena->base = data;
    arena->idx = 0;
    arena->size = size;
}

u8* Arena_alloc(Arena* arena, u64 size)
{
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");

    // Align the current index first
    u64 aligned_idx = ALIGN_UP_DEFAULT(arena->idx);
    WC_SET_RET(WC_ERR_FULL, arena->size - aligned_idx < size, NULL);

    u8* ptr = ARENA_PTR(arena, aligned_idx);
    arena->idx = aligned_idx + size;

    return ptr;
}

u8* Arena_alloc_aligned(Arena* arena, u64 size, u32 alignment)
{

    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    CHECK_FATAL((alignment & (alignment - 1)) != 0,
                "alignment must be power of two");


    u64 aligned_idx = ALIGN_UP(arena->idx, alignment);

    WC_SET_RET(WC_ERR_FULL, arena->size - aligned_idx < size, NULL);

    u8* ptr = ARENA_PTR(arena, aligned_idx);
    arena->idx = aligned_idx + size;

    return ptr;
}

#endif /* WC_ARENA_IMPL */

/* ===== views.c ===== */
#ifndef WC_VIEWS_IMPL
#define WC_VIEWS_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


StrView StrView_from_String(String* str)
{
    return (StrView){.ptr = String_data_ptr(str), .len = String_len(str)};
}

StrView StrView_from_String_explicit(String* str, u64 off, u64 len)
{
    CHECK_FATAL(off + len >= String_len(str), "invalid range");
    return (StrView){.ptr = String_data_ptr(str) + off, .len = len};
}

StrView StrView_cstr_Arena(Arena* a, const char* cstr, u64 clen)
{
    char* p = ARENA_ALLOC_N(a, char, clen + 1); // for NULL Terminator
    memcpy(p, cstr, clen + 1);
    return (StrView){.ptr = p, .len = clen};
}

void StrView_print(StrView sv)
{
    for (u64 i = 0; i < sv.len; i++) {
        putchar(sv.ptr[i]);
    }
}



#define TAIL_BUF_OFF(ss) ((ss)->tail->buf + (ss)->tail_off)

void StringStore_create(StringStore* ss)
{
    StringStore_node* node = malloc(sizeof(StringStore_node));
    CHECK_FATAL(!node, "node malloc failed");

    node->next      = NULL;
    node->owns_heap = 0;
    ss->head        = node;
    ss->tail        = node;
    ss->tail_off    = 0;
    ss->num         = 1;
}

void StringStore_destroy(StringStore* ss)
{
    if (!ss || !ss->head) {
        return;
    }

    StringStore_node* curr = ss->head;
    StringStore_node* next = NULL;
    do {
        next = curr->next;
        StringStore_destroy_node(curr);
        curr = next;
    } while (curr);
}

static inline void add_node(StringStore* ss)
{
    StringStore_node* node = malloc(sizeof(StringStore_node));
    CHECK_FATAL(!node, "node malloc failed");

    node->next              = NULL; // must terminate the chain for StringStore_destroy
    node->owns_heap         = 0;
    ss->tail->next          = node;
    ss->tail                = node;
    ss->tail_off            = 0;
    ss->num++;
}

StrView StringStore_cstr(StringStore* ss, const char* cstr, u64 clen)
{
    // Strings larger than a whole node go into a dedicated overflow node that
    // owns its buffer via the `heap` union member (malloc'd, node is head).
    // NOTE: the returned view is NOT NUL-terminated for these; only ever hand
    // (ptr, len) or memcpy out of it.
    if (clen > StringStore_NODE_SIZE) {
        StringStore_node* node = malloc(sizeof(StringStore_node));
        CHECK_FATAL(!node, "node malloc failed");

        node->heap      = malloc(clen);
        node->owns_heap = 1; // `heap` is live; StringStore_destroy must free it
        CHECK_FATAL(!node->heap, "overflow node malloc failed");

        memcpy(node->heap, cstr, clen);

        node->next = ss->head; // chain continues from the overflow node
        ss->head   = node;
        ss->num++;

        // Fresh empty tail so the old tail's data stays intact — its remaining
        // free space is abandoned (append-only store, rare path, acceptable).
        add_node(ss);

        return (StrView){.ptr = node->heap, .len = clen};
    }

    if (StringStore_NODE_SIZE - ss->tail_off < clen) {
        // we need another node
        add_node(ss);
    }

    char* ptr = TAIL_BUF_OFF(ss);
    memcpy(ptr, cstr, clen);
    ss->tail_off += (u32)clen;
    return (StrView){.ptr = ptr, .len = clen};
}


void StringStore_destroy_node(StringStore_node* node)
{
    if (!node) {
        return;
    }

    if (node->owns_heap) {
        free(node->heap);
    }
    free(node);
}

#endif /* WC_VIEWS_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_VIEWS_SINGLE_H */
