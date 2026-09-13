#ifndef WCJSON_H
#define WCJSON_H

#include "Arena.h"
#include "gen_vector.h"
#include "HashMap.h"


typedef enum {
    WCJSON_UNDEFINED  = 0,      // json null value?
    WCJSON_OBJECT     = 1 << 0, // HashMap
    WCJSON_ARRAY      = 1 << 1, // genvec
    WCJSON_STRING     = 1 << 2, // String
    WCJSON_PRIMITIVES = 1 << 3, // javascript number
    WCJSON_BOOL       = 1 << 4  // boolean
} wcjson_type;


typedef struct {
    union {
        HashMap* map;
        GenVec*  vec;
        String*  str;
        double   num;
        bool     flg;
    };
    // undefined is union ZERO'ED
    wcjson_type type;
} wcjson_val;

typedef struct {
    u64 num_objs;   // including root
    u64 num_arrs;
    u64 num_strs;
    u64 num_nums;
    u64 num_flgs;
    u64 allocated_memory;
} wcjson_metadata;

typedef struct {
    wcjson_val      root;
    wcjson_metadata metadata;
    Arena Arena;        // owns everything
} wcjson;


wcjson* wcjson_create_from_file(const char* filename);







#endif // WCJSON_H
