#ifndef WC_MACROS_H
#define WC_MACROS_H

// _Generic association expressions must name declared
// symbols in every TU that sees this header, even when the macro is never used.
#include "common.h"
#include "map_setup.h"
#include "wc_helpers.h"


/* C11 + GNU extensions used here:
 *   typeof  (__typeof__)  — GNU ext, available with Clang/GCC + -std=c11
 *   ({ })   statement expressions — GNU ext, Clang/GCC only
 */

#define typeof __typeof__

/* WC_ASSERT_ELEM_SIZE — developer guard for typed macro layers.
 * Fires when a type-asserting macro (VEC_AT, VEC_POP, ...) is used on a vec
 * whose element size doesn't match sizeof(T) — i.e. the wrong T was passed.
 * The container never knows T, so this lives in the macro layer.
 */
#define WC_ASSERT_ELEM_SIZE(vec, T)                                                               \
    WC_ASSERT((vec)->data_size == sizeof(T),                                                      \
              "element size mismatch: " #vec " (data_size=%u), macro given " #T " (sizeof=%llu)", \
              (unsigned)(vec)->data_size, (unsigned long long)sizeof(T))


/* WC_OPS — pick the right container_ops for T at compile time.
 * Requires wc_helpers.h (the ops instances it names live there).
 *   VEC_OF(int, 8)                  -> POD, NULL ops
 *   VEC_OF(String, 8)               -> &wc_str_ops (by value)
 *   VEC_OF(String*, 8)              -> &wc_str_ptr_ops
 *   VEC_OF(GenVec, 8)               -> &wc_vec_ops
 *   VEC_OF(GenVec*, 8)              -> &wc_vec_ptr_ops
 * Unknown types fall back to POD (NULL ops).
 */
#define WC_OPS(T)                                             \
    _Generic((T*)0,                                           \
        String*: (const wc_container_ops*)&wc_str_ops,        \
        String * *: (const wc_container_ops*)&wc_str_ptr_ops, \
        GenVec*: (const wc_container_ops*)&wc_vec_ops,        \
        GenVec * *: (const wc_container_ops*)&wc_vec_ptr_ops, \
        default: (const wc_container_ops*)NULL)



/* STORAGE STRATEGY
 * ================
 *
 * Strategy A — by value: slot holds the full struct (sizeof(String) bytes)
 *   GenVec v = VEC_CX(String, 10, &wc_str_ops);
 *   Better cache locality. Addresses change on realloc.
 *
 * Strategy B — by pointer: slot holds a pointer (sizeof(String*) = 8 bytes)
 *   GenVec v = VEC_CX(String*, 10, &wc_str_ptr_ops);
 *   One extra dereference. Addresses stable across growth.
 *
 * ops structs (wc_str_ops, wc_str_ptr_ops, etc.) are defined in wc_helpers.h.
 */


// Creation
// Every vector macro returns a GenVec BY VALUE. The plain form uses libc; the
// _IN form takes the allocator first (plan 3.3 macro table).

// POD types (int, float, flat structs) — NULL ops
#define VEC_IN(A, T, cap) GenVec_create((A), (cap), sizeof(T), NULL)
#define VEC(T, cap)       VEC_IN(WC_LIBC, T, cap)
#define VEC_EMPTY(T)      VEC(T, 0)

// Types with owned resources — explicit ops
#define VEC_CX_IN(A, T, cap, ops) GenVec_create((A), (cap), sizeof(T), (ops))
#define VEC_CX(T, cap, ops)       VEC_CX_IN(WC_LIBC, T, cap, ops)
#define VEC_EMPTY_CX(T, ops)      VEC_CX(T, 0, ops)

// Ops picked automatically from T (see WC_OPS).
#define VEC_OF_IN(A, T, cap) GenVec_create((A), (cap), sizeof(T), WC_OPS(T))
#define VEC_OF(T, cap)       VEC_OF_IN(WC_LIBC, T, cap)

// New vector of T on the same allocator as an existing vector `v`.
#define VEC_LIKE(v, T, cap) GenVec_create((v)->alloc, (cap), sizeof(T), WC_OPS(T))

/* Key hash/compare picked from K, like WC_OPS. Owning keys must hash their
 * CONTENT: hashing the raw String struct (pointer, capacity, allocator) makes
 * every lookup miss. NULL = wyhash / memcmp over the key bytes (POD keys).
 * GenVec keys have no content hash: pass explicit functions to HashMap_create. */
#define WC_HASH_FN(K)                               \
    _Generic((K*)0,                                 \
        String*: (custom_hash_fn)wyhash_str,        \
        String * *: (custom_hash_fn)wyhash_str_ptr, \
        default: (custom_hash_fn)NULL)

#define WC_CMP_FN(K)                            \
    _Generic((K*)0,                             \
        String*: (wc_compare_fn)str_cmp,        \
        String * *: (wc_compare_fn)str_cmp_ptr, \
        default: (wc_compare_fn)NULL)

#define MAP_OF_IN(A, K, V) \
    HashMap_create((A), sizeof(K), sizeof(V), WC_HASH_FN(K), WC_CMP_FN(K), WC_OPS(K), WC_OPS(V))
#define MAP_OF(K, V)       MAP_OF_IN(WC_LIBC, K, V)

#define VEC_MAKE_OPS(copy, move, del) \
    (wc_container_ops)                \
    {                                 \
        (copy), (move), (del)         \
    }

/* Create vector from an initializer list (the elements are copied)
Usage:
    GenVec v = VEC_FROM_ARR(int, 4, ((int[4]){1,2,3,4}));
*/
#define VEC_FROM_ARR_IN(A, T, n, arr)                           \
    ({                                                          \
        GenVec _vfa = GenVec_create((A), (n), sizeof(T), NULL); \
        for (u64 _i = 0; _i < (u64)(n); _i++) {                 \
            GenVec_push(&_vfa, (const u8*)&(arr)[_i]);          \
        }                                                       \
        _vfa;                                                   \
    })
#define VEC_FROM_ARR(T, n, arr) VEC_FROM_ARR_IN(WC_LIBC, T, n, arr)


// Push

// VEC_PUSH — push any POD value
#define VEC_PUSH(vec, val)                 \
    ({                                     \
        typeof(val) wvp_tmp = (val);       \
        GenVec_push((vec), (u8*)&wvp_tmp); \
    })

// VEC_PUSH_MOVE — transfer ownership of an LVALUE element (struct or pointer).
// The source is left zeroed (a pointer source becomes NULL).
//   String s = ...;       VEC_PUSH_MOVE(&v, s);        // by-value vec
//   String* p = ...;      VEC_PUSH_MOVE(&v, p);        // by-pointer vec, p == NULL after
#define VEC_PUSH_MOVE(vec, lval)                  \
    ({                                            \
        WC_ASSERT_ELEM_SIZE((vec), typeof(lval)); \
        GenVec_push_move((vec), (u8*)&(lval));    \
    })

// VEC_PUSH_CSTR — build a String from a C string and move it into a String
// vector, by value (wc_str_ops) or by pointer (wc_str_ptr_ops).
// Uses the container's own allocator so strings follow their vector.
#define VEC_PUSH_CSTR(vec, cstr)                                                        \
    ({                                                                                  \
        if ((vec)->data_size == sizeof(String)) {                                       \
            /* by value: build on the vec's allocator, move the struct in */            \
            String _wpc_s = String_from_cstr((vec)->alloc, (cstr));                     \
            GenVec_push_move((vec), (u8*)&_wpc_s); /* _wpc_s zeroed by move */          \
        } else {                                                                        \
            /* by pointer: box the String in the vec's allocator */                     \
            WC_ASSERT_ELEM_SIZE((vec), String*);                                        \
            String* _wpc_p = WC_BOX_IN((vec)->alloc, String, String_from_cstr, (cstr)); \
            GenVec_push_move((vec), (u8*)&_wpc_p); /* slot owns the pointer */          \
        }                                                                               \
    })


// Access
// All type-asserting macros guard with WC_ASSERT_ELEM_SIZE, pass the right T.

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

#define VEC_OF_INT(cap)     VEC(int, (cap))
#define VEC_OF_STR(cap)     VEC_CX(String, (cap), &wc_str_ops)
#define VEC_OF_STR_PTR(cap) VEC_CX(String*, (cap), &wc_str_ptr_ops)
#define VEC_OF_VEC(cap)     VEC_CX(GenVec, (cap), &wc_vec_ops)
#define VEC_OF_VEC_PTR(cap) VEC_CX(GenVec*, (cap), &wc_vec_ptr_ops)


// Push shorthands

#define VEC_PUSH_VEC(outer, inner)         VEC_PUSH_MOVE((outer), (inner))     // inner: GenVec lvalue
#define VEC_PUSH_VEC_PTR(outer, inner_ptr) VEC_PUSH_MOVE((outer), (inner_ptr)) // inner_ptr: GenVec* lvalue


// Hashmap shorthands

/*
 * MAP_PUT_INT_STR(map, int_key, cstr_literal)
 * Map must have int key, String val, created with &wc_str_ops for val.
 */
#define MAP_PUT_INT_STR(map, k, cstr_val)                                \
    ({                                                                   \
        String _v = String_from_cstr((map)->alloc, (cstr_val));          \
        b8 _r = HashMap_put_val_move((map), (u8*)&(int){(k)}, (u8*)&_v); \
        _r;                                                              \
    })

/*
 * MAP_PUT_STR_INT(map, cstr_key, int_val)
 * Map must use &wc_str_ops for key.
 */
#define MAP_PUT_STR_INT(map, cstr_key, int_val)                              \
    ({                                                                       \
        String _k = String_from_cstr((map)->alloc, (cstr_key));              \
        b8 _r = HashMap_put_key_move((map), (u8*)&_k, (u8*)&(int){int_val}); \
        _r;                                                                  \
    })

/*
 * MAP_PUT_STR_STR(map, cstr_key, cstr_val)
 * Map must use &wc_str_ops for both key and val.
 */
#define MAP_PUT_STR_STR(map, cstr_key, cstr_val)                \
    ({                                                          \
        String _k = String_from_cstr((map)->alloc, (cstr_key)); \
        String _v = String_from_cstr((map)->alloc, (cstr_val)); \
        b8 _r = HashMap_put_move((map), (u8*)&_k, (u8*)&_v);    \
        _r;                                                     \
    })


// Put (COPY semantics)

#define MAP_PUT(map, key, val)                                \
    ({                                                        \
        typeof(key) _mk = (key);                              \
        typeof(val) _mv = (val);                              \
        HashMap_put((map), (const u8*)&_mk, (const u8*)&_mv); \
    })


// Put (MOVE semantics): key/val are LVALUE elements, left zeroed after the call.

#define MAP_PUT_MOVE(map, klval, vlval) HashMap_put_move((map), (u8*)&(klval), (u8*)&(vlval))

#define MAP_PUT_KEY_MOVE(map, klval, val)                            \
    ({                                                               \
        typeof(val) _mv = (val);                                     \
        HashMap_put_key_move((map), (u8*)&(klval), (const u8*)&_mv); \
    })

#define MAP_PUT_VAL_MOVE(map, key, vlval)                            \
    ({                                                               \
        typeof(key) _mk = (key);                                     \
        HashMap_put_val_move((map), (const u8*)&_mk, (u8*)&(vlval)); \
    })


// Get

// V - Type of the value. Asserts key is present.
#define MAP_GET(map, V, key)                                                                \
    ({                                                                                      \
        V           _out;                                                                   \
        typeof(key) _mk = (key);                                                            \
        memset(&_out, 0, sizeof(_out));                                                     \
        /* the lookup must stay OUTSIDE WC_ASSERT: it is not evaluated under NDEBUG */    \
        b8 _found = HashMap_get((map), (const u8*)&_mk, (u8*)&_out);                        \
        WC_ASSERT(_found, "MAP_GET: key not found");                                        \
        (void)_found;                                                                       \
        _out;                                                                               \
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

#define SET_FROM_VEC(vec, hash_fn, cmp_fn)                                                          \
    ({                                                                                              \
        HashSet _set = HashSet_create((vec)->alloc, (vec)->data_size, hash_fn, cmp_fn, (vec)->ops); \
        for (u64 i = 0, _n = GenVec_size(vec); i < _n; i++) {                                       \
            HashSet_insert(&_set, GenVec_get_ptr((vec), i));                                        \
        }                                                                                           \
        _set;                                                                                       \
    })

#define SET_INSERT(set, elm)                  \
    ({                                        \
        typeof(elm) _temp = (elm);            \
        HashSet_insert((set), (u8*)&(_temp)); \
    })

// lval is left zeroed (moved in, or destroyed if already present)
#define SET_INSERT_MOVE(set, lval) HashSet_insert_move((set), (u8*)&(lval))

#define SET_INSERT_CSTR(set, cstr)                          \
    ({                                                      \
        String _s = String_from_cstr((set)->alloc, (cstr)); \
        b8 _r = HashSet_insert_move((set), (u8*)&_s);       \
        _r;                                                 \
    })


#define SET_FOREACH(set, T, name)                                                                                     \
    for (u64 _i = 0, _n = HashSet_bucket_count(set); _i < _n; _i++)                                                   \
        for (const T* name = HashSet_bucket_occupied((set), _i) ? (const T*)HashSet_bucket_elm_ptr((set), _i) : NULL; \
             name; name    = NULL)


// Stack macros

// Stack_create returns Stack by value. Use WC_LIBC for the common case.
// STACK_CREATE / STACK_CREATE_IN: by value on the stack (no heap shell).
#define STACK_CREATE(T, cap)            Stack_create(WC_LIBC, (cap), sizeof(T), NULL)
#define STACK_CREATE_CX(T, cap, ops)    Stack_create(WC_LIBC, (cap), sizeof(T), (ops))
#define STACK_CREATE_IN(A, T, cap)      Stack_create((A), (cap), sizeof(T), NULL)
#define STACK_CREATE_CX_IN(A, T, cap, ops) Stack_create((A), (cap), sizeof(T), (ops))
#define STACK_PUSH(stk, val)            VEC_PUSH((stk), (val))
#define STACK_PUSH_MOVE(stk, lval)      VEC_PUSH_MOVE((stk), lval)
#define STACK_POP(stk, T)               VEC_POP((stk), T)
#define STACK_AT(stk, T, i)             VEC_AT((stk), T, (i))
#define STACK_FOREACH(stk, T, name)     VEC_FOREACH((stk), T, name)


// Queue macros

// Queue_create returns Queue by value. Use WC_LIBC for the common case.
#define QUEUE_CREATE(T, cap)            Queue_create(WC_LIBC, (cap), sizeof(T), NULL)
#define QUEUE_CREATE_CX(T, cap, ops)    Queue_create(WC_LIBC, (cap), sizeof(T), (ops))
#define QUEUE_CREATE_IN(A, T, cap)      Queue_create((A), (cap), sizeof(T), NULL)
#define QUEUE_CREATE_CX_IN(A, T, cap, ops) Queue_create((A), (cap), sizeof(T), (ops))
#define QUEUE_PUSH(q, val)              \
    ({                                  \
        typeof(val) _qp_tmp = (val);    \
        Queue_push((q), (u8*)&_qp_tmp); \
    })

#define QUEUE_PUSH_MOVE(q, lval) Queue_push_move((q), (u8*)&(lval))

#define QUEUE_PUSH_CSTR(q, cstr)                               \
    ({                                                         \
        String _qp_s = String_from_cstr((q)->arr.alloc, cstr); \
        Queue_push_move((q), (u8*)&_qp_s);                     \
    })

#define QUEUE_POP(q, T)                \
    ({                                 \
        T _qp_out;                     \
        Queue_pop((q), (u8*)&_qp_out); \
        _qp_out;                       \
    })

#define QUEUE_PEEK(q, T) (*(T*)Queue_peek_ptr(q))


#endif // WC_MACROS_H
