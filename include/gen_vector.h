#ifndef GEN_VECTOR_H
#define GEN_VECTOR_H

#include "common.h"
#include "wc_allocator.h"
#include "wc_comp.h"
#include <stdalign.h>
#include <stdio.h>

/*          TLDR
 * GenVec is a value-based generic vector.
 * Elements are stored inline and managed via user-supplied
 * copy/move/destructor callbacks (a shared wc_container_ops vtable).
 *
 * The vector is a VALUE: create returns it, destroy never frees the struct.
 * It stores the allocator it was created with and uses it for every
 * allocation, reallocation and free
 *
 *   GenVec v = GenVec_create(WC_LIBC, 8, sizeof(int), NULL);        // libc
 *   GenVec w = GenVec_create(Arena_allocator(&arena), 8, sizeof(int), NULL);
 *   GenVec_push(&v, &x);
 *   GenVec_destroy(&v);                                             // v is zeroed
 *
 * Moves take a pointer to the source ELEMENT and leave it zeroed:
 *   String s = ...;  GenVec_push_move(&v, &s);                 // s is zeroed
 *
 * Taking an element out (pop, remove, swap_pop) MOVES it into `out`: no copy,
 * no allocation, and the vector forgets the slot (memory rule B7). The value
 * keeps the vector's allocator (B9). With out == NULL the element is deleted.
 *
 * Input aliasing (D4): the element passed to push/insert/replace must not live
 * inside this vector's own buffer (growth would free it before the copy).
 * Debug builds check this.
 *
 * Zero state: a zeroed GenVec (moved-from or destroyed) may only be destroyed
 * or re-created. Growing or inserting into it is a FATAL in every build
 */


// GenVec growth settings

#ifndef GENVEC_MIN_CAPACITY
#define GENVEC_MIN_CAPACITY 8
#endif

// TODO: genvec growth as a compile time option?


// generic vector container
typedef struct {
    u8* data; // pointer to generic data

    const wc_container_ops* ops;   // Pointer to shared type-ops vtable (or NULL for POD types)
    const wc_allocator*     alloc; // every data allocation goes through this

    u64 size;      // Number of elements currently in vector
    u64 capacity;  // Total allocated capacity (in elements)
    u32 data_size; // Size of each element in bytes (0 = zero state)

    // Cache: 1 if ops==NULL (POD fast path)
    bool is_pod;
} GenVec;



// 8 8 8 8 8 4 1 '3'  = 48 bytes
_Static_assert(sizeof(GenVec) == 48, "GenVec layout drifted from expected 48 bytes");


// Convenience: access ops callbacks safely
#define VEC_COPY_FN(vec) ((vec)->ops ? (vec)->ops->copy_fn : NULL)
#define VEC_DEL_FN(vec)  ((vec)->ops ? (vec)->ops->del_fn : NULL)



// Memory Management
// ===========================

// Vector with capacity n, storage from `alloc`.
// ops: pointer to a shared wc_container_ops vtable, or NULL for POD types.
wc_nodiscard wc_nonull(1) GenVec GenVec_create(const wc_allocator* alloc, u64 n, u32 data_size, const wc_container_ops* ops);

// Vector of size n with every element a copy of val.
wc_nodiscard wc_nonull(1, 3) GenVec GenVec_create_val(const wc_allocator* alloc, u64 n, const void* val, u32 data_size,
                                                      const wc_container_ops* ops);

// Vector over caller-owned storage of n elements (size 0, capacity n).
// Uses WC_BORROWED: it can never grow (growth is a FATAL) and destroy frees nothing
wc_nodiscard wc_nonull(1) GenVec GenVec_create_buf(void* buf, u64 n, u32 data_size, const wc_container_ops* ops);

// Delete every element, free the storage through the stored allocator and
// zero the struct. Never frees the struct itself. Safe on a zeroed vector.
wc_nonull(1) void GenVec_destroy(GenVec* vec);

// Remove all elements (calls del_fn on each), keep capacity.
wc_nonull(1) void GenVec_clear(GenVec* vec);

// Remove all elements and free the storage; capacity 0, allocator kept.
wc_nonull(1) void GenVec_reset(GenVec* vec);

// Ensure vector has at least new_capacity space (never shrinks).
wc_nonull(1) void GenVec_reserve(GenVec* vec, u64 new_capacity);

// Grow to new_capacity and fill new slots with val.
wc_nonull(1, 3) void GenVec_reserve_val(GenVec* vec, u64 new_capacity, const void* val);

// Shrink vector to its size (reallocates).
wc_nonull(1) void GenVec_shrink_to_fit(GenVec* vec);



// Operations
// ===========================

// Append element to end (makes deep copy if copy_fn provided).
wc_nonull(1, 2) void GenVec_push(GenVec* vec, const void* data);

// Append element to end, transfer ownership: *data is moved in and zeroed.
wc_nonull(1, 2) void GenVec_push_move(GenVec* vec, void* data);

// Remove the last element. popped != NULL: the element is MOVED into it and the
// caller owns it. popped == NULL: the element is deleted.
wc_nonull(1) void GenVec_pop(GenVec* vec, void* popped);

// O(1) removal from the middle when order doesn't matter: element i is moved
// into out (or deleted if out == NULL), the last element takes its place.
wc_nonull(1) void GenVec_swap_pop(GenVec* vec, u64 i, void* out);

// Swap elements i and j in place. Never allocates.
wc_nonull(1) void GenVec_swap(GenVec* vec, u64 i, u64 j);

// Deep COPY of element i into out (the vector keeps its own).
wc_nonull(1, 3) void GenVec_get(const GenVec* vec, u64 i, void* out);

// Get pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
wc_nodiscard wc_nonull(1) const void* GenVec_get_ptr(const GenVec* vec, u64 i);

// Get MUTABLE pointer to element at index i.
// Note: Pointer invalidated by push/insert/remove operations.
wc_nodiscard wc_nonull(1) void* GenVec_get_ptr_mut(GenVec* vec, u64 i);

// UNCHECKED variants: same as above but with the bounds WC_ASSERT elided, even in
// Debug. Preconditions are NOT validated: caller must guarantee i < vec->size.
// Use on hot paths where the check is provably redundant (macros, internal loops).
wc_nodiscard wc_nonull(1) static wc_always_inline const void* GenVec_get_ptr_unsafe(const GenVec* vec, u64 i)
{
    return (vec->data + (i * vec->data_size));
}

wc_nodiscard wc_nonull(1) static wc_always_inline void* GenVec_get_ptr_mut_unsafe(GenVec* vec, u64 i)
{
    return (vec->data + (i * vec->data_size));
}

// Replace element at index i with data (cleans up old element).
wc_nonull(1, 3) void GenVec_replace(GenVec* vec, u64 i, const void* data);

// Replace element at index i, transfer ownership (cleans up old element, zeroes *data).
wc_nonull(1, 3) void GenVec_replace_move(GenVec* vec, u64 i, void* data);

// Insert element at index i, shifting elements right.
wc_nonull(1, 3) void GenVec_insert(GenVec* vec, u64 i, const void* data);

// Insert element at index i with ownership transfer (zeroes *data), shifting elements right.
wc_nonull(1, 3) void GenVec_insert_move(GenVec* vec, u64 i, void* data);

// Insert num_data elements from data array into vec at index i.
wc_nonull(1, 3) void GenVec_insert_multi(GenVec* vec, u64 i, const void* data, u64 num_data);

// Insert (move) num_data contiguous elements from data at index i; the source range is zeroed.
wc_nonull(1, 3) void GenVec_insert_multi_move(GenVec* vec, u64 i, void* data, u64 num_data);

// Remove element at index i, shift elements left. out != NULL: the element is
// MOVED into it. out == NULL: the element is deleted.
wc_nonull(1) void GenVec_remove(GenVec* vec, u64 i, void* out);

// Remove elements in range [start, start + len)
wc_nonull(1) void GenVec_remove_range(GenVec* vec, u64 start, u64 len);

// Get pointer to first element.
wc_nodiscard wc_nonull(1) const void* GenVec_front(const GenVec* vec);

// Get pointer to last element.
wc_nodiscard wc_nonull(1) const void* GenVec_back(const GenVec* vec);

// Search
// ===========================

// if cmp_fn = NULL, then use memcmp
wc_nodiscard wc_nonull(1, 2) u64 GenVec_find(const GenVec* vec, void* elm, wc_compare_fn cmp_fn);

// New vector (storage from `alloc`) holding deep copies of [start, start + len).
wc_nodiscard wc_nonull(1, 2) GenVec GenVec_subarr(const GenVec* vec, const wc_allocator* alloc, u64 start, u64 len);


// Utility
// ===========================

// Print all elements using provided print function.
wc_nonull(1, 2) void GenVec_print(const GenVec* vec, wc_print_fn fn);

// Deep copy of src whose storage (and every element's owned resources, via
// copy_fn) comes from `alloc`. Never inherits src's allocator (A14).
// Capacity is max(size, GENVEC_MIN_CAPACITY), or 0 for an empty src: a copy
// does not carry the source's slack (F5).
wc_nodiscard wc_nonull(1, 2) GenVec GenVec_copy(const wc_allocator* alloc, const GenVec* src);

// Transfer everything from src to dest; src is left zeroed.
// dest must be uninitialised or destroyed (its old contents are overwritten, not freed).
wc_nonull(1, 2) void GenVec_move(GenVec* dest, GenVec* src);


// Get number of elements in vector.
wc_nodiscard wc_nonull(1) static wc_always_inline u64 GenVec_size(const GenVec* vec)
{
    return vec->size;
}

// Get total capacity of vector.
wc_nodiscard wc_nonull(1) static wc_always_inline u64 GenVec_capacity(const GenVec* vec)
{
    return vec->capacity;
}

// Check if vector is empty
wc_nodiscard wc_nonull(1) static wc_always_inline bool GenVec_empty(const GenVec* vec)
{
    return vec->size == 0;
}


// Helpers


/*
 * 3.  GENVEC BY VALUE  (vec of vecs)
*/

static inline void vec_copy(const wc_allocator* dst, void* dest, const void* src)
{
    *(GenVec*)dest = GenVec_copy(dst, (const GenVec*)src); // inner vec follows the outer container
}

static inline void vec_del(void* elm)
{
    GenVec_destroy((GenVec*)elm); // frees data with the inner vec's own allocator, NOT the slot
}

static inline void vec_print_int(const void* elm)
{
    const GenVec* v = (const GenVec*)elm;
    printf("[");
    for (u64 i = 0; i < v->size; i++) {
        printf("%d", *(int*)GenVec_get_ptr(v, i));
        if (i + 1 < v->size) {
            printf(", ");
        }
    }
    printf("]");
}


/*
 * 4.  GENVEC BY POINTER  (slot holds GenVec*)
*/

static inline void vec_copy_ptr(const wc_allocator* dst, void* dest, const void* src)
{
    // shell and contents both from dst: the shell allocator equals the child's (D6)
    *(GenVec**)dest = WC_BOX(dst, GenVec, GenVec_copy, *(const GenVec* const*)src);
}

static inline void vec_del_ptr(void* elm)
{
    GenVec* v = *(GenVec**)elm;
    if (!v) {
        return;
    }
    const wc_allocator* a = v->alloc; // read before destroy zeroes it
    GenVec_destroy(v);
    wc_free(a, v, sizeof(GenVec), alignof(GenVec));
}

static inline void vec_print_int_ptr(const void* elm)
{
    vec_print_int(*(const GenVec**)elm);
}


// SHARED OPS INSTANCES

static const wc_container_ops wc_vec_ops     = {.copy_fn = vec_copy, .del_fn = vec_del};
static const wc_container_ops wc_vec_ptr_ops = {.copy_fn = vec_copy_ptr, .del_fn = vec_del_ptr};




#endif // GEN_VECTOR_H
