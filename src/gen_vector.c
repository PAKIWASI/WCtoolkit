#include "gen_vector.h"
#include "common.h"
#include "wc_allocator.h"
#include "wc_errno.h"

#include <stdio.h>
#include <string.h>


// MACROS

// get ptr to elm at index i
#define GET_PTR(vec, i) (((vec)->data) + ((u64)(i) * ((vec)->data_size)))
// get total size in bytes for i elements
#define GET_SCALED(vec, i) ((u64)(i) * ((vec)->data_size))

// Growth is amortized: rare by construction, so the hint is known-correct.
#define MAYBE_GROW(vec) \
    ((void)(WC_UNLIKELY((vec)->size >= (vec)->capacity) && (GenVec_grow_to((vec), (vec)->size + 1), 0)))

// smallest safe alignment for this element size
#define DATA_ALIGN(vec) wc_align_for_size((vec)->data_size)

// a zeroed vector (moved-from / destroyed) must never allocate
#define ZERO_GUARD(vec) \
    FATAL_IF((vec)->data_size == 0, "GenVec is in the zero state (moved-from or destroyed): re-create it first")

// D4: an input element must not live inside this vector's own storage. Growth
// would free it before it is read. Checked over the whole capacity (Debug only).
#define NOT_ALIASED(vec, p)                                                         \
    WC_ASSERT(!((vec)->data && (const u8*)(p) >= (vec)->data &&                     \
                (const u8*)(p) < (vec)->data + GET_SCALED((vec), (vec)->capacity)), \
              "input element points into this vector's own buffer (rule D4): copy it out first")

// Copy one element INTO raw slot memory: copy_fn if any, else memcpy
static inline void vec_copy_into(const GenVec* vec, u8* dest, const void* src)
{
    wc_copy_fn copy = vec->is_pod ? NULL : VEC_COPY_FN(vec);
    if (copy) {
        copy(vec->alloc, dest, src);
    } else {
        memcpy(dest, src, vec->data_size);
    }
}

// Move one element (always memcpy). The source is left zeroed.
static inline void vec_move_into(const GenVec* vec, u8* dest, void* src)
{
    memcpy(dest, src, vec->data_size);
    memset(src, 0, vec->data_size);
}

// Delete `n` elements starting at `p`.
static inline void vec_delete_range(const GenVec* vec, u8* p, u64 n)
{
    wc_delete_fn del = vec->is_pod ? NULL : VEC_DEL_FN(vec);
    if (del) {
        for (u64 i = 0; i < n; i++) {
            del(p + GET_SCALED(vec, i));
        }
    }
}

// Take element at `slot` out: moved into `out`, or deleted when out == NULL (B7).
// The slot itself is left as garbage: callers drop it from the live range.
static inline void vec_take_out(const GenVec* vec, u8* slot, void* out)
{
    if (out) {
        memcpy(out, slot, vec->data_size);
    } else {
        vec_delete_range(vec, slot, 1);
    }
}


#define IS_POD(vec)   ((vec)->is_pod) // cached at init
#define CALC_POD(ops) ((ops) == NULL) // derive from ops. ONLY valid at init time


// private functions

static void GenVec_grow_to(GenVec* vec, u64 needed);


// API Implementation

#define ALLOC_OR_DIE(a, n, data_size)                                                                            \
    ({                                                                                                           \
        u8* data = NULL;                                                                                         \
        if ((n) != 0) {                                                                                          \
            data = wc_alloc(a, wc_mul(n, data_size), wc_align_for_size(data_size));                              \
            FATAL_IF(!data, "GenVec: allocation of %llu x %u bytes failed", (unsigned long long)(n), data_size); \
        }                                                                                                        \
        data;                                                                                                    \
    })


GenVec GenVec_create(const wc_allocator* alloc, u64 n, u32 data_size, const wc_container_ops* ops)
{
    FATAL_IF(data_size == 0, "GenVec: data_size can't be 0");

    return (GenVec){
        .data      = ALLOC_OR_DIE(alloc, n, data_size),
        .ops       = ops,
        .size      = 0,
        .capacity  = n,
        .alloc     = alloc,
        .data_size = data_size,
        .is_pod    = CALC_POD(ops),
    };
}


GenVec GenVec_create_val(const wc_allocator* alloc, u64 n, const void* val, u32 data_size, const wc_container_ops* ops)
{
    WC_ASSERT(n != 0, "cant init with val if n = 0");

    GenVec v = GenVec_create(alloc, n, data_size, ops);

    if (v.is_pod || !VEC_COPY_FN(&v)) {
        // First element by copy, then double the filled prefix: O(log n) memcpy calls of
        // growing length instead of n calls of runtime length.
        memcpy(v.data, val, data_size);
        for (u64 filled = 1; filled < n;) {
            u64 chunk = filled < n - filled ? filled : n - filled;
            memcpy(GET_PTR(&v, filled), v.data, chunk * data_size);
            filled += chunk;
        }
    } else {
        for (u64 i = 0; i < n; i++) {
            vec_copy_into(&v, GET_PTR(&v, i), val);
        }
    }
    v.size = n;

    return v;
}


GenVec GenVec_create_buf(void* buf, u64 n, u32 data_size, const wc_container_ops* ops)
{
    FATAL_IF(n == 0 || data_size == 0, "GenVec_create_buf: n/data_size can't be 0");

    return (GenVec){
        .data      = buf,
        .ops       = ops,
        .size      = 0,
        .capacity  = n,
        .alloc     = WC_BORROWED,
        .data_size = data_size,
        .is_pod    = CALC_POD(ops),
    };
}


void GenVec_destroy(GenVec* vec)
{
    if (vec->data) {
        vec_delete_range(vec, vec->data, vec->size);
        wc_free(vec->alloc, vec->data, GET_SCALED(vec, vec->capacity), DATA_ALIGN(vec));
    }
    memset(vec, 0, sizeof(*vec));
}

void GenVec_clear(GenVec* vec)
{
    if (vec->data) {
        vec_delete_range(vec, vec->data, vec->size);
    }
    vec->size = 0;
}

void GenVec_reset(GenVec* vec)
{
    if (vec->data) {
        vec_delete_range(vec, vec->data, vec->size);
        wc_free(vec->alloc, vec->data, GET_SCALED(vec, vec->capacity), DATA_ALIGN(vec));
    }
    vec->data     = NULL;
    vec->size     = 0;
    vec->capacity = 0;
}


void GenVec_reserve(GenVec* vec, u64 new_capacity)
{
    if (new_capacity <= vec->capacity) {
        return;
    }
    ZERO_GUARD(vec);

    u8* new_data = wc_realloc(vec->alloc, vec->data, GET_SCALED(vec, vec->capacity),
                              wc_mul(new_capacity, vec->data_size), DATA_ALIGN(vec));
    FATAL_IF(!new_data, "GenVec_reserve: realloc to %llu elements failed", (unsigned long long)new_capacity);

    vec->data     = new_data;
    vec->capacity = new_capacity;
}


void GenVec_reserve_val(GenVec* vec, u64 new_capacity, const void* val)
{
    WC_ASSERT(new_capacity >= vec->size, "new_capacity must be >= current size");
    NOT_ALIASED(vec, val);

    GenVec_reserve(vec, new_capacity);

    u64 count = new_capacity - vec->size; // elements to fill
    if (count == 0) {
        return;
    }
    if (vec->is_pod || !VEC_COPY_FN(vec)) {
        // Plain bytes: first element by copy, then double the filled run (O(log n) memcpy calls
        // of growing length instead of one runtime-length call per element).
        memcpy(GET_PTR(vec, vec->size), val, vec->data_size);
        for (u64 filled = 1; filled < count;) {
            u64 chunk = filled < count - filled ? filled : count - filled;
            memcpy(GET_PTR(vec, vec->size + filled), GET_PTR(vec, vec->size), chunk * vec->data_size);
            filled += chunk;
        }
    } else {
        for (u64 i = vec->size; i < new_capacity; i++) {
            vec_copy_into(vec, GET_PTR(vec, i), val);
        }
    }
    vec->size = new_capacity;
}


void GenVec_shrink_to_fit(GenVec* vec)
{
    u64 min_cap  = vec->size > GENVEC_MIN_CAPACITY ? vec->size : GENVEC_MIN_CAPACITY;
    u64 curr_cap = vec->capacity;

    if (curr_cap <= min_cap) {
        return;
    }

    u8* new_data =
        wc_realloc(vec->alloc, vec->data, GET_SCALED(vec, curr_cap), GET_SCALED(vec, min_cap), DATA_ALIGN(vec));
    FATAL_IF(!new_data, "GenVec_shrink_to_fit: realloc failed");

    vec->data     = new_data;
    vec->capacity = min_cap;
}


void GenVec_push(GenVec* vec, const void* data)
{
    NOT_ALIASED(vec, data);
    MAYBE_GROW(vec);
    vec_copy_into(vec, GET_PTR(vec, vec->size), data);
    vec->size++;
}

void GenVec_push_move(GenVec* vec, void* data)
{
    NOT_ALIASED(vec, data);
    MAYBE_GROW(vec);
    vec_move_into(vec, GET_PTR(vec, vec->size), data);
    vec->size++;
}

void GenVec_pop(GenVec* vec, void* popped)
{
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, );

    vec->size--;
    vec_take_out(vec, GET_PTR(vec, vec->size), popped);
}

void GenVec_swap_pop(GenVec* vec, u64 i, void* out)
{
    WC_ASSERT(i < vec->size, "index out of bounds");

    vec_take_out(vec, GET_PTR(vec, i), out);

    // the last element takes slot i (a relocation: memcpy)
    vec->size--;
    if (i != vec->size) {
        memcpy(GET_PTR(vec, i), GET_PTR(vec, vec->size), vec->data_size);
    }
}

void GenVec_swap(GenVec* vec, u64 i, u64 j)
{
    WC_ASSERT(i < vec->size && j < vec->size, "index out of bounds");

    if (i == j) {
        return;
    }

    // Swap through a small stack buffer in chunks: no growth, no spare slot
    u8* a = GET_PTR(vec, i);
    u8* b = GET_PTR(vec, j);
    u8  tmp[64];
    for (u64 left = vec->data_size; left > 0;) {
        u64 n = left < sizeof(tmp) ? left : sizeof(tmp);
        memcpy(tmp, a, n);
        memcpy(a, b, n);
        memcpy(b, tmp, n);
        a += n;
        b += n;
        left -= n;
    }
}


void GenVec_get(const GenVec* vec, u64 i, void* out)
{
    WC_ASSERT(i < vec->size, "index out of bounds");
    vec_copy_into(vec, out, GET_PTR(vec, i));
}

const void* GenVec_get_ptr(const GenVec* vec, u64 i)
{
    WC_ASSERT(i < vec->size, "index out of bounds");
    return GET_PTR(vec, i);
}

void* GenVec_get_ptr_mut(GenVec* vec, u64 i)
{
    WC_ASSERT(i < vec->size, "index out of bounds");
    return GET_PTR(vec, i);
}


void GenVec_replace(GenVec* vec, u64 i, const void* data)
{
    WC_ASSERT(i < vec->size, "index out of bounds");
    NOT_ALIASED(vec, data);

    u8* to_replace = GET_PTR(vec, i);
    vec_delete_range(vec, to_replace, 1);
    vec_copy_into(vec, to_replace, data);
}

void GenVec_replace_move(GenVec* vec, u64 i, void* data)
{
    WC_ASSERT(i < vec->size, "index out of bounds");
    NOT_ALIASED(vec, data);

    u8* to_replace = GET_PTR(vec, i);
    vec_delete_range(vec, to_replace, 1);
    vec_move_into(vec, to_replace, data);
}


void GenVec_insert(GenVec* vec, u64 i, const void* data)
{
    WC_ASSERT(i <= vec->size, "index out of bounds");
    NOT_ALIASED(vec, data);

    MAYBE_GROW(vec);

    u8* slot = GET_PTR(vec, i);
    memmove(GET_PTR(vec, i + 1), slot, GET_SCALED(vec, vec->size - i));
    vec_copy_into(vec, slot, data);
    vec->size++;
}


void GenVec_insert_move(GenVec* vec, u64 i, void* data)
{
    WC_ASSERT(i <= vec->size, "index out of bounds");
    NOT_ALIASED(vec, data);

    MAYBE_GROW(vec);

    u8* slot = GET_PTR(vec, i);
    memmove(GET_PTR(vec, i + 1), slot, GET_SCALED(vec, vec->size - i));
    vec_move_into(vec, slot, data);
    vec->size++;
}


// Open a gap of `n` slots at index i (grows geometrically). Returns the gap.
static u8* vec_open_gap(GenVec* vec, u64 i, u64 n)
{
    if (vec->size + n > vec->capacity) {
        GenVec_grow_to(vec, vec->size + n);
    }
    u8* gap = GET_PTR(vec, i);
    if (i < vec->size) {
        memmove(GET_PTR(vec, i + n), gap, GET_SCALED(vec, vec->size - i));
    }
    vec->size += n;
    return gap;
}


void GenVec_insert_multi(GenVec* vec, u64 i, const void* data, u64 num_data)
{
    WC_ASSERT(num_data != 0 && i <= vec->size, "num_data can't be 0 / index out of bounds");
    NOT_ALIASED(vec, data);

    u8* gap = vec_open_gap(vec, i, num_data);

    if (vec->is_pod || !VEC_COPY_FN(vec)) {
        memcpy(gap, data, GET_SCALED(vec, num_data));
    } else {
        for (u64 j = 0; j < num_data; j++) {
            vec_copy_into(vec, gap + GET_SCALED(vec, j), (const u8*)data + GET_SCALED(vec, j));
        }
    }
}


void GenVec_insert_multi_move(GenVec* vec, u64 i, void* data, u64 num_data)
{
    WC_ASSERT(num_data != 0 && i <= vec->size, "num_data can't be 0 / index out of bounds");
    NOT_ALIASED(vec, data);

    u8* gap = vec_open_gap(vec, i, num_data);

    // one block move: memcpy the whole range, then zero the source range
    memcpy(gap, data, GET_SCALED(vec, num_data));
    memset(data, 0, GET_SCALED(vec, num_data));
}


void GenVec_remove(GenVec* vec, u64 i, void* out)
{
    WC_ASSERT(i < vec->size, "index out of bounds");

    vec_take_out(vec, GET_PTR(vec, i), out);

    u64 elements_to_shift = vec->size - i - 1;
    if (elements_to_shift > 0) {
        memmove(GET_PTR(vec, i), GET_PTR(vec, i + 1), GET_SCALED(vec, elements_to_shift));
    }

    vec->size--;
}


void GenVec_remove_range(GenVec* vec, u64 start, u64 len)
{
    if (len == 0) {
        return;
    }
    WC_ASSERT(start < vec->size, "start out of range");

    if (len > vec->size - start) {
        len = vec->size - start;
    }

    vec_delete_range(vec, GET_PTR(vec, start), len);

    u8* dest = GET_PTR(vec, start);
    u8* src  = GET_PTR(vec, start + len);
    memmove(dest, src, GET_SCALED(vec, vec->size - start - len));

    vec->size -= len;
}


const void* GenVec_front(const GenVec* vec)
{
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, NULL);
    return GET_PTR(vec, 0);
}


const void* GenVec_back(const GenVec* vec)
{
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, NULL);
    return GET_PTR(vec, vec->size - 1);
}


u64 GenVec_find(const GenVec* vec, void* elm, wc_compare_fn cmp_fn)
{
    if (!cmp_fn) {
        cmp_fn = memcmp;
    }
    for (u64 i = 0; i < vec->size; i++) {
        if (cmp_fn(GET_PTR(vec, i), elm, vec->data_size) == 0) {
            return i;
        }
    }

    return WC_NOT_FOUND;
}


GenVec GenVec_subarr(const GenVec* vec, const wc_allocator* alloc, u64 start, u64 len)
{
    WC_ASSERT(start < vec->size, "out of bounds");

    if (len > vec->size - start) {
        len = vec->size - start;
    }

    GenVec out = GenVec_create(alloc, len, vec->data_size, vec->ops);

    if (len > 0) {
        if (vec->is_pod || !VEC_COPY_FN(vec)) {
            memcpy(GET_PTR(&out, 0), GET_PTR(vec, start), GET_SCALED(vec, len));
        } else {
            for (u64 i = 0; i < len; i++) {
                vec_copy_into(&out, GET_PTR(&out, i), GET_PTR(vec, i + start));
            }
        }
        out.size = len;
    }

    return out;
}


void GenVec_print(const GenVec* vec, wc_print_fn fn)
{
    printf("[ ");
    for (u64 i = 0; i < vec->size; i++) {
        fn(GET_PTR(vec, i));
        putchar(' ');
    }
    putchar(']');
}


GenVec GenVec_copy(const wc_allocator* alloc, const GenVec* src)
{
    if (src->data_size == 0) {
        return (GenVec){0}; // copy of a zeroed vector is a zeroed vector
    }

    // size the copy to its contents, not to src's slack.
    u64 cap = src->size == 0 ? 0 : (src->size > GENVEC_MIN_CAPACITY ? src->size : GENVEC_MIN_CAPACITY);

    // Field by field: the allocator comes from the caller, never from src
    GenVec dest = {
        .data      = ALLOC_OR_DIE(alloc, cap, src->data_size),
        .ops       = src->ops,
        .size      = src->size,
        .capacity  = cap,
        .alloc     = alloc,
        .data_size = src->data_size,
        .is_pod    = src->is_pod,
    };

    wc_copy_fn copy = src->is_pod ? NULL : VEC_COPY_FN(src);
    if (copy) {
        for (u64 i = 0; i < src->size; i++) {
            copy(alloc, GET_PTR((&dest), i), GET_PTR(src, i));
        }
    } else if (src->size > 0) {
        memcpy(dest.data, src->data, GET_SCALED(src, src->size));
    }

    return dest;
}


void GenVec_move(GenVec* dest, GenVec* src)
{
    if (dest == src) {
        return;
    }
    memcpy(dest, src, sizeof(GenVec));
    memset(src, 0, sizeof(GenVec));
}


// Grow to hold at least `needed` elements: 0 -> GENVEC_MIN_CAPACITY, then 1.5x,
// or straight to `needed` when that is larger (bulk inserts, F4).
static void GenVec_grow_to(GenVec* vec, u64 needed)
{
    ZERO_GUARD(vec);

    u64 old_cap = vec->capacity;
    u64 new_cap = old_cap == 0 ? GENVEC_MIN_CAPACITY : old_cap + (old_cap / 2); // 0 -> MIN, then 1.5x (F3)
    if (new_cap <= old_cap) {
        new_cap = old_cap + 1; // small capacities: 1.5x rounds down to no growth
    }
    if (new_cap < needed) {
        new_cap = needed;
    }

    u8* new_data =
        wc_realloc(vec->alloc, vec->data, GET_SCALED(vec, old_cap), wc_mul(new_cap, vec->data_size), DATA_ALIGN(vec));
    if (!new_data) {
        FATAL("GenVec: growth to %llu elements failed (arena full, or borrowed buffer)", (unsigned long long)new_cap);
    }

    vec->data     = new_data;
    vec->capacity = new_cap;
}
