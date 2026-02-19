#include "wc_test.h"
#include "bit_vector.h"


/* ── Creation / destruction ──────────────────────────────────────────────── */

static void test_create(void)
{
    bitVec* bv = bitVec_create();
    WC_ASSERT_NOT_NULL(bv);
    WC_ASSERT_EQ_U64(bitVec_size_bits(bv), 0);
    WC_ASSERT_EQ_U64(bitVec_size_bytes(bv), 0);
    bitVec_destroy(bv);
}


/* ── Set / Test ──────────────────────────────────────────────────────────── */

static void test_set_bit_zero(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 0);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 0), 1);
    bitVec_destroy(bv);
}

static void test_set_multiple_bits_same_byte(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 0);
    bitVec_set(bv, 3);
    bitVec_set(bv, 7);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 0), 1);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 1), 0);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 3), 1);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 7), 1);
    bitVec_destroy(bv);
}

static void test_set_crosses_byte_boundary(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 7);   /* last bit of byte 0 */
    bitVec_set(bv, 8);   /* first bit of byte 1 */
    bitVec_set(bv, 15);  /* last bit of byte 1 */
    WC_ASSERT_EQ_U64(bitVec_size_bytes(bv), 2);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 7),  1);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 8),  1);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 15), 1);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 6),  0);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 9),  0);
    bitVec_destroy(bv);
}

static void test_set_far_bit_allocates_bytes(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 31);  /* bit 31 → byte 3 */
    WC_ASSERT_EQ_U64(bitVec_size_bytes(bv), 4);
    WC_ASSERT_EQ_U64(bitVec_size_bits(bv),  32);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 31), 1);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 30), 0);
    bitVec_destroy(bv);
}

static void test_set_idempotent(void)
{
    /* Setting an already-set bit should leave it set */
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 4);
    bitVec_set(bv, 4);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 4), 1);
    WC_ASSERT_EQ_U64(bitVec_size_bytes(bv), 1);
    bitVec_destroy(bv);
}

static void test_unset_bits_are_zero(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 15); /* allocates bytes 0-1 */
    for (u64 i = 0; i < 15; i++) {
        WC_ASSERT_EQ_INT(bitVec_test(bv, i), 0);
    }
    bitVec_destroy(bv);
}


/* ── Clear ───────────────────────────────────────────────────────────────── */

static void test_clear_single_bit(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 2);
    bitVec_set(bv, 5);
    bitVec_clear(bv, 2);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 2), 0);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 5), 1); /* neighbour unaffected */
    bitVec_destroy(bv);
}

static void test_clear_does_not_affect_other_bytes(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 0);
    bitVec_set(bv, 8);
    bitVec_clear(bv, 0);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 0), 0);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 8), 1);
    bitVec_destroy(bv);
}

static void test_clear_already_zero_is_noop(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 7);    /* allocate byte 0 */
    bitVec_clear(bv, 3);  /* bit 3 was never set */
    WC_ASSERT_EQ_INT(bitVec_test(bv, 3), 0);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 7), 1);
    bitVec_destroy(bv);
}


/* ── Toggle ──────────────────────────────────────────────────────────────── */

static void test_toggle_set_to_clear(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 1);
    bitVec_toggle(bv, 1);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 1), 0);
    bitVec_destroy(bv);
}

static void test_toggle_clear_to_set(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 7); /* allocate byte 0 */
    bitVec_toggle(bv, 3);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 3), 1);
    bitVec_destroy(bv);
}

static void test_double_toggle_returns_original(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 5);
    bitVec_toggle(bv, 5);
    bitVec_toggle(bv, 5);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 5), 1);
    bitVec_destroy(bv);
}

static void test_toggle_does_not_disturb_neighbours(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 4);
    bitVec_set(bv, 6);
    bitVec_toggle(bv, 5);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 4), 1);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 5), 1);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 6), 1);
    bitVec_toggle(bv, 5);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 4), 1);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 5), 0);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 6), 1);
    bitVec_destroy(bv);
}


/* ── Push / Pop ──────────────────────────────────────────────────────────── */

static void test_push_appends_set_bit(void)
{
    bitVec* bv = bitVec_create();
    bitVec_push(bv);   /* bit 0 = 1 */
    WC_ASSERT_EQ_U64(bitVec_size_bits(bv), 1);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 0), 1);
    bitVec_destroy(bv);
}

static void test_push_multiple(void)
{
    bitVec* bv = bitVec_create();
    for (int i = 0; i < 9; i++) bitVec_push(bv);
    WC_ASSERT_EQ_U64(bitVec_size_bits(bv), 9);
    WC_ASSERT_EQ_U64(bitVec_size_bytes(bv), 2); /* 9 bits → 2 bytes */
    bitVec_destroy(bv);
}

static void test_pop_reduces_size(void)
{
    bitVec* bv = bitVec_create();
    bitVec_push(bv);
    bitVec_push(bv);
    bitVec_pop(bv);
    WC_ASSERT_EQ_U64(bitVec_size_bits(bv), 1);
    bitVec_destroy(bv);
}

static void test_pop_across_byte_boundary(void)
{
    bitVec* bv = bitVec_create();
    for (int i = 0; i < 8; i++) bitVec_push(bv); /* fill byte 0 */
    bitVec_push(bv);                               /* byte 1, bit 0 */
    WC_ASSERT_EQ_U64(bitVec_size_bytes(bv), 2);
    bitVec_pop(bv);  /* pops the bit in byte 1 */
    WC_ASSERT_EQ_U64(bitVec_size_bits(bv),  8);
    WC_ASSERT_EQ_U64(bitVec_size_bytes(bv), 1); /* byte 1 freed */
    bitVec_destroy(bv);
}


/* ── Size tracking ───────────────────────────────────────────────────────── */

static void test_size_bits_tracks_highest_set(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 0);
    WC_ASSERT_EQ_U64(bitVec_size_bits(bv), 1);
    bitVec_set(bv, 10);
    WC_ASSERT_EQ_U64(bitVec_size_bits(bv), 11);
    bitVec_set(bv, 5);   /* lower index, must not shrink size */
    WC_ASSERT_EQ_U64(bitVec_size_bits(bv), 11);
    bitVec_destroy(bv);
}

static void test_size_bytes_derived_from_bits(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 7);
    WC_ASSERT_EQ_U64(bitVec_size_bytes(bv), 1);
    bitVec_set(bv, 8);
    WC_ASSERT_EQ_U64(bitVec_size_bytes(bv), 2);
    bitVec_destroy(bv);
}


/* ── Large index stress ──────────────────────────────────────────────────── */

static void test_large_bit_index(void)
{
    bitVec* bv = bitVec_create();
    bitVec_set(bv, 255);
    WC_ASSERT_EQ_U64(bitVec_size_bytes(bv), 32);
    WC_ASSERT_EQ_INT(bitVec_test(bv, 255), 1);
    for (u64 i = 0; i < 255; i++) {
        WC_ASSERT_EQ_INT(bitVec_test(bv, i), 0);
    }
    bitVec_destroy(bv);
}

static void test_set_clear_all_bits_in_byte(void)
{
    bitVec* bv = bitVec_create();
    for (int i = 0; i < 8; i++) bitVec_set(bv, (u64)i);
    for (int i = 0; i < 8; i++) {
        WC_ASSERT_EQ_INT(bitVec_test(bv, (u64)i), 1);
    }
    for (int i = 0; i < 8; i++) bitVec_clear(bv, (u64)i);
    for (int i = 0; i < 8; i++) {
        WC_ASSERT_EQ_INT(bitVec_test(bv, (u64)i), 0);
    }
    bitVec_destroy(bv);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void bit_vector_suite(void)
{
    WC_SUITE("BitVector");

    WC_RUN(test_create);

    WC_RUN(test_set_bit_zero);
    WC_RUN(test_set_multiple_bits_same_byte);
    WC_RUN(test_set_crosses_byte_boundary);
    WC_RUN(test_set_far_bit_allocates_bytes);
    WC_RUN(test_set_idempotent);
    WC_RUN(test_unset_bits_are_zero);

    WC_RUN(test_clear_single_bit);
    WC_RUN(test_clear_does_not_affect_other_bytes);
    WC_RUN(test_clear_already_zero_is_noop);

    WC_RUN(test_toggle_set_to_clear);
    WC_RUN(test_toggle_clear_to_set);
    WC_RUN(test_double_toggle_returns_original);
    WC_RUN(test_toggle_does_not_disturb_neighbours);

    WC_RUN(test_push_appends_set_bit);
    WC_RUN(test_push_multiple);
    WC_RUN(test_pop_reduces_size);
    WC_RUN(test_pop_across_byte_boundary);

    WC_RUN(test_size_bits_tracks_highest_set);
    WC_RUN(test_size_bytes_derived_from_bits);

    WC_RUN(test_large_bit_index);
    WC_RUN(test_set_clear_all_bits_in_byte);
}
