#ifndef NEW_STR_H
#define NEW_STR_H

#include "common.h"


#define STR_SSO_SIZE 23

#ifndef STRING_GROWTH
    #define STRING_GROWTH 1.5F      // vec capacity multiplier
#endif
#ifndef STRING_SHRINK_AT
    #define STRING_SHRINK_AT 0.25F  // % filled to shrink at (25% filled)
#endif
#ifndef STRING_SHRINK_BY
    #define STRING_SHRINK_BY 0.5F   // capacity divisor (half)
#endif



typedef struct {
    union {
        char* heap;
        char stk[STR_SSO_SIZE];
    }; 
    b8 sso;
    u64 size;
    u64 capacity;
} str;
// 23 1 8 8 = 40


str* str_create(void);
str* str_from_cstr(const char* cstr);
void str_create_stk(str* str, const char* cstr);

void str_destroy(str* str);
void str_destroy_stk(str* str);


void str_append_char(str* str, char c);
void str_append_cstr(str* str, const char* cstr);

char str_pop_char(str* str);

char str_char_at(const str* str, u64 i);
void str_set_char(str* str, u64 i, char c);

void str_print(const str* str);

#endif // NEW_STR_H
