#include "bit_vector.h"



bitVec* bitVec_create(void)
{
    bitVec* bvec = malloc(sizeof(bitVec));
    CHECK_FATAL(!bvec, "bvec init failed");

    // u8 is POD — no ops needed
    bvec->arr = genVec_init(0, sizeof(u8), NULL);

    bvec->size = 0;

    return bvec;
}

void bitVec_destroy(bitVec* bvec)
{
    CHECK_FATAL(!bvec, "bvec is null");

    genVec_destroy(bvec->arr);

    free(bvec);
}

// Set bit i to 1
void bitVec_set(bitVec* bvec, u64 i)
{
    CHECK_FATAL(!bvec, "bvec is null");

    u64 byte_index = i / 8;
    u64 bit_index  = i % 8;

    while (byte_index >= bvec->arr->size) {
        u8 zero = 0;
        genVec_push(bvec->arr, &zero);
    }

    u8* byte = (u8*)genVec_get_ptr(bvec->arr, byte_index);
    *byte |= (1 << bit_index);

    if (i + 1 > bvec->size) {
        bvec->size = i + 1;
    }
}

// Clear bit i (set to 0)
void bitVec_clear(bitVec* bvec, u64 i)
{
    CHECK_FATAL(!bvec, "bvec is null");
    CHECK_FATAL(i >= bvec->size, "index out of bounds");

    u64 byte_index = i / 8;
    u64 bit_index  = i % 8;

    u8* byte = (u8*)genVec_get_ptr(bvec->arr, byte_index);
    *byte &= ~(1 << bit_index);
}

// Test bit i (returns 1 or 0)
u8 bitVec_test(bitVec* bvec, u64 i)
{
    CHECK_FATAL(!bvec, "bvec is null");
    CHECK_FATAL(i >= bvec->size, "index out of bounds");

    u64 byte_index = i / 8;
    u64 bit_index  = i % 8;

    return (*genVec_get_ptr(bvec->arr, byte_index) >> bit_index) & 1;
}

// Toggle bit i
void bitVec_toggle(bitVec* bvec, u64 i)
{
    CHECK_FATAL(!bvec, "bvec is null");
    CHECK_FATAL(i >= bvec->size, "index out of bounds");

    u64 byte_index = i / 8;
    u64 bit_index  = i % 8;

    u8* byte = (u8*)genVec_get_ptr(bvec->arr, byte_index);
    *byte ^= (1 << bit_index);
}


void bitVec_push(bitVec* bvec)
{
    bitVec_set(bvec, bvec->size);
}


void bitVec_pop(bitVec* bvec)
{
    CHECK_FATAL(!bvec, "bvec is null");

    bvec->size--;
    if (bvec->size % 8 == 0) {
        genVec_pop(bvec->arr, NULL);
    }
}

void bitVec_print(bitVec* bvec, u64 byteI)
{
    CHECK_FATAL(!bvec, "bvec is null");
    CHECK_FATAL(byteI >= bvec->arr->size, "index out of bounds");

    u8 bits_to_print = 8;
    if (byteI == bvec->arr->size - 1) {
        u64 remaining = bvec->size % 8;
        bits_to_print = (remaining == 0) ? 8 : (u8)remaining;
    }

    for (u8 i = 0; i < bits_to_print; i++) {
        printf("%d", ((*genVec_get_ptr(bvec->arr, byteI)) >> i) & 1);
    }
}
