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

#define WARN(fmt, ...)                                            \
    do {                                                          \
        printf(WC_COLOR_YELLOW "[WARN]"                              \
                            " %s:%d:%s(): " fmt "\n" WC_COLOR_RESET, \
               __FILE__, __LINE__, __func__, ##__VA_ARGS__);      \
    } while (0)

#define FATAL(fmt, ...)                                         \
    do {                                                        \
        fprintf(stderr,                                         \
                WC_COLOR_RED "[FATAL]"                             \
                          " %s:%d:%s(): " fmt "\n" WC_COLOR_RESET, \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__);   \
        exit(EXIT_FAILURE);                                     \
    } while (0)

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

#define CHECK_FATAL(cond, fmt, ...)                           \
    do {                                                      \
        if (__builtin_expect(!!(cond), 0)) {                  \
            FATAL("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                     \
    } while (0)

#define LOG(fmt, ...)                                       \
    do {                                                    \
        printf(WC_COLOR_CYAN "[LOG]"                           \
                          " : %s(): " fmt "\n" WC_COLOR_RESET, \
               __func__, ##__VA_ARGS__);                    \
    } while (0)


#define MALLOC(size, cap, name) ({\
    void* _mlcd = malloc(size * cap);\
    CHECK_FATAL(!_mlcd, "\"" #name "\"" " malloc failed");\
    _mlcd;\
})


// TYPES

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint8_t  b8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

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

/* ===== String.h ===== */
#ifndef WC_STRING_H
#define WC_STRING_H

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

// 24 8 8 = 40 bytes (same as genVec)




//  Construction / Destruction

// Create an empty String on the heap.
String* String_create(void);

// Create a String on the heap from a cstr.
String* String_from_cstr(const char* cstr);

// Create a copy of another heap-allocated String.
String* String_from_String(const String* other);

// Initialise a String whose struct lives on the Stack (data may be on heap).
void String_create_stk(String* str, const char* cstr);

// Destroy a heap-allocated String (frees struct + data).
void String_destroy(String* str);

// Destroy only the internal data of a Stack-allocated String.
void String_destroy_stk(String* str);

// Move: transfer ownership from *src to dest, nulling *src.
// *src must be heap-allocated.
void String_move(String* dest, String** src);

// Deep copy src into dest (dest is re-initialised).
void String_copy(String* dest, const String* src);


//  Capacity

// Ensure capacity >= new_cap (never shrinks).
void String_reserve(String* str, u64 new_cap);

// Reserve capacity and fill new slots with c.
void String_reserve_char(String* str, u64 new_cap, char c);

// Shrink allocation to exactly fit current size.
void String_shrink_to_fit(String* str);


//  Conversion

// Return a malloc'd NUL-terminated copy — caller must free().
char* String_to_cstr(const String* str);

void String_to_cstr_buf(const String* str, char* buff, u64 n);

// Return a raw pointer into the internal buffer (no NUL terminator).
char* String_data_ptr(const String* str);


//  Modification

void String_append_char(String* str, char c);
void String_append_cstr(String* str, const char* cstr);
void String_append_String(String* str, const String* other);
// Append other then destroy it (nulls *other).
void String_append_String_move(String* str, String** other);

char String_pop_char(String* str);

void String_insert_char(String* str, u64 i, char c);
void String_insert_cstr(String* str, u64 i, const char* cstr);
void String_insert_String(String* str, u64 i, const String* other);

void String_remove_char(String* str, u64 i);

// TODO: test
// Remove chars in range [start, start + len)
void String_remove_range(String* str, u64 start, u64 len);

// Remove all chars (keep allocation).
static inline void String_clear(String* str)
{
    CHECK_FATAL(!str, "str is null");
    str->size = 0;
}


//  Access

static inline char String_char_at(const String* str, u64 i)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(i >= str->size, "index out of bounds");
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? (str)->stk : (str)->heap)[i];
}

static inline char String_char_at_unsafe(const String* str, u64 i)
{
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? (str)->stk : (str)->heap)[i];
}

static inline void String_set_char(String* str, u64 i, char c)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(i >= str->size, "index out of bounds");
    ((str->stk[STR_SSO_SIZE - 1] != '\0') ? str->stk : str->heap)[i] = c;
}


//  Comparison

// 0 == equal, <0 == str1 < str2, >0 == str1 > str2
int              String_compare(const String* s1, const String* s2);
static inline b8 String_equals(const String* s1, const String* s2)
{
    return String_compare(s1, s2) == 0;
}
b8 String_equals_cstr(const String* str, const char* cstr);


//  Search

// Returns index, or (u64)-1 if not found.
u64 String_find_char(const String* str, char c);
u64 String_find_cstr(const String* str, const char* substr);

// Return a heap-allocated subString starting at `start` of `length` chars.
String* String_substr(const String* str, u64 start, u64 length);


//  I/O

void String_print(const String* str);


//  Inline helpers

static inline u64 String_len(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->size;
}

static inline u64 String_capacity(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->capacity;
}

static inline b8 String_empty(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->size == 0;
}

static inline b8 String_is_sso(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->stk[STR_SSO_SIZE - 1] != '\0';
}


/*
 Macro to temporarily NUL-terminate a String for read-only C APIs.
Note: Do NOT break/return/goto inside the block.

 Usage:
   TEMP_CSTR_READ(s) {
       printf("%s\n", String_data_ptr(s));
   }
*/
#define TEMP_CSTR_READ(str) \
    for (u8 _once = 0; (_once == 0) && (String_append_char((str), '\0'), 1); _once++, String_pop_char((str)))

#endif /* WC_STRING_H */

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
 *   genVec_pop, genVec_front, genVec_back WC_ERR_EMPTY   vec is empty
 *   deQueue, Queue_peek, Queue_peek_ptr   WC_ERR_EMPTY   Queue is empty
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

/* ===== Arena.h ===== */
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
Arena* Arena_create(u64 capacity);

/*
Initialize an Arena object with pointers to the Arena and a
pre-allocated region(base ptr), as well as the size of the provided
region. Good for using the Stack instead of the heap.
The Arena and the data may be Stack initialized, so no Arena_release.
Note that ARENA_DEFAULT_SIZE is not used.

Parameters:
  Arena* Arena    |   The Arena object being initialized.
  u8*    data     |   The region to be Arena-fyed.
  u64    size     |   The size of the region in bytes.
*/
void Arena_create_arr_stk(Arena* Arena, u8* data, u64 size);


void Arena_create_stk(Arena* Arena, u64 capacity);

/*
Reset the pointer to the Arena region to the beginning
of the allocation. Allows reuse of the memory without
expensive frees.

Parameters:
  Arena *Arena    |    The Arena to be cleared.
*/
static inline void Arena_clear(Arena* Arena)
{
    CHECK_FATAL(!Arena, "Arena is null");
    Arena->idx = 0;
}

/*
Free the memory allocated for the entire Arena region.

Parameters:
  Arena *Arena    |    The Arena to be destroyed.
*/
static inline void Arena_release(Arena* Arena)
{
    CHECK_FATAL(!Arena, "Arena is null");
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
u8* Arena_alloc(Arena* Arena, u64 size);

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
u8* Arena_alloc_aligned(Arena* Arena, u64 size, u32 alignment);


// Get used capacity
static inline u64 Arena_used(Arena* Arena)
{
    CHECK_FATAL(!Arena, "Arena is null");
    return Arena->idx;
}

// Get remaining capacity
static inline u64 Arena_remaining(Arena* Arena)
{
    CHECK_FATAL(!Arena, "Arena is null");
    return Arena->size - Arena->idx;
}



// explicit scratch Arena

typedef struct {
    Arena* Arena;
    u64 mark;
} ArenaScratch;


static inline ArenaScratch Arena_scratch_begin(Arena* Arena)
{
    CHECK_FATAL(!Arena, "Arena is null");
    return (ArenaScratch){ .Arena = Arena, .mark = Arena->idx };
}

static inline void Arena_scratch_end(ArenaScratch scratch)
{
    if (scratch.Arena) {
        scratch.Arena->idx = scratch.mark;
        scratch.Arena = NULL;
    }
}

// macro for automatic cleanup Arena_scratch
#define ARENA_SCRATCH(Arena_ptr) \
    for (ArenaScratch __nme__ = Arena_scratch_begin(Arena_ptr); \
         (__nme__ ).Arena != NULL; \
         Arena_scratch_end((__nme__ )), (__nme__).Arena = NULL)

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

#define ARENA_CREATE_STK_ARR(Arena, n) (Arena_create_arr_stk((Arena), (u8[nKB(n)]){0}, nKB(n)))

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

typedef struct {
    const char* ptr;
    u64         len;
} strview;

strview strview_from_String(String* str);

strview strview_from_String_explicit(String* str, u64 off, u64 len);

// allocate a cstr to an Arena and return a view over it
// kinda like an append only store
strview strview_cstr_Arena(Arena* a, const char* cstr, u64 clen);

void strview_print(strview sv);



#define STRING_STORE_NODE_SIZE 1024


typedef struct String_store_node {
    char                      buf[STRING_STORE_NODE_SIZE];
    struct String_store_node* next;
} String_store_node;

// append-only, immutable String storage with a chain Arena-like backing
// you get strviews over the immutable Strings
typedef struct {
    String_store_node* tail;
    String_store_node* head;
    u32                tail_off; // how much of th tail node is used
    u32                num;      // total number of nodes
} String_store;

void String_store_create(String_store* ss);

void String_store_destroy(String_store* ss);

strview String_store_cstr(String_store* ss, const char* cstr, u64 clen);

#endif /* WC_VIEWS_H */

#ifdef WC_IMPLEMENTATION

/* ===== String.c ===== */
#ifndef WC_STRING_IMPL
#define WC_STRING_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <String.h>


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
    CHECK_FATAL(!other, "other is null");

    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    s->size     = 0;
    s->capacity = STR_SSO_SIZE - 1;

    if (other->size > 0) {
        ensure_capacity(s, other->size);
        memcpy(GET_STR(s), GET_STR(other), other->size);
        s->size = other->size;
    }

    return s;
}

void String_create_stk(String* s, const char* cstr)
{
    CHECK_FATAL(!s, "str is null");

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
    CHECK_FATAL(!s, "str is null");
    String_destroy_stk(s);
    free(s);
}

void String_destroy_stk(String* s)
{
    CHECK_FATAL(!s, "str is null");

    if (!IS_SSO(s)) {
        free(s->heap);
    }

    s->size                  = 0;
    s->stk[STR_SSO_SIZE - 1] = 1;                // mark SSO mode; NOT preserved from heap mode
    s->capacity              = STR_SSO_SIZE - 1; // leave in valid, reusable SSO state
}

void String_move(String* dest, String** src)
{
    CHECK_FATAL(!src, "src ptr is null");
    CHECK_FATAL(!*src, "*src is null");
    CHECK_FATAL(!dest, "dest is null");

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
    CHECK_FATAL(!src, "src is null");
    CHECK_FATAL(!dest, "dest is null");

    if (src == dest) {
        return;
    }

    String_destroy_stk(dest);

    dest->size     = 0;
    dest->capacity = STR_SSO_SIZE - 1;

    if (src->size > 0) {
        ensure_capacity(dest, src->size);
        memcpy(GET_STR(dest), GET_STR(src), src->size);
        dest->size = src->size;
    }
}


//  Capacity

void String_reserve(String* s, u64 new_cap)
{
    CHECK_FATAL(!s, "str is null");

    if (new_cap <= s->capacity) {
        return;
    }
    ensure_capacity(s, new_cap);
}

void String_reserve_char(String* s, u64 new_cap, char c)
{
    CHECK_FATAL(!s, "str is null");
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
    CHECK_FATAL(!s, "str is null");

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
    CHECK_FATAL(!s, "str is null");

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
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!buff, "buff is null");
    CHECK_FATAL(n < str->size + 1, "buffer not enough");

    if (str->size > 0) {
        memcpy(buff, GET_STR(str), str->size);
    }
    buff[str->size] = '\0';
}

char* String_data_ptr(const String* s)
{
    CHECK_FATAL(!s, "str is null");
    if (s->size == 0) {
        return NULL;
    }
    // Cast away const intentionally: caller may mutate via this pointer.
    return (char*)(IS_SSO(s) ? s->stk : s->heap);
}


//  Modification

void String_append_char(String* s, char c)
{
    CHECK_FATAL(!s, "str is null");
    MAYBE_GROW_STR(s);
    GET_STR_CHAR(s, s->size++) = c;
}

void String_append_cstr(String* s, const char* cstr)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

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
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!other, "other is null");

    if (other->size == 0) {
        return;
    }

    ensure_capacity(s, s->size + other->size);
    memcpy(GET_STR(s) + s->size, GET_STR(other), other->size);
    s->size += other->size;
}

void String_append_String_move(String* s, String** other)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!other, "other ptr is null");
    CHECK_FATAL(!*other, "*other is null");

    if ((*other)->size > 0) {
        String_append_String(s, *other);
    }

    String_destroy(*other);
    *other = NULL;
}

char String_pop_char(String* s)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(s->size == 0, "cannot pop from empty String");

    char c = GET_STR_CHAR(s, --s->size);
    return c;
}

void String_insert_char(String* s, u64 i, char c)
{
    CHECK_FATAL(!s, "str is null");
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
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");
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
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!other, "other is null");
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
    CHECK_FATAL(!s, "str is null");
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
    CHECK_FATAL(!s, "str is null");
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
    CHECK_FATAL(!s1, "str1 is null");
    CHECK_FATAL(!s2, "str2 is null");

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
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

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
    CHECK_FATAL(!s, "str is null");

    if (s->size == 0) {
        return (u64)-1;
    }
    const char* buf = GET_STR(s);
    const char* p   = memchr(buf, (unsigned char)c, s->size);
    return p ? (u64)(p - buf) : (u64)-1;
}

u64 String_find_cstr(const String* s, const char* substr)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!substr, "substr is null");

    u64 len = cstr_len(substr);
    if (len == 0) {
        return 0;
    }
    if (len > s->size) {
        return (u64)-1;
    }

    const char* buf = GET_STR(s);
    for (u64 i = 0; i <= s->size - len; i++) {
        if (memcmp(buf + i, substr, len) == 0) {
            return i;
        }
    }
    return (u64)-1;
}

String* String_substr(const String* s, u64 start, u64 length)
{
    CHECK_FATAL(!s, "str is null");
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
    CHECK_FATAL(!s, "str is null");

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

#endif /* WC_STRING_IMPL */

/* ===== wc_errno.c ===== */
#ifndef WC_WC_ERRNO_IMPL
#define WC_WC_ERRNO_IMPL

/* One definition of the thread-local error variable.
 * Every translation unit that includes wc_error.h sees the extern declaration.
 * This file provides the actual storage.
 */
_Thread_local wc_err wc_errno = WC_OK;

#endif /* WC_WC_ERRNO_IMPL */

/* ===== Arena.c ===== */
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

    Arena* Arena = (Arena*)malloc(sizeof(Arena));
    CHECK_FATAL(!Arena, "Arena malloc failed");

    Arena->base = (u8*)malloc(capacity);
    CHECK_FATAL(!Arena->base, "Arena base malloc failed");

    Arena->idx = 0;
    Arena->size = capacity;

    return Arena;
}

void Arena_create_stk(Arena* Arena, u64 capacity)
{
    if (capacity == 0) {
        capacity = ARENA_DEFAULT_SIZE;
    }

    Arena->base = (u8*)malloc(capacity);
    CHECK_FATAL(!Arena->base, "Arena base malloc failed");

    Arena->idx  = 0;
    Arena->size = capacity;
}

void Arena_create_arr_stk(Arena* Arena, u8* data, u64 size)
{
    CHECK_FATAL(!Arena, "Arena is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(size == 0, "size can't be zero");

    Arena->base = data;
    Arena->idx = 0;
    Arena->size = size;
}

u8* Arena_alloc(Arena* Arena, u64 size)
{
    CHECK_FATAL(!Arena, "Arena is null");
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");

    // Align the current index first
    u64 aligned_idx = ALIGN_UP_DEFAULT(Arena->idx);
    WC_SET_RET(WC_ERR_FULL, Arena->size - aligned_idx < size, NULL);

    u8* ptr = ARENA_PTR(Arena, aligned_idx);
    Arena->idx = aligned_idx + size;

    return ptr;
}

u8* Arena_alloc_aligned(Arena* Arena, u64 size, u32 alignment)
{

    CHECK_FATAL(!Arena, "Arena is null");
    CHECK_FATAL(size == 0, "can't have allocation of size = 0");
    CHECK_FATAL((alignment & (alignment - 1)) != 0,
                "alignment must be power of two");


    u64 aligned_idx = ALIGN_UP(Arena->idx, alignment);

    WC_SET_RET(WC_ERR_FULL, Arena->size - aligned_idx < size, NULL);

    u8* ptr = ARENA_PTR(Arena, aligned_idx);
    Arena->idx = aligned_idx + size;

    return ptr;
}

#endif /* WC_ARENA_IMPL */

/* ===== views.c ===== */
#ifndef WC_VIEWS_IMPL
#define WC_VIEWS_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <String.h>


strview strview_from_String(String* str)
{
    return (strview){.ptr = String_data_ptr(str), .len = String_len(str)};
}

strview strview_from_String_explicit(String* str, u64 off, u64 len)
{
    CHECK_FATAL(off + len >= String_len(str), "invalid range");
    return (strview){.ptr = String_data_ptr(str) + off, .len = len};
}

strview strview_cstr_Arena(Arena* a, const char* cstr, u64 clen)
{
    char* p = ARENA_ALLOC_N(a, char, clen + 1); // for NULL Terminator
    memcpy(p, cstr, clen + 1);
    return (strview){.ptr = p, .len = clen};
}

void strview_print(strview sv)
{
    for (u64 i = 0; i < sv.len; i++) {
        putchar(sv.ptr[i]);
    }
}



#define TAIL_BUF_OFF(ss) ((ss)->tail->buf + (ss)->tail_off)

void String_store_create(String_store* ss)
{
    String_store_node* node = malloc(sizeof(String_store_node));
    CHECK_FATAL(!node, "node malloc failed");

    node->next   = NULL;
    ss->head     = node;
    ss->tail     = node;
    ss->tail_off = 0;
    ss->num      = 1;
}

void String_store_destroy(String_store* ss)
{
    if (!ss || !ss->head) {
        return;
    }

    String_store_node* curr = ss->head;
    String_store_node* next = NULL;
    do {
        next = curr->next;
        free(curr);
        curr = next;
    } while (curr);
}

static inline void add_node(String_store* ss)
{
    String_store_node* node = malloc(sizeof(String_store_node));
    ss->tail->next          = node;
    ss->tail                = node;
    ss->tail_off            = 0;
    ss->num++;
}

strview String_store_cstr(String_store* ss, const char* cstr, u64 clen)
{
    CHECK_FATAL(!ss, "ss is null");
    CHECK_FATAL(!cstr, "ss is null");

    if (STRING_STORE_NODE_SIZE - ss->tail_off < clen) {
        // we need another node
        add_node(ss);
    }

    char* ptr = TAIL_BUF_OFF(ss);
    memcpy(ptr, cstr, clen);
    ss->tail_off += clen;
    return (strview){.ptr = ptr, .len = clen};
}

#endif /* WC_VIEWS_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_VIEWS_SINGLE_H */
