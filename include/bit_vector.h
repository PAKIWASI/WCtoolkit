#ifndef BIT_VECTOR_H
#define BIT_VECTOR_H

#include "common.h"
#include "gen_vector.h"


// HEAP-ONLY: no _stk variant.
typedef struct {
    genVec* arr;
    u64 size;        // no of logical bits
} bitVec;


bitVec* bitVec_create(void);
void    bitVec_destroy(bitVec* bvec) __attribute__((nonnull(1)));

void bitVec_set(bitVec* bvec, u64 i) __attribute__((nonnull(1)));
void bitVec_clear(bitVec* bvec, u64 i) __attribute__((nonnull(1)));
u8   bitVec_test(const bitVec* bvec, u64 i) __attribute__((nonnull(1)));
void bitVec_toggle(bitVec* bvec, u64 i) __attribute__((nonnull(1)));

void bitVec_push(bitVec* bvec) __attribute__((nonnull(1)));
void bitVec_pop(bitVec* bvec) __attribute__((nonnull(1)));

void bitVec_print(bitVec* bvec, u64 byteI) __attribute__((nonnull(1)));

// get the no of BITS in the vector
static inline u64 bitVec_size_bits(const bitVec* bvec) __attribute__((nonnull(1)))  { return bvec->size;              }
// get the no of BYTES in the vector
static inline u64 bitVec_size_bytes(const bitVec* bvec) __attribute__((nonnull(1))) { return genVec_size(bvec->arr);  }


#endif // BIT_VECTOR_H
