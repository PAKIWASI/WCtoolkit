#ifndef GEN_VECTOR_H
#define GEN_VECTOR_H

#include "common.h"


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


#endif // GEN_VECTOR_H
