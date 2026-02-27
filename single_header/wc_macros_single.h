#ifndef WC_WC_MACROS_SINGLE_H
#define WC_WC_MACROS_SINGLE_H

/*
 * wc_macros_single.h
 * Auto-generated single-header library.
 *
 * In EXACTLY ONE .c file, before including this header:
 *     #define WC_IMPLEMENTATION
 *     #include "wc_macros_single.h"
 *
 * All other files just:
 *     #include "wc_macros_single.h"
 */

/* ===== wc_macros.h ===== */
#ifndef WC_WC_MACROS_H
#define WC_WC_MACROS_H

/* C11 + GNU extensions used here:
 *   typeof  (__typeof__)  — GNU ext, available with Clang/GCC + -std=c11
 *   ({ })   statement expressions — GNU ext, Clang/GCC only
 */
#define typeof __typeof__


/* ════════════════════════════════════════════════════════════════════════════
 * STORAGE STRATEGY
 * ════════════════════════════════════════════════════════════════════════════
 *
 * Strategy A — by value: slot holds the full struct (sizeof(String) bytes)
 *   genVec* v = VEC_CX(String, 10, &wc_str_ops);
 *   Better cache locality. Addresses change on realloc.
 *
 * Strategy B — by pointer: slot holds a pointer (sizeof(String*) = 8 bytes)
 *   genVec* v = VEC_CX(String*, 10, &wc_str_ptr_ops);
 *   One extra dereference. Addresses stable across growth.
 *
 * ops structs (wc_str_ops, wc_str_ptr_ops, etc.) are defined in wc_helpers.h.
 */


// ── Creation ───────────────────────────────────────────────────────────────

// POD types (int, float, flat structs) — pass NULL for ops
#define VEC(T, cap)  genVec_init((cap), sizeof(T), NULL)
#define VEC_EMPTY(T) genVec_init(0, sizeof(T), NULL)

// Types with owned heap resources — pass a genVec_ops pointer
#define VEC_CX(T, cap, ops)  genVec_init((cap), sizeof(T), (ops))
#define VEC_EMPTY_CX(T, ops) genVec_init(0, sizeof(T), (ops))

// TODO: test
#define VEC_MAKE_OPS(copy, move, del) \
    (container_ops)                   \
    {                                 \
        (copy), (move), (del)         \
    }

// Stack variants
#define VEC_STK(T, cap, vec)         genVec_init_stk((cap), sizeof(T), NULL, (vec))
#define VEC_CX_STK(T, cap, ops, vec) genVec_init_stk((cap), sizeof(T), (ops), (vec))

/* Create vector from an initializer list
Usage:
    genVec* v = VEC_FROM_ARR(int, 4, ((int[4]){1,2,3,4}));
*/
#define VEC_FROM_ARR(T, n, arr)                         \
    ({                                                  \
        genVec* _v = genVec_init((n), sizeof(T), NULL); \
        for (u64 _i = 0; _i < (n); _i++) {              \
            genVec_push(_v, (u8*)&(arr)[_i]);           \
        }                                               \
        _v;                                             \
    })


// ── Push ───────────────────────────────────────────────────────────────────

/* VEC_PUSH — push any POD value */
#define VEC_PUSH(vec, val)                 \
    ({                                     \
        typeof(val) wvp_tmp = (val);       \
        genVec_push((vec), (u8*)&wvp_tmp); \
    })

/* VEC_PUSH_COPY — deep copy for complex types. Source stays valid. */
#define VEC_PUSH_COPY(vec, val)            \
    ({                                     \
        typeof(val) wpc_tmp = (val);       \
        genVec_push((vec), (u8*)&wpc_tmp); \
    })

/* VEC_PUSH_MOVE — transfer ownership. Source becomes NULL. */
#define VEC_PUSH_MOVE(vec, ptr)                \
    ({                                         \
        typeof(ptr) wvm_p = (ptr);             \
        genVec_push_move((vec), (u8**)&wvm_p); \
        (ptr) = wvm_p;                         \
    })

/* VEC_PUSH_CSTR — allocate a heap String and move it in. */
#define VEC_PUSH_CSTR(vec, cstr)                \
    ({                                          \
        String* wpc_s = string_from_cstr(cstr); \
        genVec_push_move((vec), (u8**)&wpc_s);  \
    })


// ── Access ─────────────────────────────────────────────────────────────────

#define VEC_AT(vec, T, i)     (*(T*)genVec_get_ptr((vec), (i)))
#define VEC_AT_MUT(vec, T, i) ((T*)genVec_get_ptr_mut((vec), (i)))
#define VEC_FRONT(vec, T)     (*(T*)genVec_front((vec)))
#define VEC_BACK(vec, T)      (*(T*)genVec_back((vec)))


// ── Mutate ─────────────────────────────────────────────────────────────────

#define VEC_SET(vec, i, val)                       \
    ({                                             \
        typeof(val) wvs_tmp = (val);               \
        genVec_replace((vec), (i), (u8*)&wvs_tmp); \
    })


// ── Pop ────────────────────────────────────────────────────────────────────

#define VEC_POP(vec, T)                 \
    ({                                  \
        T wvpop;                        \
        genVec_pop((vec), (u8*)&wvpop); \
        wvpop;                          \
    })


// ── Iterate ────────────────────────────────────────────────────────────────

#define VEC_FOREACH(vec, T, name)                        \
    for (u64 _wvf_i = 0; _wvf_i < (vec)->size; _wvf_i++) \
        for (T* name = (T*)genVec_get_ptr_mut((vec), _wvf_i); name; name = NULL)


/* ══════════════════════════════════════════════════════════════════════════
 * TYPED CONVENIENCE MACROS
 * (require wc_helpers.h to be included for the ops structs)
 * ══════════════════════════════════════════════════════════════════════════ */

// ── Vector creation shorthands ───────────────────────────────────────────

#define VEC_OF_INT(cap)     genVec_init((cap), sizeof(int), NULL)
#define VEC_OF_STR(cap)     VEC_CX(String, (cap), &wc_str_ops)
#define VEC_OF_STR_PTR(cap) VEC_CX(String*, (cap), &wc_str_ptr_ops)
#define VEC_OF_VEC(cap)     VEC_CX(genVec, (cap), &wc_vec_ops)
#define VEC_OF_VEC_PTR(cap) VEC_CX(genVec*, (cap), &wc_vec_ptr_ops)


// ── Push shorthands ──────────────────────────────────────────────────────

#define VEC_PUSH_VEC(outer, inner_ptr)     VEC_PUSH_MOVE((outer), (inner_ptr))
#define VEC_PUSH_VEC_PTR(outer, inner_ptr) VEC_PUSH_MOVE((outer), (inner_ptr))


// ── Hashmap shorthands ───────────────────────────────────────────────────

/*
 * MAP_PUT_INT_STR(map, int_key, cstr_literal)
 * Map must have int key, String val, created with &wc_str_ops for val.
 */
#define MAP_PUT_INT_STR(map, k, cstr_val)                         \
    do {                                                          \
        String* _v = string_from_cstr(cstr_val);                  \
        hashmap_put_val_move((map), (u8*)&(int){(k)}, (u8**)&_v); \
    } while (0)

/*
 * MAP_PUT_STR_STR(map, cstr_key, cstr_val)
 * Map must use &wc_str_ops for both key and val.
 */
#define MAP_PUT_STR_STR(map, cstr_key, cstr_val)       \
    do {                                               \
        String* _k = string_from_cstr(cstr_key);       \
        String* _v = string_from_cstr(cstr_val);       \
        hashmap_put_move((map), (u8**)&_k, (u8**)&_v); \
    } while (0)


// ── Put (COPY semantics) ──────────────────────────────────────────────────

#define MAP_PUT(map, key, val)                                \
    ({                                                        \
        typeof(key) _mk = (key);                              \
        typeof(val) _mv = (val);                              \
        hashmap_put((map), (const u8*)&_mk, (const u8*)&_mv); \
    })


// ── Put (MOVE semantics) ──────────────────────────────────────────────────

#define MAP_PUT_MOVE(map, kptr, vptr)                      \
    ({                                                     \
        typeof(kptr) _mkp = (kptr);                        \
        typeof(vptr) _mvp = (vptr);                        \
        hashmap_put_move((map), (u8**)&_mkp, (u8**)&_mvp); \
        (kptr) = _mkp;                                     \
        (vptr) = _mvp;                                     \
    })

#define MAP_PUT_KEY_MOVE(map, kptr, val)                             \
    ({                                                               \
        typeof(val) _mv = (val);                                     \
        hashmap_put_key_move((map), (u8**)&(kptr), (const u8*)&_mv); \
    })

#define MAP_PUT_VAL_MOVE(map, key, vptr)                             \
    ({                                                               \
        typeof(key) _mk = (key);                                     \
        hashmap_put_val_move((map), (const u8*)&_mk, (u8**)&(vptr)); \
    })


// ── Get ───────────────────────────────────────────────────────────────────

#define MAP_GET(map, V, key)                             \
    ({                                                   \
        V           _out;                                \
        typeof(key) _mk = (key);                         \
        hashmap_get((map), (const u8*)&_mk, (u8*)&_out); \
        _out;                                            \
    })



// Iterate over every FILLED bucket.
// name is a KV* pointing to each filled bucket in turn.
#define MAP_FOREACH_BUCKET(map, name)                        \
    for (u64 _mfb_i = 0; _mfb_i < (map)->capacity; _mfb_i++) \
        for (KV* name = ((map)->buckets[_mfb_i].state == FILLED) ? &(map)->buckets[_mfb_i] : NULL; name; name = NULL)

// Iterate over the val of every FILLED bucket, typed as T*.
#define MAP_FOREACH_VAL(map, T, name)                                                                          \
    for (u64 _mfv_i = 0; _mfv_i < (map)->capacity; _mfv_i++)                                                   \
        for (T* name = ((map)->buckets[_mfv_i].state == FILLED) ? (T*)(map)->buckets[_mfv_i].val : NULL; name; \
             name    = NULL)



#define SET_FOREACH_BUCKET(set, name)                        \
    for (u64 _sfb_i = 0; _sfb_i < (set)->capacity; _sfb_i++) \
        for (ELM* name = ((set)->buckets[_sfb_i].state == FILLED) ? &(set)->buckets[_sfb_i] : NULL; name; name = NULL)

#endif /* WC_WC_MACROS_H */

#ifdef WC_IMPLEMENTATION

#endif /* WC_IMPLEMENTATION */

#endif /* WC_WC_MACROS_SINGLE_H */
