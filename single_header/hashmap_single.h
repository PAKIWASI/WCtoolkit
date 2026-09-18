#ifndef WC_HASHMAP_SINGLE_H
#define WC_HASHMAP_SINGLE_H

/*
 * hashmap_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "hashmap_single.h"
 *
 * All other files just:
 *     #include "hashmap_single.h"
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
} wc_container_ops;


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

/* ===== map_setup.h ===== */
#ifndef WC_MAP_SETUP_H
#define WC_MAP_SETUP_H

#include <string.h>


typedef u64 (*custom_hash_fn)(const u8* key, u64 size);

#define LOAD_FACTOR_GROW      0.75 // Robin Hood sweet spot
#define HASHMAP_INIT_CAPACITY 16   // power-of-2 to avoid modulo

typedef enum {
    NOT_FOUND = 0,
    FOUND,
    ROBINHOOD_EXIT,
} LOOKUP_RES;

/*
====================WYHASH====================
*/
// wyhash v4 — public domain, Wang Yi
// Best default for HashMaps: fast, excellent avalanche, low collision rate.
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

// wyhash variants for String

__attribute__((unused)) static u64 wyhash_str(const u8* key, u64 size)
{
    (void)size;
    String* str = (String*)key;
    return wyhash((const u8*)String_data_ptr(str), String_len(str));
}

__attribute__((unused)) static u64 wyhash_str_ptr(const u8* key, u64 size)
{
    (void)size;
    String* str = *(String**)key;
    return wyhash((const u8*)String_data_ptr(str), String_len(str));
}

#define ALIGN8(size) (((u64)(size) + 7u) & ~7u)

#endif /* WC_MAP_SETUP_H */

/* ===== hashmap.h ===== */
#ifndef WC_HASHMAP_H
#define WC_HASHMAP_H

/* Generic Hashmap with Ownership Semantics
  - Robin Hood Hashing
  - we have 3 arrays: keys, psls, and vals
  - PSL: probe sequence length: the distance from hashing location
  - we actuall store psl + 1 as psl = 0 means empty bucket
  - Robin Hood Invarient: all keys that hash to i come before keys that hash to i + 1
  - vals store [val] inline
*/


typedef struct {
    u8*            keys;
    u8*            psls;
    u8*            vals;
    u64            size;
    u64            capacity;
    u32            key_size;
    u32            val_size;
    u8*            scratch; // key_size + val_size bytes + alignment: temp buffer for robin hood swaps
    custom_hash_fn hash_fn;
    compare_fn     cmp_fn;

    // Shared ops vtables for keys and values.
    // Pass NULL for POD types (int, float, flat structs).
    // For types with heap resources define one static ops per type:
    const wc_container_ops* key_ops;
    const wc_container_ops* val_ops;
} HashMap;


// Safely extract callbacks — always NULL-safe on ops itself.
#define MAP_COPY(ops) ((ops) ? (ops)->copy_fn : NULL)
#define MAP_MOVE(ops) ((ops) ? (ops)->move_fn : NULL)
#define MAP_DEL(ops)  ((ops) ? (ops)->del_fn : NULL)

/* TODO:
    reserve one extra slot at the end of the key/val arrays that never holds a real entry.
    During insert, you keep the “current” key/value in registers or local variables
    and only write them into the array when the final empty slot is found.
    This requires a small rewrite of map_insert, but it saves two memcpy calls per eviction
    and eliminates scratch
*/

// Create a new HashMap.
// hash_fn and cmp_fn default to fnv1a_hash / default_compare if NULL.
// key_ops / val_ops: pass NULL for POD types.
HashMap* HashMap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const wc_container_ops* key_ops, const wc_container_ops* val_ops) __attribute__((warn_unused_result));
void     HashMap_create_stk(HashMap* map, u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                            const wc_container_ops* key_ops, const wc_container_ops* val_ops)
    __attribute__((nonnull(1)));

void HashMap_destroy(HashMap* map) __attribute__((nonnull(1)));
void HashMap_destroy_stk(HashMap* map) __attribute__((nonnull(1)));

// Insert or update — COPY semantics.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put(HashMap* map, const u8* key, const u8* val) __attribute__((nonnull(1, 2, 3)));

// Insert or update — MOVE semantics (key and val are u8**, both nulled).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put_move(HashMap* map, u8** key, u8** val) __attribute__((nonnull(1, 2, 3)));

// Mixed: key copied, val moved.
b8 HashMap_put_val_move(HashMap* map, const u8* key, u8** val) __attribute__((nonnull(1, 2, 3)));

// Mixed: key moved, val copied.
b8 HashMap_put_key_move(HashMap* map, u8** key, const u8* val) __attribute__((nonnull(1, 2, 3)));

// Get value for key — copies into val. Returns 1 if found, 0 if not.
b8 HashMap_get(const HashMap* map, const u8* key, u8* val) __attribute__((nonnull(1, 2, 3)));

// Get pointer to value
const u8* HashMap_get_ptr(const HashMap* map, const u8* key) __attribute__((nonnull(1, 2)));

__attribute__((nonnull(1, 2))) static inline u8* HashMap_get_ptr_mut(HashMap* map, const u8* key)
{
    return (u8*)HashMap_get_ptr(map, key);
}

// Bucket iteration accessors
__attribute__((nonnull(1))) static inline u64 HashMap_bucket_count(const HashMap* map)
{
    return map->capacity;
}

b8        HashMap_bucket_occupied(const HashMap* map, u64 i) __attribute__((nonnull(1)));
const u8* HashMap_bucket_key_ptr(const HashMap* map, u64 i) __attribute__((nonnull(1)));
u8*       HashMap_bucket_val_ptr(HashMap* map, u64 i) __attribute__((nonnull(1)));

// Delete key. If out is provided, value is copied to it before deletion.
// Returns 1 if found and deleted, 0 if not found.
b8 HashMap_del(HashMap* map, const u8* key, u8* out) __attribute__((nonnull(1, 2)));

// Check if key exists.
b8 HashMap_has(const HashMap* map, const u8* key) __attribute__((nonnull(1, 2)));

// Print all key-value pairs.
void HashMap_print(const HashMap* map, print_fn key_print, print_fn val_print) __attribute__((nonnull(1, 2, 3)));

// Remove all elements, keep capacity.
void HashMap_clear(HashMap* map) __attribute__((nonnull(1)));

// Deep copy src into dest
// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void HashMap_copy(HashMap* dest, const HashMap* src) __attribute__((nonnull(1, 2)));


static inline __attribute__((nonnull(1))) u64 HashMap_size(const HashMap* map)
{
    return map->size;
}
static inline __attribute__((nonnull(1))) u64 HashMap_capacity(const HashMap* map)
{
    return map->capacity;
}
static inline __attribute__((nonnull(1))) b8 HashMap_empty(const HashMap* map)
{
    return map->size == 0;
}

#endif /* WC_HASHMAP_H */

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

/* ===== hashmap.c ===== */
#ifndef WC_HASHMAP_IMPL
#define WC_HASHMAP_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define GET_KEY(map, i) ((map)->keys + ((u64)(map)->key_size * (i)))
#define GET_PSL(map, i) ((map)->psls + (i))
#define GET_VAL(map, i) ((map)->vals + ((u64)(map)->val_size * (i)))

// capacity is always power-of-2 — use bitmask instead of %
#define MAP_MASK(map)     ((map)->capacity - 1)
#define MAP_IDX(map, key) ((map)->hash_fn((key), (map)->key_size) & MAP_MASK(map))
#define MAP_NEXT(map, i)  (((i) + 1) & MAP_MASK(map))

// PSL 0 == empty bucket; stored PSL is (real_psl + 1), starting at 1
#define BUCKET_EMPTY 0

// scratch layout:
//   STAGE region (first half)  — used by HashMap_put to copy incoming key/val
//     [0                         ..  ALIGN8(key_size))             = STAGE_KEY
//     [ALIGN8(key_size)          ..  ALIGN8(key_size) + val_size)  = STAGE_VAL
//   SWAP region (second half)  — used by map_insert for Robin Hood evictions
//     [ALIGN8(key_size)+val_size ..  +ALIGN8(key_size))            = SWAP_KEY
//     [+ALIGN8(key_size)         ..  +val_size)                    = SWAP_VAL
//
// The two regions must NOT overlap: map_insert is called with pointers INTO
// the STAGE region, and it writes displaced residents into the SWAP region.
// Total: 2 * (ALIGN8(key_size) + val_size) bytes.
#define STAGE_KEY(map) ((map)->scratch)
#define STAGE_VAL(map) ((map)->scratch + ALIGN8((map)->key_size))
#define SWAP_KEY(map)  ((map)->scratch + ALIGN8((map)->key_size) + (map)->val_size)
#define SWAP_VAL(map)  ((map)->scratch + ALIGN8((map)->key_size) + (map)->val_size + ALIGN8((map)->key_size))

#define IS_POD_K(map) (map->key_ops == NULL)
#define IS_POD_V(map) (map->val_ops == NULL)


/*
====================PRIVATE DECLARATIONS====================
*/

static u64         map_lookup(const HashMap* map, const u8* key, LOOKUP_RES* res, u8* out_psl);
static void        map_insert(HashMap* map, u8* key, u8* val, u8 psl, u64 idx);
static inline void map_maybe_resize(HashMap* map);
static void        map_resize(HashMap* map, u64 new_capacity);


/*
====================PUBLIC FUNCTIONS====================
*/

void HashMap_create_stk(HashMap* map, u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn, const wc_container_ops* key_ops, const wc_container_ops* val_ops)
{
    CHECK_FATAL(key_size == 0 || val_size == 0, "key/val size can't be 0");

    map->keys = malloc((u64)HASHMAP_INIT_CAPACITY * key_size);
    CHECK_FATAL(!map->keys, "keys malloc failed");
    map->psls = calloc(HASHMAP_INIT_CAPACITY, sizeof(u8));
    CHECK_FATAL(!map->psls, "psls calloc failed");
    map->vals = malloc((u64)HASHMAP_INIT_CAPACITY * val_size);
    CHECK_FATAL(!map->vals, "vals malloc failed");

    map->scratch = malloc(2 * (ALIGN8(key_size) + val_size));
    CHECK_FATAL(!map->scratch, "scratch malloc failed");

    map->size     = 0;
    map->capacity = HASHMAP_INIT_CAPACITY;
    map->key_size = key_size;
    map->val_size = val_size;

    map->hash_fn = hash_fn ? hash_fn : wyhash;
    map->cmp_fn  = cmp_fn ? cmp_fn : default_compare;

    map->key_ops = key_ops;
    map->val_ops = val_ops;
}

HashMap* HashMap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const wc_container_ops* key_ops, const wc_container_ops* val_ops)
{
    HashMap* map = malloc(sizeof(HashMap));
    CHECK_FATAL(!map, "map malloc failed");

    HashMap_create_stk(map, key_size, val_size, hash_fn, cmp_fn, key_ops, val_ops);

    return map;
}


void HashMap_destroy(HashMap* map)
{
    HashMap_destroy_stk(map);
    free(map);
}

void HashMap_destroy_stk(HashMap* map)
{
    if (!IS_POD_K(map) || !IS_POD_V(map)) {
        delete_fn k_del = IS_POD_K(map) ? NULL : map->key_ops->del_fn;
        delete_fn v_del = IS_POD_V(map) ? NULL : map->val_ops->del_fn;
        if (k_del || v_del) {
            for (u64 i = 0; i < map->capacity; i++) {
                if (*GET_PSL(map, i) == BUCKET_EMPTY) {
                    continue;
                }
                if (k_del) {
                    k_del(GET_KEY(map, i));
                }
                if (v_del) {
                    v_del(GET_VAL(map, i));
                }
            }
        }
    }

    free(map->keys);
    free(map->psls);
    free(map->vals);
    free(map->scratch);
}


// Insert or update — COPY semantics.
// Ownership: map takes a deep copy of key and val via ops->copy_fn (or memcpy for POD).
// The caller retains ownership of its key/val and is responsible for freeing them.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put(HashMap* map, const u8* key, const u8* val)
{
    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res == FOUND) {
        if (IS_POD_V(map)) {
            memcpy(GET_VAL(map, slot), val, map->val_size);
        } else {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
            copy_fn v_cp = map->val_ops->copy_fn;
            if (v_cp) {
                v_cp(GET_VAL(map, slot), val);
            } else {
                memcpy(GET_VAL(map, slot), val, map->val_size);
            }
        }
        return 1;
    }

    if (IS_POD_K(map)) {
        memcpy(STAGE_KEY(map), key, map->key_size);
    } else {
        copy_fn k_cp = map->key_ops->copy_fn;
        if (k_cp) {
            k_cp(STAGE_KEY(map), key);
        } else {
            memcpy(STAGE_KEY(map), key, map->key_size);
        }
    }
    if (IS_POD_V(map)) {
        memcpy(STAGE_VAL(map), val, map->val_size);
    } else {
        copy_fn v_cp = map->val_ops->copy_fn;
        if (v_cp) {
            v_cp(STAGE_VAL(map), val);
        } else {
            memcpy(STAGE_VAL(map), val, map->val_size);
        }
    }

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Insert or update — MOVE semantics.
// Ownership: the map takes ownership of *key and *val directly (no copy made).
// On success both pointers are nulled. Requires move_fn for both key and val.
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put_move(HashMap* map, u8** key, u8** val)
{
    CHECK_FATAL(!*key || !*val, "*key/*val null");

    move_fn k_mv = MAP_MOVE(map->key_ops);
    move_fn v_mv = MAP_MOVE(map->val_ops);

    // move_fn is mandatory: it must transfer the heap resource and null the source.
    // For by-value types with no heap resources, use HashMap_put (copy semantics) instead.
    CHECK_FATAL(!k_mv || !v_mv, "key/val move funcs required");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, *key, &res, &out_psl);

    if (res == FOUND) {
        if (!IS_POD_V(map)) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
        }
        v_mv(GET_VAL(map, slot), val);
        if (!IS_POD_K(map)) {
            delete_fn k_del = map->key_ops->del_fn;
            if (k_del) {
                k_del(*key);
            }
        }
        free(*key);
        *key = NULL;
        return 1;
    }

    // Stage: move key into STAGE_KEY, move val into STAGE_VAL.
    // move_fn transfers the heap resource pointer into the dest slot and nulls src.
    k_mv(STAGE_KEY(map), key); // nulls *key
    v_mv(STAGE_VAL(map), val); // nulls *val

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Insert or update — mixed: key is COPIED, val is MOVED.
// Ownership: map deep-copies the key (caller retains it); map takes ownership of *val (*val nulled).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put_val_move(HashMap* map, const u8* key, u8** val)
{
    CHECK_FATAL(!*val, "*val null");

    move_fn v_mv = MAP_MOVE(map->val_ops);

    CHECK_FATAL(!v_mv, "val move func required");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res == FOUND) {
        if (!IS_POD_V(map)) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
        }
        v_mv(GET_VAL(map, slot), val);
        return 1;
    }

    if (IS_POD_K(map)) {
        memcpy(STAGE_KEY(map), key, map->key_size);
    } else {
        copy_fn k_cp = map->key_ops->copy_fn;
        if (k_cp) {
            k_cp(STAGE_KEY(map), key);
        } else {
            memcpy(STAGE_KEY(map), key, map->key_size);
        }
    }
    v_mv(STAGE_VAL(map), val);

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Insert or update — mixed: key is MOVED, val is COPIED.
// Ownership: map takes ownership of *key (*key nulled); map deep-copies val (caller retains it).
// Returns 1 if key existed (updated), 0 if new key inserted.
b8 HashMap_put_key_move(HashMap* map, u8** key, const u8* val)
{
    CHECK_FATAL(!*key, "*key null");

    move_fn k_mv = MAP_MOVE(map->key_ops);

    CHECK_FATAL(!k_mv, "key move func required for HashMap_put_key_move");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, *key, &res, &out_psl);

    if (res == FOUND) {
        if (!IS_POD_V(map)) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
            copy_fn v_cp = map->val_ops->copy_fn;
            if (v_cp) {
                v_cp(GET_VAL(map, slot), val);
            } else {
                memcpy(GET_VAL(map, slot), val, map->val_size);
            }
        } else {
            memcpy(GET_VAL(map, slot), val, map->val_size);
        }
        // Key already in map — consume (and discard) the incoming duplicate.
        if (!IS_POD_K(map)) {
            delete_fn k_del = map->key_ops->del_fn;
            if (k_del) {
                k_del(*key);
            }
        }
        free(*key);
        *key = NULL;
        return 1;
    }

    // Stage key (move) and val (copy).
    k_mv(STAGE_KEY(map), key);
    if (IS_POD_V(map)) {
        memcpy(STAGE_VAL(map), val, map->val_size);
    } else {
        copy_fn v_cp = map->val_ops->copy_fn;
        if (v_cp) {
            v_cp(STAGE_VAL(map), val);
        } else {
            memcpy(STAGE_VAL(map), val, map->val_size);
        }
    }

    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    map_maybe_resize(map);
    return 0;
}


// Get value for key — COPIES into val. Returns 1 if found, 0 if not.
// Caller owns the copy returned in val and must free it when done.
b8 HashMap_get(const HashMap* map, const u8* key, u8* val)
{
    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    if (IS_POD_V(map)) {
        memcpy(val, GET_VAL(map, slot), map->val_size);
    } else {
        copy_fn v_copy = map->val_ops->copy_fn;
        if (v_copy) {
            v_copy(val, GET_VAL(map, slot));
        } else {
            memcpy(val, GET_VAL(map, slot), map->val_size);
        }
    }
    return 1;
}


// Get pointer to value in-place (read-only). Returns NULL if not found.
// The pointer is valid until the next mutation (put/del/resize).
// Do NOT free the returned pointer — the map owns it.
const u8* HashMap_get_ptr(const HashMap* map, const u8* key)
{
    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    return (res == FOUND) ? GET_VAL(map, slot) : NULL;
}

b8 HashMap_bucket_occupied(const HashMap* map, u64 i)
{
    CHECK_FATAL(i >= map->capacity, "index out of bounds");
    return *GET_PSL(map, i) != BUCKET_EMPTY;
}

const u8* HashMap_bucket_key_ptr(const HashMap* map, u64 i)
{
    CHECK_FATAL(i >= map->capacity, "index out of bounds");
    return GET_KEY(map, i);
}

u8* HashMap_bucket_val_ptr(HashMap* map, u64 i)
{
    CHECK_FATAL(i >= map->capacity, "index out of bounds");
    return GET_VAL(map, i);
}


// Delete key.
// If out != NULL, the value is MOVED into it before deletion (caller takes ownership).
// If out == NULL, the value is destroyed via del_fn (or simply discarded for POD).
// Returns 1 if found and deleted, 0 if not found.
//
// Uses Robin Hood backward-shift deletion to maintain the probe-sequence invariant
// without tombstones: after removing a slot, we shift subsequent entries back one
// position as long as they have PSL > 1 (i.e. they are not sitting at their home slot).
b8 HashMap_del(HashMap* map, const u8* key, u8* out)
{
    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    if (out) {
        memcpy(out, GET_VAL(map, slot), map->val_size);
    } else {
        if (!IS_POD_V(map)) {
            delete_fn v_del = map->val_ops->del_fn;
            if (v_del) {
                v_del(GET_VAL(map, slot));
            }
        }
    }

    if (!IS_POD_K(map)) {
        delete_fn k_del = map->key_ops->del_fn;
        if (k_del) {
            k_del(GET_KEY(map, slot));
        }
    }

    // Backward-shift deletion: pull subsequent entries one slot back as long as
    // they have PSL > 1.  Entries at their home slot (PSL == 1) must not move.
    // This restores the Robin Hood invariant without tombstones.
    u64 cur = slot;
    for (;;) {
        u64 next     = MAP_NEXT(map, cur);
        u8  next_psl = *GET_PSL(map, next);

        // Stop if next slot is empty or the next entry is already at its home slot.
        if (next_psl <= 1) {
            *GET_PSL(map, cur) = BUCKET_EMPTY;
            break;
        }

        // Shift next entry one slot back; its PSL decreases by 1.
        *GET_PSL(map, cur) = next_psl - 1;
        memcpy(GET_KEY(map, cur), GET_KEY(map, next), map->key_size);
        memcpy(GET_VAL(map, cur), GET_VAL(map, next), map->val_size);

        cur = next;
    }

    map->size--;
    return 1;
}


// Check if key exists.
b8 HashMap_has(const HashMap* map, const u8* key)
{
    LOOKUP_RES res;
    u8         out_psl;
    map_lookup(map, key, &res, &out_psl);
    return res == FOUND;
}


// Print all key-value pairs.
void HashMap_print(const HashMap* map, print_fn key_print, print_fn val_print)
{
    printf("\t=========\n");
    printf("\tSize: %lu / Capacity: %lu\n", map->size, map->capacity);
    printf("\t=========\n");

    for (u64 i = 0; i < map->capacity; i++) {
        if (*GET_PSL(map, i) == BUCKET_EMPTY) {
            continue;
        }
        putchar('\t');
        key_print(GET_KEY(map, i));
        printf(" => ");
        val_print(GET_VAL(map, i));
        putchar('\n');
    }

    printf("\t=========\n");
}


// Remove all elements, keep capacity.
// Destroys all keys and values via their del_fn callbacks, then zeroes the arrays.
void HashMap_clear(HashMap* map)
{
    if (!IS_POD_K(map) || !IS_POD_V(map)) {
        delete_fn k_del = IS_POD_K(map) ? NULL : map->key_ops->del_fn;
        delete_fn v_del = IS_POD_V(map) ? NULL : map->val_ops->del_fn;
        for (u64 i = 0; i < map->capacity; i++) {
            if (*GET_PSL(map, i) == BUCKET_EMPTY) {
                continue;
            }
            if (k_del) {
                k_del(GET_KEY(map, i));
            }
            if (v_del) {
                v_del(GET_VAL(map, i));
            }
        }
    }

    memset(map->psls, 0, map->capacity * sizeof(u8));
    map->size = 0;
}


// TODO: test
// Deep copy src into dest.
// Ownership: dest gets independently owned copies of all keys and values.
void HashMap_copy(HashMap* dest, const HashMap* src)
{
    if (dest == src) {
        return;
    }

    dest->keys = malloc(src->capacity * src->key_size);
    CHECK_FATAL(!dest->keys, "copy keys calloc failed");
    dest->psls = calloc(src->capacity, sizeof(u8));
    CHECK_FATAL(!dest->psls, "copy psls calloc failed");
    dest->vals = malloc(src->capacity * src->val_size);
    CHECK_FATAL(!dest->vals, "copy vals calloc failed");
    dest->scratch = malloc(2 * (ALIGN8(src->key_size) + src->val_size));
    CHECK_FATAL(!dest->scratch, "copy scratch malloc failed");

    dest->size     = src->size;
    dest->capacity = src->capacity;
    dest->key_size = src->key_size;
    dest->val_size = src->val_size;
    dest->hash_fn  = src->hash_fn;
    dest->cmp_fn   = src->cmp_fn;
    dest->key_ops  = src->key_ops;
    dest->val_ops  = src->val_ops;

    copy_fn k_cp = IS_POD_K(src) ? NULL : src->key_ops->copy_fn;
    copy_fn v_cp = IS_POD_V(src) ? NULL : src->val_ops->copy_fn;

    for (u64 i = 0; i < src->capacity; i++) {
        u8 psl = *GET_PSL(src, i);
        if (psl == BUCKET_EMPTY) {
            continue;
        }

        *GET_PSL(dest, i) = psl;

        if (k_cp) {
            k_cp(GET_KEY(dest, i), GET_KEY(src, i));
        } else {
            memcpy(GET_KEY(dest, i), GET_KEY(src, i), src->key_size);
        }

        if (v_cp) {
            v_cp(GET_VAL(dest, i), GET_VAL(src, i));
        } else {
            memcpy(GET_VAL(dest, i), GET_VAL(src, i), src->val_size);
        }
    }
}


/*
====================PRIVATE FUNCTIONS====================
*/

static inline void map_maybe_resize(HashMap* map)
{
    // integer multiply avoids float — equivalent to load > 0.75
    if (map->size * 4 >= map->capacity * 3) {
        map_resize(map, map->capacity * 2);
    }
}

static u64 map_lookup(const HashMap* map, const u8* key, LOOKUP_RES* res, u8* out_psl)
{
    u64        idx = MAP_IDX(map, key);
    u8         psl = 1; // stored PSL=1 means real probe distance 0 (home slot)
    compare_fn cmp = map->cmp_fn;

    for (u64 i = idx;; i = MAP_NEXT(map, i)) {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == BUCKET_EMPTY) {
            *res     = NOT_FOUND;
            *out_psl = psl;
            return i;
        }

        if (slot_psl < psl) {
            // The resident has a lower PSL — it's closer to its home than we are.
            // Under Robin Hood, our key would have displaced this resident on insert,
            // so our key cannot exist at or beyond this slot.
            *res     = ROBINHOOD_EXIT;
            *out_psl = psl;
            return i;
        }

        if (cmp(GET_KEY(map, i), key, map->key_size) == 0) {
            *res     = FOUND;
            *out_psl = psl;
            return i;
        }

        psl++;
    }
}



// Insert key/val with the given starting psl at slot idx.
// key and val are already OWNED by the caller (staged copy or moved pointer).
// This function never calls copy/del — it only shuffles raw bytes between slots.
// Displaced residents are temporarily buffered in SWAP_KEY/SWAP_VAL (second half
// of scratch), which is disjoint from the STAGE region where key/val came from.
static void map_insert(HashMap* map, u8* key, u8* val, u8 psl, u64 idx)
{
    // Use two alternating scratch halves to avoid aliasing
    u8* cur_key = STAGE_KEY(map);
    u8* cur_val = STAGE_VAL(map);
    u8* swp_key = SWAP_KEY(map);
    u8* swp_val = SWAP_VAL(map);

    // key/val may already be STAGE — only copy if not already there
    if (key != cur_key) {
        memcpy(cur_key, key, map->key_size);
    }
    if (val != cur_val) {
        memcpy(cur_val, val, map->val_size);
    }
    key = cur_key;
    val = cur_val;

    for (u64 i = idx;; i = MAP_NEXT(map, i)) {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == BUCKET_EMPTY) {
            *GET_PSL(map, i) = psl;
            memcpy(GET_KEY(map, i), key, map->key_size);
            memcpy(GET_VAL(map, i), val, map->val_size);
            map->size++;
            return;
        }

        if (slot_psl < psl) {
            u8 tmp_psl = slot_psl;
            // Evict into swp (disjoint from key which is in cur)
            memcpy(swp_key, GET_KEY(map, i), map->key_size);
            memcpy(swp_val, GET_VAL(map, i), map->val_size);

            *GET_PSL(map, i) = psl;
            memcpy(GET_KEY(map, i), key, map->key_size);
            memcpy(GET_VAL(map, i), val, map->val_size);

            // Swap roles: evicted becomes current, current becomes swap buffer
            u8* tmp = cur_key;
            cur_key = swp_key;
            swp_key = tmp;
            tmp     = cur_val;
            cur_val = swp_val;
            swp_val = tmp;
            key     = cur_key;
            val     = cur_val;
            psl     = tmp_psl + 1;
            continue;
        }

        psl++;
    }
}


// Rehash into a new array of new_capacity (must be power-of-2).
// Ownership transfers as raw bytes — no copy/del callbacks are invoked.
// This is safe because the data itself doesn't move, only the slot positions.
static void map_resize(HashMap* map, u64 new_capacity)
{
    if (new_capacity < HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    u8* old_keys = map->keys;
    u8* old_psls = map->psls;
    u8* old_vals = map->vals;
    u64 old_cap  = map->capacity;

    // map->keys = calloc(new_capacity, map->key_size);
    map->keys = malloc(new_capacity * map->key_size);
    CHECK_FATAL(!map->keys, "resize keys calloc failed");
    map->psls = calloc(new_capacity, sizeof(u8));
    CHECK_FATAL(!map->psls, "resize psls calloc failed");
    // map->vals = calloc(new_capacity, map->val_size);
    map->vals = malloc(new_capacity * map->val_size);
    CHECK_FATAL(!map->vals, "resize vals calloc failed");

    map->capacity = new_capacity;
    map->size     = 0;


    for (u64 i = 0; i < old_cap; i++) {

        if (old_psls[i] == BUCKET_EMPTY) {
            continue;
        }

        u8* old_key = old_keys + ((u64)map->key_size * i);
        u8* old_val = old_vals + ((u64)map->val_size * i);

        // Stage into scratch first — map_insert uses SWAP region of scratch
        // and would clobber old_key/old_val if they happened to alias it
        memcpy(STAGE_KEY(map), old_key, map->key_size);
        memcpy(STAGE_VAL(map), old_val, map->val_size);

        LOOKUP_RES res;
        u8         out_psl;
        u64        slot = map_lookup(map, STAGE_KEY(map), &res, &out_psl);

        map_insert(map, STAGE_KEY(map), STAGE_VAL(map), out_psl, slot);
    }

    free(old_keys);
    free(old_psls);
    free(old_vals);
}

#endif /* WC_HASHMAP_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_HASHMAP_SINGLE_H */
