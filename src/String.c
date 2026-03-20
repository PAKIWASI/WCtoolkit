#include "String.h"
#include "wc_errno.h"

#include <string.h>


//  Internal macros

#define IS_SSO(s)        ((s)->capacity == STR_SSO_SIZE)
#define GET_STR(s)       (IS_SSO(s) ? (s)->stk : (s)->heap)
#define GET_STR_AT(s, i) (GET_STR(s)[i])
#define STR_REMAINING(s) ((s)->capacity - (s)->size)

// Grow if full.
#define MAYBE_GROW(s)                     \
    do {                                  \
        if ((s)->size >= (s)->capacity) { \
            if (IS_SSO(s)) {              \
                stk_to_heap(s);           \
            } else {                      \
                string_grow(s);           \
            }                             \
        }                                 \
    } while (0)



//  Private helpers

static u64  cstr_len(const char* cstr);
static void str_copy_n(char* dest, const char* src, u64 n);
static void stk_to_heap(String* s);
static void heap_to_stk(String* s);
static void string_grow(String* s);
static void ensure_capacity(String* s, u64 needed);



//  Construction / Destruction

String* string_create(void)
{
    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    s->size     = 0;
    s->capacity = STR_SSO_SIZE;

    return s;
}

String* string_from_cstr(const char* cstr)
{
    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    string_create_stk(s, cstr);
    return s;
}

String* string_from_string(const String* other)
{
    CHECK_FATAL(!other, "other is null");

    String* s = malloc(sizeof(String));
    CHECK_FATAL(!s, "malloc failed");

    s->size     = 0;
    s->capacity = STR_SSO_SIZE;

    if (other->size > 0) {
        ensure_capacity(s, other->size);
        str_copy_n(GET_STR(s), GET_STR(other), other->size);
        s->size = other->size;
    }

    return s;
}

void string_create_stk(String* s, const char* cstr)
{
    CHECK_FATAL(!s, "str is null");

    s->size     = 0;
    s->capacity = STR_SSO_SIZE;

    if (!cstr) {
        return;
    }

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, len);
    str_copy_n(GET_STR(s), cstr, len);
    s->size = len;
}

void string_destroy(String* s)
{
    CHECK_FATAL(!s, "str is null");
    string_destroy_stk(s);
    free(s);
}

void string_destroy_stk(String* s)
{
    CHECK_FATAL(!s, "str is null");

    if (!IS_SSO(s)) {
        free(s->heap);
        s->heap = NULL;
    }

    s->size     = 0;
    s->capacity = STR_SSO_SIZE; // leave in valid SSO state, not capacity=0
}

void string_move(String* dest, String** src)
{
    CHECK_FATAL(!src, "src ptr is null");
    CHECK_FATAL(!*src, "*src is null");
    CHECK_FATAL(!dest, "dest is null");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    string_destroy_stk(dest);
    memcpy(dest, *src, sizeof(String));

    // Zero out src so its destructor is harmless, then free the struct
    (*src)->size     = 0;
    (*src)->capacity = STR_SSO_SIZE;
    free(*src);
    *src = NULL;
}

void string_copy(String* dest, const String* src)
{
    CHECK_FATAL(!src, "src is null");
    CHECK_FATAL(!dest, "dest is null");

    if (src == dest) {
        return;
    }

    string_destroy_stk(dest);

    dest->size     = 0;
    dest->capacity = STR_SSO_SIZE;

    if (src->size > 0) {
        ensure_capacity(dest, src->size);
        str_copy_n(GET_STR(dest), GET_STR(src), src->size);
        dest->size = src->size;
    }
}


//  Capacity

void string_reserve(String* s, u64 new_cap)
{
    CHECK_FATAL(!s, "str is null");

    if (new_cap <= s->capacity) {
        return;
    }
    ensure_capacity(s, new_cap);
}

void string_reserve_char(String* s, u64 new_cap, char c)
{
    CHECK_FATAL(!s, "str is null");
    if (new_cap <= s->capacity) {
        // Fill from current size up to new_cap within existing allocation.
        char* buf = GET_STR(s);
        for (u64 i = s->size; i < new_cap; i++) {
            buf[i] = c;
        }
        s->size = new_cap;
        return;
    }

    u64 old_size = s->size;
    ensure_capacity(s, new_cap);

    char* buf = GET_STR(s);
    for (u64 i = old_size; i < new_cap; i++) {
        buf[i] = c;
    }
    s->size = new_cap;
}

void string_shrink_to_fit(String* s)
{
    CHECK_FATAL(!s, "str is null");

    if (IS_SSO(s)) {
        return;
    } // already optimal

    if (s->size == 0) {
        free(s->heap);
        s->heap     = NULL;
        s->capacity = STR_SSO_SIZE;
        return;
    }

    if (s->size <= STR_SSO_SIZE) {
        // Bring back to SSO.
        heap_to_stk(s);
        return;
    }

    char* new_data = realloc(s->heap, s->size);
    if (!new_data) {
        WARN("shrink_to_fit realloc failed");
        return;
    }
    s->heap     = new_data;
    s->capacity = s->size;
}


//  Conversion

char* string_to_cstr(const String* s)
{
    CHECK_FATAL(!s, "str is null");

    char* out = malloc(s->size + 1);
    CHECK_FATAL(!out, "malloc failed");

    if (s->size > 0) {
        str_copy_n(out, GET_STR(s), s->size);
    }
    out[s->size] = '\0';

    return out;
}

char* string_data_ptr(const String* s)
{
    CHECK_FATAL(!s, "str is null");
    if (s->size == 0) {
        return NULL;
    }
    // Cast away const intentionally: caller may mutate via this pointer.
    return (char*)(IS_SSO(s) ? s->stk : s->heap);
}


//  Modification

void string_append_char(String* s, char c)
{
    CHECK_FATAL(!s, "str is null");
    MAYBE_GROW(s);
    GET_STR_AT(s, s->size++) = c;
}

void string_append_cstr(String* s, const char* cstr)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, s->size + len);
    str_copy_n(GET_STR(s) + s->size, cstr, len);
    s->size += len;
}

void string_append_string(String* s, const String* other)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!other, "other is null");

    if (other->size == 0) {
        return;
    }

    ensure_capacity(s, s->size + other->size);
    str_copy_n(GET_STR(s) + s->size, GET_STR(other), other->size);
    s->size += other->size;
}

void string_append_string_move(String* s, String** other)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!other, "other ptr is null");
    CHECK_FATAL(!*other, "*other is null");

    if ((*other)->size > 0) {
        string_append_string(s, *other);
    }

    string_destroy(*other);
    *other = NULL;
}

char string_pop_char(String* s)
{
    CHECK_FATAL(!s, "str is null");
    WC_SET_RET(WC_ERR_EMPTY, s->size == 0, '\0');

    char c = GET_STR_AT(s, --s->size);
    return c;
}

void string_insert_char(String* s, u64 i, char c)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(i > s->size, "index out of bounds");

    MAYBE_GROW(s);

    char* buf = GET_STR(s);
    // Shift right.
    for (u64 j = s->size; j > i; j--) {
        buf[j] = buf[j - 1];
    }
    buf[i] = c;
    s->size++;
}

void string_insert_cstr(String* s, u64 i, const char* cstr)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");
    CHECK_FATAL(i > s->size, "index out of bounds");

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, s->size + len);

    char* buf = GET_STR(s);
    // Shift existing chars right by len positions.
    for (u64 j = s->size; j > i; j--) {
        buf[j + len - 1] = buf[j - 1];
    }
    str_copy_n(buf + i, cstr, len);
    s->size += len;
}

void string_insert_string(String* s, u64 i, const String* other)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!other, "other is null");
    CHECK_FATAL(i > s->size, "index out of bounds");

    if (other->size == 0) {
        return;
    }

    CHECK_WARN_RET(s == other, , "can't insert aliasing(same) strings");

    u64 len = other->size;
    ensure_capacity(s, s->size + len);

    char* buf = GET_STR(s);
    for (u64 j = s->size; j > i; j--) {
        buf[j + len - 1] = buf[j - 1];
    }
    str_copy_n(buf + i, GET_STR(other), len);
    s->size += len;
}

void string_remove_char(String* s, u64 i)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(i >= s->size, "index out of bounds");

    char* buf = GET_STR(s);
    for (u64 j = i; j < s->size - 1; j++) {
        buf[j] = buf[j + 1];
    }
    s->size--;
}

// 1 2 3 4 5
//       |    |
// (3, 3)

void string_remove_range(String* s, u64 start, u64 len)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(start >= s->size, "l out of bounds");

    if (start + len >= s->size) {
        len = s->size - start;
    }

    char* buf = GET_STR(s);

    for (u64 j = start; j < len; j++) {
        buf[j] = buf[j + len];
    }
    s->size -= len;
}

void string_clear(String* s)
{
    CHECK_FATAL(!s, "str is null");
    s->size = 0;
}


//  Access

char string_char_at(const String* s, u64 i)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(i >= s->size, "index out of bounds");
    return GET_STR_AT(s, i);
}

void string_set_char(String* s, u64 i, char c)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(i >= s->size, "index out of bounds");
    GET_STR_AT(s, i) = c;
}


//  Comparison

int string_compare(const String* s1, const String* s2)
{
    CHECK_FATAL(!s1, "str1 is null");
    CHECK_FATAL(!s2, "str2 is null");

    u64 min_len = s1->size < s2->size ? s1->size : s2->size;

    if (min_len > 0) {
        int cmp = memcmp(GET_STR(s1), GET_STR(s2), min_len);
        if (cmp != 0) {
            return cmp;
        }
    }

    if (s1->size < s2->size) {
        return -1;
    }
    if (s1->size > s2->size) {
        return 1;
    }
    return 0;
}

b8 string_equals(const String* s1, const String* s2)
{
    return string_compare(s1, s2) == 0;
}

b8 string_equals_cstr(const String* s, const char* cstr)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!cstr, "cstr is null");

    u64 len = cstr_len(cstr);

    if (s->size != len) {
        return false;
    }
    if (len == 0) {
        return true;
    }

    return memcmp(GET_STR(s), cstr, len) == 0;
}


//  Search

u64 string_find_char(const String* s, char c)
{
    CHECK_FATAL(!s, "str is null");

    const char* buf = GET_STR(s);
    for (u64 i = 0; i < s->size; i++) {
        if (buf[i] == c) {
            return i;
        }
    }
    return (u64)-1;
}

u64 string_find_cstr(const String* s, const char* substr)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(!substr, "substr is null");

    u64 len = cstr_len(substr);
    if (len == 0) {
        return 0;
    }
    if (len > s->size) {
        return (u64)-1;
    }

    const char* buf = GET_STR(s);
    for (u64 i = 0; i <= s->size - len; i++) {
        if (memcmp(buf + i, substr, len) == 0) {
            return i;
        }
    }
    return (u64)-1;
}

String* string_substr(const String* s, u64 start, u64 length)
{
    CHECK_FATAL(!s, "str is null");
    CHECK_FATAL(start >= s->size, "start out of bounds");

    u64 end = start + length;
    if (end > s->size) {
        end = s->size;
    }

    u64 actual_len = end - start;

    String* result = string_create();
    if (actual_len > 0) {
        ensure_capacity(result, actual_len);
        str_copy_n(GET_STR(result), GET_STR(s) + start, actual_len);
        result->size = actual_len;
    }

    return result;
}


//  I/O

void string_print(const String* s)
{
    CHECK_FATAL(!s, "str is null");

    putchar('"');
    const char* buf = GET_STR(s);
    for (u64 i = 0; i < s->size; i++) {
        putchar(buf[i]);
    }
    putchar('"');
}



static u64 cstr_len(const char* cstr)
{
    u64 i = 0;
    while (cstr[i] != '\0') {
        i++;
    }
    return i;
}

static void str_copy_n(char* dest, const char* src, u64 n)
{
    for (u64 i = 0; i < n; i++) {
        dest[i] = src[i];
    }
}

// Promote SSO buffer to heap allocation.
static void stk_to_heap(String* s)
{
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);

    char* new_data = malloc(new_cap);
    CHECK_FATAL(!new_data, "malloc failed");

    str_copy_n(new_data, s->stk, s->size);

    s->heap     = new_data;
    s->capacity = new_cap;
}

static void heap_to_stk(String* s)
{
    char tmp[STR_SSO_SIZE];
    str_copy_n(tmp, s->heap, s->size);
    free(s->heap);
    str_copy_n(s->stk, tmp, s->size);
    s->capacity = STR_SSO_SIZE;
}

static void string_grow(String* s)
{
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);

    char* new_data = realloc(s->heap, new_cap);
    CHECK_FATAL(!new_data, "realloc failed");

    s->heap     = new_data;
    s->capacity = new_cap;
}

static void ensure_capacity(String* s, u64 needed)
{
    if (needed <= s->capacity) {
        return;
    }

    // Grow by at least STRING_GROWTH factor so we don't alloc on every push.
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);
    if (new_cap < needed) {
        new_cap = needed;
    }

    // currently in sso but sso_cap is not enough
    if (IS_SSO(s)) {
        char* new_data = malloc(new_cap);
        CHECK_FATAL(!new_data, "malloc failed");
        str_copy_n(new_data, s->stk, s->size);
        s->heap     = new_data;
        s->capacity = new_cap;
    } else {
        char* new_data = realloc(s->heap, new_cap);
        CHECK_FATAL(!new_data, "realloc failed");
        s->heap     = new_data;
        s->capacity = new_cap;
    }
}
