#include "hashmap.h"
#include "common.h"
#include "map_setup.h"


/*
====================PRIVATE DECLERATIONS====================
*/



/*
====================PUBLIC FUNCTIONS====================
*/

hashmap* hashmap_create(u32 key_size, u32 val_size, custom_hash_fn hash_fn, compare_fn cmp_fn,
                        const container_ops* key_ops, const container_ops* val_ops)
{
    CHECK_FATAL(key_size == 0, "key_size can't be 0");
    CHECK_FATAL(val_size == 0, "val_size can't be 0");

    hashmap* map = malloc(sizeof(hashmap));

    map->size = 0;
    map->capacity = HASHMAP_INIT_CAPACITY;

    map->key_size = key_size;
    map->val_size = val_size;

    map->hash_fn = hash_fn ? hash_fn : fnv1a_hash;
    map->cmp_fn = cmp_fn ? cmp_fn : default_compare;

    map->key_ops = key_ops;
    map->val_ops = val_ops;
}



/*
====================PRIVATE FUNCTIONS====================
*/
