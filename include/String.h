#ifndef STRING_H
#define STRING_H


#include "gen_vector.h"


// ===== STRING =====
// the string is just a genVec of char type (length based string - not cstr)
typedef genVec String;
// ==================


// Construction/Destruction

// create string on the heap
String* string_create(void);

// create string with struct on the stack and data on heap
void string_create_stk(String* str, const char* cstr);

// create string on heap from a cstr
String* string_from_cstr(const char* cstr);

// get copy of a string (heap allocated)
String* string_from_string(const String* other);

// reserve a capacity for a string (must be greater than current cap)
void string_reserve(String* str, u64 capacity);

// reserve a capacity with a char
void string_reserve_char(String* str, u64 capacity, char c);

// destroy the heap allocated string
void string_destroy(String* str);

// destroy only the data ptr of string struct (for stk created str)
void string_destroy_stk(String* str);

// move string contents (nulls source)
// Note: src must be heap allocated
void string_move(String* dest, String** src);

// make deep copy
void string_copy(String* dest, const String* src);

// get cstr as COPY ('\0' present)
// cstr is MALLOCED and must be freed by user
char* string_to_cstr(const String* str);

// get ptr to the cstr buffer
// Note: NO NULL TERMINATOR
char* string_data_ptr(const String* str);


// Modification

// append a cstr to the end of a string
void string_append_cstr(String* str, const char* cstr);

// append a string "other" to the end of a string "str"
void string_append_string(String* str, const String* other);

// Concatenate string "other" and destroy source "str"
void string_append_string_move(String* str, String** other);

// append a char to the end of a string
void string_append_char(String* str, char c);

// insert a char at index i of string
void string_insert_char(String* str, u64 i, char c);

// insert a cstr at index i
void string_insert_cstr(String* str, u64 i, const char* cstr);

// insert a string "str" at index i
void string_insert_string(String* str, u64 i, const String* other);

// remove char from end of a string
char string_pop_char(String* str);

// remove a char from index i of string
void string_remove_char(String* str, u64 i);

// remove elements from l to r (inclusive)
void string_remove_range(String* str, u64 l, u64 r);

// remove all chars (keep memory)
void string_clear(String* str);


// Access

// return char at index i
char string_char_at(const String* str, u64 i);

// set the value of char at index i
void string_set_char(String* str, u64 i, char c);


// Comparison

// compare (c-style) two strings
// 0 -> equal, negative -> str1 < str2, positive -> str1 > str2
int string_compare(const String* str1, const String* str2);

// return true if string's data matches
b8 string_equals(const String* str1, const String* str2);

// return true if a string's data matches a cstr
b8 string_equals_cstr(const String* str, const char* cstr);


// Search

// return index of char c (UINT_MAX otherwise)
u64 string_find_char(const String* str, char c);

// return index of cstr "substr" (UINT_MAX otherwise)
u64 string_find_cstr(const String* str, const char* substr);

// Set a heap allocated string of a substring starting at index "start", upto length
String* string_substr(const String* str, u64 start, u64 length);


// I/O

// print the content of str
void string_print(const String* str);


// Basic properties

// get the current length of the string
static inline u64 string_len(const String* str)
{
    CHECK_FATAL(!str, "str is null");
    return str->size;
}

// get the capacity of the genVec container of string
static inline u64 string_capacity(const String* str)
{
    return str->capacity;
}

// return true if str is empty
static inline b8 string_empty(const String* str)
{
    return string_len(str) == 0;
}

/*
 Macro to create a temporary cstr for read ops.
 Note: Must not break or return in the block.
 Usage:

TEMP_CSTR_READ(s) {
    printf("%s\n", string_data_ptr(s));
}
*/
#define TEMP_CSTR_READ(str) \
    for (u8 _once = 0; (_once == 0) && (string_append_char((str), '\0'), 1); _once++, string_pop_char((str)))

// TODO: how to do this??
#define TEMP_CSTR_READ_NAMED(str, name) \
    for (u8 _once = 0; (_once == 0) && (string_append_char((str), '\0'), 1); _once++, string_pop_char((str)))

// TODO: remove genVec dependency ?
// we only use a handfull of genVec funcs so we could write them specificly for str

typedef struct {
    union {
        char* heap;
        char stk[23];
    }; 
    b8 sso;
    u64 size;
    u64 capacity;
} str;

// 23 1 8 8 = 40

#endif // STRING_H
