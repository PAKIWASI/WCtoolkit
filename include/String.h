#ifndef STRING_H
#define STRING_H

#include "common.h"


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

// Create an empty string on the heap.
String* string_create(void) __attribute__((warn_unused_result));

// Create a string on the heap from a cstr.
String* string_from_cstr(const char* cstr) __attribute__((warn_unused_result));

// Create a copy of another heap-allocated string.
String* string_from_string(const String* other) __attribute__((nonnull(1), warn_unused_result));

// Initialise a String whose struct lives on the stack (data may be on heap).
void string_create_stk(const char* cstr, String* str) __attribute__((nonnull(2)));

// Destroy a heap-allocated String (frees struct + data).
void string_destroy(String* str) __attribute__((nonnull(1)));

// Destroy only the internal data of a stack-allocated String.
void string_destroy_stk(String* str) __attribute__((nonnull(1)));

// Move: transfer ownership from *src to dest, nulling *src.
// *src must be heap-allocated.
void string_move(String* dest, String** src) __attribute__((nonnull(1, 2)));

// Deep copy src into dest (dest is re-initialised).
// SAFE ON: raw/uninitialized dest. Never reads dest before writing it.
void string_copy(String* dest, const String* src) __attribute__((nonnull(1, 2)));


//  Capacity

// Ensure capacity >= new_cap (never shrinks).
void string_reserve(String* str, u64 new_cap) __attribute__((nonnull(1)));

// Reserve capacity and fill new slots with c.
void string_reserve_char(String* str, u64 new_cap, char c) __attribute__((nonnull(1)));

// Shrink allocation to exactly fit current size.
void string_shrink_to_fit(String* str) __attribute__((nonnull(1)));


//  Conversion

// Return a malloc'd NUL-terminated copy — caller must free().
char* string_to_cstr(const String* str) __attribute__((nonnull(1), warn_unused_result));

void string_to_cstr_buf(const String* str, char* buff, u64 n) __attribute__((nonnull(1, 2)));

// Return a raw pointer into the internal buffer (no NUL terminator).
char* string_data_ptr(const String* str) __attribute__((nonnull(1)));


//  Modification

void string_append_char(String* str, char c) __attribute__((nonnull(1)));
void string_append_cstr(String* str, const char* cstr) __attribute__((nonnull(1, 2)));
void string_append_string(String* str, const String* other) __attribute__((nonnull(1, 2)));
// Append other then destroy it (nulls *other).
void string_append_string_move(String* str, String** other) __attribute__((nonnull(1, 2)));

char string_pop_char(String* str) __attribute__((nonnull(1)));

void string_insert_char(String* str, u64 i, char c) __attribute__((nonnull(1)));
void string_insert_cstr(String* str, u64 i, const char* cstr) __attribute__((nonnull(1, 3)));
void string_insert_string(String* str, u64 i, const String* other) __attribute__((nonnull(1, 3)));

void string_remove_char(String* str, u64 i) __attribute__((nonnull(1)));

// TODO: test
// Remove chars in range [start, start + len)
void string_remove_range(String* str, u64 start, u64 len) __attribute__((nonnull(1)));

// Remove all chars (keep allocation).
__attribute__((nonnull(1))) static inline void string_clear(String* str)
{
    str->size = 0;
}


//  Access

__attribute__((nonnull(1))) static inline char string_char_at(const String* str, u64 i)
{
    CHECK_FATAL(i >= str->size, "index out of bounds");
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? (str)->stk : (str)->heap)[i];
}

__attribute__((nonnull(1))) static inline char string_char_at_unsafe(const String* str, u64 i)
{
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? (str)->stk : (str)->heap)[i];
}

__attribute__((nonnull(1))) static inline void string_set_char(String* str, u64 i, char c)
{
    CHECK_FATAL(i >= str->size, "index out of bounds");
    ((str->stk[STR_SSO_SIZE - 1] != '\0') ? str->stk : str->heap)[i] = c;
}


//  Comparison

// 0 == equal, <0 == str1 < str2, >0 == str1 > str2
int string_compare(const String* s1, const String* s2) __attribute__((nonnull(1, 2)));
__attribute__((nonnull(1, 2))) static inline b8 string_equals(const String* s1, const String* s2)
{
    return string_compare(s1, s2) == 0;
}
b8 string_equals_cstr(const String* str, const char* cstr) __attribute__((nonnull(1, 2)));


//  Search

// Returns index, or WC_NOT_FOUND if not found.
u64 string_find_char(const String* str, char c) __attribute__((nonnull(1)));
u64 string_find_cstr(const String* str, const char* substr) __attribute__((nonnull(1, 2)));

// Return a heap-allocated substring starting at `start` of `length` chars.
String* string_substr(const String* str, u64 start, u64 length) __attribute__((nonnull(1), warn_unused_result));


//  I/O

void string_print(const String* str) __attribute__((nonnull(1)));


//  Inline helpers

__attribute__((nonnull(1))) static inline u64 string_len(const String* str)
{
    return str->size;
}

__attribute__((nonnull(1))) static inline u64 string_capacity(const String* str)
{
    return str->capacity;
}

__attribute__((nonnull(1))) static inline b8 string_empty(const String* str)
{
    return str->size == 0;
}

__attribute__((nonnull(1))) static inline b8 string_is_sso(const String* str)
{
    return str->stk[STR_SSO_SIZE - 1] != '\0';
}


/*
 Macro to temporarily NUL-terminate a String for read-only C APIs.
 Safe with break/return/goto using cleanup attribute (gcc/clang)

 Usage:
   TEMP_CSTR_READ(s) {
       printf("%s\n", string_data_ptr(s));
   }
*/
static inline void wctemp_cstr_read_cleanup(String** s)
{
    if (s && *s) {
        string_pop_char(*s);
    }
}

#define TEMP_CSTR_READ(str)                                                      \
    for (int _tcr_once = 1; _tcr_once; _tcr_once = 0)                            \
        for (String* __attribute__((cleanup(wctemp_cstr_read_cleanup))) _tcr_s = \
                 ((str) ? (string_append_char((str), '\0'), (str)) : NULL);      \
             _tcr_once; _tcr_once = 0)



#endif // STRING_H
