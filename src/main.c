#include "hashmap.h"


int main(void)
{
    hashmap* map = hashmap_create(sizeof(int), sizeof(int), NULL, NULL, NULL, NULL);

    print_hex(map->keys, (1 + sizeof(int)) * HASHMAP_INIT_CAPACITY, 5);

    hashmap_destroy(map);
    return 0;
}
