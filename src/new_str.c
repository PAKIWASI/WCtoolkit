#include "new_str.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#define GET_STR(str)       ((str)->sso ? (str)->stk : (str)->heap)
#define GET_STR_AT(str, i) (((str)->sso ? (str)->stk : (str)->heap)[i])
#define STR_REMAINING(str) ((str)->capacity - (str)->size)

#define MAYBE_GROW(str)                       \
    do {                                      \
        if ((str)->size >= (str)->capacity) { \
            if ((str)->sso) {                 \
                stk_to_heap(str);             \
            } else {                          \
                string_grow(str);             \
            }                                 \
        }                                     \
    } while (0)

#define MAYBE_SHRINK(str)                                                                     \
    do {                                                                                      \
        if (!str->sso && ((str)->size <= (u64)((float)(str)->capacity * STRING_SHRINK_AT))) { \
            string_shrink(str);                                                               \
        }                                                                                     \
    } while (0)

static u64  cstr_len(const char* cstr);
static void str_copy_len(char* dest, const char* src, u64 len);
static void stk_to_heap(str* str);
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
        s->heap = malloc(len);
        CHECK_FATAL(!s->heap, "str heap malloc failed");
        s->sso      = false;
        s->capacity = len;
    } else {
        s->sso      = true;
        s->capacity = STR_SSO_SIZE;
    }

    s->size = len;
    // memcpy(GET_STR(s), cstr, len);
    str_copy_len(GET_STR(s), cstr, len);

    return s;
}

void str_create_stk(str* str, const char* cstr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

    u64 len = cstr_len(cstr);
    if (len > STR_SSO_SIZE) {
        str->heap = malloc(len);
        CHECK_FATAL(!str->heap, "str heap malloc failed");
        str->sso      = false;
        str->capacity = len;
    } else {
        str->sso      = true;
        str->capacity = STR_SSO_SIZE;
    }

    str->size = len;
    // memcpy(GET_STR(str), cstr, len);
    str_copy_len(GET_STR(str), cstr, len);
}

void str_destroy(str* str)
{
    CHECK_FATAL(!str, "str is null");

    if (!str->sso) {
        free(str->heap);
    }

    free(str);
}

void str_destroy_stk(str* str)
{
    CHECK_FATAL(!str, "str is null");

    if (!str->sso) {
        free(str->heap);
    }
}

void str_reserve(str* str, u64 len)
{
    CHECK_FATAL(!str, "str is null");
    // don't shrink
    if (len <= str->capacity) { return; }

    if (str->sso) { stk_to_heap(str); }

    char* new_data = realloc(str->heap, len);
    CHECK_FATAL(!new_data, "new_data realloc failed");

    str->heap = new_data;
    str->capacity = len;
}

void str_append_char(str* str, char c)
{
    CHECK_FATAL(!str, "str is null");

    MAYBE_GROW(str);

    GET_STR_AT(str, str->size++) = c;
}

void str_append_cstr(str* str, const char* cstr)
{
    CHECK_FATAL(!str, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

    u64 len = cstr_len(cstr);
    if (len > STR_REMAINING(str)) {
        str_reserve(str, str->size + len);
    }

}

void str_print(const str* str)
{
    CHECK_FATAL(!str, "str is null");

    putchar('"');
    for (u64 i = 0; i < str->size; i++) {
        putchar(GET_STR_AT(str, i));
    }
    putchar('"');
}


static u64 cstr_len(const char* cstr)
{
    u64 i = 0;
    while (cstr[i++] != '\0') {
    }
    return i - 1;
}

static void str_copy_len(char* dest, const char* src, u64 len)
{
    u64 i = 0;
    while (i != len) {
        dest[i] = src[i];
        i++;
    }
}

// only called in sso mode
static void stk_to_heap(str* str)
{
    u64   new_cap  = (u64)((float)str->capacity * STRING_GROWTH);
    char* new_data = malloc(new_cap);
    CHECK_FATAL(!new_data, "new data malloc failed");

    // memcpy(new_data, str->stk, str->size);
    str_copy_len(new_data, str->stk, str->size);

    str->heap     = new_data;
    str->capacity = new_cap;
    str->sso      = false;
}

// only get here when in heap mode
static void string_grow(str* str)
{
    u64   new_cap  = (u64)((float)str->capacity * STRING_GROWTH);
    char* new_data = realloc(str->heap, new_cap);
    CHECK_FATAL(!new_data, "data realloc failed");

    str->heap     = new_data;
    str->capacity = new_cap;
}

// only get here in heap mode
static void string_shrink(str* str)
{
    u64 reduced_cap = (u64)((float)str->capacity * STRING_SHRINK_BY);
    if (reduced_cap < str->size || reduced_cap == 0) {
        return;
    }

    char* new_data = realloc(str->heap, reduced_cap);
    if (!new_data) {
        WARN("shrink realloc failed");
        return;
    }

    str->heap     = new_data;
    str->capacity = reduced_cap;
}
