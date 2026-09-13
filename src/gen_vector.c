#include "common.h"
#include "gen_vector.h"
#include "wc_errno.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define GENVEC_MIN_CAPACITY 4


// MACROS

// get ptr to elm at index i
#define GET_PTR(vec, i) ((vec->data) + ((u64)(i) * ((vec)->data_size)))
// get total size in bytes for i elements
#define GET_SCALED(vec, i) ((i) * ((vec)->data_size))

#define MAYBE_GROW(vec)                                 \
    do {                                                \
        if (!vec->data || vec->size >= vec->capacity) { \
            GenVec_grow(vec);                           \
        }                                               \
    } while (0)


// ops accessors (safe when ops is NULL)
#define COPY_FN(vec) VEC_COPY_FN(vec)
#define MOVE_FN(vec) VEC_MOVE_FN(vec)
#define DEL_FN(vec)  VEC_DEL_FN(vec)

#define IS_POD(vec)   (vec->is_pod)    // cached at init (6-M) — consume this everywhere
#define CALC_POD(ops) ((ops) == NULL)  // derive from ops — ONLY valid at init time


// private functions

static void GenVec_grow(GenVec* vec);


// API Implementation

GenVec* GenVec_create(u64 n, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    GenVec* vec = malloc(sizeof(GenVec));
    CHECK_FATAL(!vec, "vec init failed");

    // Only allocate memory if n > 0, otherwise data can be NULL
    vec->data = (n > 0) ? malloc(data_size * n) : NULL;

    if (n > 0 && !vec->data) {
        free(vec);
        FATAL("data init failed");
    }

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->ops       = ops;
    vec->is_pod    = CALC_POD(ops);

    return vec;
}


void GenVec_create_stk(u64 n, u32 data_size, const container_ops* ops, GenVec* vec)
{
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    vec->data = (n > 0) ? malloc(data_size * n) : NULL;
    CHECK_FATAL(n > 0 && !vec->data, "data init failed");

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->ops       = ops;
    vec->is_pod    = CALC_POD(ops);
}


GenVec* GenVec_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    GenVec* vec = GenVec_create(n, data_size, ops);

    vec->size = n; // capacity set to n in GenVec_create

    if (vec->is_pod) {
        for (u64 i = 0; i < n; i++) {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            for (u64 i = 0; i < n; i++) {
                copy(GET_PTR(vec, i), val);
            }
        } else {
            for (u64 i = 0; i < n; i++) {
                memcpy(GET_PTR(vec, i), val, data_size);
            }
        }
    }

    return vec;
}


void GenVec_create_val_stk(u64 n, const u8* val, u32 data_size, const container_ops* ops, GenVec* vec)
{
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    GenVec_create_stk(n, data_size, ops, vec);

    vec->size = n;

    if (vec->is_pod) {
        for (u64 i = 0; i < n; i++) {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            for (u64 i = 0; i < n; i++) {
                copy(GET_PTR(vec, i), val);
            }
        } else {
            for (u64 i = 0; i < n; i++) {
                memcpy(GET_PTR(vec, i), val, data_size);
            }
        }
    }
}


GenVec* GenVec_create_arr(u64 n, u32 data_size, const container_ops* ops, u8* arr)
{
    GenVec* v = GenVec_create(n, data_size, ops);

    memcpy(v->data, arr, n * data_size);
    v->size = n;

    return v;
}


void GenVec_create_stk_arr(u64 n, u8* arr, u32 data_size, const container_ops* ops, GenVec* vec)
{
    CHECK_FATAL(n == 0 || data_size == 0, "size/data_size of arr can't be 0");

    vec->data      = arr;
    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->ops       = ops;
    vec->is_pod    = CALC_POD(ops);
}


void GenVec_destroy(GenVec* vec)
{
    GenVec_destroy_stk(vec);
    free(vec);
}


void GenVec_destroy_stk(GenVec* vec)
{
    if (!vec->data) {
        return;
    }

    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            for (u64 i = 0; i < vec->size; i++) {
                del(GET_PTR(vec, i));
            }
        }
    }

    free(vec->data);
    vec->data = NULL;
}


void GenVec_clear(GenVec* vec)
{
    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
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
        delete_fn del = vec->ops->del_fn;
        if (del) {
            for (u64 i = 0; i < vec->size; i++) {
                del(GET_PTR(vec, i));
            }
        }
    }

    free(vec->data);
    vec->data     = NULL;
    vec->size     = 0;
    vec->capacity = 0;
}


void GenVec_reserve(GenVec* vec, u64 new_capacity)
{
    if (new_capacity <= vec->capacity) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, new_capacity));
    CHECK_FATAL(!new_data, "realloc failed");

    vec->data     = new_data;
    vec->capacity = new_capacity;
}


void GenVec_reserve_val(GenVec* vec, u64 new_capacity, const u8* val)
{
    CHECK_FATAL(new_capacity < vec->size, "new_capacity must be >= current size");

    GenVec_reserve(vec, new_capacity);

    if (vec->is_pod) {
        for (u64 i = vec->size; i < new_capacity; i++) {
            memcpy(GET_PTR(vec, i), val, vec->data_size);
        }
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            for (u64 i = vec->size; i < new_capacity; i++) {
                copy(GET_PTR(vec, i), val);
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

    u8* new_data = realloc(vec->data, GET_SCALED(vec, min_cap));
    CHECK_FATAL(!new_data, "data realloc failed");

    vec->data     = new_data;
    vec->capacity = min_cap;
}


void GenVec_push(GenVec* vec, const u8* data)
{
    MAYBE_GROW(vec);

    if (vec->is_pod) {
        memcpy(GET_PTR(vec, vec->size), data, vec->data_size);
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            copy(GET_PTR(vec, vec->size), data);
        } else {
            memcpy(GET_PTR(vec, vec->size), data, vec->data_size);
        }
    }

    vec->size++;
}


void GenVec_push_move(GenVec* vec, u8** data)
{
    CHECK_FATAL(!*data, "*data is null");

    MAYBE_GROW(vec);

    if (vec->is_pod) {
        memcpy(GET_PTR(vec, vec->size), *data, vec->data_size);
        *data = NULL;
    } else {
        move_fn move = vec->ops->move_fn;
        if (move) {
            move(GET_PTR(vec, vec->size), data);
        } else {
            memcpy(GET_PTR(vec, vec->size), *data, vec->data_size);
            *data = NULL;
        }
    }

    vec->size++;
}


void GenVec_pop(GenVec* vec, u8* popped)
{
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, );

    u8* last_elm = GET_PTR(vec, vec->size - 1);

    if (popped) {
        if (vec->is_pod) {
            memcpy(popped, last_elm, vec->data_size);
        } else {
            copy_fn copy = vec->ops->copy_fn;
            if (copy) {
                copy(popped, last_elm);
            } else {
                memcpy(popped, last_elm, vec->data_size);
            }
        }
    }

    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            del(last_elm);
        }
    }

    vec->size--;
}

void GenVec_swap_pop(GenVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (out) {
        if (vec->is_pod) {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        } else {
            copy_fn copy = vec->ops->copy_fn;
            if (copy) {
                copy(out, GET_PTR(vec, i));
            } else {
                memcpy(out, GET_PTR(vec, i), vec->data_size);
            }
        }
    }

    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
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
    CHECK_FATAL(i >= vec->size || j >= vec->size, "index out of bounds");

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


void GenVec_get(const GenVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (vec->is_pod) {
        memcpy(out, GET_PTR(vec, i), vec->data_size);
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            copy(out, GET_PTR(vec, i));
        } else {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        }
    }
}


const u8* GenVec_get_ptr(const GenVec* vec, u64 i)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    return GET_PTR(vec, i);
}


u8* GenVec_get_ptr_mut(GenVec* vec, u64 i)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    return GET_PTR(vec, i);
}


const u8* GenVec_get_ptr_unsafe(const GenVec* vec, u64 i)
{
    // Preconditions NOT validated — caller guarantees i < vec->size.
    return GET_PTR(vec, i);
}


u8* GenVec_get_ptr_mut_unsafe(GenVec* vec, u64 i)
{
    // Preconditions NOT validated — caller guarantees i < vec->size.
    return GET_PTR(vec, i);
}


void GenVec_replace(GenVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    u8* to_replace = GET_PTR(vec, i);

    if (vec->is_pod) {
        memcpy(to_replace, data, vec->data_size);
    } else {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            del(to_replace);
        }
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            copy(to_replace, data);
        } else {
            memcpy(to_replace, data, vec->data_size);
        }
    }
}


void GenVec_replace_move(GenVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(i >= vec->size || !*data, "index out of bounds / need a valid *data variable");

    u8* to_replace = GET_PTR(vec, i);

    if (vec->is_pod) {
        memcpy(to_replace, *data, vec->data_size);
        *data = NULL;
    } else {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            del(to_replace);
        }
        move_fn move = vec->ops->move_fn;
        if (move) {
            move(to_replace, data);
        } else {
            memcpy(to_replace, *data, vec->data_size);
            *data = NULL;
        }
    }
}


void GenVec_insert(GenVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(i > vec->size, "index out of bounds");

    u64 elements_to_shift = vec->size - i;

    MAYBE_GROW(vec);

    u8* src  = GET_PTR(vec, i);
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift));

    if (vec->is_pod) {
        memcpy(src, data, vec->data_size);
    } else {
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            copy(src, data);
        } else {
            memcpy(src, data, vec->data_size);
        }
    }

    vec->size++;
}


void GenVec_insert_move(GenVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(!*data || i > vec->size, "*data is null / index out of bounds");

    u64 elements_to_shift = vec->size - i;

    MAYBE_GROW(vec);

    u8* src  = GET_PTR(vec, i);
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift));

    if (vec->is_pod) {
        memcpy(src, *data, vec->data_size);
        *data = NULL;
    } else {
        move_fn move = vec->ops->move_fn;
        if (move) {
            move(src, data);
        } else {
            memcpy(src, *data, vec->data_size);
            *data = NULL;
        }
    }

    vec->size++;
}


void GenVec_insert_multi(GenVec* vec, u64 i, const u8* data, u64 num_data)
{
    CHECK_FATAL(num_data == 0 || i > vec->size, "num_data can't be 0 / index out of bounds");

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
        copy_fn copy = vec->ops->copy_fn;
        if (copy) {
            for (u64 j = 0; j < num_data; j++) {
                copy(GET_PTR(vec, j + i), data + (size_t)(j * vec->data_size));
            }
        } else {
            memcpy(src, data, GET_SCALED(vec, num_data));
        }
    }
}


void GenVec_insert_multi_move(GenVec* vec, u64 i, u8** data, u64 num_data)
{
    CHECK_FATAL(!*data || num_data == 0 || i > vec->size, "*data is null / num_data can't be 0 / index out of bounds");

    u64 elements_to_shift = vec->size - i;

    vec->size += num_data;
    GenVec_reserve(vec, vec->size);

    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i + num_data);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
    }

    if (vec->is_pod) {
        memcpy(src, *data, GET_SCALED(vec, num_data));
    } else {
        move_fn move = vec->ops->move_fn;
        if (move) {
            for (u64 j = 0; j < num_data; j++) {
                u8* elm_src = *data + (size_t)(j * vec->data_size);
                move(GET_PTR(vec, j + i), &elm_src);
            }
        } else {
            memcpy(src, *data, GET_SCALED(vec, num_data));
        }
    }

    *data = NULL;
}


void GenVec_remove(GenVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (out) {
        if (vec->is_pod) {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        } else {
            copy_fn copy = vec->ops->copy_fn;
            if (copy) {
                copy(out, GET_PTR(vec, i));
            } else {
                memcpy(out, GET_PTR(vec, i), vec->data_size);
            }
        }
    }

    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
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


/*
    0 1 2 3 4 5, (1, 3) -> [1, 4)
    start = 1
    len = 3
    end = 1 + 3 - 1 = 3
*/
void GenVec_remove_range(GenVec* vec, u64 start, u64 len)
{
    if (len == 0) {
        return;
    }
    CHECK_FATAL(start >= vec->size, "start out of range");

    if (start + len >= vec->size) {
        len = vec->size - start;
    }

    if (!vec->is_pod) {
        delete_fn del = vec->ops->del_fn;
        if (del) {
            for (u64 i = 0; i < len; i++) {
                del(GET_PTR(vec, start + i));
            }
        }
    }

    u8* dest = GET_PTR(vec, start);
    u8* src  = GET_PTR(vec, start + len);
    memmove(dest, src, GET_SCALED(vec, vec->size - start - len));   // TODO: is this right

    vec->size -= len;
}


const u8* GenVec_front(const GenVec* vec)
{
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, NULL);
    return GET_PTR(vec, 0);
}


const u8* GenVec_back(const GenVec* vec)
{
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, NULL);
    return GET_PTR(vec, vec->size - 1);
}


u64 GenVec_find(const GenVec* vec, u8* elm, compare_fn cmp_fn)
{
    for (u64 i = 0; i < vec->size; i++) {
        if (cmp_fn) {
            if (cmp_fn(GET_PTR(vec, i), elm, vec->data_size) == 0) {
                return i;
            }
        } else {
            if (memcmp(GET_PTR(vec, i), elm, vec->data_size) == 0) {
                return i;
            }
        }
    }

    return WC_NOT_FOUND;
}


GenVec* GenVec_subarr(const GenVec* vec, u64 start, u64 len)
{
    CHECK_FATAL(start >= vec->size, "out of bounds");

    if (start + len >= vec->size) {
        len = vec->size - start;
    }

    GenVec* v = GenVec_create(len, vec->data_size, vec->ops);

    if (len > 0) {
        if (vec->is_pod) {
            memcpy(GET_PTR(v, 0), GET_PTR(vec, start), len * vec->data_size);
        } else {
            copy_fn copy = vec->ops->copy_fn;
            if (copy) {
                for (u64 i = 0; i < len; i++) {
                    copy(GET_PTR(v, i), GET_PTR(vec, i + start));
                }
            } else {
                memcpy(GET_PTR(v, 0), GET_PTR(vec, start), len * vec->data_size);
            }
        }

        v->size = len;
    }

    return v;
}


void GenVec_print(const GenVec* vec, print_fn fn)
{
    printf("[ ");
    for (u64 i = 0; i < vec->size; i++) {
        fn(GET_PTR(vec, i));
        putchar(' ');
    }
    putchar(']');
}


void GenVec_copy(GenVec* dest, const GenVec* src)
{
    if (dest == src) {
        return;
    }

    // Copy all fields (including ops pointer)
    memcpy(dest, src, sizeof(GenVec));

    dest->data = malloc(GET_SCALED(src, src->capacity));
    CHECK_FATAL(!dest->data, "dest data malloc failed");

    if (src->is_pod) {
        memcpy(dest->data, src->data, GET_SCALED(src, src->size));
    } else {
        copy_fn copy = src->ops->copy_fn;
        if (copy) {
            for (u64 i = 0; i < src->size; i++) {
                copy(GET_PTR(dest, i), GET_PTR(src, i));
            }
        } else {
            memcpy(dest->data, src->data, GET_SCALED(src, src->size));
        }
    }
}


void GenVec_move(GenVec* dest, GenVec** src)
{
    CHECK_FATAL(!*src, "*src is null");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    memcpy(dest, *src, sizeof(GenVec));

    (*src)->data = NULL;
    free(*src);
    *src = NULL;
}


static void GenVec_grow(GenVec* vec)
{
    u64 new_cap;
    if (vec->capacity < GENVEC_MIN_CAPACITY) {
        new_cap = vec->capacity + 1;
    } else {
        new_cap = (u64)((float)vec->capacity * GENVEC_GROWTH);
        if (new_cap <= vec->capacity) {
            new_cap = vec->capacity + 1;
        }
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, new_cap));
    CHECK_FATAL(!new_data, "data realloc failed");

    vec->data     = new_data;
    vec->capacity = new_cap;
}
