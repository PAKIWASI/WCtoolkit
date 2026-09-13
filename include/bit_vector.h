#ifndef BIT_VECTOR_H
#define BIT_VECTOR_H

#include "common.h"
#include "gen_vector.h"


// HEAP-ONLY: no _stk variant.
typedef struct {
    GenVec* arr;
    u64 size;        // no of logical bits
} BitVec;


BitVec* BitVec_create(void) __attribute__((warn_unused_result));
void    BitVec_destroy(BitVec* bvec) __attribute__((nonnull(1)));

void BitVec_set(BitVec* bvec, u64 i) __attribute__((nonnull(1)));
void BitVec_clear(BitVec* bvec, u64 i) __attribute__((nonnull(1)));
u8   BitVec_test(const BitVec* bvec, u64 i) __attribute__((nonnull(1)));
void BitVec_toggle(BitVec* bvec, u64 i) __attribute__((nonnull(1)));

void BitVec_push(BitVec* bvec) __attribute__((nonnull(1)));
void BitVec_pop(BitVec* bvec) __attribute__((nonnull(1)));

void BitVec_print(BitVec* bvec, u64 byteI) __attribute__((nonnull(1)));

// get the no of BITS in the vector
static inline __attribute__((nonnull(1))) u64 BitVec_size_bits(const BitVec* bvec)  { return bvec->size;              }
// get the no of BYTES in the vector
static inline __attribute__((nonnull(1))) u64 BitVec_size_bytes(const BitVec* bvec) { return GenVec_size(bvec->arr);  }


#endif // BIT_VECTOR_H
