#include "new_str.h"
#include "common.h"
#include <string.h>



#define GET_STR(str) ((str)->sso ? (str)->stk : (str)->heap)

#define MAYBE_GROW(str)                                                      \
    do {                                                                     \
        if (!str->sso && (!(str)->heap || (str)->size >= (str)->capacity)) { \
            string_grow(str);                                                \
        }                                                                    \
    } while (0)

#define MAYBE_SHRINK(str)                                                                     \
    do {                                                                                      \
        if (!str->sso && ((str)->size <= (u64)((float)(str)->capacity * STRING_SHRINK_AT))) { \
            string_shrink(str);                                                               \
        }                                                                                     \
    } while (0)

static u64  cstr_len(const char* cstr);
static void string_grow(str* str);
static void string_shrink(str* str);


str* str_create(void)
{
    str* s = malloc(sizeof(str));
    CHECK_FATAL(!s, "str malloc failed");

    s->size     = 0;
    s->capacity = STR_SSO_SIZE;
    s->sso      = true;

    return s;
}

str* str_from_cstr(const char* cstr)
{
    CHECK_FATAL(!cstr, "cstr is null");

    str* s = malloc(sizeof(str));
    CHECK_FATAL(!s, "str malloc failed");

    u64 len = cstr_len(cstr);
    if (len > STR_SSO_SIZE) {
        s->sso  = false;
        s->heap = malloc(len);
        CHECK_FATAL(!s->heap, "str heap malloc failed");
    } else {
        s->sso = true;
    }

    s->capacity = len;
    memcpy(GET_STR(s), cstr, len);

    return s;
}

void str_create_stk(str* str, const char* cstr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");
}



static u64 cstr_len(const char* cstr)
{
    u64 i = 0;
    while (cstr[i++] != '\0') {
    }
    return i - 1;
}


static void string_grow(str* str)
{
    CHECK_FATAL(!str, "str is null");

    u64 new_cap;
    if (str->capacity < STRING_MIN_CAPACITY) {
        new_cap = str->capacity + 1;
    } else {
        new_cap = (u64)((float)str->capacity * GENVEC_GROWTH);
        if (new_cap <= str->capacity) {
            new_cap = str->capacity + 1;
        }
    }

    u8* new_data = realloc(str->heap, new_cap);
    CHECK_FATAL(!new_data, "data realloc failed");

    str->heap     = new_data;
    str->capacity = new_cap;
}


static void string_shrink(str* str)
{
    CHECK_FATAL(!vec, "vec is null");

    u64 reduced_cap = (u64)((float)vec->capacity * GENVEC_SHRINK_BY);
    if (reduced_cap < vec->size || reduced_cap == 0) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, reduced_cap));
    if (!new_data) {
        WARN("shrink realloc failed");
        return;
    }

    vec->data     = new_data;
    vec->capacity = reduced_cap;
}
