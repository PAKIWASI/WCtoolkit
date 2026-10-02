#include "wc_string.h"
#include "common.h"
#include "wc_allocator.h"

#include <stdio.h>
#include <string.h>


//  Internal macros

#define GET_STR_PTR(s, i)  (GET_STR(s) + (i))
#define GET_STR_CHAR(s, i) (GET_STR(s)[i])
#define STR_REMAINING(s)   ((s)->capacity - (s)->size)
#define IS_SSO(s)          ((s)->stk[STR_SSO_SIZE - 1] != '\0')
#define GET_STR(s)         (IS_SSO(s) ? (s)->stk : (s)->heap)

// Grow if full.
// D4: capacity == 0 means zeroed / moved-from; mutation is fatal.
#define MAYBE_GROW_STR(s)                                                            \
    ({                                                                               \
        FATAL_IF((s)->capacity == 0, "String mutation on zeroed/moved-from String"); \
        if (WC_UNLIKELY((s)->size >= (s)->capacity)) {                               \
            if (IS_SSO(s)) {                                                         \
                (s)->stk[STR_SSO_SIZE - 1] = '\0';                                   \
                stk_to_heap(s);                                                      \
            } else {                                                                 \
                String_grow(s);                                                      \
            }                                                                        \
        }                                                                            \
    })



//  Private helpers

static inline u64  cstr_len(const char* cstr);
static inline void stk_to_heap(String* s);
static inline void heap_to_stk(String* s);
static inline void String_grow(String* s);
static inline void ensure_capacity(String* s, u64 needed);

// Initialise the struct to SSO mode. a is stored so all subsequent allocations
// use it.  Does NOT allocate.
static inline void str_init_sso(String* s, wc_allocator a)
{
    s->size                  = 0;
    s->capacity              = STR_SSO_SIZE - 1; // 0..30 usable, last byte is mode flag
    s->stk[STR_SSO_SIZE - 1] = 1;                // nonzero = SSO mode
    s->alloc                 = a;
}



//  Construction / Destruction

String String_create(wc_allocator a)
{
    String s;
    str_init_sso(&s, a);
    return s;
}

String String_from_cstr(wc_allocator a, const char* cstr)
{
    String s;
    str_init_sso(&s, a);

    if (!cstr) {
        return s;
    }

    u64 len = cstr_len(cstr);
    if (len == 0) {
        return s;
    }

    ensure_capacity(&s, len);
    memcpy(GET_STR(&s), cstr, len);
    s.size = len;
    return s;
}

String String_from_String(wc_allocator a, const String* other)
{
    String s;
    str_init_sso(&s, a);

    if (other->size > 0) {
        ensure_capacity(&s, other->size);
        memcpy(GET_STR(&s), GET_STR(other), other->size);
        s.size = other->size;
    }

    return s;
}

void String_destroy(String* s)
{
    if (!IS_SSO(s) && s->heap) {
        wc_free(s->alloc, s->heap, s->capacity, 1);
    }
    // Leave zeroed (capacity == 0 → zero state, safe for another destroy)
    memset(s, 0, sizeof(String));
}

String String_copy(wc_allocator a, const String* src)
{
    return String_from_String(a, src);
}

void String_move(String* dest, String* src)
{
    *dest = *src;
    memset(src, 0, sizeof(String));
}


//  Capacity

void String_reserve(String* s, u64 new_cap)
{
    FATAL_IF(s->capacity == 0, "String_reserve on zeroed/moved-from String");
    if (new_cap <= s->capacity) {
        return;
    }
    ensure_capacity(s, new_cap);
}

void String_reserve_char(String* s, u64 new_cap, char c)
{
    FATAL_IF(s->capacity == 0, "String_reserve_char on zeroed/moved-from String");
    if (new_cap <= s->capacity) {
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

void String_shrink_to_fit(String* s)
{
    if (IS_SSO(s)) {
        return; // already optimal
    }

    if (s->size <= STR_SSO_SIZE - 1) {
        // Bring back to SSO.
        // Covers size == 0: heap_to_stk frees the buffer AND restores the SSO flag.
        // (A8: the old size == 0 branch freed the buffer but stayed in heap mode,
        //  leaving heap == NULL with capacity 23; the next append wrote to NULL.)
        heap_to_stk(s);
        return;
    }

    void* new_data = wc_realloc(s->alloc, s->heap, s->capacity, s->size, 1);
    if (!new_data) {
        WARN("shrink_to_fit realloc failed");
        return;
    }
    s->heap     = (char*)new_data;
    s->capacity = s->size;
}


//  Conversion

char* String_to_cstr(wc_allocator a, const String* s)
{
    char* out = wc_alloc(a, s->size + 1, 1);
    FATAL_IF(!out, "String_to_cstr: allocation failed");

    if (s->size > 0) {
        memcpy(out, GET_STR(s), s->size);
    }
    out[s->size] = '\0';

    return out;
}

void String_to_cstr_buf(const String* str, char* buff, u64 n)
{
    WC_ASSERT(n >= str->size + 1, "buffer not enough");

    if (str->size > 0) {
        memcpy(buff, GET_STR(str), str->size);
    }
    buff[str->size] = '\0';
}

char* String_data_ptr(const String* s)
{
    if (s->size == 0) {
        return NULL;
    }
    // Cast away const intentionally: caller may mutate via this pointer.
    return (char*)(IS_SSO(s) ? s->stk : s->heap);
}

// Same growth path as String_append_char, minus the size++: writes '\0'
// at index s->size and leaves size untouched. Safe against the SSO
// mode-flag byte because MAYBE_GROW_STR converts to heap (or reallocs
// the heap buffer) whenever size == capacity, before we ever write —
// so the write always lands one past the last real char, never on the
// flag byte at stk[STR_SSO_SIZE - 1].
void String_ensure_null_term(String* s)
{
    MAYBE_GROW_STR(s);
    GET_STR_CHAR(s, s->size) = '\0';
}


//  Modification

void String_append_char(String* s, char c)
{
    MAYBE_GROW_STR(s);
    GET_STR_CHAR(s, s->size++) = c;
}

void String_append_cstr(String* s, const char* cstr)
{
    FATAL_IF(s->capacity == 0, "String_append_cstr on zeroed/moved-from String");
    u64 len = cstr_len(cstr);
    if (len == 0) {
        return;
    }

    ensure_capacity(s, s->size + len);
    memcpy(GET_STR(s) + s->size, cstr, len);
    s->size += len;
}

void String_append_String(String* s, const String* other)
{
    FATAL_IF(s->capacity == 0, "String_append_String on zeroed/moved-from String");
    if (other->size == 0) {
        return;
    }

    ensure_capacity(s, s->size + other->size);
    memcpy(GET_STR(s) + s->size, GET_STR(other), other->size);
    s->size += other->size;
}

void String_append_String_move(String* s, String* other)
{
    if (other->size > 0) {
        String_append_String(s, other);
    }
    String_destroy(other);
}

char String_pop_char(String* s)
{
    WC_ASSERT(s->size != 0, "cannot pop from empty String");

    char c = GET_STR_CHAR(s, --s->size);
    return c;
}

void String_insert_char(String* s, u64 i, char c)
{
    WC_ASSERT(i <= s->size, "index out of bounds");

    MAYBE_GROW_STR(s);

    char* buf = GET_STR(s);
    // Shift right.
    for (u64 j = s->size; j > i; j--) {
        buf[j] = buf[j - 1];
    }
    buf[i] = c;
    s->size++;
}

void String_insert_cstr(String* s, u64 i, const char* cstr)
{
    WC_ASSERT(i <= s->size, "index out of bounds");

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
    memcpy(buf + i, cstr, len);
    s->size += len;
}

void String_insert_String(String* s, u64 i, const String* other)
{
    WC_ASSERT(i <= s->size, "index out of bounds");

    if (other->size == 0) {
        return;
    }

    WARN_IF_RET(s == other, , "can't insert aliasing(same) Strings");

    u64 len = other->size;
    ensure_capacity(s, s->size + len);

    char* buf = GET_STR(s);
    for (u64 j = s->size; j > i; j--) {
        buf[j + len - 1] = buf[j - 1];
    }
    memcpy(buf + i, GET_STR(other), len);
    s->size += len;
}

void String_remove_char(String* s, u64 i)
{
    WC_ASSERT(i < s->size, "index out of bounds");

    char* buf = GET_STR(s);
    for (u64 j = i; j < s->size - 1; j++) {
        buf[j] = buf[j + 1];
    }
    s->size--;
}

/*
    0 1 2 3 4 5  (1, 2)
      ^ ^
    start = 1
    len = 2
    end = 2
*/

void String_remove_range(String* s, u64 start, u64 len)
{
    WC_ASSERT(start < s->size, "start out of bounds");

    if (len == 0) {
        return;
    }

    if (start + len >= s->size) {
        len = s->size - start;
    }

    memmove(GET_STR_PTR(s, start), GET_STR_PTR(s, start + len), s->size - start - len);

    s->size -= len;
}


//  Access


//  Comparison

int String_compare(const String* s1, const String* s2)
{
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

b8 String_equals_cstr(const String* s, const char* cstr)
{
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

u64 String_find_char(const String* s, char c)
{
    if (s->size == 0) {
        return WC_NOT_FOUND;
    }
    const char* buf = GET_STR(s);
    const char* p   = memchr(buf, (unsigned char)c, s->size);
    return p ? (u64)(p - buf) : WC_NOT_FOUND;
}

u64 String_find_cstr(const String* s, const char* substr)
{
    u64 len = cstr_len(substr);
    if (len == 0) {
        return 0;
    }
    if (len > s->size) {
        return WC_NOT_FOUND;
    }

    const char* buf = GET_STR(s);
    for (u64 i = 0; i <= s->size - len; i++) {
        if (memcmp(buf + i, substr, len) == 0) {
            return i;
        }
    }
    return WC_NOT_FOUND;
}

// NOLINTBEGIN(clang-analyzer-unix.Malloc): false positive: the returned String owns its heap buffer
String String_substr(wc_allocator a, const String* s, u64 start, u64 length)
{
    WC_ASSERT(start < s->size, "start out of bounds");

    if (start + length > s->size) {
        length = s->size - start;
    }

    String result = String_create(a);

    if (length > 0) {
        ensure_capacity(&result, length);
        memcpy(GET_STR(&result), GET_STR(s) + start, length);
        result.size = length;
    }

    return result;
}
// NOLINTEND(clang-analyzer-unix.Malloc)


//  I/O

void String_print(const String* s)
{
    putchar('"');
    const char* buf = GET_STR(s);
    for (u64 i = 0; i < s->size; i++) {
        putchar(buf[i]);
    }
    putchar('"');
}


static inline u64 cstr_len(const char* cstr)
{
    return (u64)strlen(cstr);
}


// Promote SSO buffer to heap allocation.
static inline void stk_to_heap(String* s)
{
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);
    if (new_cap == 0) {
        new_cap = STR_SSO_SIZE;
    }

    char* new_data = wc_alloc(s->alloc, new_cap, 1);
    FATAL_IF(!new_data, "String stk_to_heap: allocation failed");

    memcpy(new_data, s->stk, s->size);

    s->heap     = new_data;
    s->capacity = new_cap;
    // stk[STR_SSO_SIZE - 1] was already set to '\0' by MAYBE_GROW_STR before calling this
}

static inline void heap_to_stk(String* s)
{
    // save the ptr as memcpy on stk will overwrite
    char*        heap = s->heap;
    wc_allocator a    = s->alloc;
    u64          cap  = s->capacity;
    memcpy(s->stk, heap, s->size);
    wc_free(a, heap, cap, 1);
    s->stk[STR_SSO_SIZE - 1] = 1; // mark SSO mode
    s->capacity              = STR_SSO_SIZE - 1;
}

static inline void String_grow(String* s)
{
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);
    if (new_cap <= s->capacity) {
        new_cap = s->capacity + 1;
    }

    char* new_data = wc_realloc(s->alloc, s->heap, s->capacity, new_cap, 1);
    FATAL_IF(!new_data, "String_grow: realloc failed");

    s->heap     = new_data;
    s->capacity = new_cap;
}

static inline void ensure_capacity(String* s, u64 needed)
{
    if (needed <= s->capacity) {
        return;
    }

    // Grow by at least STRING_GROWTH factor
    u64 new_cap = (u64)((float)s->capacity * STRING_GROWTH);
    if (new_cap < needed) {
        new_cap = needed;
    }

    // currently in SSO but SSO cap is not enough
    if (IS_SSO(s)) {
        s->stk[STR_SSO_SIZE - 1] = '\0'; // switch to heap mode
        char* new_data           = wc_alloc(s->alloc, new_cap, 1);
        FATAL_IF(!new_data, "ensure_capacity: allocation failed");
        memcpy(new_data, s->stk, s->size);
        s->heap     = new_data;
        s->capacity = new_cap;
    } else {
        char* new_data = wc_realloc(s->alloc, s->heap, s->capacity, new_cap, 1);
        FATAL_IF(!new_data, "ensure_capacity: realloc failed");
        s->heap     = new_data;
        s->capacity = new_cap;
    }
}
