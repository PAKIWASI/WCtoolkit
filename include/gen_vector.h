#ifndef GEN_VECTOR_H
#define GEN_VECTOR_H

#include "common.h"
#include "wc_allocator.h"


/*          TLDR
 * GenVec is a value-based generic vector.
 * Elements are stored inline and managed via user-supplied
 * copy/move/destructor callbacks (a shared wc_container_ops vtable).
 *
 * The vector is a VALUE: create returns it, destroy never frees the struct.
 * It stores the allocator it was created with and uses it for every
 * allocation, reallocation and free (plan 3.3).
 *
 *   GenVec v = GenVec_create(WC_LIBC, 8, sizeof(int), NULL);        // libc
 *   GenVec w = GenVec_create(Arena_allocator(&arena), 8, sizeof(int), NULL);
 *   GenVec_push(&v, &x);
 *   GenVec_destroy(&v);                                             // v is zeroed
 *
 * Moves take a pointer to the source ELEMENT and leave it zeroed:
 *   String s = ...;  GenVec_push_move(&v, &s);                 // s is zeroed
 *
 * Zero state: a zeroed GenVec (moved-from or destroyed) may only be destroyed
 * or re-created. Growing or inserting into it is a FATAL in every build (D4).
 */


// GenVec growth settings

#ifndef GENVEC_GROWTH
#define GENVEC_GROWTH 1.5F // vec capacity multiplier
#endif


// generic vector container
typedef struct {
    u8* data; // pointer to generic data

    // Pointer to shared type-ops vtable (or NULL for POD types)
    const wc_container_ops* ops;

    u64 size;     // Number of elements currently in vector
    u64 capacity; // Total allocated capacity (in elements)

    wc_allocator alloc; // every data allocation goes through this (16 B, by value)

    u32 data_size; // Size of each element in bytes (0 = zero state)

    // Cache: 1 if ops==NULL (POD fast path)
    b8 is_pod;
} GenVec;

// 8 8 8 8 16 4 1 '3'  = 56 bytes
_Static_assert(sizeof(GenVec) == 56, "GenVec layout drifted from expected 56 bytes");


// Convenience: access ops callbacks safely
#define VEC_COPY_FN(vec) ((vec)->ops ? (vec)->ops->copy_fn : NULL)
#define VEC_MOVE_FN(vec) ((vec)->ops ? (vec)->ops->move_fn : NULL)
#define VEC_DEL_FN(vec)  ((vec)->ops ? (vec)->ops->del_fn : NULL)



// Memory Management
// ===========================

// Vector with capacity n, storage from `alloc`.
// ops: pointer to a shared wc_container_ops vtable, or NULL for POD types.
GenVec GenVec_create(wc_allocator alloc, u64 n, u32 data_size, const wc_container_ops* ops)
    __attribute__((warn_unused_result));

// Vector of size n with every element a copy of val.
GenVec GenVec_create_val(wc_allocator alloc, u64 n, const void* val, u32 data_size, const wc_container_ops* ops)
    __attribute__((nonnull(3), warn_unused_result));

// Vector over caller-owned storage of n elements (size 0, capacity n).
// Uses wc_borrowed: it can never grow (growth is a FATAL) and destroy frees nothing.
GenVec GenVec_create_buf(void* buf, u64 n, u32 data_size, const wc_container_ops* ops)
    __attribute__((nonnull(1), warn_unused_result));

// Delete every element, free the storage through the stored allocator and
// zero the struct. Never frees the struct itself. Safe on a zeroed vector.
void GenVec_destroy(GenVec* vec) __attribute__((nonnull(1)));

// Remove all elements (calls del_fn on each), keep capacity.
void GenVec_clear(GenVec* vec) __attribute__((nonnull(1)));

// Remove all elements and free the storage; capacity 0, allocator kept.
void GenVec_reset(GenVec* vec) __attribute__((nonnull(1)));

// Ensure vector has at least new_capacity space (never shrinks).
void GenVec_reserve(GenVec* vec, u64 new_capacity) __attribute__((nonnull(1)));

// Grow to new_capacity and fill new slots with val.
void GenVec_reserve_val(GenVec* vec, u64 new_capacity, const void* val) __attribute__((nonnull(1, 3)));

// Shrink vector to its size (reallocates).
void GenVec_shrink_to_fit(GenVec* vec) __attribute__((nonnull(1)));



// Operations
// ===========================

// Append element to end (makes deep copy if copy_fn provided).
void GenVec_push(GenVec* vec, const void* data) __attribute__((nonnull(1, 2)));

// Append element to end, transfer ownership: *data is moved in and zeroed.
void GenVec_push_move(GenVec* vec, void* data) __attribute__((nonnull(1, 2)));

// Remove element from end. If popped is provided, copies element before deletion
// (owned resources of the copy come from the vector's allocator).
// Note: del_fn is called regardless to clean up owned resources.
void GenVec_pop(GenVec* vec, void* popped) __attribute__((nonnull(1)));

// If order doesn't matter, O(1) deletion from middle
void GenVec_swap_pop(GenVec* vec, u64 i, void* out) __attribute__((nonnull(1)));

// swap element at i with element at j
void GenVec_swap(GenVec* vec, u64 i, u64 j) __attribute__((nonnull(1)));

// Copy element at index i into out buffer.
void GenVec_get(const GenVec* vec, u64 i, void* out) __attribute__((nonnull(1, 3)));

// Get pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
const void* GenVec_get_ptr(const GenVec* vec, u64 i) __attribute__((nonnull(1)));

// Get MUTABLE pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
void* GenVec_get_ptr_mut(GenVec* vec, u64 i) __attribute__((nonnull(1)));

// UNCHECKED variants: same as above but with the bounds WC_ASSERT elided.
// Preconditions are NOT validated: caller must guarantee i < vec->size.
// Use on hot paths where the check is provably redundant (macros, internal loops).
const void* GenVec_get_ptr_unsafe(const GenVec* vec, u64 i) __attribute__((nonnull(1)));

static inline __attribute__((nonnull(1))) void* GenVec_get_ptr_mut_unsafe(GenVec* vec, u64 i)
{
    return (vec->data + (i * vec->data_size));
}

// Replace element at index i with data (cleans up old element).
void GenVec_replace(GenVec* vec, u64 i, const void* data) __attribute__((nonnull(1, 3)));

// Replace element at index i, transfer ownership (cleans up old element, zeroes *data).
void GenVec_replace_move(GenVec* vec, u64 i, void* data) __attribute__((nonnull(1, 3)));

// Insert element at index i, shifting elements right.
void GenVec_insert(GenVec* vec, u64 i, const void* data) __attribute__((nonnull(1, 3)));

// Insert element at index i with ownership transfer (zeroes *data), shifting elements right.
void GenVec_insert_move(GenVec* vec, u64 i, void* data) __attribute__((nonnull(1, 3)));

// Insert num_data elements from data array into vec at index i.
void GenVec_insert_multi(GenVec* vec, u64 i, const void* data, u64 num_data) __attribute__((nonnull(1, 3)));

// Insert (move) num_data contiguous elements from data at index i; the source range is zeroed.
void GenVec_insert_multi_move(GenVec* vec, u64 i, void* data, u64 num_data) __attribute__((nonnull(1, 3)));

// Remove element at index i, optionally copy to out, shift elements left.
void GenVec_remove(GenVec* vec, u64 i, void* out) __attribute__((nonnull(1)));

// Remove elements in range [start, start + len)
void GenVec_remove_range(GenVec* vec, u64 start, u64 len) __attribute__((nonnull(1)));

// Get pointer to first element.
const void* GenVec_front(const GenVec* vec) __attribute__((nonnull(1)));

// Get pointer to last element.
const void* GenVec_back(const GenVec* vec) __attribute__((nonnull(1)));

// Search
// ===========================

// if cmp_fn = NULL, then use memcmp
u64 GenVec_find(const GenVec* vec, void* elm, wc_compare_fn cmp_fn) __attribute__((nonnull(1, 2)));

// New vector (storage from `alloc`) holding deep copies of [start, start + len).
GenVec GenVec_subarr(const GenVec* vec, wc_allocator alloc, u64 start, u64 len)
    __attribute__((nonnull(1), warn_unused_result));


// Utility
// ===========================

// Print all elements using provided print function.
void GenVec_print(const GenVec* vec, wc_print_fn fn) __attribute__((nonnull(1, 2)));

// Deep copy of src whose storage (and every element's owned resources, via
// copy_fn) comes from `alloc`. Never inherits src's allocator (A7).
GenVec GenVec_copy(wc_allocator alloc, const GenVec* src) __attribute__((nonnull(2), warn_unused_result));

// Transfer everything from src to dest; src is left zeroed.
// dest must be uninitialised or destroyed (its old contents are overwritten, not freed).
void GenVec_move(GenVec* dest, GenVec* src) __attribute__((nonnull(1, 2)));


// Get number of elements in vector.
static inline __attribute__((nonnull(1))) u64 GenVec_size(const GenVec* vec)
{
    return vec->size;
}

// Get total capacity of vector.
static inline __attribute__((nonnull(1))) u64 GenVec_capacity(const GenVec* vec)
{
    return vec->capacity;
}

// Check if vector is empty
static inline __attribute__((nonnull(1))) b8 GenVec_empty(const GenVec* vec)
{
    return vec->size == 0;
}



#endif // GEN_VECTOR_H
