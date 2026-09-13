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

#endif /* WC_IMPLEMENTATION */

#endif /* WC_MAP_SETUP_SINGLE_H */
