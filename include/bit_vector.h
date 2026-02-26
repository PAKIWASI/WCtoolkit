#ifndef BIT_VECTOR_H
#define BIT_VECTOR_H

#include "gen_vector.h"


typedef struct {
    genVec* arr;
    u64 size;        // no of logical bits
} bitVec;


bitVec* bitVec_create(void);
void    bitVec_destroy(bitVec* bvec);

void bitVec_set(bitVec* bvec, u64 i);
void bitVec_clear(bitVec* bvec, u64 i);
u8   bitVec_test(bitVec* bvec, u64 i);
void bitVec_toggle(bitVec* bvec, u64 i);

void bitVec_push(bitVec* bvec);
void bitVec_pop(bitVec* bvec);

void bitVec_print(bitVec* bvec, u64 byteI);

// get the no of BITS in the vector
static inline u64 bitVec_size_bits(bitVec* bvec)  { return bvec->size;              }
// get the no of BYTES in the vector
static inline u64 bitVec_size_bytes(bitVec* bvec) { return genVec_size(bvec->arr);  }


#endif // BIT_VECTOR_H
