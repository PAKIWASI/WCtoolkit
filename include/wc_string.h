#ifndef STRING_H
#define STRING_H

#include "common.h"
#include "wc_allocator.h"

#include <stdint.h>
#include <string.h>


#ifndef STRING_GROWTH
#define STRING_GROWTH 1.5F // capacity multiplier on grow
#endif

// SSO inline buffer size (bytes).  Last byte is the mode flag:
//   nonzero = SSO mode (the string lives in stk[]).
//   '\0'    = heap mode (the string lives in heap).
// Usable SSO bytes = STR_SSO_SIZE - 1 = 23.
#define STR_SSO_SIZE 24

typedef struct {
    union {
        char* heap;
        char  stk[STR_SSO_SIZE];
    };
    u64           size;
    u64           capacity;
    const wc_allocator* alloc;
} String;


_Static_assert(sizeof(String) == 48, "String must be 48 bytes");


//  Construction / Destruction

// Create an empty String using allocator `a`.
String String_create(const wc_allocator* a) __attribute__((nonnull(1), warn_unused_result));

// Create a String from a C string using allocator `a`.
String String_from_cstr(const wc_allocator* a, const char* cstr) __attribute__((nonnull(1), warn_unused_result));

// Destroy the String's internal buffer
// Safe on a zeroed/moved-from String.
void String_destroy(String* str) __attribute__((nonnull(1)));

// Deep copy src → new String in allocator `a`.
// dest is raw/uninitialized: never reads it before writing.
String String_copy(const wc_allocator* a, const String* src) __attribute__((nonnull(1, 2), warn_unused_result));

// Transfer ownership: dest gets src's contents, src is zeroed.
void String_move(String* dest, String* src) __attribute__((nonnull(1, 2)));

// TODO: just use a StrView for this? can we for hashmap lookup?
// Non-owning, READ-ONLY String over `len` existing bytes: no allocation.
// For lookups only: a String-keyed map or set hashes and compares it like an
// owning String with the same bytes. Never store it in a container.
// Any mutation is fatal (WC_BORROWED cannot allocate); destroy is a no-op.
__attribute__((nonnull(1))) static inline String String_borrow(const char* p, u64 len)
{
    String s;
    memset(&s, 0, sizeof(s)); // stk[STR_SSO_SIZE - 1] == 0: heap mode
    s.heap     = (char*)(uintptr_t)p;
    s.size     = len;
    s.capacity = len ? len : 1; // capacity 0 is the zero state
    s.alloc    = WC_BORROWED;
    return s;
}


//  Capacity

// Ensure capacity >= new_cap (never shrinks).
void String_reserve(String* str, u64 new_cap) __attribute__((nonnull(1)));

// Reserve capacity and fill new slots with c.
void String_reserve_char(String* str, u64 new_cap, char c) __attribute__((nonnull(1)));

// Shrink allocation to exactly fit current size.
void String_shrink_to_fit(String* str) __attribute__((nonnull(1)));


//  Conversion

// Return a buffer (allocated from `a`) holding a NUL-terminated copy — caller
// must free with wc_free(a, ptr, str->size + 1, 1).
char* String_to_cstr(const wc_allocator* a, const String* str) __attribute__((nonnull(1, 2), warn_unused_result));

void String_to_cstr_buf(const String* str, char* buff, u64 n) __attribute__((nonnull(1, 2)));

// Return a raw pointer into the internal buffer (no NUL terminator).
char* String_data_ptr(const String* str) __attribute__((nonnull(1)));

// Guarantee a '\0' sits one byte past the last real character, WITHOUT
// touching str->size. Grows exactly like String_append_char would.
void String_ensure_null_term(String* str) __attribute__((nonnull(1)));


//  Modification

void String_append_char(String* str, char c) __attribute__((nonnull(1)));
void String_append_cstr(String* str, const char* cstr) __attribute__((nonnull(1, 2)));
void String_append_String(String* str, const String* other) __attribute__((nonnull(1, 2)));
// Append other then destroy it (leaves other zeroed). When str is empty and
// both share an allocator, str takes over other's buffer: no copy.
void String_append_String_move(String* str, String* other) __attribute__((nonnull(1, 2)));

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
    WC_ASSERT(i < str->size, "index out of bounds");
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? str->stk : str->heap)[i];
}

// UNCHECKED: no bounds check, even in Debug. Caller guarantees i < size.
__attribute__((nonnull(1))) static inline char String_char_at_unsafe(const String* str, u64 i)
{
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? str->stk : str->heap)[i];
}

__attribute__((nonnull(1))) static inline void String_set_char(String* str, u64 i, char c)
{
    WC_ASSERT(i < str->size, "index out of bounds");
    ((str->stk[STR_SSO_SIZE - 1] != '\0') ? str->stk : str->heap)[i] = c;
}


//  Comparison

// 0 == equal, <0 == str1 < str2, >0 == str1 > str2
int String_compare(const String* s1, const String* s2) __attribute__((nonnull(1, 2)));
__attribute__((nonnull(1, 2))) static inline bool String_equals(const String* s1, const String* s2)
{
    return String_compare(s1, s2) == 0;
}
bool String_equals_cstr(const String* str, const char* cstr) __attribute__((nonnull(1, 2)));


//  Search

// Returns index, or WC_NOT_FOUND if not found.
u64 String_find_char(const String* str, char c) __attribute__((nonnull(1)));
u64 String_find_cstr(const String* str, const char* substr) __attribute__((nonnull(1, 2)));

// Return a String (in allocator `a`) holding `length` chars starting at `start`.
String String_substr(const wc_allocator* a, const String* str, u64 start, u64 length)
    __attribute__((nonnull(1, 2), warn_unused_result));


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

__attribute__((nonnull(1))) static inline bool String_empty(const String* str)
{
    return str->size == 0;
}

__attribute__((nonnull(1))) static inline bool String_is_sso(const String* str)
{
    return str->stk[STR_SSO_SIZE - 1] != '\0';
}

// Read-only pointer into the buffer. Unlike String_data_ptr, this never
// returns NULL for an empty String. Internally, it uses String_ensure_null_term(),
// which places a '\0' right after the last valid char without touching the size of the string.
__attribute__((nonnull(1))) static inline const char* String_cstr_view(String* str)
{
    String_ensure_null_term(str);
    return String_is_sso(str) ? str->stk : str->heap;
}


#endif // STRING_H
