#include "bit_vector.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_errno.h"
#include <stdio.h>
#include <stdlib.h>



BitVec* BitVec_create(void)
{
    BitVec* bvec = malloc(sizeof(BitVec));
    CHECK_FATAL(!bvec, "bvec init failed");

    // u8 is POD — no ops needed
    bvec->arr = GenVec_create(0, sizeof(u8), NULL);

    bvec->size = 0;

    return bvec;
}

void BitVec_destroy(BitVec* bvec)
{
    GenVec_destroy(bvec->arr);

    free(bvec);
}

// Set bit i to 1
void BitVec_set(BitVec* bvec, u64 i)
{
    u64 byte_index = i / 8;
    u64 bit_index  = i % 8;

    while (byte_index >= bvec->arr->size) {
        u8 zero = 0;
        GenVec_push(bvec->arr, &zero);
    }

    u8* byte = (u8*)GenVec_get_ptr(bvec->arr, byte_index);
    *byte |= (u8)(1u << bit_index);

    if (i + 1 > bvec->size) {
        bvec->size = i + 1;
    }
}

// Clear bit i (set to 0)
void BitVec_clear(BitVec* bvec, u64 i)
{
    CHECK_FATAL(i >= bvec->size, "index out of bounds");

    u64 byte_index = i / 8;
    u64 bit_index  = i % 8;

    u8* byte = (u8*)GenVec_get_ptr(bvec->arr, byte_index);
    *byte &= (u8)~(1u << bit_index);
}

// Test bit i (returns 1 or 0)
u8 BitVec_test(const BitVec* bvec, u64 i)
{
    CHECK_FATAL(i >= bvec->size, "index out of bounds");

    u64 byte_index = i / 8;
    u64 bit_index  = i % 8;

    return (*GenVec_get_ptr(bvec->arr, byte_index) >> bit_index) & 1;
}

// Toggle bit i
void BitVec_toggle(BitVec* bvec, u64 i)
{
    CHECK_FATAL(i >= bvec->size, "index out of bounds");

    u64 byte_index = i / 8;
    u64 bit_index  = i % 8;

    u8* byte = (u8*)GenVec_get_ptr(bvec->arr, byte_index);
    *byte ^= (u8)(1u << bit_index);
}


void BitVec_push(BitVec* bvec)
{
    BitVec_set(bvec, bvec->size);
}


void BitVec_pop(BitVec* bvec)
{
    WC_SET_RET(WC_ERR_EMPTY, bvec->size == 0, );

    bvec->size--;
    if (bvec->size % 8 == 0) {
        GenVec_pop(bvec->arr, NULL);
    }
}

void BitVec_print(BitVec* bvec, u64 byteI)
{
    CHECK_FATAL(byteI >= bvec->arr->size, "index out of bounds");

    u8 bits_to_print = 8;
    if (byteI == bvec->arr->size - 1) {
        u64 remaining = bvec->size % 8;
        bits_to_print = (remaining == 0) ? 8 : (u8)remaining;
    }

    for (u8 i = 0; i < bits_to_print; i++) {
        printf("%d", ((*GenVec_get_ptr(bvec->arr, byteI)) >> i) & 1);
    }
}
