// One test (or more) per rule in docs/memory-rules.md. Allocation-sensitive rules
// are checked by COUNTING allocator calls through wc_test_alloc, not only by
// leak-freedom: a rule like "taking out is a move" is only proven when the
// number of allocations does not change.

#include "arena.h"
#include "chain_arena.h"
#include "common.h"
#include "gen_vector.h"
#include "hashmap.h"
#include "matrix.h"
#include "priority_queue.h"
#include "queue.h"
#include "random.h"
#include "test_support.h"
#include "wc_allocator.h"
#include "wc_helpers.h"
#include "wc_macros.h"
#include "wc_string.h"
#include "wc_test_allocator.h"

#include <setjmp.h>
#include <stdint.h>
#include <string.h>


// 30+ chars: always heap mode (SSO holds 23), so every String costs an allocation.
#define LONG_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define LONG_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define LONG_C "cccccccccccccccccccccccccccccccccccccccc"

// allocator calls so far (alloc + realloc + free)
static u64 calls(const wc_test_alloc* ta)
{
    return ta->n_alloc + ta->n_realloc + ta->n_free;
}

static int cmp_int(const void* a, const void* b, u64 size)
{
    (void)size;
    int x = *(const int*)a, y = *(const int*)b;
    return (x > y) - (x < y);
}


/* ── A: allocators ───────────────────────────────────────────────────────── */

UTEST(memory_rules, A4_arena_allocator_lives_in_the_arena)
{
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(1));
    const wc_allocator* al = Arena_allocator(&a);
    EXPECT_TRUE((const void*)al == (const void*)&a.self); // points into the arena: same lifetime
    EXPECT_TRUE(al->ctx == &a);
    Arena_destroy(&a);

    ChainArena ca;
    ChainArena_create(&ca, WC_LIBC);
    EXPECT_TRUE(ChainArena_allocator(&ca)->ctx == &ca);
    ChainArena_destroy(&ca);
}

UTEST(memory_rules, A9_set_elements_aligned_on_arena)
{
    // An odd-sized burn first so the arena's next address is misaligned.
    Arena a;
    Arena_create(&a, WC_LIBC, nKB(16));
    EXPECT_TRUE(Arena_alloc_aligned(&a, 3, 1) != NULL);

    HashMap s = HashMap_create(Arena_allocator(&a), sizeof(String), 0, wc_hash_str, str_cmp, &wc_str_ops, NULL);
    EXPECT_EQ((uintptr_t)s.keys % alignof(String), 0u);
    SET_INSERT_CSTR(&s, LONG_A);
    SET_FOREACH (&s, String, e) {
        EXPECT_EQ((uintptr_t)e % alignof(String), 0u);
    }

    HashMap m = MAP_OF_IN(Arena_allocator(&a), u64, double);
    EXPECT_EQ((uintptr_t)m.keys % alignof(u64), 0u);
    EXPECT_EQ((uintptr_t)m.vals % alignof(double), 0u);

    HashMap_destroy(&s);
    HashMap_destroy(&m);
    Arena_destroy(&a);
}


/* ── B7 / B8: taking out is a move, copies only when both sides keep it ─── */

UTEST(memory_rules, B7_vec_pop_remove_swap_pop_never_allocate)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    GenVec v = VEC_OF_IN(wc_test_alloc_allocator(&ta), String, 8);
    VEC_PUSH_CSTR(&v, LONG_A);
    VEC_PUSH_CSTR(&v, LONG_B);
    VEC_PUSH_CSTR(&v, LONG_C);
    VEC_PUSH_CSTR(&v, LONG_A);

    u64 before = calls(&ta);

    String popped = VEC_POP(&v, String);
    String removed;
    GenVec_remove(&v, 0, &removed);
    String swapped;
    GenVec_swap_pop(&v, 0, &swapped);

    EXPECT_EQ(calls(&ta), before); // three take-outs, zero allocator calls
    EXPECT_TRUE(String_equals_cstr(&popped, LONG_A));
    EXPECT_TRUE(String_equals_cstr(&removed, LONG_A));
    EXPECT_TRUE(String_equals_cstr(&swapped, LONG_B));
    EXPECT_EQ(GenVec_size(&v), 1u);

    // B9: the moved-out value keeps the vector's allocator
    EXPECT_TRUE(popped.alloc == wc_test_alloc_allocator(&ta));

    String_destroy(&popped);
    String_destroy(&removed);
    String_destroy(&swapped);
    GenVec_destroy(&v);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, B7_take_out_with_null_out_deletes)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    GenVec v = VEC_OF_IN(wc_test_alloc_allocator(&ta), String, 4);
    VEC_PUSH_CSTR(&v, LONG_A);
    VEC_PUSH_CSTR(&v, LONG_B);
    u64 frees = ta.n_free;
    GenVec_pop(&v, NULL);
    GenVec_remove(&v, 0, NULL);
    EXPECT_EQ(ta.n_free, frees + 2); // both buffers released, nothing leaked

    GenVec_destroy(&v);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, B7_queue_pop_and_hashmap_del_never_allocate)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    Queue q = QUEUE_CREATE_CX_IN(al, String, 4, &wc_str_ops);
    QUEUE_PUSH_CSTR(&q, LONG_A);
    QUEUE_PUSH_CSTR(&q, LONG_B);

    HashMap m = MAP_OF_IN(al, int, String);
    MAP_PUT_INT_STR(&m, 1, LONG_C);

    u64    before = calls(&ta);
    String front  = QUEUE_POP(&q, String);
    String back;
    Queue_pop_back(&q, &back);
    String val;
    EXPECT_TRUE(HashMap_del(&m, &(int){1}, &val));
    EXPECT_EQ(calls(&ta), before);

    EXPECT_TRUE(String_equals_cstr(&front, LONG_A));
    EXPECT_TRUE(String_equals_cstr(&back, LONG_B));
    EXPECT_TRUE(String_equals_cstr(&val, LONG_C));

    String_destroy(&front);
    String_destroy(&back);
    String_destroy(&val);
    Queue_destroy(&q);
    HashMap_destroy(&m);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, B8_copy_api_still_copies)
{
    GenVec v = VEC_OF(String, 2);
    String s = String_from_cstr(WC_LIBC, LONG_A);
    VEC_PUSH_COPY(&v, s);
    String_append_cstr(&s, "!"); // the vector's copy is independent
    EXPECT_TRUE(String_equals_cstr(VEC_REF(&v, String, 0), LONG_A));

    String got;
    GenVec_get(&v, 0, &got); // get = deep copy: both sides keep it
    EXPECT_TRUE(got.heap != VEC_REF(&v, String, 0)->heap);

    String_destroy(&got);
    String_destroy(&s);
    GenVec_destroy(&v);
}


/* ── Queue: the two bugs from the audit, plus growth ─────────────────────── */

UTEST(memory_rules, queue_pop_back_then_push_no_double_free)
{
    // Old code: push wrote into slots inside arr.size with GenVec_replace, which
    // deleted the slot's previous occupant. pop_back had already freed it.
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    Queue q = QUEUE_CREATE_CX_IN(wc_test_alloc_allocator(&ta), String*, 4, &wc_str_ptr_ops);
    for (int round = 0; round < 3; round++) {
        String* a = WC_BOX_IN(wc_test_alloc_allocator(&ta), String, String_from_cstr, LONG_A);
        String* b = WC_BOX_IN(wc_test_alloc_allocator(&ta), String, String_from_cstr, LONG_B);
        QUEUE_PUSH_MOVE(&q, a);
        QUEUE_PUSH_MOVE(&q, b);
        Queue_pop_back(&q, NULL); // deletes b
        String* c = WC_BOX_IN(wc_test_alloc_allocator(&ta), String, String_from_cstr, LONG_C);
        QUEUE_PUSH_MOVE(&q, c); // lands in b's old slot
    }
    EXPECT_EQ(Queue_size(&q), 6u);

    Queue_destroy(&q);
    EXPECT_EQ(ta.n_errors, 0u); // a double free is reported here
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, queue_swap_on_wrapped_full_queue)
{
    // Old code: Queue_swap -> GenVec_swap grew the buffer for a temp slot,
    // changing the capacity the ring indexes by.
    Queue q = QUEUE_CREATE(int, 4);
    for (int i = 0; i < 4; i++) {
        QUEUE_PUSH(&q, i);
    }
    (void)QUEUE_POP(&q, int); // head moves to 1
    QUEUE_PUSH(&q, 4);        // wraps into slot 0: ring is full and wrapped
    u64 cap = Queue_capacity(&q);

    Queue_swap(&q, 0, 3); // logical [1 2 3 4] -> [4 2 3 1]
    EXPECT_EQ(Queue_capacity(&q), cap); // no growth

    int expect[] = {4, 2, 3, 1};
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(QUEUE_POP(&q, int), expect[i]);
    }
    Queue_destroy(&q);
}

UTEST(memory_rules, B5_queue_growth_relocates_without_copying)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    Queue q = QUEUE_CREATE_CX_IN(wc_test_alloc_allocator(&ta), String, 4, &wc_str_ops);
    for (int i = 0; i < 4; i++) {
        QUEUE_PUSH_CSTR(&q, LONG_A);
    }
    String drop = QUEUE_POP(&q, String);
    String_destroy(&drop);
    QUEUE_PUSH_CSTR(&q, LONG_B); // ring wrapped and full

    u64 allocs = ta.n_alloc, reallocs = ta.n_realloc, frees = ta.n_free;
    QUEUE_PUSH_CSTR(&q, LONG_C); // grows
    EXPECT_EQ(ta.n_realloc, reallocs + 1); // the ring: one realloc
    EXPECT_EQ(ta.n_alloc, allocs + 1);     // the new String only: no element copies
    EXPECT_EQ(ta.n_free, frees);

    const char* expect[] = {LONG_A, LONG_A, LONG_A, LONG_B, LONG_C};
    for (int i = 0; i < 5; i++) {
        String s = QUEUE_POP(&q, String);
        EXPECT_TRUE(String_equals_cstr(&s, expect[i]));
        String_destroy(&s);
    }
    Queue_destroy(&q);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, F2_queue_never_shrinks_on_its_own)
{
    Queue q = QUEUE_CREATE(int, 4);
    for (int i = 0; i < 100; i++) {
        QUEUE_PUSH(&q, i);
    }
    u64 cap = Queue_capacity(&q);
    for (int i = 0; i < 99; i++) {
        (void)QUEUE_POP(&q, int);
    }
    EXPECT_EQ(Queue_capacity(&q), cap);
    Queue_shrink_to_fit(&q); // only on request
    EXPECT_EQ(Queue_capacity(&q), (u64)QUEUE_MIN_CAP);
    EXPECT_EQ(QUEUE_POP(&q, int), 99);
    Queue_destroy(&q);
}

UTEST(memory_rules, F5_queue_copy_is_compacted)
{
    Queue q = QUEUE_CREATE(int, 64);
    for (int i = 0; i < 40; i++) {
        QUEUE_PUSH(&q, i);
    }
    for (int i = 0; i < 30; i++) {
        (void)QUEUE_POP(&q, int);
    }
    Queue c = Queue_copy(WC_LIBC, &q);
    EXPECT_EQ(Queue_capacity(&c), 10u);
    EXPECT_EQ(c.head, 0u);
    for (int i = 30; i < 40; i++) {
        EXPECT_EQ(QUEUE_POP(&c, int), i);
    }
    Queue_destroy(&c);
    Queue_destroy(&q);
}


/* ── F: GenVec growth ────────────────────────────────────────────────────── */

UTEST(memory_rules, F3_first_growth_jumps_to_minimum)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    GenVec v = VEC_IN(wc_test_alloc_allocator(&ta), int, 0);
    for (int i = 0; i < GENVEC_MIN_CAPACITY; i++) {
        VEC_PUSH(&v, i);
    }
    EXPECT_EQ(ta.n_alloc + ta.n_realloc, 1u); // 0 -> 8 in one step (was 0->1->2->3->4)

    GenVec_destroy(&v);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, F4_insert_multi_grows_geometrically)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    GenVec v     = VEC_IN(wc_test_alloc_allocator(&ta), int, 0);
    int    xs[2] = {1, 2};
    for (int i = 0; i < 1000; i++) {
        GenVec_insert_multi(&v, GenVec_size(&v), xs, 2);
    }
    EXPECT_EQ(GenVec_size(&v), 2000u);
    EXPECT_LT(ta.n_alloc + ta.n_realloc, 20u); // exact-size reserve would be 1000 reallocs

    GenVec_destroy(&v);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, swap_never_allocates)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    // full vector, element larger than the 64-byte swap chunk
    typedef struct {
        u8 bytes[100];
    } Big;
    GenVec v = VEC_IN(wc_test_alloc_allocator(&ta), Big, 2);
    Big    a, b;
    memset(&a, 'a', sizeof(a));
    memset(&b, 'b', sizeof(b));
    VEC_PUSH(&v, a);
    VEC_PUSH(&v, b);

    u64 before = calls(&ta);
    GenVec_swap(&v, 0, 1);
    EXPECT_EQ(calls(&ta), before);
    EXPECT_EQ(VEC_REF(&v, Big, 0)->bytes[99], 'b');
    EXPECT_EQ(VEC_REF(&v, Big, 1)->bytes[0], 'a');

    GenVec_destroy(&v);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, F5_vec_copy_sized_to_contents)
{
    GenVec v = VEC(int, 1000);
    for (int i = 0; i < 20; i++) {
        VEC_PUSH(&v, i);
    }
    GenVec c = GenVec_copy(WC_LIBC, &v);
    EXPECT_EQ(GenVec_capacity(&c), 20u);
    GenVec e = VEC(int, 100);
    GenVec ce = GenVec_copy(WC_LIBC, &e);
    EXPECT_EQ(GenVec_capacity(&ce), 0u); // empty copy allocates nothing
    GenVec_destroy(&ce);
    GenVec_destroy(&e);
    GenVec_destroy(&c);
    GenVec_destroy(&v);
}


/* ── D4: input aliasing ──────────────────────────────────────────────────── */

#ifndef NDEBUG
static void die_push_aliased(void)
{
    GenVec v = VEC(int, 1);
    VEC_PUSH(&v, 7);
    GenVec_push(&v, GenVec_get_ptr(&v, 0)); // would read freed memory after growth
}

UTEST(memory_rules, D4_push_from_own_buffer_asserts)
{
    EXPECT_DIES(die_push_aliased);
}
#endif


/* ── Hash tables: one block ──────────────────────────────────────────────── */

UTEST(memory_rules, hashmap_single_block)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    HashMap m = MAP_OF_IN(wc_test_alloc_allocator(&ta), int, int);
    EXPECT_EQ(ta.n_alloc, 1u); // keys + vals + scratch + psls

    u64 allocs = ta.n_alloc, frees = ta.n_free;
    for (int i = 0; i < 12; i++) { // 16 buckets at 0.75: the 12th insert resizes
        MAP_PUT(&m, i, i * 10);
    }
    EXPECT_EQ(ta.n_alloc, allocs + 1);
    EXPECT_EQ(ta.n_free, frees + 1);

    HashMap c = HashMap_copy(wc_test_alloc_allocator(&ta), &m);
    EXPECT_EQ(ta.n_alloc, allocs + 2); // POD copy: one block, one memcpy
    for (int i = 0; i < 12; i++) {
        EXPECT_EQ(MAP_GET(&c, int, i), i * 10);
    }

    HashMap_destroy(&c);
    HashMap_destroy(&m);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, set_single_block_and_reserve)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    HashMap s = HashMap_create(wc_test_alloc_allocator(&ta), sizeof(int), 0, NULL, NULL, NULL, NULL);
    EXPECT_EQ(ta.n_alloc, 1u);
    HashMap_reserve(&s, 1000);
    u64 allocs = ta.n_alloc;
    for (int i = 0; i < 1000; i++) {
        SET_INSERT(&s, i);
    }
    EXPECT_EQ(ta.n_alloc, allocs); // reserved: no resize
    EXPECT_EQ(HashMap_size(&s), 1000u);

    HashMap_destroy(&s);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, set_from_vec_reserves_once)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    GenVec v = VEC_IN(wc_test_alloc_allocator(&ta), int, 500);
    for (int i = 0; i < 500; i++) {
        VEC_PUSH(&v, i);
    }
    u64     allocs = ta.n_alloc;
    HashMap s      = SET_FROM_VEC(&v, NULL, NULL);
    EXPECT_LE(ta.n_alloc, allocs + 2); // create + one reserve
    EXPECT_EQ(HashMap_size(&s), 500u);

    HashMap_destroy(&s);
    GenVec_destroy(&v);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}


/* ── E: String-keyed lookups by C string ─────────────────────────────────── */

UTEST(memory_rules, E1_cstr_lookups_never_allocate)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    HashMap m = MAP_OF_IN(al, String, int);
    MAP_PUT_STR_INT(&m, LONG_A, 1);
    MAP_PUT_STR_INT(&m, LONG_B, 2);

    u64 before = calls(&ta);
    EXPECT_TRUE(MAP_HAS_CSTR(&m, LONG_A));
    EXPECT_FALSE(MAP_HAS_CSTR(&m, LONG_C));
    EXPECT_EQ(MAP_GET_CSTR(&m, int, LONG_B), 2);
    EXPECT_EQ(*MAP_GET_PTR_CSTR(&m, int, LONG_A), 1);
    EXPECT_TRUE(MAP_GET_PTR_CSTR(&m, int, LONG_C) == NULL);

    // existing key: value updated, the key String is never built
    EXPECT_TRUE(MAP_PUT_STR_INT(&m, LONG_A, 11));
    EXPECT_EQ(calls(&ta), before);
    EXPECT_EQ(MAP_GET_CSTR(&m, int, LONG_A), 11);

    // new key: copied exactly once into the map's allocator
    u64 allocs = ta.n_alloc;
    EXPECT_FALSE(MAP_PUT_STR_INT(&m, LONG_C, 3));
    EXPECT_EQ(ta.n_alloc, allocs + 1);

    HashMap s = HashMap_create(al, sizeof(String), 0, wc_hash_str, str_cmp, &wc_str_ops, NULL);
    SET_INSERT_CSTR(&s, LONG_A);
    before = calls(&ta);
    EXPECT_TRUE(SET_INSERT_CSTR(&s, LONG_A)); // duplicate: nothing built, nothing freed
    EXPECT_TRUE(SET_HAS_CSTR(&s, LONG_A));
    EXPECT_FALSE(SET_HAS_CSTR(&s, LONG_B));
    EXPECT_EQ(calls(&ta), before);

    HashMap_destroy(&s);
    HashMap_destroy(&m);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, string_borrow_is_read_only_and_free)
{
    const char* text = LONG_A;
    String      b    = String_borrow(text, strlen(text));
    EXPECT_TRUE(String_equals_cstr(&b, LONG_A));
    EXPECT_TRUE(String_data_ptr(&b) == text); // no copy
    String_destroy(&b);                       // no-op free (WC_BORROWED)

    String e = String_borrow("", 0);
    EXPECT_EQ(String_len(&e), 0u);
    EXPECT_NE(String_capacity(&e), 0u); // not the zero state
}

static void die_mutating_borrowed(void)
{
    String b = String_borrow(LONG_A, 40);
    String_append_cstr(&b, "x"); // borrowed cannot allocate: fatal
}
UTEST(memory_rules, string_borrow_mutation_dies)
{
    EXPECT_DIES(die_mutating_borrowed); // FATAL: every build
}

UTEST(memory_rules, string_append_move_takes_the_buffer)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);
    const wc_allocator* al = wc_test_alloc_allocator(&ta);

    String dst = String_create(al);
    String src = String_from_cstr(al, LONG_A);
    char*  buf = src.heap;

    u64 before = calls(&ta);
    String_append_String_move(&dst, &src);
    EXPECT_EQ(calls(&ta), before); // empty destination: buffer taken over, no copy
    EXPECT_TRUE(dst.heap == buf);
    EXPECT_EQ(src.capacity, 0u); // source zeroed
    EXPECT_TRUE(String_equals_cstr(&dst, LONG_A));

    String more = String_from_cstr(al, LONG_B);
    String_append_String_move(&dst, &more); // non-empty: append, then free source
    EXPECT_EQ(String_len(&dst), 80u);

    String_destroy(&dst);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, vec_from_arr_deep_copies_owning_types)
{
    String arr[2] = {String_from_cstr(WC_LIBC, LONG_A), String_from_cstr(WC_LIBC, LONG_B)};
    GenVec v      = VEC_FROM_ARR(String, 2, arr);
    EXPECT_TRUE(VEC_REF(&v, String, 0)->heap != arr[0].heap); // own copy, not shared
    EXPECT_TRUE(String_equals_cstr(VEC_REF(&v, String, 1), LONG_B));
    GenVec_destroy(&v);
    String_destroy(&arr[0]);
    String_destroy(&arr[1]);

    GenVec ints = VEC_FROM_ARR(int, 4, ((int[4]){1, 2, 3, 4}));
    EXPECT_EQ(VEC_AT(&ints, int, 3), 4);
    GenVec_destroy(&ints);
}


/* ── PriorityQueue ───────────────────────────────────────────────────────── */

UTEST(memory_rules, pq_orders_and_moves)
{
    PriorityQueue pq = PriorityQueue_create(WC_LIBC, 0, sizeof(int), NULL, cmp_int);
    int           xs[] = {5, 1, 9, 3, 7, 2, 8, 6, 4, 0};
    for (int i = 0; i < 10; i++) {
        PriorityQueue_push(&pq, &xs[i]);
    }
    EXPECT_EQ(*(const int*)PriorityQueue_peek(&pq), 0);
    int out = -1;
    PriorityQueue_remove(&pq, 4, NULL); // any element
    int prev = -1;
    while (!PriorityQueue_empty(&pq)) {
        PriorityQueue_pop(&pq, &out);
        EXPECT_GE(out, prev);
        prev = out;
    }
    PriorityQueue_destroy(&pq);
}

static int cmp_str(const void* a, const void* b, u64 size)
{
    (void)size;
    return String_compare((const String*)a, (const String*)b);
}

UTEST(memory_rules, pq_from_vec_move_and_pop_never_allocate)
{
    wc_test_alloc ta;
    wc_test_alloc_init(&ta, WC_LIBC);

    GenVec v = VEC_OF_IN(wc_test_alloc_allocator(&ta), String, 8);
    VEC_PUSH_CSTR(&v, LONG_C);
    VEC_PUSH_CSTR(&v, LONG_A);
    VEC_PUSH_CSTR(&v, LONG_B);

    u64           before = calls(&ta);
    PriorityQueue pq     = PriorityQueue_from_vec_move(&v, cmp_str);
    String        top;
    PriorityQueue_pop(&pq, &top);
    EXPECT_EQ(calls(&ta), before); // heapify + pop: relocation only
    EXPECT_TRUE(String_equals_cstr(&top, LONG_A));
    EXPECT_EQ(v.data_size, 0u); // vec handed over, zeroed

    String_destroy(&top);
    PriorityQueue_destroy(&pq);
    EXPECT_EQ(ta.n_errors, 0u);
    EXPECT_EQ(wc_test_alloc_destroy(&ta), 0u);
}

UTEST(memory_rules, pq_random_against_sorted)
{
    WC_Pcg32 rng = PCG32_INITIALIZER;
    pcg32_rand_seed(&rng, 7, 3);
    PriorityQueue pq = PriorityQueue_create(WC_LIBC, 4, sizeof(int), NULL, cmp_int);
    int           count[100] = {0};
    for (int i = 0; i < 2000; i++) {
        int x = (int)pcg32_rand_bounded(&rng, 100);
        if (i % 3 == 2 && !PriorityQueue_empty(&pq)) {
            int idx = (int)pcg32_rand_bounded(&rng, (u32)PriorityQueue_size(&pq));
            int y;
            PriorityQueue_remove(&pq, (u64)idx, &y);
            count[y]--;
        } else {
            PriorityQueue_push(&pq, &x);
            count[x]++;
        }
    }
    int prev = -1;
    while (!PriorityQueue_empty(&pq)) {
        int y;
        PriorityQueue_pop(&pq, &y);
        EXPECT_GE(y, prev);
        prev = y;
        count[y]--;
    }
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(count[i], 0);
    }
    PriorityQueue_destroy(&pq);
}


/* ── Remaining TODO decisions ────────────────────────────────────────────── */

UTEST(memory_rules, rng_instances_are_independent)
{
    WC_Pcg32 a = PCG32_INITIALIZER, b = PCG32_INITIALIZER;
    pcg32_rand_seed(&a, 1, 1);
    pcg32_rand_seed(&b, 1, 1);
    float ga = pcg32_rand_gaussian(&a); // a now holds a spare
    EXPECT_EQ(pcg32_rand_gaussian(&b), ga); // b's first value: not a's spare
    EXPECT_EQ(pcg32_rand_gaussian(&a), pcg32_rand_gaussian(&b));
}

UTEST(memory_rules, matrix_inverse)
{
    float   m3[9]  = {0, 2, 1, 1, 0, 3, 4, 1, 0}; // zero leading pivot: needs row swaps
    Matrixf a      = matrix_create_arr(WC_LIBC, 3, 3, m3);
    Matrixf inv    = matrix_create(WC_LIBC, 3, 3);
    Matrixf ident  = matrix_create(WC_LIBC, 3, 3);
    EXPECT_TRUE(matrix_inv(&inv, &a));
    matrix_xply(&ident, &a, &inv);
    for (u64 i = 0; i < 3; i++) {
        for (u64 j = 0; j < 3; j++) {
            EXPECT_NEAR(MATRIX_AT(&ident, i, j), i == j ? 1.0F : 0.0F, 1e-5F);
        }
    }

    float   s3[9] = {1, 2, 3, 2, 4, 6, 1, 1, 1}; // row 2 = 2 * row 1
    Matrixf sing  = matrix_create_arr(WC_LIBC, 3, 3, s3);
    EXPECT_FALSE(matrix_inv(&inv, &sing));

    matrix_destroy(&sing);
    matrix_destroy(&ident);
    matrix_destroy(&inv);
    matrix_destroy(&a);
}

static jmp_buf     g_fatal_jmp;
static const char* g_fatal_msg_seen;
static char        g_fatal_msg[256];

static void longjmp_handler(const char* file, int line, const char* func, const char* msg)
{
    (void)file;
    (void)line;
    (void)func;
    snprintf(g_fatal_msg, sizeof(g_fatal_msg), "%s", msg);
    g_fatal_msg_seen = g_fatal_msg;
    longjmp(g_fatal_jmp, 1);
}

UTEST(memory_rules, fatal_handler_can_recover_in_tests)
{
    g_fatal_msg_seen = NULL;
    wc_fatal_fn prev = wc_set_fatal_handler(longjmp_handler);
    EXPECT_TRUE(prev == NULL);
    if (setjmp(g_fatal_jmp) == 0) {
        FATAL("custom %d", 42);
    }
    wc_set_fatal_handler(prev);
    EXPECT_TRUE(g_fatal_msg_seen != NULL);
    EXPECT_STREQ(g_fatal_msg, "custom 42");
}
