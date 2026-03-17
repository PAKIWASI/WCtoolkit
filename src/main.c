#include "hashmap.h"
#include "wc_helpers.h"



static void m_put_cp_int_str(hashmap* m, int key, const char* cstr)
{
    String str;
    string_create_stk(&str, cstr);
    hashmap_put(m, (u8*)&key, (u8*)&str);
}


int main(void)
{
    hashmap* map = hashmap_create(sizeof(int), sizeof(String), NULL, NULL, NULL, &wc_str_ops);

    m_put_cp_int_str(map, 1, "hello");
    m_put_cp_int_str(map, 2, "world");
    m_put_cp_int_str(map, 1, "HellO");
    m_put_cp_int_str(map, 10000, "hey");
    m_put_cp_int_str(map, -1, "what");

    // BUG: multiple keys?
    for (int i = 0; i < 1000; i++) {
        m_put_cp_int_str(map, i, "hel");
    }

    hashmap_print(map, wc_print_int, str_print);

    hashmap_destroy(map);
}
