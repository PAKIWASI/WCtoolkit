#ifndef WC_BIT_VECTOR_H
#define WC_BIT_VECTOR_H

/*
 * bit_vector_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "bit_vector_single.h"
 *
 * All other files just:
 *     #include "bit_vector_single.h"
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

/* ===== bit_vector.h ===== */
#ifndef WC_BIT_VECTOR_H
#define WC_BIT_VECTOR_H

typedef struct {
    genVec* arr;
    u64 size;        // no of set bits
} bitVec;



bitVec* bitVec_create(void);
void bitVec_destroy(bitVec* bvec);


void bitVec_set(bitVec* bvec, u64 i);
void bitVec_clear(bitVec* bvec, u64 i);
u8 bitVec_test(bitVec* bvec, u64 i);
void bitVec_toggle(bitVec* bvec, u64 i);


void bitVec_push(bitVec* bvec);
void bitVec_pop(bitVec* bvec);


void bitVec_print(bitVec* bvec, u64 byteI);


// get the no of BITS in the vector
static inline u64 bitVec_size_bits(bitVec* bvec) {
    return bvec->size;
}

// get the no of BYTES in the vector
static inline u64 bitVec_size_bytes(bitVec* bvec) {
    return genVec_size(bvec->arr);
}

#endif /* WC_BIT_VECTOR_H */

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

/* ===== bit_vector.c ===== */
#ifndef WC_BIT_VECTOR_IMPL
#define WC_BIT_VECTOR_IMPL

bitVec* bitVec_create(void)
{
    bitVec* bvec = malloc(sizeof(bitVec));
    CHECK_FATAL(!bvec, "bvec init failed");

    bvec->arr = genVec_init(0, sizeof(u8), NULL, NULL, NULL);

    bvec->size = 0;

    return bvec;
}

void bitVec_destroy(bitVec* bvec)
{
    CHECK_FATAL(!bvec, "bvec is null");

    genVec_destroy(bvec->arr);

    free(bvec);
}

// Set bit i to 1
void bitVec_set(bitVec* bvec, u64 i)
{
    CHECK_FATAL(!bvec, "bvec is null");
    
    u64 byte_index = i / 8; // which byte (elm) 
    u64 bit_index = i % 8; // which bit in the byte
    
    // Ensure byte capacity
    while (byte_index >= bvec->arr->size) {
        u8 zero = 0;
        genVec_push(bvec->arr, &zero);
    }

    u8* byte = (u8*)genVec_get_ptr(bvec->arr, byte_index);
    *byte |= (1 << bit_index);  // Set the bit
    // we create a new 8 bit arr with left shift 
    // it has 1 at the pos we want to set
    // we or it with the arr so the 1 is set
    // we set the bits till the pos where we want 1
    // rest are 0, rhs of 1 is implicitly set, lhs of 1 is explici by <<

    if (i + 1 > bvec->size) { // bits upto ith are considered allocated
        bvec->size = i + 1;  // ith bit is 1 (set)
    }  
}

// Clear bit i (set to 0)
void bitVec_clear(bitVec* bvec, u64 i)
{
    CHECK_FATAL(!bvec, "bvec is null");

    CHECK_FATAL(i >= bvec->size, "index out of bounds");     

    u64 byte_index = i / 8;
    u64 bit_index = i % 8;

    u8* byte = (u8*)genVec_get_ptr(bvec->arr, byte_index);
    *byte &= ~(1 << bit_index);  // Clear the bit
    // we create a new 8 bit arr with left shift 
    // it has 0 at the pos we want to clear (the not puts 0 there and 1 everywhere else)
    // we and it with the arr so 0 is cleared
}

// Test bit i (returns 1 or 0)
u8 bitVec_test(bitVec* bvec, u64 i)
{
    CHECK_FATAL(!bvec, "bvec is null");
    
    CHECK_FATAL(i >= bvec->size, "index out of bounds");

    u64 byte_index = i / 8;
    u64 bit_index = i % 8;

    //u8* byte = (u8*)genVec_get_ptr(bvec->arr, byte_index); 
    return (*genVec_get_ptr(bvec->arr, byte_index) >> bit_index) & 1;  // copy of dereferenced byte data returned
     // create new arr, move needed bit to LSB
    //The `& 1` masks off everything except the LSB: // 1 = 00000001
}

// Toggle bit i
void bitVec_toggle(bitVec* bvec, u64 i)
{
    CHECK_FATAL(!bvec, "bvec is null");
    
    CHECK_FATAL(i >= bvec->size, "index out of bounds");

    u64 byte_index = i / 8;
    u64 bit_index = i % 8;

    u8* byte = (u8*)genVec_get_ptr(bvec->arr, byte_index);
    *byte ^= (1 << bit_index); // lvalue so byte is modified
    // xor with 1 toggles the bit 
    // while with 0 it does nothing
}


void bitVec_push(bitVec* bvec)
{
    bitVec_set(bvec, bvec->size); 
}


void bitVec_pop(bitVec* bvec)
{
    CHECK_FATAL(!bvec, "bvec is null");

    bvec->size--;
    if (bvec->size % 8 == 0) {
        genVec_pop(bvec->arr, NULL);
    }
}

void bitVec_print(bitVec *bvec, u64 byteI)
{
    CHECK_FATAL(!bvec, "bvec is null");

    CHECK_FATAL(byteI >= bvec->arr->size, "index out of bounds");
    
    u8 bits_to_print = 8;
    // If this is the last byte, only print the valid bits
    if (byteI == bvec->arr->size - 1) {
        u64 remaining = bvec->size % 8;
        bits_to_print = (remaining == 0) ? 8 : (u8)remaining;
    }

    for (u8 i = 0; i < bits_to_print; i++) {
        // we print from 0th bit to 7th bit (there are no lsb, msb)
        printf("%d", ((*genVec_get_ptr(bvec->arr, byteI)) >> i) & 1);// we lose data from right
    }
}

#endif /* WC_BIT_VECTOR_IMPL */

#endif /* WC_IMPLEMENTATION */

#endif /* WC_BIT_VECTOR_H */
