#include "gen_vector.h"
#include "common.h"
#include "wc_allocator.h"
#include "wc_errno.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wc_poison.h" // must stay last: bans raw malloc/free below


#define GENVEC_MIN_CAPACITY 4


// MACROS

// get ptr to elm at index i
#define GET_PTR(vec, i) ((vec->data) + ((u64)(i) * ((vec)->data_size)))
// get total size in bytes for i elements
#define GET_SCALED(vec, i) ((u64)(i) * ((vec)->data_size))

// Growth is amortized: rare by construction, so the hint is known-correct.
#define MAYBE_GROW(vec) \
    ((void)(WC_UNLIKELY(!(vec)->data || (vec)->size >= (vec)->capacity) && (GenVec_grow(vec), 0)))

// smallest safe alignment for this element size
#define DATA_ALIGN(vec) wc_align_for_size((vec)->data_size)

// a zeroed vector (moved-from / destroyed) must never allocate
#define ZERO_GUARD(vec) \
    FATAL_IF((vec)->data_size == 0, "GenVec is in the zero state (moved-from or destroyed): re-create it first")

// Move one element: move_fn if provided, else memcpy. The source is always left zeroed.
static inline void move_elm(const GenVec* vec, u8* dest, u8* src)
{
    wc_move_fn move = vec->is_pod ? NULL : VEC_MOVE_FN(vec);
    if (move) {
        move(dest, src);
    } else {
        memcpy(dest, src, vec->data_size);
    }
    memset(src, 0, vec->data_size);
}


// ops accessors (safe when ops is NULL)
#define COPY_FN(vec) VEC_COPY_FN(vec)
#define MOVE_FN(vec) VEC_MOVE_FN(vec)
#define DEL_FN(vec)  VEC_DEL_FN(vec)

#define IS_POD(vec)   ((vec)->is_pod) // cached at init
#define CALC_POD(ops) ((ops) == NULL) // derive from ops. ONLY valid at init time


// private functions

static void GenVec_grow(GenVec* vec);


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


GenVec GenVec_create(wc_allocator alloc, u64 n, u32 data_size, const wc_container_ops* ops)
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


GenVec GenVec_create_val(wc_allocator alloc, u64 n, const void* val, u32 data_size, const wc_container_ops* ops)
{
    WC_ASSERT(n != 0, "cant init with val if n = 0");

    GenVec  v   = GenVec_create(alloc, n, data_size, ops);
    GenVec* vec = &v;

    if (vec->is_pod) {
        for (u64 i = 0; i < n; i++) {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    } else {
        wc_copy_fn copy = VEC_COPY_FN(vec);
        for (u64 i = 0; i < n; i++) {
            if (copy) {
                copy(vec->alloc, GET_PTR(vec, i), val);
            } else {
                memcpy(GET_PTR(vec, i), val, data_size);
            }
        }
    }
    vec->size = n;

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
        .alloc     = wc_borrowed,
        .data_size = data_size,
        .is_pod    = CALC_POD(ops),
    };
}


void GenVec_destroy(GenVec* vec)
{
    if (vec->data) {
        if (!vec->is_pod) {
            wc_delete_fn del = VEC_DEL_FN(vec);
            if (del) {
                for (u64 i = 0; i < vec->size; i++) {
                    del(GET_PTR(vec, i));
                }
            }
        }
        wc_allocator a = vec->alloc; // read before zeroing
        wc_free(a, vec->data, GET_SCALED(vec, vec->capacity), DATA_ALIGN(vec));
    }
    memset(vec, 0, sizeof(*vec));
}

void GenVec_clear(GenVec* vec)
{
    if (!vec->is_pod) {
        wc_delete_fn del = VEC_DEL_FN(vec);
        if (del) {
            for (u64 i = 0; i < vec->size; i++) {
                del(GET_PTR(vec, i));
            }
        }
    }

    vec->size = 0;
}

void GenVec_reset(GenVec* vec)
{
    if (!vec->is_pod) {
        wc_delete_fn del = VEC_DEL_FN(vec);
        if (del) {
            for (u64 i = 0; i < vec->size; i++) {
                del(GET_PTR(vec, i));
            }
        }
    }

    wc_free(vec->alloc, vec->data, GET_SCALED(vec, vec->capacity), DATA_ALIGN(vec));
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

    GenVec_reserve(vec, new_capacity);

    if (vec->is_pod) {
        for (u64 i = vec->size; i < new_capacity; i++) {
            memcpy(GET_PTR(vec, i), val, vec->data_size);
        }
    } else {
        wc_copy_fn copy = VEC_COPY_FN(vec);
        if (copy) {
            for (u64 i = vec->size; i < new_capacity; i++) {
                copy(vec->alloc, GET_PTR(vec, i), val);
            }
        } else {
            for (u64 i = vec->size; i < new_capacity; i++) {
                memcpy(GET_PTR(vec, i), val, vec->data_size);
            }
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

    u8* new_data = wc_realloc(vec->alloc, vec->data, GET_SCALED(vec, curr_cap), GET_SCALED(vec, min_cap), DATA_ALIGN(vec));
    FATAL_IF(!new_data, "GenVec_shrink_to_fit: realloc failed");

    vec->data     = new_data;
    vec->capacity = min_cap;
}


void GenVec_push(GenVec* vec, const void* data)
{
    MAYBE_GROW(vec);

    if (vec->is_pod) {
        memcpy(GET_PTR(vec, vec->size), data, vec->data_size);
    } else {
        wc_copy_fn copy = VEC_COPY_FN(vec);
        if (copy) {
            copy(vec->alloc, GET_PTR(vec, vec->size), data);
        } else {
            memcpy(GET_PTR(vec, vec->size), data, vec->data_size);
        }
    }

    vec->size++;
}

void GenVec_push_move(GenVec* vec, void* data)
{
    MAYBE_GROW(vec);
    move_elm(vec, GET_PTR(vec, vec->size), data);
    vec->size++;
}

void GenVec_pop(GenVec* vec, void* popped)
{
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, );

    u8* last_elm = GET_PTR(vec, vec->size - 1);

    if (popped) {
        if (vec->is_pod) {
            memcpy(popped, last_elm, vec->data_size);
        } else {
            wc_copy_fn copy = VEC_COPY_FN(vec);
            if (copy) {
                copy(vec->alloc, popped, last_elm);
            } else {
                memcpy(popped, last_elm, vec->data_size);
            }
        }
    }

    if (!vec->is_pod) {
        wc_delete_fn del = VEC_DEL_FN(vec);
        if (del) {
            del(last_elm);
        }
    }

    vec->size--;
}

void GenVec_swap_pop(GenVec* vec, u64 i, void* out)
{
    WC_ASSERT(i < vec->size, "index out of bounds");

    if (out) {
        if (vec->is_pod) {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        } else {
            wc_copy_fn copy = VEC_COPY_FN(vec);
            if (copy) {
                copy(vec->alloc, out, GET_PTR(vec, i));
            } else {
                memcpy(out, GET_PTR(vec, i), vec->data_size);
            }
        }
    }

    if (!vec->is_pod) {
        wc_delete_fn del = VEC_DEL_FN(vec);
        if (del) {
            del(GET_PTR(vec, i));
        }
    }

    // swap the last container with the removed one
    // if owns memory elsewhere, those are still valid, only container location changes
    memcpy(GET_PTR(vec, i), GET_PTR(vec, vec->size - 1), vec->data_size);
    vec->size--;
}

void GenVec_swap(GenVec* vec, u64 i, u64 j)
{
    WC_ASSERT(i < vec->size && j < vec->size, "index out of bounds");

    if (i == j) {
        return;
    }

    // we need one empty container as temp space for swap
    MAYBE_GROW(vec);

    // shallow copy of the jth container to temp space
    memcpy(GET_PTR(vec, vec->size), GET_PTR(vec, j), vec->data_size);
    // shallow copy into jth container
    memcpy(GET_PTR(vec, j), GET_PTR(vec, i), vec->data_size);
    // shallow copy from temp space into ith container
    memcpy(GET_PTR(vec, i), GET_PTR(vec, vec->size), vec->data_size);

    // temp container will be over written on next push
}


void GenVec_get(const GenVec* vec, u64 i, void* out)
{
    WC_ASSERT(i < vec->size, "index out of bounds");

    if (vec->is_pod) {
        memcpy(out, GET_PTR(vec, i), vec->data_size);
    } else {
        wc_copy_fn copy = VEC_COPY_FN(vec);
        if (copy) {
            copy(vec->alloc, out, GET_PTR(vec, i));
        } else {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        }
    }
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

// TODO: place this in .h, make it inline
const void* GenVec_get_ptr_unsafe(const GenVec* vec, u64 i)
{
    return GET_PTR(vec, i);
}

void GenVec_replace(GenVec* vec, u64 i, const void* data)
{
    WC_ASSERT(i < vec->size, "index out of bounds");

    u8* to_replace = GET_PTR(vec, i);

    if (vec->is_pod) {
        memcpy(to_replace, data, vec->data_size);
    } else {
        wc_delete_fn del = VEC_DEL_FN(vec);
        if (del) {
            del(to_replace);
        }
        wc_copy_fn copy = VEC_COPY_FN(vec);
        if (copy) {
            copy(vec->alloc, to_replace, data);
        } else {
            memcpy(to_replace, data, vec->data_size);
        }
    }
}

void GenVec_replace_move(GenVec* vec, u64 i, void* data)
{
    WC_ASSERT(i < vec->size, "index out of bounds");

    u8* to_replace = GET_PTR(vec, i);

    if (!vec->is_pod) {
        wc_delete_fn del = VEC_DEL_FN(vec);
        if (del) {
            del(to_replace);
        }
    }
    move_elm(vec, to_replace, data);
}


void GenVec_insert(GenVec* vec, u64 i, const void* data)
{
    WC_ASSERT(i <= vec->size, "index out of bounds");

    u64 elements_to_shift = vec->size - i;

    MAYBE_GROW(vec);

    u8* src  = GET_PTR(vec, i);
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift));

    if (vec->is_pod) {
        memcpy(src, data, vec->data_size);
    } else {
        wc_copy_fn copy = VEC_COPY_FN(vec);
        if (copy) {
            copy(vec->alloc, src, data);
        } else {
            memcpy(src, data, vec->data_size);
        }
    }

    vec->size++;
}


void GenVec_insert_move(GenVec* vec, u64 i, void* data)
{
    WC_ASSERT(i <= vec->size, "index out of bounds");

    u64 elements_to_shift = vec->size - i;

    MAYBE_GROW(vec);

    u8* src  = GET_PTR(vec, i);
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift));

    move_elm(vec, src, data);

    vec->size++;
}


void GenVec_insert_multi(GenVec* vec, u64 i, const void* data, u64 num_data)
{
    WC_ASSERT(num_data != 0 && i <= vec->size, "num_data can't be 0 / index out of bounds");

    u64 elements_to_shift = vec->size - i;

    vec->size += num_data;
    GenVec_reserve(vec, vec->size);

    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i + num_data);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
    }

    if (vec->is_pod) {
        memcpy(src, data, GET_SCALED(vec, num_data));
    } else {
        wc_copy_fn copy = VEC_COPY_FN(vec);
        if (copy) {
            for (u64 j = 0; j < num_data; j++) {
                copy(vec->alloc, GET_PTR(vec, j + i), (const u8*)data + (size_t)(j * vec->data_size));
            }
        } else {
            memcpy(src, data, GET_SCALED(vec, num_data));
        }
    }
}


void GenVec_insert_multi_move(GenVec* vec, u64 i, void* data, u64 num_data)
{
    WC_ASSERT(num_data != 0 && i <= vec->size, "num_data can't be 0 / index out of bounds");

    u64 elements_to_shift = vec->size - i;

    GenVec_reserve(vec, vec->size + num_data);
    vec->size += num_data;

    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i + num_data);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
    }

    for (u64 j = 0; j < num_data; j++) {
        move_elm(vec, GET_PTR(vec, j + i), (u8*)data + GET_SCALED(vec, j)); // u8* inside: byte offsets
    }
}


void GenVec_remove(GenVec* vec, u64 i, void* out)
{
    WC_ASSERT(i < vec->size, "index out of bounds");

    if (out) {
        if (vec->is_pod) {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        } else {
            wc_copy_fn copy = VEC_COPY_FN(vec);
            if (copy) {
                copy(vec->alloc, out, GET_PTR(vec, i));
            } else {
                memcpy(out, GET_PTR(vec, i), vec->data_size);
            }
        }
    }

    if (!vec->is_pod) {
        wc_delete_fn del = VEC_DEL_FN(vec);
        if (del) {
            del(GET_PTR(vec, i));
        }
    }

    u64 elements_to_shift = vec->size - i - 1;
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i);
        u8* src  = GET_PTR(vec, i + 1);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
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

    if (!vec->is_pod) {
        wc_delete_fn del = VEC_DEL_FN(vec);
        if (del) {
            for (u64 i = 0; i < len; i++) {
                del(GET_PTR(vec, start + i));
            }
        }
    }

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


GenVec GenVec_subarr(const GenVec* vec, wc_allocator alloc, u64 start, u64 len)
{
    WC_ASSERT(start < vec->size, "out of bounds");

    if (len > vec->size - start) {
        len = vec->size - start;
    }

    GenVec  out = GenVec_create(alloc, len, vec->data_size, vec->ops);
    GenVec* v   = &out;

    if (len > 0) {
        if (vec->is_pod) {
            memcpy(GET_PTR(v, 0), GET_PTR(vec, start), GET_SCALED(vec, len));
        } else {
            wc_copy_fn copy = VEC_COPY_FN(vec);
            if (copy) {
                for (u64 i = 0; i < len; i++) {
                    copy(v->alloc, GET_PTR(v, i), GET_PTR(vec, i + start));
                }
            } else {
                memcpy(GET_PTR(v, 0), GET_PTR(vec, start), GET_SCALED(vec, len));
            }
        }

        v->size = len;
    }

    return out;
}


void GenVec_print(const GenVec* vec, wc_print_fn fn)
{
    printf("[ ");
    for (u64 i = 0; i < vec->size; i++) {
        fn(GET_PTR(vec, i));
    }
    putchar(']');
}


GenVec GenVec_copy(wc_allocator alloc, const GenVec* src)
{
    // Field by field: the allocator comes from the caller, never from src (A7).
    GenVec dest = {
        .data      = NULL,
        .ops       = src->ops,
        .size      = src->size,
        .capacity  = src->capacity,
        .alloc     = alloc,
        .data_size = src->data_size,
        .is_pod    = src->is_pod,
    };
    if (src->data_size == 0) {
        memset(&dest, 0, sizeof(dest)); // copy of a zeroed vector is a zeroed vector
        return dest;
    }

    dest.data = ALLOC_OR_DIE(alloc, src->capacity, src->data_size);

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


static void GenVec_grow(GenVec* vec)
{
    ZERO_GUARD(vec);

    u64 old_cap = vec->capacity;
    u64 new_cap;
    if (old_cap < GENVEC_MIN_CAPACITY) {
        new_cap = old_cap + 1;
    } else {
        new_cap = (u64)((float)old_cap * GENVEC_GROWTH);
        if (new_cap <= old_cap) {
            new_cap = old_cap + 1;
        }
    }

    u8* new_data =
        wc_realloc(vec->alloc, vec->data, GET_SCALED(vec, old_cap), wc_mul(new_cap, vec->data_size), DATA_ALIGN(vec));
    if (!new_data) {
        FATAL("GenVec: growth to %llu elements failed (arena full, or borrowed buffer)",
              (unsigned long long)new_cap); // D5
    }

    vec->data     = new_data;
    vec->capacity = new_cap;
}
