#include "gen_vector.h"
#include "wc_errno.h"

#include <string.h>



#define GENVEC_MIN_CAPACITY 4


// MACROS

// get ptr to elm at index i
#define GET_PTR(vec, i) \
    ((vec->data) + ((u64)(i) * ((vec)->data_size)))
// get total_size in bytes for i elements
#define GET_SCALED(vec, i) \
    ((u64)(i) * ((vec)->data_size))

#define MAYBE_GROW(vec)                                 \
    do {                                                \
        if (!vec->data || vec->size >= vec->capacity) { \
            genVec_grow(vec);                           \
        }                                               \
    } while (0)

#define MAYBE_SHRINK(vec)                                                  \
    do {                                                                   \
        if (vec->size <= (u64)((float)vec->capacity * GENVEC_SHRINK_AT)) { \
            genVec_shrink(vec);                                            \
        }                                                                  \
    } while (0)


//private functions

static void genVec_grow(genVec* vec);
static void genVec_shrink(genVec* vec);


// API Implementation

genVec* genVec_init(u64 n, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn)
{
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    genVec* vec = malloc(sizeof(genVec));
    CHECK_FATAL(!vec, "vec init failed");

    // Only allocate memory if n > 0, otherwise data can be NULL
    vec->data = (n > 0) ? malloc(data_size * n) : NULL;

    // Only check for allocation failure if we actually tried to allocate
    if (n > 0 && !vec->data) {
        free(vec);
        FATAL("data init failed");
    }

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->copy_fn   = copy_fn;
    vec->move_fn   = move_fn;
    vec->del_fn    = del_fn;

    return vec;
}


void genVec_init_stk(u64 n, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn, genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(data_size == 0, "data_size can't be 0");

    // Only allocate memory if n > 0, otherwise data can be NULL
    vec->data = (n > 0) ? malloc(data_size * n) : NULL;
    CHECK_FATAL(n > 0 && !vec->data, "data init failed");

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;
    vec->copy_fn   = copy_fn;
    vec->move_fn   = move_fn;
    vec->del_fn    = del_fn;
}

genVec* genVec_init_val(u64 n, const u8* val, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn)
{
    CHECK_FATAL(!val, "val can't be null");
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    genVec* vec = genVec_init(n, data_size, copy_fn, move_fn, del_fn);

    vec->size = n; //capacity set to n in upper func

    for (u64 i = 0; i < n; i++) {
        if (copy_fn) {
            copy_fn(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    }

    return vec;
}

void genVec_init_val_stk(u64 n, const u8* val, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn,
                         genVec* vec)
{
    CHECK_FATAL(!val, "val can't be null");
    CHECK_FATAL(n == 0, "cant init with val if n = 0");

    genVec_init_stk(n, data_size, copy_fn, move_fn, del_fn, vec);

    vec->size = n;

    for (u64 i = 0; i < n; i++) {
        if (copy_fn) {
            copy_fn(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, data_size);
        }
    }
}

void genVec_init_arr(u64 n, u8* arr, u32 data_size, copy_fn copy_fn, move_fn move_fn, delete_fn del_fn, genVec* vec)
{
    CHECK_FATAL(!arr, "arr is null");
    CHECK_FATAL(!vec, "vec is null");

    CHECK_FATAL(n == 0, "size of arr can't be 0");
    CHECK_FATAL(data_size == 0, "data_size of arr can't be 0");

    vec->data = arr;

    vec->size      = 0;
    vec->capacity  = n;
    vec->data_size = data_size;

    vec->copy_fn = copy_fn;
    vec->move_fn = move_fn;
    vec->del_fn  = del_fn;
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

    if (vec->del_fn) {
        // Custom cleanup for each element
        for (u64 i = 0; i < vec->size; i++) {
            vec->del_fn(GET_PTR(vec, i));
        }
    }

    free(vec->data);
    vec->data = NULL;
    // dont free vec as on stk (don't own memory)
}

void genVec_clear(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    if (vec->del_fn) { // owns resources
        for (u64 i = 0; i < vec->size; i++) {
            vec->del_fn(GET_PTR(vec, i));
        }
    }
    // doesn't free container
    vec->size = 0;
}

void genVec_reset(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    if (vec->del_fn) {
        for (u64 i = 0; i < vec->size; i++) {
            vec->del_fn(GET_PTR(vec, i));
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

    // Only grow, never shrink with reserve
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

    for (u64 i = vec->size; i < new_capacity; i++) {
        if (vec->copy_fn) {
            vec->copy_fn(GET_PTR(vec, i), val);
        } else {
            memcpy(GET_PTR(vec, i), val, vec->data_size);
        }
    }
    vec->size = new_capacity;
}

void genVec_shrink_to_fit(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    // min allowd cap or size
    u64 min_cap  = vec->size > GENVEC_MIN_CAPACITY ? vec->size : GENVEC_MIN_CAPACITY;
    u64 curr_cap = vec->capacity;

    // if curr cap is already equal (or less??) than min allowed cap
    if (curr_cap <= min_cap) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, min_cap));
    CHECK_FATAL(!new_data, "data realloc failed");

    // update data ptr
    vec->data     = new_data;
    vec->size     = min_cap;
    vec->capacity = min_cap;
}


void genVec_push(genVec* vec, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");

    // Check if we need to allocate or grow
    MAYBE_GROW(vec);

    if (vec->copy_fn) {
        vec->copy_fn(GET_PTR(vec, vec->size), data);
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

    // Check if we need to allocate or grow
    MAYBE_GROW(vec);

    if (vec->move_fn) {
        vec->move_fn(GET_PTR(vec, vec->size), data);
    } else {
        // copy the pointer to resource
        memcpy(GET_PTR(vec, vec->size), *data, vec->data_size);
        *data = NULL; // now arr owns the resource
    }

    vec->size++;
}

// pop can't be a move operation (array is contiguos)
void genVec_pop(genVec* vec, u8* popped)
{
    CHECK_FATAL(!vec, "vec is null");

    WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, );

    u8* last_elm = GET_PTR(vec, vec->size - 1);

    if (popped) {
        if (vec->copy_fn) {
            vec->copy_fn(popped, last_elm);
        } else {
            memcpy(popped, last_elm, vec->data_size);
        }
    }

    if (vec->del_fn) { // del func frees the resources owned by last_elm, but not ptr
        vec->del_fn(last_elm);
    }

    vec->size--; // set for re-write

    MAYBE_SHRINK(vec);
}

void genVec_get(const genVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
    CHECK_FATAL(!out, "need a valid out variable to get the element");

    if (vec->copy_fn) {
        vec->copy_fn(out, GET_PTR(vec, i));
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

void genVec_insert(genVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    if (i == vec->size) {
        genVec_push(vec, data);
        return;
    }

    MAYBE_GROW(vec);

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;
    // the place where we want to insert
    u8* src = GET_PTR(vec, i);

    // Shift elements right by one unit
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // Use memmove for overlapping regions

    //src pos is now free to insert (it's data copied to next location)
    if (vec->copy_fn) {
        vec->copy_fn(src, data);
    } else {
        memcpy(src, data, vec->data_size);
    }

    vec->size++;
}

void genVec_insert_move(genVec* vec, u64 i, u8** data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(!data, "data is null");
    CHECK_FATAL(!*data, "*data is null");
    CHECK_FATAL(i > vec->size, "index out of bounds");

    if (i == vec->size) {
        genVec_push_move(vec, data);
        return;
    }

    MAYBE_GROW(vec);

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;
    // the place where we want to insert
    u8* src = GET_PTR(vec, i);

    // Shift elements right by one unit
    u8* dest = GET_PTR(vec, i + 1);
    memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // Use memmove for overlapping regions


    if (vec->move_fn) {
        vec->move_fn(src, data);
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

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;

    vec->size += num_data; // no of new elements in chunk

    genVec_reserve(vec, vec->size); // reserve with new size

    // the place where we want to insert
    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        // Shift elements right by num_data units to right
        u8* dest = GET_PTR(vec, i + num_data);

        memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // using memmove for overlapping regions
    }

    //src pos is now free to insert (it's data copied to next location)
    if (vec->copy_fn) {
        for (u64 j = 0; j < num_data; j++) {
            vec->copy_fn(GET_PTR(vec, j + i), (data + (size_t)(j * vec->data_size)));
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

    // Calculate the number of elements to shift to right
    u64 elements_to_shift = vec->size - i;

    vec->size += num_data; // no of new elements in chunk

    genVec_reserve(vec, vec->size); // reserve with new size

    // the place where we want to insert
    u8* src = GET_PTR(vec, i);
    if (elements_to_shift > 0) {
        // Shift elements right by num_data units to right
        u8* dest = GET_PTR(vec, i + num_data);

        memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // using memmove for overlapping regions
    }

    //src pos is now free to insert (it's data copied to next location)
    // Move entire contiguous block at once
    memcpy(src, *data, GET_SCALED(vec, num_data));
    *data = NULL; // Transfer ownership
}

void genVec_remove(genVec* vec, u64 i, u8* out)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");

    if (out) {
        if (vec->copy_fn) {
            vec->copy_fn(out, GET_PTR(vec, i));
        } else {
            memcpy(out, GET_PTR(vec, i), vec->data_size);
        }
    }

    if (vec->del_fn) {
        vec->del_fn(GET_PTR(vec, i));
    }
    // Calculate the number of elements to shift
    u64 elements_to_shift = vec->size - i - 1;

    if (elements_to_shift > 0) {
        // Shift elements left to overwrite the deleted element
        u8* dest = GET_PTR(vec, i);
        u8* src  = GET_PTR(vec, i + 1);

        memmove(dest, src, GET_SCALED(vec, elements_to_shift)); // Use memmove for overlapping regions
    }

    vec->size--;

    MAYBE_SHRINK(vec);
}


void genVec_remove_range(genVec* vec, u64 l, u64 r)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(l >= vec->size, "index out of range");
    CHECK_FATAL(l > r, "invalid range");

    if (r >= vec->size) {
        r = vec->size - 1;
    }

    if (vec->del_fn) {
        for (u64 i = l; i <= r; i++) {
            u8* elm = GET_PTR(vec, i);
            vec->del_fn(elm);
        }
    }

    u64 elms_to_shift = vec->size - (r + 1);

    // move from r + 1 to l
    u8* dest = GET_PTR(vec, l);
    u8* src  = GET_PTR(vec, r + 1);
    memmove(dest, src, GET_SCALED(vec, elms_to_shift)); // Use memmove for overlapping regions

    vec->size -= (r - l + 1);

    MAYBE_SHRINK(vec);
}


void genVec_replace(genVec* vec, u64 i, const u8* data)
{
    CHECK_FATAL(!vec, "vec is null");
    CHECK_FATAL(i >= vec->size, "index out of bounds");
    CHECK_FATAL(!data, "data is null");

    u8* to_replace = GET_PTR(vec, i);

    if (vec->del_fn) {
        vec->del_fn(to_replace);
    }

    if (vec->copy_fn) {
        vec->copy_fn(to_replace, data);
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

    if (vec->del_fn) {
        vec->del_fn(to_replace);
    }

    if (vec->move_fn) {
        vec->move_fn(to_replace, data);
    } else {
        memcpy(to_replace, *data, vec->data_size);
        *data = NULL;
    }
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

    // if data ptr is null, no op
    genVec_destroy_stk(dest);

    // copy all fields
    memcpy(dest, src, sizeof(genVec));

    // malloc data ptr
    dest->data = malloc(GET_SCALED(src, src->capacity));
    CHECK_FATAL(!dest->data, "dest data malloc failed");

    // Copy elements
    if (src->copy_fn) {
        for (u64 i = 0; i < src->size; i++) {
            src->copy_fn(GET_PTR(dest, i), GET_PTR(src, i));
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
    // CHECK_FATAL((*src)->svo, "can't move a SVO (stack) vec");

    if (dest == *src) {
        *src = NULL;
        return;
    }

    // Transfer all fields from src to dest
    memcpy(dest, *src, sizeof(genVec));

    // Null out src's data pointer so it doesn't get freed
    (*src)->data = NULL;

    // Free src if it was-allocated
    // This only frees the genVec struct itself, not the data
    // (which was transferred to dest)
    free(*src);
    *src = NULL;
}


static void genVec_grow(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    u64 new_cap;
    if (vec->capacity < GENVEC_MIN_CAPACITY) {
        new_cap = vec->capacity + 1;
    } else {
        new_cap = (u64)((float)vec->capacity * GENVEC_GROWTH);
        if (new_cap <= vec->capacity) { // Ensure at least +1 growth
            new_cap = vec->capacity + 1;
        }
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, new_cap));
    CHECK_FATAL(!new_data, "data realloc failed");

    vec->data     = new_data;
    vec->capacity = new_cap;
}


static void genVec_shrink(genVec* vec)
{
    CHECK_FATAL(!vec, "vec is null");

    u64 reduced_cap = (u64)((float)vec->capacity * GENVEC_SHRINK_BY);
    if (reduced_cap < vec->size || reduced_cap == 0) {
        return;
    }

    u8* new_data = realloc(vec->data, GET_SCALED(vec, reduced_cap));
    if (!new_data) {
        WARN("shrink realloc failed");
        return; // keep original
    }

    vec->data     = new_data;
    vec->capacity = reduced_cap;
}


