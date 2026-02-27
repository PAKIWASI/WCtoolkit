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
 *   genVec* vec = genVec_init(8, sizeof(String), &string_ops);
 *
 * For POD types (int, float, flat structs) pass NULL for ops:
 *   genVec* vec = genVec_init(8, sizeof(int), NULL);
 */


// genVec growth/shrink settings (user can change)

#ifndef GENVEC_GROWTH
    #define GENVEC_GROWTH 1.5F      // vec capacity multiplier
#endif
#ifndef GENVEC_SHRINK_AT
    #define GENVEC_SHRINK_AT 0.25F  // % filled to shrink at (25% filled)
#endif
#ifndef GENVEC_SHRINK_BY
    #define GENVEC_SHRINK_BY 0.5F   // capacity divisor (half)
#endif


// Vtable: one instance shared across all vectors of the same type.
// Pass NULL for any callback not needed.
// For POD types, pass NULL for the whole ops pointer.
typedef struct {
    copy_fn   copy_fn; // Deep copy function for owned resources (or NULL)
    move_fn   move_fn; // Transfer ownership and null original (or NULL)
    delete_fn del_fn;  // Cleanup function for owned resources (or NULL)
} genVec_ops;


// generic vector container
typedef struct {
    u8* data; // pointer to generic data

    u64 size;      // Number of elements currently in vector
    u64 capacity;  // Total allocated capacity (in elements)
    u32 data_size; // Size of each element in bytes

    // Pointer to shared type-ops vtable (or NULL for POD types)
    const genVec_ops* ops;
} genVec;

// sizeof(genVec) == 48


// Convenience: access ops callbacks safely
#define VEC_COPY_FN(vec) ((vec)->ops ? (vec)->ops->copy_fn : NULL)
#define VEC_MOVE_FN(vec) ((vec)->ops ? (vec)->ops->move_fn : NULL)
#define VEC_DEL_FN(vec)  ((vec)->ops ? (vec)->ops->del_fn  : NULL)



// Memory Management
// ===========================

// Initialize vector with capacity n.
// ops: pointer to a shared genVec_ops vtable, or NULL for POD types.
genVec* genVec_init(u64 n, u32 data_size, const genVec_ops* ops);

// Initialize vector on stack (struct on stack, data on heap).
void genVec_init_stk(u64 n, u32 data_size, const genVec_ops* ops, genVec* vec);

// Initialize vector of size n with all elements set to val.
genVec* genVec_init_val(u64 n, const u8* val, u32 data_size, const genVec_ops* ops);

void genVec_init_val_stk(u64 n, const u8* val, u32 data_size, const genVec_ops* ops, genVec* vec);

// Vector COMPLETELY on stack (can't grow in size).
// You provide a stack-allocated array which becomes the internal array.
// WARNING: crashes when size == capacity and you try to push.
void genVec_init_arr(u64 n, u8* arr, u32 data_size, const genVec_ops* ops, genVec* vec);

// Destroy heap-allocated vector and clean up all elements.
void genVec_destroy(genVec* vec);

// Destroy stack-allocated vector (cleans up data, but not vec itself).
void genVec_destroy_stk(genVec* vec);

// Remove all elements (calls del_fn on each), keep capacity.
void genVec_clear(genVec* vec);

// Remove all elements and free memory, shrink capacity to 0.
void genVec_reset(genVec* vec);

// Ensure vector has at least new_capacity space (never shrinks).
void genVec_reserve(genVec* vec, u64 new_capacity);

// Grow to new_capacity and fill new slots with val.
void genVec_reserve_val(genVec* vec, u64 new_capacity, const u8* val);

// Shrink vector to its size (reallocates).
void genVec_shrink_to_fit(genVec* vec);



// Operations
// ===========================

// Append element to end (makes deep copy if copy_fn provided).
void genVec_push(genVec* vec, const u8* data);

// Append element to end, transfer ownership (nulls original pointer).
void genVec_push_move(genVec* vec, u8** data);

// Remove element from end. If popped is provided, copies element before deletion.
// Note: del_fn is called regardless to clean up owned resources.
void genVec_pop(genVec* vec, u8* popped);

// Copy element at index i into out buffer.
void genVec_get(const genVec* vec, u64 i, u8* out);

// Get pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
const u8* genVec_get_ptr(const genVec* vec, u64 i);

// Get MUTABLE pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
u8* genVec_get_ptr_mut(const genVec* vec, u64 i);

// Replace element at index i with data (cleans up old element).
void genVec_replace(genVec* vec, u64 i, const u8* data);

// Replace element at index i, transfer ownership (cleans up old element).
void genVec_replace_move(genVec* vec, u64 i, u8** data);

// Insert element at index i, shifting elements right.
void genVec_insert(genVec* vec, u64 i, const u8* data);

// Insert element at index i with ownership transfer, shifting elements right.
void genVec_insert_move(genVec* vec, u64 i, u8** data);

// Insert num_data elements from data array into vec at index i.
void genVec_insert_multi(genVec* vec, u64 i, const u8* data, u64 num_data);

// Insert (move) num_data elements from data starting at index i.
void genVec_insert_multi_move(genVec* vec, u64 i, u8** data, u64 num_data);

// Remove element at index i, optionally copy to out, shift elements left.
void genVec_remove(genVec* vec, u64 i, u8* out);

// Remove elements in range [l, r] inclusive.
void genVec_remove_range(genVec* vec, u64 l, u64 r);

// Get pointer to first element.
const u8* genVec_front(const genVec* vec);

// Get pointer to last element.
const u8* genVec_back(const genVec* vec);


// Utility
// ===========================

// Print all elements using provided print function.
void genVec_print(const genVec* vec, print_fn fn);

// Deep copy src vector into dest.
// Note: cleans up dest (if already inited).
void genVec_copy(genVec* dest, const genVec* src);

// Transfer ownership from src to dest.
// Note: src must be heap-allocated.
void genVec_move(genVec* dest, genVec** src);


// Get number of elements in vector.
static inline u64 genVec_size(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    return vec->size;
}

// Get total capacity of vector.
static inline u64 genVec_capacity(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    return vec->capacity;
}

// Check if vector is empty.
static inline b8 genVec_empty(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    return vec->size == 0;
}



#endif // GEN_VECTOR_H
