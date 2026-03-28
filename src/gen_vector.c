#include "gen_vector.h"
#include "wc_errno.h"

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
            genVec_grow(vec);                           \
        }                                               \
    } while (0)


// ops accessors (safe when ops is NULL)
#define COPY_FN(vec) VEC_COPY_FN(vec)
#define MOVE_FN(vec) VEC_MOVE_FN(vec)
#define DEL_FN(vec)  VEC_DEL_FN(vec)


// private functions

static void genVec_grow(genVec* vec);


// API Implementation

genVec* genVec_init(u64 n, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    genVec* vec = malloc(sizeof(genVec));
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

    return vec;
}


void genVec_init_stk(u64 n, u32 data_size, const container_ops* ops, genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    vec->data = (n > 0) ? malloc(data_size * n) : NULL;
    CHECK_FATAL(n > 0 && !vec->data, "data init failed");

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->ops       = ops;
}


genVec* genVec_init_val(u64 n, const u8* val, u32 data_size, const container_ops* ops)
{
    CHECK_FATAL(!val, "val can't be null");
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    genVec* vec = genVec_init(n, data_size, ops);

    vec->size = n; // capacity set to n in genVec_init

    copy_fn copy = COPY_FN(vec);
    if (copy) {
        for (u64 i = 0; i < n; i++) {
            copy(GET_PTR(vec, i), val);
        }
    } else {
        for (u64 i = 0; i < n; i++) {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    }

    return vec;
}


void genVec_init_val_stk(u64 n, const u8* val, u32 data_size, const container_ops* ops, genVec* vec)
{
    CHECK_FATAL(!val, "val can't be null");
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    genVec_init_stk(n, data_size, ops, vec);

    vec->size = n;

    copy_fn copy = COPY_FN(vec);
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


void genVec_init_arr(u64 n, u8* arr, u32 data_size, const container_ops* ops, genVec* vec)
{
    CHECK_FATAL(!arr, "arr is null");
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(n == 0, "size of arr can't be 0");
    CHECK_FATAL(data_size == 0, "data_size of arr can't be 0");

    vec->data      = arr;
    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->ops       = ops;
}


void genVec_destroy(genVec* vec)
{
    genVec_destroy_stk(vec);
    free(vec);
}


void genVec_destroy_stk(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    if (!vec->data) {
        return;
    }

    delete_fn del = DEL_FN(vec);
    if (del) {
        for (u64 i = 0; i < vec->size; i++) {
            del(GET_PTR(vec, i));
        }
    }

    free(vec->data);
    vec->data = NULL;
}


void genVec_clear(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    delete_fn del = DEL_FN(vec);
    if (del) {
        for (u64 i = 0; i < vec->size; i++) {
            del(GET_PTR(vec, i));
        }
    }

    vec->size = 0;
}


void genVec_reset(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    delete_fn del = DEL_FN(vec);
    if (del) {
        for (u64 i = 0; i < vec->size; i++) {
            del(GET_PTR(vec, i));
        }
    }

    free(vec->data);
    vec->data     = NULL;
    vec->size     = 0;
    vec->capacity = 0;
}


void genVec_reserve(genVec* vec, u64 new_capacity)
{
    CHECK_FATAL(!vec, "vec is null");

    if (new_capacity <= vec->capacity) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, new_capacity));
    CHECK_FATAL(!new_data, "realloc failed");

    vec->data     = new_data;
    vec->capacity = new_capacity;
}


void genVec_reserve_val(genVec* vec, u64 new_capacity, const u8* val)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!val, "val is null");
    CHECK_FATAL(new_capacity < vec->size, "new_capacity must be >= current size");

    genVec_reserve(vec, new_capacity);

    copy_fn copy = COPY_FN(vec);
    if (copy) {
        for (u64 i = vec->size; i < new_capacity; i++) {
            copy(GET_PTR(vec, i), val);
        }
    } else {
        for (u64 i = vec->size; i < new_capacity; i++) {
            memcpy(GET_PTR(vec, i), val, vec->data_size);
        }
    }
    vec->size = new_capacity;
}


void genVec_shrink_to_fit(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

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


void genVec_push(genVec* vec, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");

    MAYBE_GROW(vec);

    copy_fn copy = COPY_FN(vec);
    if (copy) {
        copy(GET_PTR(vec, vec->size), data);
    } else {
        memcpy(GET_PTR(vec, vec->size), data, vec->data_size);
    }

    vec->size++;
}


void genVec_push_move(genVec* vec, u8** data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(!*data, "*data is null");

    MAYBE_GROW(vec);

    move_fn move = MOVE_FN(vec);

    if (move) {
        move(GET_PTR(vec, vec->size), data);
    } else {
        memcpy(GET_PTR(vec, vec->size), *data, vec->data_size);
        *data = NULL;
    }

    vec->size++;
}


void genVec_pop(genVec* vec, u8* popped)
{
    CHECK_FATAL(!vec, "vec is null");

    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, );

    u8* last_elm = GET_PTR(vec, vec->size - 1);

    if (popped) {
        copy_fn copy = COPY_FN(vec);
        if (copy) {
            copy(popped, last_elm);
        } else {
            memcpy(popped, last_elm, vec->data_size);
        }
    }

    delete_fn del = DEL_FN(vec);
    if (del) {
        del(last_elm);
    }

    vec->size--;
}

void genVec_swap_pop(genVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (out) {
        copy_fn copy = COPY_FN(vec);
        if (copy) {
            copy(out, GET_PTR(vec, i)); 
        } else {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        }
    }

    delete_fn del = DEL_FN(vec);
    if (del) {
        del(GET_PTR(vec, i));
    }

    // swap the last container with the removed one
    // if owns memory elsewhere, those are still valid, only container location changes 
    memcpy(GET_PTR(vec, i), GET_PTR(vec, vec->size-1), vec->data_size);
    vec->size--;
}

void genVec_swap(genVec* vec, u64 i, u64 j)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size || j >= vec->size, "index out of bounds");

    if (i == j) { return; }

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


void genVec_get(const genVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!out, "out is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    copy_fn copy = COPY_FN(vec);

    if (copy) {
        copy(out, GET_PTR(vec, i));
    } else {
        memcpy(out, GET_PTR(vec, i), vec->data_size);
    }
}


const u8* genVec_get_ptr(const genVec* vec, u64 i)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    return GET_PTR(vec, i);
}


u8* genVec_get_ptr_mut(const genVec* vec, u64 i)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    return GET_PTR(vec, i);
}


void genVec_replace(genVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
    CHECK_FATAL(!data, "data is null");

    u8* to_replace = GET_PTR(vec, i);

    delete_fn del = DEL_FN(vec);
    if (del) {
        del(to_replace);
    }

    copy_fn copy = COPY_FN(vec);
    if (copy) {
        copy(to_replace, data);
    } else {
        memcpy(to_replace, data, vec->data_size);
    }
}


void genVec_replace_move(genVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
    CHECK_FATAL(!data, "need a valid data variable");
    CHECK_FATAL(!*data, "need a valid *data variable");

    u8* to_replace = GET_PTR(vec, i);

    delete_fn del = DEL_FN(vec);
    if (del) {
        del(to_replace);
    }

    move_fn move = MOVE_FN(vec);
    if (move) {
        move(to_replace, data);
    } else {
        memcpy(to_replace, *data, vec->data_size);
        *data = NULL;
    }
}


void genVec_insert(genVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    u64 elements_to_shift = vec->size - i;

    MAYBE_GROW(vec);

    u8* src  = GET_PTR(vec, i);
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift));

    copy_fn copy = COPY_FN(vec);
    if (copy) {
        copy(src, data);
    } else {
        memcpy(src, data, vec->data_size);
    }

    vec->size++;
}


void genVec_insert_move(genVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data ptr is null");
    CHECK_FATAL(!*data, "*data is null");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    u64 elements_to_shift = vec->size - i;

    MAYBE_GROW(vec);

    u8* src  = GET_PTR(vec, i);
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift));

    move_fn move = MOVE_FN(vec);
    if (move) {
        move(src, data);
    } else {
        memcpy(src, *data, vec->data_size);
        *data = NULL;
    }

    vec->size++;
}


void genVec_insert_multi(genVec* vec, u64 i, const u8* data, u64 num_data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(num_data == 0, "num_data can't be 0");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    u64 elements_to_shift = vec->size - i;

    vec->size += num_data;
    genVec_reserve(vec, vec->size);

    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i + num_data);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
    }

    copy_fn copy = COPY_FN(vec);
    if (copy) {
        for (u64 j = 0; j < num_data; j++) {
            copy(GET_PTR(vec, j + i), data + (size_t)(j * vec->data_size));
        }
    } else {
        memcpy(src, data, GET_SCALED(vec, num_data));
    }
}


void genVec_insert_multi_move(genVec* vec, u64 i, u8** data, u64 num_data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(!*data, "*data is null");
    CHECK_FATAL(num_data == 0, "num_data can't be 0");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    u64 elements_to_shift = vec->size - i;

    vec->size += num_data;
    genVec_reserve(vec, vec->size);

    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        u8* dest = GET_PTR(vec, i + num_data);
        memmove(dest, src, GET_SCALED(vec, elements_to_shift));
    }

    memcpy(src, *data, GET_SCALED(vec, num_data));
    *data = NULL;
}


void genVec_remove(genVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (out) {
        copy_fn copy = COPY_FN(vec);
        if (copy) {
            copy(out, GET_PTR(vec, i));
        } else {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        }
    }

    delete_fn del = DEL_FN(vec);
    if (del) {
        del(GET_PTR(vec, i));
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
void genVec_remove_range(genVec* vec, u64 start, u64 len)
{
    CHECK_FATAL(!vec, "vec is null");
    if (len == 0) { return; }
    CHECK_FATAL(start >= vec->size, "start out of range");

    if (start + len >= vec->size) {
        len = vec->size - start;
    }

    delete_fn del = DEL_FN(vec);
    if (del) {
        for (u64 i = 0; i < len; i++) {
            del(GET_PTR(vec, start + i));
        }
    }

    u8* dest = GET_PTR(vec, start);
    u8* src  = GET_PTR(vec, start + len);
    memmove(dest, src, GET_SCALED(vec, vec->size - len));

    vec->size -= len;
}


const u8* genVec_front(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, NULL);
    return GET_PTR(vec, 0);
}


const u8* genVec_back(const genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, NULL);
    return GET_PTR(vec, vec->size - 1);
}


u64 genVec_find(const genVec* vec, u8* elm, compare_fn cmp_fn)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!elm, "elm is null");

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

    return (u64)-1;
}


genVec* genVec_subarr(const genVec* vec, u64 start, u64 len)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(start >= vec->size, "out of bounds");

    copy_fn copy = COPY_FN(vec);

    if (start + len >= vec->size) {
        len = vec->size - start;
    }

    genVec* v = genVec_init(len, vec->data_size, vec->ops);

    if (len > 0) {
        if (copy) {
            for (u64 i = 0; i < len; i++) {
                copy(GET_PTR(v, i), GET_PTR(vec, i + start));
            }
        } else {
            memcpy(GET_PTR(v, 0), GET_PTR(vec, start), len * vec->data_size);
        }

        v->size = len;
    }

    return v;
}


void genVec_print(const genVec* vec, print_fn print_fn)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!print_fn, "print func is null");

    printf("[ ");
    for (u64 i = 0; i < vec->size; i++) {
        print_fn(GET_PTR(vec, i));
        putchar(' ');
    }
    putchar(']');
}


void genVec_copy(genVec* dest, const genVec* src)
{
    CHECK_FATAL(!dest, "dest is null");
    CHECK_FATAL(!src, "src is null");

    if (dest == src) {
        return;
    }

    genVec_destroy_stk(dest);

    // Copy all fields (including ops pointer)
    memcpy(dest, src, sizeof(genVec));

    // TODO: fix for copying into uninited memory ?
    // dest->data = calloc(src->capacity, src->data_size);
    dest->data = malloc(GET_SCALED(src, src->capacity));
    CHECK_FATAL(!dest->data, "dest data calloc failed");

    copy_fn copy = COPY_FN(src);
    if (copy) {
        for (u64 i = 0; i < src->size; i++) {
            copy(GET_PTR(dest, i), GET_PTR(src, i));
        }
    } else {
        memcpy(dest->data, src->data, GET_SCALED(src, src->size));
    }
}


void genVec_move(genVec* dest, genVec** src)
{
    CHECK_FATAL(!src, "src is null");
    CHECK_FATAL(!*src, "*src is null");
    CHECK_FATAL(!dest, "dest is null");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    memcpy(dest, *src, sizeof(genVec));

    (*src)->data = NULL;
    free(*src);
    *src = NULL;
}


static void genVec_grow(genVec* vec)
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
