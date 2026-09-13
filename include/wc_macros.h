#ifndef WC_MACROS_H
#define WC_MACROS_H

// Required by WC_OPS (6-I): _Generic association expressions must name declared
// symbols in every TU that sees this header, even when the macro is never used.
#include "common.h"
#include "wc_helpers.h"


/* C11 + GNU extensions used here:
 *   typeof  (__typeof__)  — GNU ext, available with Clang/GCC + -std=c11
 *   ({ })   statement expressions — GNU ext, Clang/GCC only
 */

#define typeof __typeof__

/* WC_ASSERT_ELEM_SIZE — developer guard for typed macro layers (6-H).
 * Fires when a type-asserting macro (VEC_AT, VEC_POP, ...) is used on a vec
 * whose element size doesn't match sizeof(T) — i.e. the wrong T was passed.
 * The container never knows T, so this lives in the macro layer.
 */
#define WC_ASSERT_ELEM_SIZE(vec, T)                                                                     \
    do {                                                                                                \
        CHECK_FATAL((vec)->data_size != sizeof(T),                                                      \
                    "element size mismatch: " #vec " (data_size=%u), macro given " #T " (sizeof=%llu)", \
                    (unsigned)(vec)->data_size, (unsigned long long)sizeof(T));                         \
    } while (0)


/* WC_OPS — pick the right container_ops for T at compile time (6-I).
 * Requires wc_helpers.h (the ops instances it names live there).
 *   VEC_CREATE_OF(int, 8)                  -> POD, NULL ops
 *   VEC_CREATE_OF(String, 8)               -> &wc_str_ops (by value)
 *   VEC_CREATE_OF(String*, 8)              -> &wc_str_ptr_ops
 *   VEC_CREATE_OF(GenVec, 8)               -> &wc_vec_ops
 *   VEC_CREATE_OF(GenVec*, 8)              -> &wc_vec_ptr_ops
 * Unknown types fall back to POD (NULL ops).
 */
#define WC_OPS(T)                                          \
    _Generic((T*)0,                                        \
        String*: (const container_ops*)&wc_str_ops,        \
        String * *: (const container_ops*)&wc_str_ptr_ops, \
        GenVec*: (const container_ops*)&wc_vec_ops,        \
        GenVec * *: (const container_ops*)&wc_vec_ptr_ops, \
        default: (const container_ops*)NULL)



/* STORAGE STRATEGY
 * ================
 *
 * Strategy A — by value: slot holds the full struct (sizeof(String) bytes)
 *   GenVec* v = VEC_CX(String, 10, &wc_str_ops);
 *   Better cache locality. Addresses change on realloc.
 *
 * Strategy B — by pointer: slot holds a pointer (sizeof(String*) = 8 bytes)
 *   GenVec* v = VEC_CX(String*, 10, &wc_str_ptr_ops);
 *   One extra dereference. Addresses stable across growth.
 *
 * ops structs (wc_str_ops, wc_str_ptr_ops, etc.) are defined in wc_helpers.h.
 */


// Creation

// POD types (int, float, flat structs) — pass NULL for ops
#define VEC(T, cap)  GenVec_create((cap), sizeof(T), NULL)
#define VEC_EMPTY(T) GenVec_create(0, sizeof(T), NULL)

// Types with owned heap resources — pass a GenVec_ops pointer
#define VEC_CX(T, cap, ops)  GenVec_create((cap), sizeof(T), (ops))
#define VEC_EMPTY_CX(T, ops) GenVec_create(0, sizeof(T), (ops))

// Ops picked automatically from T (see WC_OPS).
#define VEC_CREATE_OF(T, cap) GenVec_create((cap), sizeof(T), WC_OPS(T))
#define MAP_CREATE_OF(K, V)   HashMap_create(sizeof(K), sizeof(V), NULL, NULL, WC_OPS(K), WC_OPS(V))

#define VEC_MAKE_OPS(copy, move, del) \
    (container_ops)                   \
    {                                 \
        (copy), (move), (del)         \
    }

// Stack variants
#define VEC_STK(T, cap, vec)         GenVec_create_stk((vec), (cap), sizeof(T), NULL)
#define VEC_CX_STK(T, cap, ops, vec) GenVec_create_stk((vec), (cap), sizeof(T), (ops))

/* Create vector from an initializer list
Usage:
    GenVec* v = VEC_FROM_ARR(int, 4, ((int[4]){1,2,3,4}));
*/
#define VEC_FROM_ARR(T, n, arr)                           \
    ({                                                    \
        GenVec* _v = GenVec_create((n), sizeof(T), NULL); \
        for (u64 _i = 0; _i < (n); _i++) {                \
            GenVec_push(_v, (u8*)&(arr)[_i]);             \
        }                                                 \
        _v;                                               \
    })


// Push

// VEC_PUSH — push any POD value
#define VEC_PUSH(vec, val)                 \
    ({                                     \
        typeof(val) wvp_tmp = (val);       \
        GenVec_push((vec), (u8*)&wvp_tmp); \
    })

// VEC_PUSH_MOVE — transfer ownership. Source becomes NULL.
#define VEC_PUSH_MOVE(vec, ptr)                \
    ({                                         \
        typeof(ptr) wvm_p = (ptr);             \
        GenVec_push_move((vec), (u8**)&wvm_p); \
        (ptr) = wvm_p;                         \
    })

// VEC_PUSH_CSTR — allocate a heap String and move it in.
#define VEC_PUSH_CSTR(vec, cstr)                \
    ({                                          \
        String* wpc_s = String_from_cstr(cstr); \
        GenVec_push_move((vec), (u8**)&wpc_s);  \
    })


// Access
// All type-asserting macros guard with WC_ASSERT_ELEM_SIZE (6-H) — pass the right T.

#define VEC_AT(vec, T, i)                \
    ({                                   \
        WC_ASSERT_ELEM_SIZE((vec), T);   \
        *(T*)GenVec_get_ptr((vec), (i)); \
    })

#define VEC_AT_MUT(vec, T, i)               \
    ({                                      \
        WC_ASSERT_ELEM_SIZE((vec), T);      \
        (T*)GenVec_get_ptr_mut((vec), (i)); \
    })

#define VEC_FRONT(vec, T)              \
    ({                                 \
        WC_ASSERT_ELEM_SIZE((vec), T); \
        *(T*)GenVec_front((vec));      \
    })

#define VEC_BACK(vec, T)               \
    ({                                 \
        WC_ASSERT_ELEM_SIZE((vec), T); \
        *(T*)GenVec_back((vec));       \
    })


// Mutate

#define VEC_SET(vec, i, val)                       \
    ({                                             \
        typeof(val) wvs_tmp = (val);               \
        GenVec_replace((vec), (i), (u8*)&wvs_tmp); \
    })


// Pop

#define VEC_POP(vec, T)                 \
    ({                                  \
        WC_ASSERT_ELEM_SIZE((vec), T);  \
        T wvpop;                        \
        GenVec_pop((vec), (u8*)&wvpop); \
        wvpop;                          \
    })


// Iterate
/* bounds hoisted into the outer loop (_wvf_n) and element fetch elided via
 * GenVec_get_ptr_mut_unsafe — the index is provably < size. NOTE: no size assert
 * here (a leading statement would break `if (x) VEC_FOREACH(...) ...` usage);
 * the T* assignment still gives compile-time type checking.
 */
#define VEC_FOREACH(vec, T, name)                                         \
    for (u64 _wvf_n = (vec)->size, _wvf_i = 0; _wvf_i < _wvf_n; _wvf_i++) \
        for (T* name = (T*)GenVec_get_ptr_mut_unsafe((vec), _wvf_i); name; name = NULL)



/* ══════════════════════════════════════════════════════════════════════════
 * TYPED CONVENIENCE MACROS
 * (require wc_helpers.h to be included for the ops structs)
 * ══════════════════════════════════════════════════════════════════════════ */

// Vector creation shorthands

#define VEC_OF_INT(cap)     GenVec_create((cap), sizeof(int), NULL)
#define VEC_OF_STR(cap)     VEC_CX(String, (cap), &wc_str_ops)
#define VEC_OF_STR_PTR(cap) VEC_CX(String*, (cap), &wc_str_ptr_ops)
#define VEC_OF_VEC(cap)     VEC_CX(GenVec, (cap), &wc_vec_ops)
#define VEC_OF_VEC_PTR(cap) VEC_CX(GenVec*, (cap), &wc_vec_ptr_ops)


// Push shorthands

#define VEC_PUSH_VEC(outer, inner_ptr)     VEC_PUSH_MOVE((outer), (inner_ptr))
#define VEC_PUSH_VEC_PTR(outer, inner_ptr) VEC_PUSH_MOVE((outer), (inner_ptr))


// Hashmap shorthands

/*
 * MAP_PUT_INT_STR(map, int_key, cstr_literal)
 * Map must have int key, String val, created with &wc_str_ops for val.
 */
#define MAP_PUT_INT_STR(map, k, cstr_val)                         \
    ({                                                            \
        String* _v = String_from_cstr(cstr_val);                  \
        HashMap_put_val_move((map), (u8*)&(int){(k)}, (u8**)&_v); \
    })

/*
 * MAP_PUT_STR_STR(map, cstr_key, cstr_val)
 * Map must use &wc_str_ops for both key and val.
 */
#define MAP_PUT_STR_STR(map, cstr_key, cstr_val)       \
    ({                                                 \
        String* _k = String_from_cstr(cstr_key);       \
        String* _v = String_from_cstr(cstr_val);       \
        HashMap_put_move((map), (u8**)&_k, (u8**)&_v); \
    })


// Put (COPY semantics)

#define MAP_PUT(map, key, val)                                \
    ({                                                        \
        typeof(key) _mk = (key);                              \
        typeof(val) _mv = (val);                              \
        HashMap_put((map), (const u8*)&_mk, (const u8*)&_mv); \
    })


// Put (MOVE semantics)

#define MAP_PUT_MOVE(map, kptr, vptr)                      \
    ({                                                     \
        typeof(kptr) _mkp = (kptr);                        \
        typeof(vptr) _mvp = (vptr);                        \
        HashMap_put_move((map), (u8**)&_mkp, (u8**)&_mvp); \
        (kptr) = _mkp;                                     \
        (vptr) = _mvp;                                     \
    })

#define MAP_PUT_KEY_MOVE(map, kptr, val)                             \
    ({                                                               \
        typeof(val) _mv = (val);                                     \
        HashMap_put_key_move((map), (u8**)&(kptr), (const u8*)&_mv); \
    })

#define MAP_PUT_VAL_MOVE(map, key, vptr)                             \
    ({                                                               \
        typeof(key) _mk = (key);                                     \
        HashMap_put_val_move((map), (const u8*)&_mk, (u8**)&(vptr)); \
    })


// Get

// V - Type of the value. Asserts key is present.
#define MAP_GET(map, V, key)                                                                     \
    ({                                                                                           \
        V           _out;                                                                        \
        typeof(key) _mk = (key);                                                                 \
        CHECK_FATAL(!HashMap_get((map), (const u8*)&_mk, (u8*)&_out), "MAP_GET: key not found"); \
        _out;                                                                                    \
    })

// Returns b8 (1 if found, 0 if not). Writes *out_ptr on hit.
#define MAP_TRY_GET(map, V, key, out_ptr)                    \
    ({                                                       \
        typeof(key) _mk = (key);                             \
        HashMap_get((map), (const u8*)&_mk, (u8*)(out_ptr)); \
    })



// Iterate

#define MAP_FOREACH_KEY(map, T, name)                                                                                 \
    for (u64 _i = 0, _n = HashMap_bucket_count(map); _i < _n; _i++)                                                   \
        for (const T* name = HashMap_bucket_occupied((map), _i) ? (const T*)HashMap_bucket_key_ptr((map), _i) : NULL; \
             name; name    = NULL)

#define MAP_FOREACH_VAL(map, T, name)                                                                           \
    for (u64 _i = 0, _n = HashMap_bucket_count(map); _i < _n; _i++)                                             \
        for (T* name = HashMap_bucket_occupied((map), _i) ? (T*)HashMap_bucket_val_ptr((map), _i) : NULL; name; \
             name    = NULL)


// Hashset shorthands

#define SET_FROM_VEC(vec, hash_fn, cmp_fn)                                             \
    ({                                                                                 \
        HashSet* _set = HashSet_create((vec)->data_size, hash_fn, cmp_fn, (vec)->ops); \
        for (u64 i = 0, _n = GenVec_size(vec); i < _n; i++) {                          \
            HashSet_insert(_set, GenVec_get_ptr((vec), i));                            \
        }                                                                              \
        _set;                                                                          \
    })

#define SET_INSERT(set, elm)                  \
    ({                                        \
        typeof(elm) _temp = (elm);            \
        HashSet_insert((set), (u8*)&(_temp)); \
    })

#define SET_INSERT_MOVE(set, ptr)                  \
    ({                                             \
        typeof(ptr) _ptr = (ptr);                  \
        HashSet_insert_move((set), (u8**)&(_ptr)); \
        (ptr) = _ptr;                              \
    })

#define SET_INSERT_CSTR(set, cstr)               \
    ({                                           \
        String* _s = String_from_cstr(cstr);     \
        HashSet_insert_move((set), (u8**)&(_s)); \
    })


#define SET_FOREACH(set, T, name)                                                                                     \
    for (u64 _i = 0, _n = HashSet_bucket_count(set); _i < _n; _i++)                                                   \
        for (const T* name = HashSet_bucket_occupied((set), _i) ? (const T*)HashSet_bucket_elm_ptr((set), _i) : NULL; \
             name; name    = NULL)


// Stack macros

#define STACK_CREATE(T, cap)         VEC(T, cap)
#define STACK_CREATE_CX(T, cap, ops) VEC_CX(T, cap, ops)
#define STACK_STK(T, cap, vec)       VEC_STK(T, cap, vec)
#define STACK_PUSH(stk, val)         VEC_PUSH((stk), (val))
#define STACK_PUSH_MOVE(stk, ptr)    VEC_PUSH_MOVE((stk), (ptr))
#define STACK_POP(stk, T)            VEC_POP((stk), T)
#define STACK_AT(stk, T, i)          VEC_AT((stk), T, (i))
#define STACK_FOREACH(stk, T, name)  VEC_FOREACH((stk), T, name)


// Queue macros

#define QUEUE_CREATE(T, cap)         Queue_create((cap), sizeof(T), NULL)
#define QUEUE_CREATE_CX(T, cap, ops) Queue_create((cap), sizeof(T), (ops))
#define QUEUE_PUSH(q, val)              \
    ({                                  \
        typeof(val) _qp_tmp = (val);    \
        Queue_push((q), (u8*)&_qp_tmp); \
    })

#define QUEUE_PUSH_MOVE(q, ptr)             \
    ({                                      \
        typeof(ptr) _qp_p = (ptr);          \
        Queue_push_move((q), (u8**)&_qp_p); \
        (ptr) = _qp_p;                      \
    })

#define QUEUE_PUSH_CSTR(q, cstr)                \
    ({                                          \
        String* _qp_s = String_from_cstr(cstr); \
        Queue_push_move((q), (u8**)&_qp_s);     \
    })

#define QUEUE_POP(q, T)                \
    ({                                 \
        T _qp_out;                     \
        Queue_pop((q), (u8*)&_qp_out); \
        _qp_out;                       \
    })

#define QUEUE_PEEK(q, T) (*(T*)Queue_peek_ptr(q))


#endif // WC_MACROS_H
