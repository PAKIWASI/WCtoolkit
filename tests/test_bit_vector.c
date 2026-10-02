#include "bit_vector.h"
#include "common.h"
#include "utest.h"
#include "wc_allocator.h"


/* ── Creation / destruction ──────────────────────────────────────────────── */

UTEST(bit_vector, create)
{
    BitVec bv = BitVec_create(WC_LIBC);
    EXPECT_EQ(BitVec_size_bits(&bv), 0u);
    EXPECT_EQ(BitVec_size_bytes(&bv), 0u);
    BitVec_destroy(&bv);
}


/* ── Set / Test ──────────────────────────────────────────────────────────── */

UTEST(bit_vector, set_bit_zero)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 0);
    EXPECT_EQ(BitVec_test(&bv, 0), 1);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, set_multiple_bits_same_byte)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 0);
    BitVec_set(&bv, 3);
    BitVec_set(&bv, 7);
    EXPECT_EQ(BitVec_test(&bv, 0), 1);
    EXPECT_EQ(BitVec_test(&bv, 1), 0);
    EXPECT_EQ(BitVec_test(&bv, 3), 1);
    EXPECT_EQ(BitVec_test(&bv, 7), 1);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, set_crosses_byte_boundary)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 7);  /* last bit of byte 0 */
    BitVec_set(&bv, 8);  /* first bit of byte 1 */
    BitVec_set(&bv, 15); /* last bit of byte 1 */
    EXPECT_EQ(BitVec_size_bytes(&bv), 2u);
    EXPECT_EQ(BitVec_test(&bv, 7), 1);
    EXPECT_EQ(BitVec_test(&bv, 8), 1);
    EXPECT_EQ(BitVec_test(&bv, 15), 1);
    EXPECT_EQ(BitVec_test(&bv, 6), 0);
    EXPECT_EQ(BitVec_test(&bv, 9), 0);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, set_far_bit_allocates_bytes)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 31); /* bit 31 → byte 3 */
    EXPECT_EQ(BitVec_size_bytes(&bv), 4u);
    EXPECT_EQ(BitVec_size_bits(&bv), 32u);
    EXPECT_EQ(BitVec_test(&bv, 31), 1);
    EXPECT_EQ(BitVec_test(&bv, 30), 0);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, set_idempotent)
{
    /* Setting an already-set bit should leave it set */
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 4);
    BitVec_set(&bv, 4);
    EXPECT_EQ(BitVec_test(&bv, 4), 1);
    EXPECT_EQ(BitVec_size_bytes(&bv), 1u);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, unset_bits_are_zero)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 15); /* allocates bytes 0-1 */
    for (u64 i = 0; i < 15; i++) {
        EXPECT_EQ(BitVec_test(&bv, i), 0);
    }
    BitVec_destroy(&bv);
}


/* ── Clear ───────────────────────────────────────────────────────────────── */

UTEST(bit_vector, clear_single_bit)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 2);
    BitVec_set(&bv, 5);
    BitVec_clear(&bv, 2);
    EXPECT_EQ(BitVec_test(&bv, 2), 0);
    EXPECT_EQ(BitVec_test(&bv, 5), 1); /* neighbour unaffected */
    BitVec_destroy(&bv);
}

UTEST(bit_vector, clear_does_not_affect_other_bytes)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 0);
    BitVec_set(&bv, 8);
    BitVec_clear(&bv, 0);
    EXPECT_EQ(BitVec_test(&bv, 0), 0);
    EXPECT_EQ(BitVec_test(&bv, 8), 1);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, clear_already_zero_is_noop)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 7);   /* allocate byte 0 */
    BitVec_clear(&bv, 3); /* bit 3 was never set */
    EXPECT_EQ(BitVec_test(&bv, 3), 0);
    EXPECT_EQ(BitVec_test(&bv, 7), 1);
    BitVec_destroy(&bv);
}


/* ── Toggle ──────────────────────────────────────────────────────────────── */

UTEST(bit_vector, toggle_set_to_clear)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 1);
    BitVec_toggle(&bv, 1);
    EXPECT_EQ(BitVec_test(&bv, 1), 0);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, toggle_clear_to_set)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 7); /* allocate byte 0 */
    BitVec_toggle(&bv, 3);
    EXPECT_EQ(BitVec_test(&bv, 3), 1);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, double_toggle_returns_original)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 5);
    BitVec_toggle(&bv, 5);
    BitVec_toggle(&bv, 5);
    EXPECT_EQ(BitVec_test(&bv, 5), 1);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, toggle_does_not_disturb_neighbours)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 4);
    BitVec_set(&bv, 6);
    BitVec_toggle(&bv, 5);
    EXPECT_EQ(BitVec_test(&bv, 4), 1);
    EXPECT_EQ(BitVec_test(&bv, 5), 1);
    EXPECT_EQ(BitVec_test(&bv, 6), 1);
    BitVec_toggle(&bv, 5);
    EXPECT_EQ(BitVec_test(&bv, 4), 1);
    EXPECT_EQ(BitVec_test(&bv, 5), 0);
    EXPECT_EQ(BitVec_test(&bv, 6), 1);
    BitVec_destroy(&bv);
}


/* ── Push / Pop ──────────────────────────────────────────────────────────── */

UTEST(bit_vector, push_appends_set_bit)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_push(&bv); /* bit 0 = 1 */
    EXPECT_EQ(BitVec_size_bits(&bv), 1u);
    EXPECT_EQ(BitVec_test(&bv, 0), 1);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, push_multiple)
{
    BitVec bv = BitVec_create(WC_LIBC);
    for (int i = 0; i < 9; i++) {
        BitVec_push(&bv);
    }
    EXPECT_EQ(BitVec_size_bits(&bv), 9u);
    EXPECT_EQ(BitVec_size_bytes(&bv), 2u); /* 9 bits → 2 bytes */
    BitVec_destroy(&bv);
}

UTEST(bit_vector, pop_reduces_size)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_push(&bv);
    BitVec_push(&bv);
    BitVec_pop(&bv);
    EXPECT_EQ(BitVec_size_bits(&bv), 1u);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, pop_across_byte_boundary)
{
    BitVec bv = BitVec_create(WC_LIBC);
    for (int i = 0; i < 8; i++) {
        BitVec_push(&bv); /* fill byte 0 */
    }
    BitVec_push(&bv); /* byte 1, bit 0 */
    EXPECT_EQ(BitVec_size_bytes(&bv), 2u);
    BitVec_pop(&bv); /* pops the bit in byte 1 */
    EXPECT_EQ(BitVec_size_bits(&bv), 8u);
    EXPECT_EQ(BitVec_size_bytes(&bv), 1u); /* byte 1 freed */
    BitVec_destroy(&bv);
}


/* ── Size tracking ───────────────────────────────────────────────────────── */

UTEST(bit_vector, size_bits_tracks_highest_set)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 0);
    EXPECT_EQ(BitVec_size_bits(&bv), 1u);
    BitVec_set(&bv, 10);
    EXPECT_EQ(BitVec_size_bits(&bv), 11u);
    BitVec_set(&bv, 5); /* lower index, must not shrink size */
    EXPECT_EQ(BitVec_size_bits(&bv), 11u);
    BitVec_destroy(&bv);
}

UTEST(bit_vector, size_bytes_derived_from_bits)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 7);
    EXPECT_EQ(BitVec_size_bytes(&bv), 1u);
    BitVec_set(&bv, 8);
    EXPECT_EQ(BitVec_size_bytes(&bv), 2u);
    BitVec_destroy(&bv);
}


/* ── Large index stress ──────────────────────────────────────────────────── */

UTEST(bit_vector, large_bit_index)
{
    BitVec bv = BitVec_create(WC_LIBC);
    BitVec_set(&bv, 255);
    EXPECT_EQ(BitVec_size_bytes(&bv), 32u);
    EXPECT_EQ(BitVec_test(&bv, 255), 1);
    for (u64 i = 0; i < 255; i++) {
        EXPECT_EQ(BitVec_test(&bv, i), 0);
    }
    BitVec_destroy(&bv);
}

UTEST(bit_vector, set_clear_all_bits_in_byte)
{
    BitVec bv = BitVec_create(WC_LIBC);
    for (int i = 0; i < 8; i++) {
        BitVec_set(&bv, (u64)i);
    }
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(BitVec_test(&bv, (u64)i), 1);
    }
    for (int i = 0; i < 8; i++) {
        BitVec_clear(&bv, (u64)i);
    }
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(BitVec_test(&bv, (u64)i), 0);
    }
    BitVec_destroy(&bv);
}
