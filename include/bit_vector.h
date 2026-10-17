#ifndef BIT_VECTOR_H
#define BIT_VECTOR_H

#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_comp.h"


// BitVec embeds a GenVec<u8> directly (no heap shell).
// Use BitVec_create(WC_LIBC) for the common case.
typedef struct {
    GenVec arr;  // element type: u8; each byte holds 8 bits
    u64    size; // no of logical bits
} BitVec;


wc_nodiscard wc_nonull(1) BitVec BitVec_create(const wc_allocator* a);
wc_nonull(1) void   BitVec_destroy(BitVec* bvec);

wc_nonull(1) void BitVec_set(BitVec* bvec, u64 i);
wc_nonull(1) void BitVec_clear(BitVec* bvec, u64 i);
wc_nodiscard wc_nonull(1) u8   BitVec_test(const BitVec* bvec, u64 i);
wc_nonull(1) void BitVec_toggle(BitVec* bvec, u64 i);

wc_nonull(1) void BitVec_push(BitVec* bvec);
wc_nonull(1) void BitVec_pop(BitVec* bvec);

wc_nonull(1) void BitVec_print(BitVec* bvec, u64 byteI);

// get the no of BITS in the vector
wc_nodiscard wc_nonull(1) static wc_always_inline u64 BitVec_size_bits(const BitVec* bvec)
{
    return bvec->size;
}
// get the no of BYTES in the vector
wc_nodiscard wc_nonull(1) static wc_always_inline u64 BitVec_size_bytes(const BitVec* bvec)
{
    return GenVec_size(&bvec->arr);
}


#endif // BIT_VECTOR_H
