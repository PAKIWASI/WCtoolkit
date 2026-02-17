#ifndef WC_STRING_H
#define WC_STRING_H

/*
 * String_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "String_single.h"
 *
 * All other files just:
 *     #include "String_single.h"
 */

/* ===== common.h ===== */
#ifndef WC_COMMON_H
#define WC_COMMON_H

/*
 * C Data Structures Library
 * Copyright (c) 2026 Wasi Ullah (PAKIWASI)
 * Licensed under the MIT License. See LICENSE file for details.
 */



// LOGGING/ERRORS

#include <stdio.h>
#include <stdlib.h>

// ANSI Color Codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_CYAN    "\033[1;36m"

#define WARN(fmt, ...)                                                                       \
    do {                                                                                     \
        printf(COLOR_YELLOW "[WARN]" " %s:%d:%s(): " fmt "\n" COLOR_RESET, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    } while (0)

#define FATAL(fmt, ...)                                                                                \
    do {                                                                                               \
        fprintf(stderr, COLOR_RED "[FATAL]" " %s:%d:%s(): " fmt "\n" COLOR_RESET, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        exit(EXIT_FAILURE);                                                                            \
    } while (0)

#define ASSERT_WARN(cond, fmt, ...)                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            WARN("Assertion failed: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                               \
    } while (0)

#define ASSERT_WARN_RET(cond, ret, fmt, ...)                            \
    do {                                                                \
        if (!(cond)) {                                                  \
            WARN("Assertion failed: (%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                                                 \
        }                                                               \
    } while (0)

#define ASSERT_FATAL(cond, fmt, ...)                                     \
    do {                                                                 \
        if (!(cond)) {                                                   \
            FATAL("Assertion failed: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                                \
    } while (0)

#define CHECK_WARN(cond, fmt, ...)                           \
    do {                                                     \
        if ((cond)) {                                        \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                    \
    } while (0)

#define CHECK_WARN_RET(cond, ret, fmt, ...)                  \
    do {                                                     \
        if ((cond)) {                                        \
            WARN("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
            return ret;                                      \
        }                                                    \
    } while (0)

#define CHECK_FATAL(cond, fmt, ...)                           \
    do {                                                      \
        if (cond) {                                           \
            FATAL("Check: (%s): " fmt, #cond, ##__VA_ARGS__); \
        }                                                     \
    } while (0)

#define LOG(fmt, ...)                               \
    do {                                            \
        printf(COLOR_CYAN "[LOG]" " : %s(): " fmt "\n" COLOR_RESET, __func__, ##__VA_ARGS__); \
    } while (0)

// TYPES

#include <stdint.h>

typedef uint8_t  u8;
typedef uint8_t  b8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define false ((b8)0)
#define true  ((b8)1)


// GENERIC FUNCTIONS
typedef void (*copy_fn)(u8* dest, const u8* src);
typedef void (*move_fn)(u8* dest, u8** src);
typedef void (*delete_fn)(u8* key);
typedef void (*print_fn)(const u8* elm);
typedef int  (*compare_fn)(const u8* a, const u8* b, u64 size);
typedef void (*for_each_fn)(u8* elm); 


// CASTING

#define cast(x)    ((u8*)(&(x)))
#define castptr(x) ((u8*)(x))


// COMMON SIZES

#define KB (1 << 10)
#define MB (1 << 20)

#define nKB(n) ((u64)((n) * KB))
#define nMB(n) ((u64)((n) * MB))


// RAW BYTES TO HEX

void print_hex(const u8* ptr, u64 size, u32 bytes_per_line);

#endif /* WC_COMMON_H */

/* ===== gen_vector.h ===== */
#ifndef WC_GEN_VECTOR_H
#define WC_GEN_VECTOR_H

/*          TLDR
 * genVec is a value-based generic vector.
 * Elements are stored inline and managed via user-supplied
 * copy/move/destructor callbacks.
 *
 * This avoids pointer ownership ambiguity and improves cache locality.
 */


// User-provided callback functions
// moved to common.h


// genVec growth/shrink settings (user can change)
#ifndef GENVEC_GROWTH
    #define GENVEC_GROWTH 1.5F // vec capacity multiplier
#endif
#ifndef GENVEC_SHRINK_AT
    #define GENVEC_SHRINK_AT 0.25F // % filled to shrink at (25% filled)
#endif
#ifndef GENVEC_SHRINK_BY
    #define GENVEC_SHRINK_BY 0.5F // capacity dividor (half)
#endif


// generic vector container
typedef struct {
    u8* data; // pointer to generic data

    u64 size;      // Number of elements currently in vector
    u64 capacity;  // Total allocated capacity
    u32 data_size; // Size of each element in bytes

    // Function Pointers (Type based Memory Management)
    copy_fn   copy_fn; // Deep copy function for owned resources (or NULL)
    move_fn   move_fn; // Get a double pointer, transfer ownership and null original (or NULL)
    delete_fn del_fn;  // Cleanup function for owned resources (or NULL)
} genVec;

// 8 8 8 4  '4'  8 8 8  = 56



// Memory Management
// ===========================

// Initialize vector with capacity n. If elements own heap resources,
// provide copy_fn (deep copy) and del_fn (cleanup). Otherwise pass NULL.
genVec* genVec_init(u64 n, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn);

// Initialize vector on stack with data on heap
// SVO works best here as it is on the stack***
void genVec_init_stk(u64 n, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn,
                     genVec* vec);

// Initialize vector of size n, all elements set to val
genVec* genVec_init_val(u64 n, const u8* val, u32 data_size, copy_fn copy_fn, move_fn move_fn,
                        delete_fn del_fn);

void genVec_init_val_stk(u64 n, const u8* val, u32 data_size, copy_fn copy_fn, move_fn move_fn,
                         delete_fn del_fn, genVec* vec);

// vector COMPLETELY on stack (can't grow in size)
// you provide a stack inited array which becomes internal array of vector
// WARNING - This crashes when size = capacity and you try to push
void genVec_init_arr(u64 n, u8* arr, u32 data_size, copy_fn copy_fn, move_fn move_fn,
                     delete_fn del_fn, genVec* vec);

// Destroy heap-allocated vector and clean up all elements
void genVec_destroy(genVec* vec);

// Destroy stack-allocated vector (cleans up data, but not vec itself)
void genVec_destroy_stk(genVec* vec);

// Remove all elements (calls del_fn on each), keep capacity
void genVec_clear(genVec* vec);

// Remove all elements and free memory, shrink capacity to 0
void genVec_reset(genVec* vec);

// Ensure vector has at least new_capacity space (never shrinks)
void genVec_reserve(genVec* vec, u64 new_capacity);

// Grow to new_capacity and fill new slots with val
void genVec_reserve_val(genVec* vec, u64 new_capacity, const u8* val);

// Shrink vector to it's size (reallocates)
void genVec_shrink_to_fit(genVec* vec);



// Operations
// ===========================

// Append element to end (makes deep copy if copy_fn provided)
void genVec_push(genVec* vec, const u8* data);

// Append element to end, transfer ownership (nulls original pointer)
void genVec_push_move(genVec* vec, u8** data);

// Remove element from end. If popped provided, copies element before deletion.
// Note: del_fn is called regardless to clean up owned resources.
void genVec_pop(genVec* vec, u8* popped);

// Copy element at index i into out buffer
void genVec_get(const genVec* vec, u64 i, u8* out);

// Get pointer to element at index i
// Note: Pointer invalidated by push/insert/remove operations
const u8* genVec_get_ptr(const genVec* vec, u64 i);

// Get MUTABLE pointer to element at index i
// Note: Pointer invalidated by push/insert/remove operations
u8* genVec_get_ptr_mut(const genVec* vec, u64 i);

// apply a function on each value of the array
void genVec_for_each(genVec* vec, for_each_fn for_each);

// Replace element at index i with data (cleans up old element)
void genVec_replace(genVec* vec, u64 i, const u8* data);

// Replace element at index i, transfer ownership (cleans up old element)
void genVec_replace_move(genVec* vec, u64 i, u8** data);

// Insert element at index i, shifting elements right
void genVec_insert(genVec* vec, u64 i, const u8* data);

// Insert element at index i with ownership transfer, shifting elements right
void genVec_insert_move(genVec* vec, u64 i, u8** data);

// Insert num_data elements from data arr to vec. data should have same size data as vec
void genVec_insert_multi(genVec* vec, u64 i, const u8* data, u64 num_data);

// Insert (move) num_data  elements from data starting at index i. Transfers ownership
void genVec_insert_multi_move(genVec* vec, u64 i, u8** data, u64 num_data);

// Remove element at index i, optionally copy to out, shift elements left
void genVec_remove(genVec* vec, u64 i, u8* out);

// Remove elements in range [l, r] inclusive.
void genVec_remove_range(genVec* vec, u64 l, u64 r);

// Get pointer to first element
const u8* genVec_front(const genVec* vec);

// Get pointer to last element
const u8* genVec_back(const genVec* vec);


// Utility
// ===========================

// Print all elements using provided print function
void genVec_print(const genVec* vec, print_fn fn);

// Deep copy src vector into dest
// Note: cleans up dest (if already inited)
void genVec_copy(genVec* dest, const genVec* src);

// transfers ownership from src to dest
// Note: src must be heap-allocated
void genVec_move(genVec* dest, genVec** src);


// Get number of elements in vector
static inline u64 genVec_size(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    return vec->size;
}

// Get total capacity of vector
static inline u64 genVec_capacity(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    return vec->capacity;
}

// Check if vector is empty
static inline b8 genVec_empty(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    return vec->size == 0;
}

#endif /* WC_GEN_VECTOR_H */

/* ===== String.h ===== */
#ifndef WC_STRING_H
#define WC_STRING_H

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


// TODO:
// void string_to_cstr_buf(const String* str, char* buf, u32 buf_size);
// void string_to_cstr_buf_move(const String* str, char* buf, u32 buf_size);


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
// 0 -> equal, 1 -> not equal
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

// TODO: pass a buffer version of substr??

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

// TODO: test
/*
 macro to create a temporary cstr for read ops
 Note: Must not break or return in the block
 Usage:

TEMP_CSTR_READ(s)
{
    printf("%s\n", string_data_ptr(s));
}
*/
#define TEMP_CSTR_READ(str) \
    for (u8 _once = 0; (_once == 0) && (string_append_char((str), '\0'), 1); _once++, string_pop_char((str)))

#endif /* WC_STRING_H */

#ifdef WC_IMPLEMENTATION

/* ===== common.c ===== */
#ifndef WC_COMMON_IMPL
#define WC_COMMON_IMPL

void print_hex(const u8* ptr, u64 size, u32 bytes_per_line) 
{
    if (ptr == NULL | size == 0 | bytes_per_line == 0) { return; }

    // hex rep 0-15
    const char* hex = "0123456789ABCDEF";
    
    for (u64 i = 0; i < size; i++) 
    {
        u8 val1 = ptr[i] >> 4;      // get upper 4 bits as num b/w 0-15
        u8 val2 = ptr[i] & 0x0F;    // get lower 4 bits as num b/w 0-15
        
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

#endif /* WC_COMMON_IMPL */

/* ===== gen_vector.c ===== */
#ifndef WC_GEN_VECTOR_IMPL
#define WC_GEN_VECTOR_IMPL

#include <string.h>



#define GENVEC_MIN_CAPACITY 4

// MACROS

// get ptr to elm at index i
#define GET_PTR(vec, i) ((vec->data) + ((u64)(i) * ((vec)->data_size)))
// get total_size in bytes for i elements
#define GET_SCALED(vec, i) ((u64)(i) * ((vec)->data_size))

#define MAYBE_GROW(vec)                                 \
    do {                                                \
        if (!vec->data || vec->size >= vec->capacity) { \
            genVec_grow(vec);                           \
        }                                               \
    } while (0)

#define MAYBE_SHRINK(vec)                                                  \
    do {                                                                   \
        if (vec->size <= (u64)((float)vec->capacity * GENVEC_SHRINK_AT)) { \
            genVec_shrink(vec);                                            \
        }                                                                  \
    } while (0)



//private functions

void genVec_grow(genVec* vec);
void genVec_shrink(genVec* vec);


// API Implementation

genVec* genVec_init(u64 n, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn)
{
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    genVec* vec = malloc(sizeof(genVec));
    CHECK_FATAL(!vec, "vec init failed");

    // Only allocate memory if n > 0, otherwise data can be NULL
    vec->data = (n > 0) ? malloc(data_size * n) : NULL;

    // Only check for allocation failure if we actually tried to allocate
    if (n > 0 && !vec->data) {
        free(vec);
        FATAL("data init failed");
    }

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->copy_fn   = copy_fn;
    vec->move_fn   = move_fn;
    vec->del_fn    = del_fn;

    return vec;
}


void genVec_init_stk(u64 n, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn,
                     genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    // Only allocate memory if n > 0, otherwise data can be NULL
    vec->data = (n > 0) ? malloc(data_size * n) : NULL;
    CHECK_FATAL(n > 0 && !vec->data, "data init failed");

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->copy_fn   = copy_fn;
    vec->move_fn   = move_fn;
    vec->del_fn    = del_fn;
}

genVec* genVec_init_val(u64 n, const u8* val, u32 data_size, copy_fn copy_fn, move_fn move_fn,
                        delete_fn del_fn)
{
    CHECK_FATAL(!val, "val can't be null");
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    genVec* vec = genVec_init(n, data_size, copy_fn, move_fn, del_fn);

    vec->size = n; //capacity set to n in upper func

    for (u64 i = 0; i < n; i++) {
        if (copy_fn) {
            copy_fn(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    }

    return vec;
}

void genVec_init_val_stk(u64 n, const u8* val, u32 data_size, copy_fn copy_fn, move_fn move_fn,
                         delete_fn del_fn, genVec* vec)
{
    CHECK_FATAL(!val, "val can't be null");
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    genVec_init_stk(n, data_size, copy_fn, move_fn, del_fn, vec);

    vec->size = n;

    for (u64 i = 0; i < n; i++) {
        if (copy_fn) {
            copy_fn(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    }
}

void genVec_init_arr(u64 n, u8* arr, u32 data_size, copy_fn copy_fn, move_fn move_fn,
                     delete_fn del_fn, genVec* vec)
{
    CHECK_FATAL(!arr, "arr is null");
    CHECK_FATAL(!vec, "vec is null");

    CHECK_FATAL(n == 0, "size of arr can't be 0");
    CHECK_FATAL(data_size == 0, "data_size of arr can't be 0");

    vec->data = arr;

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;

    vec->copy_fn = copy_fn;
    vec->move_fn = move_fn;
    vec->del_fn  = del_fn;
}

void genVec_destroy(genVec* vec)
{
    genVec_destroy_stk(vec);

    free(vec);
}


void genVec_destroy_stk(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    if (!vec->data) {
        return;
    }

    if (vec->del_fn) {
        // Custom cleanup for each element
        for (u64 i = 0; i < vec->size; i++) {
            vec->del_fn(GET_PTR(vec, i));
        }
    }

    free(vec->data);
    vec->data = NULL;
    // dont free vec as on stk (don't own memory)
}

void genVec_clear(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    if (vec->del_fn) { // owns resources
        for (u64 i = 0; i < vec->size; i++) {
            vec->del_fn(GET_PTR(vec, i));
        }
    }
    // doesn't free container
    vec->size = 0;
}

void genVec_reset(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    if (vec->del_fn) {
        for (u64 i = 0; i < vec->size; i++) {
            vec->del_fn(GET_PTR(vec, i));
        }
    }

    free(vec->data);
    vec->data     = NULL;
    vec->size     = 0;
    vec->capacity = 0;
}


void genVec_reserve(genVec* vec, u64 new_capacity)
{
    CHECK_FATAL(!vec, "vec is null");

    // Only grow, never shrink with reserve
    if (new_capacity <= vec->capacity) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, new_capacity));
    CHECK_FATAL(!new_data, "realloc failed");

    vec->data     = new_data;
    vec->capacity = new_capacity;
}

void genVec_reserve_val(genVec* vec, u64 new_capacity, const u8* val)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!val, "val is null");
    CHECK_FATAL(new_capacity < vec->size, "new_capacity must be >= current size");

    genVec_reserve(vec, new_capacity);

    for (u64 i = vec->size; i < new_capacity; i++) {
        if (vec->copy_fn) {
            vec->copy_fn(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, vec->data_size);
        }
    }
    vec->size = new_capacity;
}

void genVec_shrink_to_fit(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    // min allowd cap or size
    u64 min_cap  = vec->size > GENVEC_MIN_CAPACITY ? vec->size : GENVEC_MIN_CAPACITY;
    u64 curr_cap = vec->capacity;

    // if curr cap is already equal (or less??) than min allowed cap
    if (curr_cap <= min_cap) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, min_cap));
    // update data ptr
    vec->data     = new_data;
    vec->size     = min_cap;
    vec->capacity = min_cap;
}


void genVec_push(genVec* vec, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");

    // Check if we need to allocate or grow
    MAYBE_GROW(vec);

    if (vec->copy_fn) {
        vec->copy_fn(GET_PTR(vec, vec->size), data);
    } else {
        memcpy(GET_PTR(vec, vec->size), data, vec->data_size);
    }

    vec->size++;
}


void genVec_push_move(genVec* vec, u8** data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(!*data, "*data is null");

    // Check if we need to allocate or grow
    MAYBE_GROW(vec);

    if (vec->move_fn) {
        vec->move_fn(GET_PTR(vec, vec->size), data);
    } else {
        // copy the pointer to resource
        memcpy(GET_PTR(vec, vec->size), *data, vec->data_size);
        *data = NULL; // now arr owns the resource
    }

    vec->size++;
}

// pop can't be a move operation (array is contiguos)
void genVec_pop(genVec* vec, u8* popped)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_WARN_RET(vec->size == 0, , "vec is empty");

    u8* last_elm = GET_PTR(vec, vec->size - 1);

    if (popped) {
        if (vec->copy_fn) {
            vec->copy_fn(popped, last_elm);
        } else {
            memcpy(popped, last_elm, vec->data_size);
        }
    }

    if (vec->del_fn) { // del func frees the resources owned by last_elm, but not ptr
        vec->del_fn(last_elm);
    }

    vec->size--; // set for re-write

    MAYBE_SHRINK(vec);
}

void genVec_get(const genVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
    CHECK_FATAL(!out, "need a valid out variable to get the element");

    if (vec->copy_fn) {
        vec->copy_fn(out, GET_PTR(vec, i));
    } else {
        memcpy(out, GET_PTR(vec, i), vec->data_size);
    }
}

const u8* genVec_get_ptr(const genVec* vec, u64 i)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    return GET_PTR(vec, i);
}

u8* genVec_get_ptr_mut(const genVec* vec, u64 i)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    return GET_PTR(vec, i);
}

void genVec_for_each(genVec* vec, void (*for_each)(u8* elm))
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!for_each, "for_each function is null");

    for (u64 i = 0; i < vec->size; i++) {
        for_each(GET_PTR(vec, i));
    }
}

void genVec_insert(genVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    if (i == vec->size) {
        genVec_push(vec, data);
        return;
    }

    MAYBE_GROW(vec);

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;
    // the place where we want to insert
    u8* src = GET_PTR(vec, i);

    // Shift elements right by one unit
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // Use memmove for overlapping regions

    //src pos is now free to insert (it's data copied to next location)
    if (vec->copy_fn) {
        vec->copy_fn(src, data);
    } else {
        memcpy(src, data, vec->data_size);
    }

    vec->size++;
}

void genVec_insert_move(genVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(!*data, "*data is null");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    if (i == vec->size) {
        genVec_push_move(vec, data);
        return;
    }

    MAYBE_GROW(vec);

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;
    // the place where we want to insert
    u8* src = GET_PTR(vec, i);

    // Shift elements right by one unit
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // Use memmove for overlapping regions


    if (vec->move_fn) {
        vec->move_fn(src, data);
    } else {
        memcpy(src, *data, vec->data_size);
        *data = NULL;
    }

    vec->size++;
}


void genVec_insert_multi(genVec* vec, u64 i, const u8* data, u64 num_data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(num_data == 0, "num_data can't be 0");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;

    vec->size += num_data; // no of new elements in chunk

    genVec_reserve(vec, vec->size); // reserve with new size

    // the place where we want to insert
    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        // Shift elements right by num_data units to right
        u8* dest = GET_PTR(vec, i + num_data);

        memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // using memmove for overlapping regions
    }

    //src pos is now free to insert (it's data copied to next location)
    if (vec->copy_fn) {
        for (u64 j = 0; j < num_data; j++) {
            vec->copy_fn(GET_PTR(vec, j + i), (data + (size_t)(j * vec->data_size)));
        }
    } else {
        memcpy(src, data, GET_SCALED(vec, num_data));
    }
}

void genVec_insert_multi_move(genVec* vec, u64 i, u8** data, u64 num_data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(!*data, "*data is null");
    CHECK_FATAL(num_data == 0, "num_data can't be 0");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;

    vec->size += num_data; // no of new elements in chunk

    genVec_reserve(vec, vec->size); // reserve with new size

    // the place where we want to insert
    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        // Shift elements right by num_data units to right
        u8* dest = GET_PTR(vec, i + num_data);

        memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // using memmove for overlapping regions
    }

    //src pos is now free to insert (it's data copied to next location)
    // Move entire contiguous block at once
    memcpy(src, *data, GET_SCALED(vec, num_data));
    *data = NULL; // Transfer ownership
}

void genVec_remove(genVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (out) {
        if (vec->copy_fn) {
            vec->copy_fn(out, GET_PTR(vec, i));
        } else {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        }
    }

    if (vec->del_fn) {
        vec->del_fn(GET_PTR(vec, i));
    }
    // Calculate the number of elements to shift
    u64 elements_to_shift = vec->size - i - 1;

    if (elements_to_shift > 0) {
        // Shift elements left to overwrite the deleted element
        u8* dest = GET_PTR(vec, i);
        u8* src  = GET_PTR(vec, i + 1);

        memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // Use memmove for overlapping regions
    }

    vec->size--;

    MAYBE_SHRINK(vec);
}


void genVec_remove_range(genVec* vec, u64 l, u64 r)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(l >= vec->size, "index out of range");
    CHECK_FATAL(l > r, "invalid range");

    if (r >= vec->size) {
        r = vec->size - 1;
    }

    if (vec->del_fn) {
        for (u64 i = l; i <= r; i++) {
            u8* elm = GET_PTR(vec, i);
            vec->del_fn(elm);
        }
    }

    u64 elms_to_shift = vec->size - (r + 1);

    // move from r + 1 to l
    u8* dest = GET_PTR(vec, l);
    u8* src  = GET_PTR(vec, r + 1);
    memmove(dest, src, GET_SCALED(vec, elms_to_shift)); // Use memmove for overlapping regions

    vec->size -= (r - l + 1);

    MAYBE_SHRINK(vec);
}


void genVec_replace(genVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
    CHECK_FATAL(!data, "data is null");

    u8* to_replace = GET_PTR(vec, i);

    if (vec->del_fn) {
        vec->del_fn(to_replace);
    }

    if (vec->copy_fn) {
        vec->copy_fn(to_replace, data);
    } else {
        memcpy(to_replace, data, vec->data_size);
    }
}


void genVec_replace_move(genVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
    CHECK_FATAL(!data, "need a valid data variable");
    CHECK_FATAL(!*data, "need a valid *data variable");

    u8* to_replace = GET_PTR(vec, i);

    if (vec->del_fn) {
        vec->del_fn(to_replace);
    }

    if (vec->move_fn) {
        vec->move_fn(to_replace, data);
    } else {
        memcpy(to_replace, *data, vec->data_size);
        *data = NULL;
    }
}


const u8* genVec_front(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(vec->size == 0, "vec is empty");

    return (const u8*)vec->data;
}


const u8* genVec_back(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(vec->size == 0, "vec is empty");

    return GET_PTR(vec, vec->size - 1);
}


void genVec_print(const genVec* vec, print_fn print_fn)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!print_fn, "print func is null");

    printf("[ ");
    for (u64 i = 0; i < vec->size; i++) {
        print_fn(GET_PTR(vec, i));
        putchar(' ');
    }
    putchar(']');
}


void genVec_copy(genVec* dest, const genVec* src)
{
    CHECK_FATAL(!dest, "dest is null");
    CHECK_FATAL(!src, "src is null");

    if (dest == src) {
        return;
    }

    // if data ptr is null, no op
    genVec_destroy_stk(dest);

    // copy all fields
    memcpy(dest, src, sizeof(genVec));

    // malloc data ptr
    dest->data = malloc(GET_SCALED(src, src->capacity));

    // Copy elements
    if (src->copy_fn) {
        for (u64 i = 0; i < src->size; i++) {
            src->copy_fn(GET_PTR(dest, i), GET_PTR(src, i));
        }
    } else {
        memcpy(dest->data, src->data, GET_SCALED(src, src->size));
    }
}


void genVec_move(genVec* dest, genVec** src)
{
    CHECK_FATAL(!src, "src is null");
    CHECK_FATAL(!*src, "*src is null");
    CHECK_FATAL(!dest, "dest is null");
    // CHECK_FATAL((*src)->svo, "can't move a SVO (stack) vec");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    // Transfer all fields from src to dest
    memcpy(dest, *src, sizeof(genVec));

    // Null out src's data pointer so it doesn't get freed
    (*src)->data = NULL;

    // Free src if it was-allocated
    // This only frees the genVec struct itself, not the data
    // (which was transferred to dest)
    free(*src);
    *src = NULL;
}


void genVec_grow(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    u64 new_cap;
    if (vec->capacity < GENVEC_MIN_CAPACITY) {
        new_cap = vec->capacity + 1;
    } else {
        new_cap = (u64)((float)vec->capacity * GENVEC_GROWTH);
        if (new_cap <= vec->capacity) { // Ensure at least +1 growth
            new_cap = vec->capacity + 1;
        }
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, new_cap));
    CHECK_FATAL(!new_data, "realloc failed");

    vec->data     = new_data;
    vec->capacity = new_cap;
}


void genVec_shrink(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    u64 reduced_cap = (u64)((float)vec->capacity * GENVEC_SHRINK_BY);
    if (reduced_cap < vec->size || reduced_cap == 0) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, reduced_cap));
    if (!new_data) {
        CHECK_WARN_RET(1, , "data realloc failed");
        return; // Keep original allocation
    }

    vec->data     = new_data;
    vec->capacity = reduced_cap;
}

#endif /* WC_GEN_VECTOR_IMPL */

/* ===== String.c ===== */
#ifndef WC_STRING_IMPL
#define WC_STRING_IMPL

#include <string.h>




// private func
u64 cstr_len(const char* cstr);



String* string_create(void)
{
    return (String*)genVec_init(0, sizeof(char), NULL, NULL, NULL);
}


void string_create_stk(String* str, const char* cstr)
{
    // the difference is that we dont use string_create(), so str is not initilised
    CHECK_FATAL(!str, "str is null");

    u64 len = 0;
    if (cstr) {
        len = cstr_len(cstr);
    }

    genVec_init_stk(len, sizeof(char), NULL, NULL, NULL, str);
    
    if (len != 0) {
        genVec_insert_multi(str, 0, (const u8*)cstr, len);
    }
}


String* string_from_cstr(const char* cstr)
{
    String* str = malloc(sizeof(String));
    CHECK_FATAL(!str, "str malloc failed");

    string_create_stk(str, cstr);
    return str;
}


String* string_from_string(const String* other)
{
    CHECK_FATAL(!other, "other str is null");

    String* str = malloc(sizeof(String));
    CHECK_FATAL(!str, "str malloc failed");

    genVec_init_stk(other->size, sizeof(char), NULL, NULL, NULL, str);

    if (other->size != 0) {
        genVec_insert_multi(str, 0, other->data, other->size);
    }

    return str;
}


void string_reserve(String* str, u64 capacity)
{
    genVec_reserve(str, capacity);
}


void string_reserve_char(String* str, u64 capacity, char c)
{
    genVec_reserve_val(str, capacity, cast(c));
}


void string_destroy(String* str)
{
    string_destroy_stk(str);
    free(str);
}

void string_destroy_stk(String* str)
{
    genVec_destroy_stk(str);
}


void string_move(String* dest, String** src)
{
    CHECK_FATAL(!src, "src is null");
    CHECK_FATAL(!*src, "src is null");
    CHECK_FATAL(!dest, "dest is null");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    // no op if dest's data ptr is null (like stack inited)
    string_destroy_stk(dest);

    // copy fields (including data ptr)
    memcpy(dest, *src, sizeof(String));

    (*src)->data = NULL;
    free(*src);
    *src = NULL;
}


void string_copy(String* dest, const String* src)
{
    CHECK_FATAL(!src, "src is null");
    CHECK_FATAL(!dest, "dest is null");

    if (src == dest) {
        return;
    }

    // no op if data ptr is null
    string_destroy_stk(dest);

    // copy all fields (data ptr too)
    memcpy(dest, src, sizeof(String));

    // malloc new data ptr
    dest->data = malloc(src->capacity);

    // copy all data (arr of chars)
    memcpy(dest->data, src->data, src->size);
}


char* string_to_cstr(const String* str)
{
    CHECK_FATAL(!str, "str is null");

    if (str->size == 0) {
        char* empty = malloc(1);
        CHECK_FATAL(!empty, "malloc failed");
        empty[0] = '\0';
        return empty;
    }

    char* out = malloc(str->size + 1); // + 1 for null term
    CHECK_FATAL(!out, "out str malloc failed");

    memcpy(out, str->data, str->size);

    out[str->size] = '\0'; // add null term

    return out;
}


char* string_data_ptr(const String* str)
{
    CHECK_FATAL(!str, "str is null");

    if (str->size == 0) {
        return NULL;
    }

    return (char*)str->data;
}


void string_append_cstr(String* str, const char* cstr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    genVec_insert_multi(str, str->size, (const u8*)cstr, len);
}


void string_append_string(String* str, const String* other)
{
    CHECK_FATAL(!str, "str is empty");
    CHECK_FATAL(!other, "other is empty");

    if (other->size == 0) {
        return;
    }

    // direct insertion from other's buffer
    genVec_insert_multi(str, str->size, other->data, other->size);
}

// append and consume source string
void string_append_string_move(String* str, String** other)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!other, "other ptr is null");
    CHECK_FATAL(!*other, "*other is null");

    if ((*other)->size > 0) {
        genVec_insert_multi(str, str->size, (*other)->data, (*other)->size);
    }

    string_destroy(*other);
    *other = NULL;
}


void string_append_char(String* str, char c)
{
    CHECK_FATAL(!str, "str is null");
    genVec_push(str, cast(c));
}


char string_pop_char(String* str)
{
    CHECK_FATAL(!str, "str is null");

    char c;
    genVec_pop(str, cast(c));

    return c;
}


void string_insert_char(String* str, u64 i, char c)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(i > str->size, "index out of bounds");

    genVec_insert(str, i, cast(c));
}


void string_insert_cstr(String* str, u64 i, const char* cstr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");
    CHECK_FATAL(i > str->size, "index out of bounds");

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    genVec_insert_multi(str, i, castptr(cstr), len);
}


void string_insert_string(String* str, u64 i, const String* other)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!other, "other is null");
    CHECK_FATAL(i > str->size, "index out of bounds");

    if (other->size == 0) {
        return;
    }

    // direct insertion
    genVec_insert_multi(str, i, other->data, other->size);
}


void string_remove_char(String* str, u64 i)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(i >= str->size, "index out of bounds");

    genVec_remove(str, i, NULL);
}


void string_remove_range(String* str, u64 l, u64 r)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(l >= str->size, "index out of bounds");
    CHECK_FATAL(l > r, "invalid range");

    genVec_remove_range(str, l, r);
}


void string_clear(String* str)
{
    CHECK_FATAL(!str, "str is null");
    genVec_clear(str);
}


char string_char_at(const String* str, u64 i)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(i >= str->size, "index out of bounds");

    return ((char*)str->data)[i];
}


void string_set_char(String* str, u64 i, char c)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(i >= str->size, "index out of bounds");

    ((char*)str->data)[i] = c;
}


int string_compare(const String* str1, const String* str2)
{
    CHECK_FATAL(!str1, "str1 is null");
    CHECK_FATAL(!str2, "str2 is null");

    u64 min_len = str1->size < str2->size ? str1->size : str2->size;

    // Compare byte by byte
    int cmp = memcmp(str1->data, str2->data, min_len);

    if (cmp != 0) {
        return cmp;
    }

    // If equal so far, shorter string is "less"
    if (str1->size < str2->size) {
        return -1;
    }
    if (str1->size > str2->size) {
        return 1;
    }

    return 0;
}


b8 string_equals(const String* str1, const String* str2)
{
    return string_compare(str1, str2) == 0;
}


b8 string_equals_cstr(const String* str, const char* cstr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

    u64 len = cstr_len(cstr);

    // Different lengths = not equal
    if (str->size != len) {
        return false;
    }
    // Both empty
    if (len == 0) {
        return true;
    }

    return memcmp(str->data, cstr, len) == 0;
}


u64 string_find_char(const String* str, char c)
{
    CHECK_FATAL(!str, "str is null");

    for (u64 i = 0; i < str->size; i++) {
        if (((char*)str->data)[i] == c) {
            return i;
        }
    }

    return (u64)-1; // Not found
}


u64 string_find_cstr(const String* str, const char* substr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!substr, "substr is null");

    u64 len = cstr_len(substr);

    // Empty substring is found at index 0
    if (len == 0) {
        return 0;
    }

    if (len > str->size) {
        return (u64)-1;
    }

    for (u64 i = 0; i <= str->size - len; i++) {
        if (memcmp(str->data + i, substr, len) == 0) {
            return i;
        }
    }

    return (u64)-1;
}


String* string_substr(const String* str, u64 start, u64 length)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(start >= str->size, "index out of bounds");

    String* result = string_create();

    u64 end     = start + length;
    u64 str_len = string_len(str);
    if (end > str_len) {
        end = str_len;
    }

    u64 actual_len = end - start;

    if (actual_len > 0) { // Insert substring all at once
        const char* csrc = string_data_ptr(str) + start;
        genVec_insert_multi(result, 0, (const u8*)csrc, actual_len);
    }

    return result;
}


void string_print(const String* str)
{
    CHECK_FATAL(!str, "str is null");

    putchar('\"');
    for (u64 i = 0; i < str->size; i++) {
        putchar(((char*)str->data)[i]);
    }
    putchar('\"');
}


u64 cstr_len(const char* cstr)
{
    u64 len = 0;
    u64 i   = 0;

    while (cstr[i++] != '\0') {
        len++;
    }

    return len;
}

#endif /* WC_STRING_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_STRING_H */
