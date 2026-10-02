#ifndef STRING_H
#define STRING_H

#include "common.h"
#include "wc_allocator.h"


#ifndef STRING_GROWTH
#define STRING_GROWTH 1.5F // capacity multiplier on grow
#endif

// SSO inline buffer size (bytes).  Last byte is the mode flag:
//   nonzero = SSO mode (the string lives in stk[]).
//   '\0'    = heap mode (the string lives in heap).
// Usable SSO bytes = STR_SSO_SIZE - 1 = 31.
#define STR_SSO_SIZE 32


typedef struct {
    union {
        char* heap;
        char  stk[STR_SSO_SIZE];
    };
    u64          size;
    u64          capacity;
    wc_allocator alloc;
} String;

_Static_assert(sizeof(String) == 64, "String must be 64 bytes");



//  Construction / Destruction

// Create an empty String using allocator `a`.
String String_create(wc_allocator a) __attribute__((warn_unused_result));

// Create a String from a C string using allocator `a`.
String String_from_cstr(wc_allocator a, const char* cstr) __attribute__((warn_unused_result));

// Create a deep copy of `other` into allocator `a`.
String String_from_String(wc_allocator a, const String* other) __attribute__((nonnull(2), warn_unused_result));

// Destroy the String's internal buffer (does NOT free the String struct).
// Safe on a zeroed/moved-from String.
void String_destroy(String* str) __attribute__((nonnull(1)));

// Deep copy src → new String in allocator `a`.
// dest is raw/uninitialized: never reads it before writing.
String String_copy(wc_allocator a, const String* src) __attribute__((nonnull(2), warn_unused_result));

// Transfer ownership: dest gets src's contents, src is zeroed.
void String_move(String* dest, String* src) __attribute__((nonnull(1, 2)));


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
char* String_to_cstr(wc_allocator a, const String* str) __attribute__((nonnull(2), warn_unused_result));

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
// Append other then destroy it (leaves other zeroed).
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
__attribute__((nonnull(1, 2))) static inline b8 String_equals(const String* s1, const String* s2)
{
    return String_compare(s1, s2) == 0;
}
b8 String_equals_cstr(const String* str, const char* cstr) __attribute__((nonnull(1, 2)));


//  Search

// Returns index, or WC_NOT_FOUND if not found.
u64 String_find_char(const String* str, char c) __attribute__((nonnull(1)));
u64 String_find_cstr(const String* str, const char* substr) __attribute__((nonnull(1, 2)));

// Return a String (in allocator `a`) holding `length` chars starting at `start`.
String String_substr(wc_allocator a, const String* str, u64 start, u64 length)
    __attribute__((nonnull(2), warn_unused_result));


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


#endif // STRING_H
