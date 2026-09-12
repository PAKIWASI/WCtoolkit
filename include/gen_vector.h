#ifndef GEN_VECTOR_H
#define GEN_VECTOR_H

#include "common.h"


/*          TLDR
 * genVec is a value-based generic vector.
 * Elements are stored inline and managed via user-supplied
 * copy/move/destructor callbacks.
 *
 * This avoids pointer ownership ambiguity and improves cache locality.
 *
 * Callbacks are grouped into a shared genVec_ops struct (vtable).
 * Define one static ops instance per type and share it across all
 * vectors of that type —  improves cache locality when many vectors of the same type exist.
 *
 * Example:
 *   static const genVec_ops string_ops = { str_copy, str_move, str_del };
 *   genVec* vec = genVec_create(8, sizeof(String), &string_ops);
 *
 * For POD types (int, float, flat structs) pass NULL for ops:
 *   genVec* vec = genVec_create(8, sizeof(int), NULL);
 */


// genVec growth settings

#ifndef GENVEC_GROWTH
#define GENVEC_GROWTH 1.5F // vec capacity multiplier
#endif


// generic vector container
typedef struct {
    u8* data; // pointer to generic data

    // Pointer to shared type-ops vtable (or NULL for POD types)
    const container_ops* ops;

    u64 size;      // Number of elements currently in vector
    u64 capacity;  // Total allocated capacity (in elements)
    u32 data_size; // Size of each element in bytes

    // Cache: 1 if ops==NULL (POD fast path). Wired by genVec_create* in 6-M.
    b8 is_pod;
} genVec;

// 8 8 8 8 4 '4'  = 40 bytes
_Static_assert(sizeof(genVec) == 40, "genVec layout drifted from expected 40 bytes");


// Convenience: access ops callbacks safely
#define VEC_COPY_FN(vec) ((vec)->ops ? (vec)->ops->copy_fn : NULL)
#define VEC_MOVE_FN(vec) ((vec)->ops ? (vec)->ops->move_fn : NULL)
#define VEC_DEL_FN(vec)  ((vec)->ops ? (vec)->ops->del_fn : NULL)



// Memory Management
// ===========================

// Initialize vector with capacity n.
// ops: pointer to a shared genVec_ops vtable, or NULL for POD types.
genVec* genVec_create(u64 n, u32 data_size, const container_ops* ops);

// Initialize vector on stack (struct on stack, data on heap).
void genVec_create_stk(u64 n, u32 data_size, const container_ops* ops, genVec* vec) __attribute__((nonnull(4)));

// Initialize vector of size n with all elements set to val.
genVec* genVec_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops) __attribute__((nonnull(2)));

void genVec_create_val_stk(u64 n, const u8* val, u32 data_size, const container_ops* ops, genVec* vec)
    __attribute__((nonnull(2, 5)));

genVec* genVec_create_arr(u64 n, u32 data_size, const container_ops* ops, u8* arr) __attribute__((nonnull(4)));

// Vector COMPLETELY on stack (can't grow in size).
// You provide a stack-allocated array which becomes the internal array.
// should only use if you need genVec operations on C array
// WARNING: crashes when size == capacity and you try to push.
void genVec_create_stk_arr(u64 n, u8* arr, u32 data_size, const container_ops* ops, genVec* vec)
    __attribute__((nonnull(2, 5)));

// Destroy heap-allocated vector and clean up all elements.
void genVec_destroy(genVec* vec) __attribute__((nonnull(1)));

// Destroy stack-allocated vector (cleans up data, but not vec itself).
void genVec_destroy_stk(genVec* vec) __attribute__((nonnull(1)));

// Remove all elements (calls del_fn on each), keep capacity.
void genVec_clear(genVec* vec) __attribute__((nonnull(1)));

// Remove all elements and free memory, shrink capacity to 0.
void genVec_reset(genVec* vec) __attribute__((nonnull(1)));

// Ensure vector has at least new_capacity space (never shrinks).
void genVec_reserve(genVec* vec, u64 new_capacity) __attribute__((nonnull(1)));

// Grow to new_capacity and fill new slots with val.
void genVec_reserve_val(genVec* vec, u64 new_capacity, const u8* val) __attribute__((nonnull(1, 3)));

// Shrink vector to its size (reallocates).
void genVec_shrink_to_fit(genVec* vec) __attribute__((nonnull(1)));



// Operations
// ===========================

// Append element to end (makes deep copy if copy_fn provided).
void genVec_push(genVec* vec, const u8* data) __attribute__((nonnull(1, 2)));

// Append element to end, transfer ownership (nulls original pointer).
void genVec_push_move(genVec* vec, u8** data) __attribute__((nonnull(1, 2)));

// Remove element from end. If popped is provided, copies element before deletion.
// Note: del_fn is called regardless to clean up owned resources.
void genVec_pop(genVec* vec, u8* popped) __attribute__((nonnull(1)));

// If order doesn't matter, O(1) deletion from middle
void genVec_swap_pop(genVec* vec, u64 i, u8* out) __attribute__((nonnull(1)));

// swap element at i with element at j
void genVec_swap(genVec* vec, u64 i, u64 j) __attribute__((nonnull(1)));

// Copy element at index i into out buffer.
void genVec_get(const genVec* vec, u64 i, u8* out) __attribute__((nonnull(1, 3)));

// Get pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
const u8* genVec_get_ptr(const genVec* vec, u64 i) __attribute__((nonnull(1)));

// Get MUTABLE pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
u8* genVec_get_ptr_mut(genVec* vec, u64 i) __attribute__((nonnull(1)));

// Replace element at index i with data (cleans up old element).
void genVec_replace(genVec* vec, u64 i, const u8* data) __attribute__((nonnull(1, 3)));

// Replace element at index i, transfer ownership (cleans up old element).
void genVec_replace_move(genVec* vec, u64 i, u8** data) __attribute__((nonnull(1, 3)));

// Insert element at index i, shifting elements right.
void genVec_insert(genVec* vec, u64 i, const u8* data) __attribute__((nonnull(1, 3)));

// Insert element at index i with ownership transfer, shifting elements right.
void genVec_insert_move(genVec* vec, u64 i, u8** data) __attribute__((nonnull(1, 3)));

// Insert num_data elements from data array into vec at index i.
void genVec_insert_multi(genVec* vec, u64 i, const u8* data, u64 num_data) __attribute__((nonnull(1, 3)));

// Insert (move) num_data elements from data starting at index i.
void genVec_insert_multi_move(genVec* vec, u64 i, u8** data, u64 num_data) __attribute__((nonnull(1, 3)));

// Remove element at index i, optionally copy to out, shift elements left.
void genVec_remove(genVec* vec, u64 i, u8* out) __attribute__((nonnull(1)));

// Remove elements in range [start, start + len)
void genVec_remove_range(genVec* vec, u64 start, u64 len) __attribute__((nonnull(1)));

// Get pointer to first element.
const u8* genVec_front(const genVec* vec) __attribute__((nonnull(1)));

// Get pointer to last element.
const u8* genVec_back(const genVec* vec) __attribute__((nonnull(1)));

// Search
// ===========================

// if cmp_fn = NULL, then use memcmp
u64 genVec_find(const genVec* vec, u8* elm, compare_fn cmp_fn) __attribute__((nonnull(1, 2)));

genVec* genVec_subarr(const genVec* vec, u64 start, u64 len) __attribute__((nonnull(1)));


// Utility
// ===========================

// Print all elements using provided print function.
void genVec_print(const genVec* vec, print_fn fn) __attribute__((nonnull(1, 2)));

// Deep copy src vector into dest.
// REQUIRES: dest must be uninitialized (or already destroyed/reset) before calling.
// This does NOT clean up any existing dest->data / elements — it overwrites
// dest's fields directly. Calling this on an already-populated dest leaks
// its old buffer and skips del_fn on its old elements.
void genVec_copy(genVec* dest, const genVec* src) __attribute__((nonnull(1, 2)));

// Transfer ownership from src to dest.
// Note: src must be heap-allocated.
void genVec_move(genVec* dest, genVec** src) __attribute__((nonnull(1, 2)));


// Get number of elements in vector.
static inline __attribute__((nonnull(1))) u64 genVec_size(const genVec* vec)
{
    return vec->size;
}

// Get total capacity of vector.
static inline __attribute__((nonnull(1))) u64 genVec_capacity(const genVec* vec)
{
    return vec->capacity;
}

// Check if vector is empty.
static inline __attribute__((nonnull(1))) b8 genVec_empty(const genVec* vec)
{
    return vec->size == 0;
}



#endif // GEN_VECTOR_H
