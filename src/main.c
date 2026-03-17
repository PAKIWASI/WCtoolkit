#include "String.h"
#include "common.h"
#include "hashmap.h"
#include "wc_helpers.h"
#include <string.h>


static void m_put_cp_int_str(hashmap* m, int key, const char* cstr)
{
    String str;
    string_create_stk(&str, cstr);
    hashmap_put(m, (u8*)&key, (u8*)&str);
    string_destroy_stk(&str);
}

static void m_put_int_str_mv(hashmap* m, int key, const char* cstr)
{
    String* str = string_from_cstr(cstr);
    hashmap_put_val_move(m, (u8*)&key, (u8**)&str);
}


static int test1(void)
{
    hashmap* map = hashmap_create(sizeof(int), sizeof(String), NULL, NULL, NULL, &wc_str_ops);

    // m_put_cp_int_str(map, 1, "hello");
    // m_put_cp_int_str(map, 2, "world");
    // m_put_cp_int_str(map, 1, "HellO");
    // m_put_cp_int_str(map, 1000, "hey");
    // m_put_cp_int_str(map, -1, "what");

    // BUG: multiple keys?
    for (int i = 0; i < 100; i++) {
        m_put_int_str_mv(map, i, "hel");
    }

    // m_put_int_str_mv(map, 11, "hello");
    // m_put_int_str_mv(map, 12, "hello");
    // m_put_int_str_mv(map, 13, "hello");
    // m_put_int_str_mv(map, 14, "hello");
    // m_put_int_str_mv(map, 15, "hello");
    // m_put_int_str_mv(map, 16, "hello");

    hashmap_print(map, wc_print_int, str_print);

    print_hex(map->psls, map->capacity, 8);


    hashmap_destroy(map);
    return 0;
}


static void m_put_cp_int_sptr(hashmap* m, int k, const char* cstr)
{
    String* str = string_from_cstr(cstr);
    hashmap_put(m, (u8*)&k, (u8*)&str);
    string_destroy(str);
}

static void m_put_int_sptr_mv(hashmap* m, int k, const char* cstr)
{
    String* str = string_from_cstr(cstr);
    hashmap_put_val_move(m, (u8*)&k, (u8**)&str);
}

static int test2(void)
{
    hashmap* map = hashmap_create(sizeof(int), sizeof(String*), NULL, NULL, NULL, &wc_str_ptr_ops);

    m_put_cp_int_sptr(map, 1, "hello");
    m_put_cp_int_sptr(map, 1, "HELLO");
    m_put_cp_int_sptr(map, 2, "hello");
    m_put_cp_int_sptr(map, -1, "hello");

    m_put_int_sptr_mv(map, 10, "hel");

    hashmap_print(map, wc_print_int, str_print_ptr);

    hashmap_destroy(map);
    return 0;
}


int main(void)
{
    test1();
}


