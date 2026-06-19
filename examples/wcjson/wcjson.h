#ifndef WCJSON_H
#define WCJSON_H

#include "gen_vector.h"
#include "hashmap.h"


typedef enum {
    WCJSON_UNDEFINED  = 0,      // json null value?
    WCJSON_OBJECT     = 1 << 0, // hashmap
    WCJSON_ARRAY      = 1 << 1, // genvec
    WCJSON_STRING     = 1 << 2, // String
    WCJSON_PRIMITIVES = 1 << 3  // pod
} wcjson_type;


typedef struct {
    union {
        hashmap* map;
        genVec*  vec;
        String*  str;
        u64*     pod;   // primitives or undefined (=NULL)
    };
    wcjson_type type;
} wcjson_val;



wcjson_val wcjson_create(void);







#endif // WCJSON_H
