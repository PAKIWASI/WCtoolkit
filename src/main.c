#include "String.h"
#include "hashmap.h"
#include "wc_helpers.h"

#include <string.h>



static void m_put(hashmap* m, int k, const char* v)
{
    String* s = string_from_cstr(v);
    hashmap_put(m, cast(k), castptr(s));
    string_destroy(s);
}



int main(void)
{
    hashmap* m = hashmap_create(sizeof(int), sizeof(String), NULL, NULL, NULL, &wc_str_ops);

    m_put(m, 15, "hello");
    m_put(m, 14, "hello");
    m_put(m, 13, "hello");
    m_put(m, 12, "hello");
    m_put(m, 11, "hello");
    m_put(m, 10, "hello");
    m_put(m, 9, "hello");
    m_put(m, 8, "hello");
    m_put(m, 7, "hello");
    m_put(m, 6, "hello");
    m_put(m, 5, "hello");
    // m_put(m, 4, "hello");
    // m_put(m, 3, "hello");
    // m_put(m, 2, "hello");
    // m_put(m, 1, "hello");

    hashmap_print(m, wc_print_int, str_print);

    int k = 4;
    printf("%d\n", hashmap_has(m, cast(k)));

    hashmap_destroy(m);
    return 0;
}
