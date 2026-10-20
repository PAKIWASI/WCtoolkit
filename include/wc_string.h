#ifndef STRING_H
#define STRING_H

#include "common.h"
#include "wc_allocator.h"
#include "wc_comp.h"
#include "wc_hash.h"

#include <stdalign.h>
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
    u64                 size;
    u64                 capacity;
    const wc_allocator* alloc;
} String;


_Static_assert(sizeof(String) == 48, "String must be 48 bytes");


//  Construction / Destruction

// Create an empty String using allocator `a`.
wc_nodiscard wc_nonull(1) String String_create(const wc_allocator* a);

wc_nodiscard wc_nonull(1) String String_with_cap(const wc_allocator* a, u64 cap);

// Create a String from a NUL-terminated C string using allocator `a`.
// The length is computed internally with strlen.
wc_nodiscard wc_nonull(1, 2) String String_from_cstr(const wc_allocator* a, const char* cstr);

// Destroy the String's internal buffer
// Safe on a zeroed/moved-from String.
wc_nonull(1) void String_destroy(String* str);

// Deep copy src → new String in allocator `a`.
// dest is raw/uninitialized: never reads it before writing.
wc_nodiscard wc_nonull(1, 2) String String_copy(const wc_allocator* a, const String* src);

// Transfer ownership: dest gets src's contents, src is zeroed.
wc_nonull(1, 2) void String_move(String* dest, String* src);

//  Capacity

// Ensure capacity >= new_cap (never shrinks).
wc_nonull(1) void String_reserve(String* str, u64 new_cap);

// Reserve capacity and fill new slots with c.
wc_nonull(1) void String_reserve_char(String* str, u64 new_cap, char c);

// Shrink allocation to exactly fit current size.
wc_nonull(1) void String_shrink_to_fit(String* str);


//  Conversion

// Return a buffer (allocated from `a`) holding a NUL-terminated copy, caller
// must free with wc_free(a, ptr, str->size + 1, 1).
wc_nodiscard wc_nonull(1, 2) char* String_to_cstr(const wc_allocator* a, const String* str);

wc_nonull(1, 2) void String_to_cstr_buf(const String* str, char* buff, u64 n);

// Return a raw pointer into the internal buffer (no NUL terminator).
wc_nodiscard wc_nonull(1) char* String_data_ptr(const String* str);

// Guarantee a '\0' sits one byte past the last real character, WITHOUT
// touching str->size. Grows exactly like String_append_char would.
wc_nonull(1) void String_ensure_null_term(String* str);


//  Modification

wc_nonull(1) void String_append_char(String* str, char c);
// TODO:
wc_nonull(1, 2) void String_append_multi(String* str, const char* data, u64 len);
wc_nonull(1, 2) void String_append_cstr(String* str, const char* cstr);
wc_nonull(1, 2) void String_append_String(String* str, const String* other);
// Append other then destroy it (leaves other zeroed). When str is empty and
// both share an allocator, str takes over other's buffer: no copy.
wc_nonull(1, 2) void String_append_String_move(String* str, String* other);

wc_nodiscard wc_nonull(1) char String_pop_char(String* str);

wc_nonull(1) void String_insert_char(String* str, u64 i, char c);
wc_nonull(1, 3) void String_insert_cstr(String* str, u64 i, const char* cstr);
// TODO:
wc_nonull(1, 3) void String_insert_multi(String* str, u64 i, const char* data, u64 len);
wc_nonull(1, 3) void String_insert_String(String* str, u64 i, const String* other);

wc_nonull(1) void String_remove_char(String* str, u64 i);

// Remove chars in range [start, start + len)
wc_nonull(1) void String_remove_range(String* str, u64 start, u64 len);

// Remove all chars (keep allocation).
wc_nonull(1) static wc_always_inline void String_clear(String* str)
{
    str->size = 0;
}


//  Access

wc_nodiscard wc_nonull(1) static wc_always_inline char String_char_at(const String* str, u64 i)
{
    WC_ASSERT(i < str->size, "index out of bounds");
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? str->stk : str->heap)[i];
}

// UNCHECKED: no bounds check, even in Debug. Caller guarantees i < size.
wc_nodiscard wc_nonull(1) static wc_always_inline char String_char_at_unsafe(const String* str, u64 i)
{
    return ((str->stk[STR_SSO_SIZE - 1] != '\0') ? str->stk : str->heap)[i];
}

wc_nonull(1) static wc_always_inline void String_set_char(String* str, u64 i, char c)
{
    WC_ASSERT(i < str->size, "index out of bounds");
    ((str->stk[STR_SSO_SIZE - 1] != '\0') ? str->stk : str->heap)[i] = c;
}


//  Comparison

// 0 == equal, <0 == str1 < str2, >0 == str1 > str2
wc_nodiscard wc_nonull(1, 2) int String_compare(const String* s1, const String* s2);

wc_nodiscard wc_nonull(1, 2) static wc_always_inline bool String_equals(const String* s1, const String* s2)
{
    return String_compare(s1, s2) == 0;
}

wc_nodiscard wc_nonull(1, 2) bool String_equals_cstr(const String* str, const char* cstr);


//  Search

// Returns index, or WC_NOT_FOUND if not found.
wc_nodiscard wc_nonull(1) u64 String_find_char(const String* str, char c);
wc_nodiscard wc_nonull(1, 2) u64 String_find_cstr(const String* str, const char* substr);

// Return a String (in allocator `a`) holding `length` chars starting at `start`.
wc_nodiscard wc_nonull(1, 2) String String_substr(const wc_allocator* a, const String* str, u64 start, u64 length);


//  I/O

wc_nonull(1) void String_print(const String* str);


//  Inline helpers

wc_nodiscard wc_nonull(1) static wc_always_inline u64 String_len(const String* str)
{
    return str->size;
}

wc_nodiscard wc_nonull(1) static wc_always_inline u64 String_capacity(const String* str)
{
    return str->capacity;
}

wc_nodiscard wc_nonull(1) static wc_always_inline bool String_empty(const String* str)
{
    return str->size == 0;
}

wc_nodiscard wc_nonull(1) static wc_always_inline bool String_is_sso(const String* str)
{
    return str->stk[STR_SSO_SIZE - 1] != '\0';
}

// Read-only pointer into the buffer. Unlike String_data_ptr, this never
// returns NULL for an empty String. Internally, it uses String_ensure_null_term(),
// which places a '\0' right after the last valid char without touching the size of the string.
wc_nodiscard wc_nonull(1) static wc_always_inline const char* String_cstr_view(String* str)
{
    String_ensure_null_term(str);
    return String_is_sso(str) ? str->stk : str->heap;
}


// Helpers

/* 
 * 1.  STRING BY VALUE
 *
 * String stores its own allocator.
 * str_copy  — copies using dst allocator (so a String follows its outer container)
 * str_del   — delegates to String_destroy() (frees heap buf via str->alloc, NOT the slot)
*/

static inline void str_copy(const wc_allocator* dst, void* dest, const void* src)
{
    *(String*)dest = String_copy(dst, (const String*)src);
}

static inline void str_del(void* elm)
{
    String_destroy((String*)elm); // free data buffer via str->alloc, NOT the slot
}

static inline void str_print(const void* elm)
{
    String_print((const String*)elm);
}


/*
 * 2.  STRING BY POINTER
 *
 * str_copy_ptr — box a deep copy in allocator `dst` (shell + contents from dst)
 * str_del_ptr  — destroy pointee via its own allocator, then free shell
*/

static inline void str_copy_ptr(const wc_allocator* dst, void* dest, const void* src)
{
    // Box a deep copy: shell and contents both from dst
    *(String**)dest = (String*)WC_BOX(dst, String, String_copy, *(const String* const*)src);
}

static inline void str_del_ptr(void* elm)
{
    String* s = *(String**)elm;
    if (!s) {
        return;
    }
    // Read allocator before destroy zeroes the struct
    const wc_allocator* a = s->alloc;
    String_destroy(s);                              // free heap buffer via s->alloc
    wc_free(a, s, sizeof(String), alignof(String)); // free the shell
}

static inline void str_print_ptr(const void* elm)
{
    String_print(*(const String**)elm);
}

// SHARED OPS INSTANCES (for String VALUES and vectors; String is not a map key)

static const wc_container_ops wc_str_ops     = {.copy_fn = str_copy, .del_fn = str_del};
static const wc_container_ops wc_str_ptr_ops = {.copy_fn = str_copy_ptr, .del_fn = str_del_ptr};


#endif // STRING_H
