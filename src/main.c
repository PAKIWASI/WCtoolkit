#include "gen_vector.h"
#include "hashmap.h"
#include "map_setup.h"
#include "wc_allocator.h"
#include "wc_helpers.h"
#include "wc_macros.h"
#include "wc_string.h"


static int run1(void);
static int run2(void);

int main(void)
{
    // run1();
    run2();
    return 0;
}


static int run1(void)
{
    HashMap m = HashMap_create(WC_LIBC, sizeof(String), sizeof(String), wyhash_str, str_cmp, &wc_str_ops, &wc_str_ops);

    String key = String_from_cstr(WC_LIBC, "hello");
    String val = String_from_cstr(WC_LIBC, "world");
    HashMap_put_move(&m, &key, &val);

    // key, val zeroed out
    // TODO: should we check for zero struct?
    HashMap_put_move(&m, &key, &val);


    HashMap_print(&m, str_print, str_print);

    HashMap_destroy(&m);
    return 0;
}

static void vec_of_str_print(const void* elm)
{
    GenVec_print((const GenVec*)elm, str_print);
}

static int run2(void)
{
    HashMap m = MAP_OF(String, GenVec); // String -> GenVec<String>

    String k1 = String_from_cstr(WC_LIBC, "hello");
    GenVec v1 = VEC_OF(String, 10);
    VEC_PUSH_CSTR(&v1, "world");
    VEC_PUSH_CSTR(&v1, "w2");
    VEC_PUSH_CSTR(&v1, "!");
    MAP_PUT_MOVE(&m, k1, v1);

    String k2 = String_from_cstr(WC_LIBC, "world");
    GenVec v2 = VEC_OF(String, 10);
    VEC_PUSH_CSTR(&v2, "hello");
    MAP_PUT_MOVE(&m, k2, v2);

    HashMap_print(&m, str_print, vec_of_str_print);

    String k3 = String_from_cstr(WC_LIBC, "hello");
    GenVec v3 = VEC_OF(String, 10);
    VEC_PUSH_CSTR(&v3, "jfkdsjlkfjdslj");
    // BUG: ✘  Static assertion failed due to requirement '!_Generic((Strin
    // MAP_PUT_VAL_MOVE(&m, *(String*)GenVec_get_ptr(HashMap_get_ptr(&m, &k3), 1), v3);
    MAP_PUT_MOVE(&m, *(String*)GenVec_get_ptr(HashMap_get_ptr(&m, &k3), 1), v3);

    HashMap_print(&m, str_print, vec_of_str_print);

    HashMap_destroy(&m);
    return 0;
}

static int run3(void)
{
    // TODO: any way to enforce genvec of string?
    GenVec v = VEC_OF(GenVec, 10); // GenVec<GenVec<String>>



    GenVec_destroy(&v);
    return 0;
}


