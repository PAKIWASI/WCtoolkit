#ifndef STRING_H
#define STRING_H

#include "common.h"


#define STR_SSO_SIZE 24

#ifndef STRING_GROWTH
    #define STRING_GROWTH    1.5F    // capacity multiplier on grow
#endif


typedef struct {
    union {
        char* heap;
        char  stk[STR_SSO_SIZE];
    };
    // b8  sso;     // HACK: if cap is = STR_SSO_SIZE then we are in sso mode, if greater then heap mode
    u64 size;
    u64 capacity;
} String;

// 24 8 8 = 40 bytes


//  Construction / Destruction 

// Create an empty string on the heap.
String* string_create(void);

// Create a string on the heap from a cstr.
String* string_from_cstr(const char* cstr);

// Create a copy of another heap-allocated string.
String* string_from_string(const String* other);

// Initialise a String whose struct lives on the stack (data may be on heap).
void string_create_stk(String* str, const char* cstr);

// Destroy a heap-allocated String (frees struct + data).
void string_destroy(String* str);

// Destroy only the internal data of a stack-allocated String.
void string_destroy_stk(String* str);

// Move: transfer ownership from *src to dest, nulling *src.
// *src must be heap-allocated.
void string_move(String* dest, String** src);

// Deep copy src into dest (dest is re-initialised).
void string_copy(String* dest, const String* src);


//  Capacity 

// Ensure capacity >= new_cap (never shrinks).
void string_reserve(String* str, u64 new_cap);

// Reserve capacity and fill new slots with c.
void string_reserve_char(String* str, u64 new_cap, char c);

// Shrink allocation to exactly fit current size.
void string_shrink_to_fit(String* str);


//  Conversion 

// Return a malloc'd NUL-terminated copy — caller must free().
char* string_to_cstr(const String* str);

// Return a raw pointer into the internal buffer (no NUL terminator).
char* string_data_ptr(const String* str);


//  Modification 

void string_append_char(String* str, char c);
void string_append_cstr(String* str, const char* cstr);
void string_append_string(String* str, const String* other);
// Append other then destroy it (nulls *other).
void string_append_string_move(String* str, String** other);

char string_pop_char(String* str);

void string_insert_char(String* str, u64 i, char c);
void string_insert_cstr(String* str, u64 i, const char* cstr);
void string_insert_string(String* str, u64 i, const String* other);

void string_remove_char(String* str, u64 i);
// Remove chars in range [l, r] inclusive.
void string_remove_range(String* str, u64 l, u64 r);

// Remove all chars (keep allocation).
void string_clear(String* str);


//  Access 

char string_char_at(const String* str, u64 i);
void string_set_char(String* str, u64 i, char c);


//  Comparison 

// 0 == equal, <0 == str1 < str2, >0 == str1 > str2
int string_compare(const String* s1, const String* s2);
b8  string_equals(const String* s1, const String* s2);
b8  string_equals_cstr(const String* str, const char* cstr);


//  Search 

// Returns index, or (u64)-1 if not found.
u64 string_find_char(const String* str, char c);
u64 string_find_cstr(const String* str, const char* substr);

// Return a heap-allocated substring starting at `start` of `length` chars.
String* string_substr(const String* str, u64 start, u64 length);


//  I/O 

void string_print(const String* str);


//  Inline helpers 

static inline u64 string_len(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->size;
}

static inline u64 string_capacity(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->capacity;
}

static inline b8 string_empty(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->size == 0;
}

static inline b8 string_sso(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->capacity == STR_SSO_SIZE;
}


/*
 Macro to temporarily NUL-terminate a String for read-only C APIs.
Note: Do NOT break/return/goto inside the block.

 Usage:
   TEMP_CSTR_READ(s) {
       printf("%s\n", string_data_ptr(s));
   }
*/
#define TEMP_CSTR_READ(str) \
    for (u8 _once = 0; \
         (_once == 0) && (string_append_char((str), '\0'), 1); \
         _once++, string_pop_char((str)))



#endif // STRING_H
