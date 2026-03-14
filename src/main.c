#include "hashmap.h"


static inline void put(hashmap* map, int key, int val) {
    hashmap_put(map, cast(key), cast(val));
}


int main(void)
{
    hashmap* map = hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);

    put(map, 1, 1);
    put(map, 2, 2);
    put(map, 3, 3);
    put(map, 4, 4);

    hashmap_print(map, wc_print_int, wc_print_int);

    hashmap_del(map, (u8*)&(int){1}, NULL);

    hashmap_print(map, wc_print_int, wc_print_int);

    hashmap_destroy(map);
    return 0;
}
