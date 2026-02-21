#include "hashmap.h"
#include "wc_test.h"
#include "wc_helpers.h"
#include "wc_macros.h"


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 1 — Vec<String> by VALUE (Strategy A)
 *
 * The slot IS the String struct (40 bytes).
 * copy_fn deep-copies the data buffer.
 * move_fn transfers the heap struct, frees container only.
 * del_fn  frees the data buffer, NOT the slot.
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_strval_push_copy_independent(void)
{
    genVec* v = VEC_OF_STR(4);
    String  s;
    string_create_stk(&s, "hello");

    VEC_PUSH_COPY(v, s);
    VEC_PUSH_COPY(v, s);

    /* mutate source — stored copies must be independent */
    string_append_cstr(&s, "_MUTATED");
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 0), "hello"));
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 1), "hello"));

    string_destroy_stk(&s);
    genVec_destroy(v);
}

static void test_strval_push_move_nulls_src(void)
{
    genVec* v  = VEC_OF_STR(4);
    String* s  = string_from_cstr("world");
    VEC_PUSH_MOVE(v, s);

    WC_ASSERT_NULL(s);
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 0), "world"));
    genVec_destroy(v);
}

static void test_strval_push_cstr(void)
{
    genVec* v = VEC_OF_STR(4);
    VEC_PUSH_CSTR(v, "alpha");
    VEC_PUSH_CSTR(v, "beta");
    VEC_PUSH_CSTR(v, "gamma");

    WC_ASSERT_EQ_U64(genVec_size(v), 3);
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 0), "alpha"));
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 1), "beta"));
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 2), "gamma"));
    genVec_destroy(v);
}

static void test_strval_foreach_mutates_in_place(void)
{
    genVec* v = VEC_OF_STR(4);
    VEC_PUSH_CSTR(v, "one");
    VEC_PUSH_CSTR(v, "two");

    VEC_FOREACH(v, String, s) {
        string_append_char(s, '!');
    }

    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 0), "one!"));
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 1), "two!"));
    genVec_destroy(v);
}

static void test_strval_pop_returns_owned_string(void)
{
    genVec* v = VEC_OF_STR(4);
    VEC_PUSH_CSTR(v, "first");
    VEC_PUSH_CSTR(v, "second");

    /* VEC_POP copies element out via copy_fn, then del_fn cleans slot */
    String popped = VEC_POP(v, String);
    WC_ASSERT(string_equals_cstr(&popped, "second"));
    WC_ASSERT_EQ_U64(genVec_size(v), 1);
    string_destroy_stk(&popped); /* caller owns it */
    genVec_destroy(v);
}

static void test_strval_at_mut_modifies_in_place(void)
{
    genVec* v = VEC_OF_STR(4);
    VEC_PUSH_CSTR(v, "hello");

    String* slot = VEC_AT_MUT(v, String, 0);
    string_append_cstr(slot, "_world");
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(v, String, 0), "hello_world"));
    genVec_destroy(v);
}

static void test_strval_copy_vec(void)
{
    genVec* src = VEC_OF_STR(4);
    VEC_PUSH_CSTR(src, "a");
    VEC_PUSH_CSTR(src, "b");

    genVec dest;
    genVec_init_stk(0, sizeof(String), str_copy, str_move, str_del, &dest);
    genVec_copy(&dest, src);

    /* modifying src must not affect dest */
    string_append_cstr(VEC_AT_MUT(src, String, 0), "_mutated");
    WC_ASSERT(string_equals_cstr(VEC_AT_MUT(&dest, String, 0), "a"));

    genVec_destroy(src);
    genVec_destroy_stk(&dest);
}

static void test_strval_triggers_growth(void)
{
    genVec* v = VEC_OF_STR(2);
    for (int i = 0; i < 20; i++) {
        VEC_PUSH_CSTR(v, "x");
    }
    WC_ASSERT_EQ_U64(genVec_size(v), 20);
    /* all elements must survive multiple reallocations */
    VEC_FOREACH(v, String, s) {
        WC_ASSERT(string_equals_cstr(s, "x"));
    }
    genVec_destroy(v);
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 2 — Vec<String*> by POINTER (Strategy B)
 *
 * Slot holds String* (8 bytes). Addresses are stable across growth.
 * VEC_AT gives String*, VEC_AT_MUT gives String** (pointer to slot).
 * VEC_FOREACH — name is String** — use *name to reach the String.
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_strptr_push_copy_independent(void)
{
    genVec* v  = VEC_OF_STR_PTR(4);
    String* s  = string_from_cstr("hello");

    VEC_PUSH_COPY(v, s);
    VEC_PUSH_COPY(v, s);

    /* mutate source — copies in vec must be independent */
    string_append_cstr(s, "_MUTATED");
    WC_ASSERT(string_equals_cstr(VEC_AT(v, String*, 0), "hello"));
    WC_ASSERT(string_equals_cstr(VEC_AT(v, String*, 1), "hello"));

    string_destroy(s);
    genVec_destroy(v);
}

static void test_strptr_push_move_nulls_src(void)
{
    genVec* v  = VEC_OF_STR_PTR(4);
    String* s  = string_from_cstr("world");
    VEC_PUSH_MOVE(v, s);

    WC_ASSERT_NULL(s);
    WC_ASSERT(string_equals_cstr(VEC_AT(v, String*, 0), "world"));
    genVec_destroy(v);
}

static void test_strptr_address_stable_after_growth(void)
{
    /* key advantage of Strategy B: address of String doesn't change on realloc */
    genVec* v    = VEC_OF_STR_PTR(2);
    String* s    = string_from_cstr("stable");
    VEC_PUSH_MOVE(v, s);
    String* addr = VEC_AT(v, String*, 0); /* address of the heap String */

    /* force many reallocations */
    for (int i = 0; i < 30; i++) {
        VEC_PUSH_CSTR(v, "filler");
    }

    WC_ASSERT_TRUE(VEC_AT(v, String*, 0) == addr); /* same address */
    WC_ASSERT(string_equals_cstr(addr, "stable"));
    genVec_destroy(v);
}

static void test_strptr_foreach_dereference(void)
{
    genVec* v = VEC_OF_STR_PTR(4);
    VEC_PUSH_CSTR(v, "one");
    VEC_PUSH_CSTR(v, "two");

    VEC_FOREACH(v, String*, sp) {   /* sp is String** */
        string_append_char(*sp, '!');
    }

    WC_ASSERT(string_equals_cstr(VEC_AT(v, String*, 0), "one!"));
    WC_ASSERT(string_equals_cstr(VEC_AT(v, String*, 1), "two!"));
    genVec_destroy(v);
}

static void test_strptr_replace_slot_pointer(void)
{
    genVec* v = VEC_OF_STR_PTR(4);
    VEC_PUSH_CSTR(v, "old");

    /* VEC_AT_MUT gives String** — we can replace which String the slot points to */
    String** slot        = VEC_AT_MUT(v, String*, 0);
    String*  replacement = string_from_cstr("new");
    string_destroy(*slot);  /* free old String */
    *slot = replacement;    /* put new String* in slot */

    WC_ASSERT(string_equals_cstr(VEC_AT(v, String*, 0), "new"));
    genVec_destroy(v);
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 3 — Vec<genVec> by VALUE  (vec of int vecs)
 *
 * Outer slot IS the inner genVec struct (56 bytes).
 * Inner vec's data buffer lives on the heap.
 * vec_copy:  malloc new data buffer, memcpy or element-copy.
 * vec_move:  memcpy fields, free(*src) [container only], *src = NULL.
 * vec_del:   genVec_destroy_stk [frees data buffer only, not slot].
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_vecval_push_move(void)
{
    genVec* outer = VEC_OF_VEC(4);

    genVec* inner = VEC_OF_INT(8);
    for (int i = 0; i < 5; i++) { VEC_PUSH(inner, i); }

    VEC_PUSH_VEC(outer, inner); /* inner nulled, data lives in outer slot */
    WC_ASSERT_NULL(inner);
    WC_ASSERT_EQ_U64(genVec_size(outer), 1);

    genVec* slot = VEC_AT_MUT(outer, genVec, 0);
    WC_ASSERT_EQ_U64(genVec_size(slot), 5);
    for (int i = 0; i < 5; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(slot, (u64)i), i);
    }

    genVec_destroy(outer);
}

static void test_vecval_push_copy_independent(void)
{
    genVec* outer = VEC_OF_VEC(4);

    genVec* inner = VEC_OF_INT(4);
    for (int i = 0; i < 3; i++) { VEC_PUSH(inner, i); }

    /* push by copy — inner stays valid */
    genVec_push(outer, (u8*)inner);
    WC_ASSERT_NOT_NULL(inner);

    /* mutate original — stored copy must be independent */
    int x = 999;
    genVec_replace(inner, 0, (u8*)&x);

    genVec* slot = VEC_AT_MUT(outer, genVec, 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(slot, 0), 0); /* copy untouched */

    genVec_destroy(inner);
    genVec_destroy(outer);
}

static void test_vecval_multiple_inner_vecs(void)
{
    genVec* outer = VEC_OF_VEC(4);

    for (int row = 0; row < 4; row++) {
        genVec* inner = VEC_OF_INT(4);
        for (int col = 0; col < (row + 1); col++) {
            int v = (row * 10) + col;
            VEC_PUSH(inner, v);
        }
        VEC_PUSH_VEC(outer, inner);
    }

    WC_ASSERT_EQ_U64(genVec_size(outer), 4);
    for (int row = 0; row < 4; row++) {
        genVec* slot = VEC_AT_MUT(outer, genVec, (u64)row);
        WC_ASSERT_EQ_U64(genVec_size(slot), (u64)(row + 1));
        for (int col = 0; col < (row + 1); col++) {
            WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(slot, (u64)col), (row * 10) + col);
        }
    }

    genVec_destroy(outer);
}

static void test_vecval_copy_outer(void)
{
    genVec* src = VEC_OF_VEC(4);
    for (int row = 0; row < 3; row++) {
        genVec* inner = VEC_OF_INT(4);
        for (int i = 0; i < 3; i++) { VEC_PUSH(inner, i); }
        VEC_PUSH_VEC(src, inner);
    }

    genVec dest;
    genVec_init_stk(0, sizeof(genVec), vec_copy, vec_move, vec_del, &dest);
    genVec_copy(&dest, src);

    /* modify src inner — dest must be independent */
    genVec* src_slot  = VEC_AT_MUT(src, genVec, 0);
    int     x         = 777;
    genVec_replace(src_slot, 0, (u8*)&x);

    genVec* dest_slot = VEC_AT_MUT(&dest, genVec, 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(dest_slot, 0), 0);

    genVec_destroy(src);
    genVec_destroy_stk(&dest);
}

static void test_vecval_triggers_growth(void)
{
    genVec* outer = VEC_OF_VEC(2);

    for (int i = 0; i < 20; i++) {
        genVec* inner = VEC_OF_INT(2);
        int v = i;
        VEC_PUSH(inner, v);
        VEC_PUSH_VEC(outer, inner);
    }

    WC_ASSERT_EQ_U64(genVec_size(outer), 20);
    for (int i = 0; i < 20; i++) {
        genVec* slot = VEC_AT_MUT(outer, genVec, (u64)i);
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(slot, 0), i);
    }

    genVec_destroy(outer);
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 4 — Vec<genVec*> by POINTER  (vec of int vec pointers)
 *
 * Slot holds genVec* (8 bytes). Inner vec fully on heap.
 * Stable addresses across outer vec growth.
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_vecptr_push_move_nulls_src(void)
{
    genVec* outer = VEC_OF_VEC_PTR(4);
    genVec* inner = VEC_OF_INT(4);
    VEC_PUSH(inner, 42);

    VEC_PUSH_VEC_PTR(outer, inner);
    WC_ASSERT_NULL(inner);

    genVec* stored = VEC_AT(outer, genVec*, 0);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(stored, 0), 42);
    genVec_destroy(outer);
}

static void test_vecptr_address_stable_after_growth(void)
{
    genVec* outer = VEC_OF_VEC_PTR(2);
    genVec* inner = VEC_OF_INT(4);
    VEC_PUSH(inner, 99);
    VEC_PUSH_VEC_PTR(outer, inner);

    genVec* addr = VEC_AT(outer, genVec*, 0); /* address of inner vec */

    for (int i = 0; i < 30; i++) {
        genVec* filler = VEC_OF_INT(1);
        VEC_PUSH(filler, i);
        VEC_PUSH_VEC_PTR(outer, filler);
    }

    WC_ASSERT_TRUE(VEC_AT(outer, genVec*, 0) == addr);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(addr, 0), 99);
    genVec_destroy(outer);
}

static void test_vecptr_copy_outer(void)
{
    genVec* src   = VEC_OF_VEC_PTR(4);
    genVec* inner = VEC_OF_INT(4);
    int v = 5;
    VEC_PUSH(inner, v);
    VEC_PUSH_VEC_PTR(src, inner);

    genVec dest;
    genVec_init_stk(0, sizeof(genVec*), vec_copy_ptr, vec_move_ptr, vec_del_ptr, &dest);
    genVec_copy(&dest, src);

    /* modify src inner — dest copy must be independent */
    int x = 123;
    genVec_replace(VEC_AT(src, genVec*, 0), 0, (u8*)&x);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(VEC_AT(&dest, genVec*, 0), 0), 5);

    genVec_destroy(src);
    genVec_destroy_stk(&dest);
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 5 — HashMap<int, genVec> (map with complex value)
 *
 * Tests the MAP_PUT_INT_STR and vec-as-value patterns.
 * ══════════════════════════════════════════════════════════════════════════ */

static hashmap* int_vec_map(void)
{
    return hashmap_create(sizeof(int), sizeof(genVec),
                          NULL, NULL,
                          NULL, vec_copy,
                          NULL, vec_move,
                          NULL, vec_del);
}

static void test_map_int_vec_put_move(void)
{
    hashmap* m = int_vec_map();
    genVec*  v = VEC_OF_INT(4);
    for (int i = 0; i < 5; i++) { VEC_PUSH(v, i); }

    int key = 10;
    hashmap_put_val_move(m, (u8*)&key, (u8**)&v);
    WC_ASSERT_NULL(v);

    genVec* stored = (genVec*)hashmap_get_ptr(m, (u8*)&key);
    WC_ASSERT_NOT_NULL(stored);
    WC_ASSERT_EQ_U64(genVec_size(stored), 5);
    for (int i = 0; i < 5; i++) {
        WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(stored, (u64)i), i);
    }

    hashmap_destroy(m);
}

static void test_map_int_vec_copy_independence(void)
{
    hashmap* m   = int_vec_map();
    genVec*  src = VEC_OF_INT(4);
    for (int i = 0; i < 3; i++) { VEC_PUSH(src, i); }

    int key = 1;
    hashmap_put(m, (u8*)&key, (u8*)src);

    /* mutate src — stored copy must not be affected */
    int x = 999;
    genVec_replace(src, 0, (u8*)&x);

    genVec* stored = (genVec*)hashmap_get_ptr(m, (u8*)&key);
    WC_ASSERT_EQ_INT(*(int*)genVec_get_ptr(stored, 0), 0);

    genVec_destroy(src);
    hashmap_destroy(m);
}

static void test_map_str_str_macro(void)
{
    hashmap* m = hashmap_create(
        sizeof(String), sizeof(String),
        murmurhash3_str, str_cmp,
        str_copy, str_copy,
        str_move, str_move,
        str_del,  str_del);

    MAP_PUT_STR_STR(m, "name",  "Alice");
    MAP_PUT_STR_STR(m, "city",  "Cairo");
    MAP_PUT_STR_STR(m, "lang",  "C");

    String probe;
    string_create_stk(&probe, "city");
    String* val = (String*)hashmap_get_ptr(m, (u8*)&probe);
    WC_ASSERT_NOT_NULL(val);
    WC_ASSERT(string_equals_cstr(val, "Cairo"));
    string_destroy_stk(&probe);

    WC_ASSERT_EQ_U64(hashmap_size(m), 3);
    hashmap_destroy(m);
}

static void test_map_int_str_macro(void)
{
    hashmap* m = hashmap_create(
        sizeof(int), sizeof(String),
        NULL, NULL,
        NULL, str_copy,
        NULL, str_move,
        NULL, str_del);

    MAP_PUT_INT_STR(m, 1, "one");
    MAP_PUT_INT_STR(m, 2, "two");
    MAP_PUT_INT_STR(m, 3, "three");

    int key = 2;
    String* val = (String*)hashmap_get_ptr(m, (u8*)&key);
    WC_ASSERT_NOT_NULL(val);
    WC_ASSERT(string_equals_cstr(val, "two"));
    WC_ASSERT_EQ_U64(hashmap_size(m), 3);
    hashmap_destroy(m);
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 6 — Strategy A vs B comparison
 * Both strategies produce identical observable behaviour at the API level.
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_strategy_a_b_same_content(void)
{
    genVec* by_val = VEC_OF_STR(4);
    genVec* by_ptr = VEC_OF_STR_PTR(4);

    const char* words[] = {"hello", "world", "foo", "bar"};
    for (int i = 0; i < 4; i++) {
        VEC_PUSH_CSTR(by_val, words[i]);
        VEC_PUSH_CSTR(by_ptr, words[i]);
    }

    for (int i = 0; i < 4; i++) {
        String* a = VEC_AT_MUT(by_val, String,  (u64)i);
        String* b = VEC_AT    (by_ptr, String*, (u64)i);
        WC_ASSERT(string_equals(a, b));
    }

    genVec_destroy(by_val);
    genVec_destroy(by_ptr);
}

static void test_strategy_b_pointer_outlives_growth(void)
{
    genVec* v   = VEC_OF_STR_PTR(2);
    VEC_PUSH_CSTR(v, "anchor");
    String* anchor = VEC_AT(v, String*, 0);

    /* force 10x growth */
    for (int i = 0; i < 60; i++) { VEC_PUSH_CSTR(v, "x"); }

    /* anchor still points to the same heap String, content intact */
    WC_ASSERT_TRUE(VEC_AT(v, String*, 0) == anchor);
    WC_ASSERT(string_equals_cstr(anchor, "anchor"));
    genVec_destroy(v);
}


/* ── Suite entry point ───────────────────────────────────────────────────── */

void complex_suite(void)
{
    WC_SUITE("Vec<String> by value (Strategy A)");
    WC_RUN(test_strval_push_copy_independent);
    WC_RUN(test_strval_push_move_nulls_src);
    WC_RUN(test_strval_push_cstr);
    WC_RUN(test_strval_foreach_mutates_in_place);
    WC_RUN(test_strval_pop_returns_owned_string);
    WC_RUN(test_strval_at_mut_modifies_in_place);
    WC_RUN(test_strval_copy_vec);
    WC_RUN(test_strval_triggers_growth);

    WC_SUITE("Vec<String*> by pointer (Strategy B)");
    WC_RUN(test_strptr_push_copy_independent);
    WC_RUN(test_strptr_push_move_nulls_src);
    WC_RUN(test_strptr_address_stable_after_growth);
    WC_RUN(test_strptr_foreach_dereference);
    WC_RUN(test_strptr_replace_slot_pointer);

    WC_SUITE("Vec<genVec> by value (vec of int vecs)");
    WC_RUN(test_vecval_push_move);
    WC_RUN(test_vecval_push_copy_independent);
    WC_RUN(test_vecval_multiple_inner_vecs);
    WC_RUN(test_vecval_copy_outer);
    WC_RUN(test_vecval_triggers_growth);

    WC_SUITE("Vec<genVec*> by pointer");
    WC_RUN(test_vecptr_push_move_nulls_src);
    WC_RUN(test_vecptr_address_stable_after_growth);
    WC_RUN(test_vecptr_copy_outer);

    WC_SUITE("HashMap with complex values");
    WC_RUN(test_map_int_vec_put_move);
    WC_RUN(test_map_int_vec_copy_independence);
    WC_RUN(test_map_str_str_macro);
    WC_RUN(test_map_int_str_macro);

    WC_SUITE("Strategy A vs B comparison");
    WC_RUN(test_strategy_a_b_same_content);
    WC_RUN(test_strategy_b_pointer_outlives_growth);
}


