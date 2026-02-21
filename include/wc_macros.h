#ifndef WC_MACROS_H
#define WC_MACROS_H


/* C11 + GNU extensions used here:
 *   typeof  (__typeof__)  — GNU ext, available with Clang/GCC + -std=c11
 *   ({ })   statement expressions — GNU ext, Clang/GCC only
 */
#define typeof __typeof__


/* ════════════════════════════════════════════════════════════════════════════
 * STORAGE STRATEGY
 * ════════════════════════════════════════════════════════════════════════════
 *
 * Strategy A — by value: slot holds the full struct (sizeof(String) = 40 bytes)
 *   genVec* v = VEC_CX(String, 10, str_copy, str_move, str_del);
 *   Better cache locality. Addresses change on realloc — don't store elem pointers.
 *
 * Strategy B — by pointer: slot holds a pointer (sizeof(String*) = 8 bytes)
 *   genVec* v = VEC_CX(String*, 10, str_copy_ptr, str_move_ptr, str_del_ptr);
 *   One extra dereference. Addresses are stable across growth.
 *
 * The only visible difference at the call site: the T in VEC_AT and VEC_FOREACH.
 */


// ── Creation ───────────────────────────────────────────────────────────────

#define VEC(T, cap)                      genVec_init((cap), sizeof(T), NULL, NULL, NULL)
#define VEC_CX(T, cap, copy, move, del)  genVec_init((cap), sizeof(T), (copy), (move), (del))
#define VEC_EMPTY(T)                     VEC(T, 0)
#define VEC_EMPTY_CX(T, copy, move, del) VEC_CX(T, 0, copy, move, del)

#define VEC_STK(T, cap, vec)                     genVec_init_stk((cap), sizeof(T), NULL, NULL, NULL, vec)
#define VEC_CX_STK(T, cap, copy, move, del, vec) genVec_init_stk((cap), sizeof(T), (copy), (move), (del), vec)

/* Create vector with elements from an initilizer list
Usage:
    genVec* v = VEC_FROM_ARR(int, 4, ((int[4]){1,2,3,4}));
*/
#define VEC_FROM_ARR(T, n, arr)                                     \
    ({                                                              \
        genVec* _v = genVec_init((n), sizeof(T), NULL, NULL, NULL); \
        for (u64 _i = 0; _i < n; _i++) {                            \
            genVec_push(_v, (u8*)&arr[_i]);                         \
        }                                                           \
        _v;                                                         \
    })


// ── Push ───────────────────────────────────────────────────────────────────

/* VEC_PUSH — push any POD value. typeof deduces the type from the value.
 *
 *   VEC_PUSH(v, 42);
 *   VEC_PUSH(v, 3.14f);
 *   VEC_PUSH(v, my_struct);
 *
 * Statement expression ({ }) sequences the temp declaration and the push,
 * letting us safely take the address of the temporary.
 * Key: (u8*)&wvp_tmp — address of local. NOT (u8*)wvp_tmp — that's wrong.
 */
#define VEC_PUSH(vec, val)                 \
    ({                                     \
        typeof(val) wvp_tmp = (val);       \
        genVec_push((vec), (u8*)&wvp_tmp); \
    })

/* VEC_PUSH_COPY — deep copy for complex types. Source stays valid.
 *
 *   String s; string_create_stk(&s, "hello");
 *   VEC_PUSH_COPY(v, s);    // str_copy called, s still valid
 *
 *   String* s = string_from_cstr("hello");
 *   VEC_PUSH_COPY(v, s);    // str_copy_ptr called, s still valid
 */
#define VEC_PUSH_COPY(vec, val)            \
    ({                                     \
        typeof(val) wpc_tmp = (val);       \
        genVec_push((vec), (u8*)&wpc_tmp); \
    })

/* VEC_PUSH_MOVE — transfer ownership. Source becomes NULL.
 * Works for both strategies — the registered move_fn handles the difference.
 *
 *   String* s = string_from_cstr("hello");
 *   VEC_PUSH_MOVE(v, s);    // s == NULL after this
 *
 * NOTE: Do NOT pass a stack String — strategy A's move_fn calls free(*src).
 */
#define VEC_PUSH_MOVE(vec, ptr)                  \
    ({                                           \
        typeof(ptr) wvm_p = (ptr);               \
        genVec_push_move((vec), (u8**)&wvm_p);   \
        (ptr) = wvm_p; /* propagate NULL back */ \
    })

/* VEC_PUSH_CSTR — allocate a heap String and move it in. One line.
 * Works for both strategies.
 */
#define VEC_PUSH_CSTR(vec, cstr)                \
    ({                                          \
        String* wpc_s = string_from_cstr(cstr); \
        genVec_push_move((vec), (u8**)&wpc_s);  \
    })


// ── Access ─────────────────────────────────────────────────────────────────

/* typeof can't help here — we need T to know the return type.
 *
 * Strategy A:  VEC_AT(v, String, i)     → String (value)
 *              VEC_AT_MUT(v, String, i) → String* (into slot)
 *
 * Strategy B:  VEC_AT(v, String*, i)     → String* (stored pointer)
 *              VEC_AT_MUT(v, String*, i) → String** (pointer to slot)
 */
#define VEC_AT(vec, T, i)     (*(T*)genVec_get_ptr((vec), (i)))
#define VEC_AT_MUT(vec, T, i) ((T*)genVec_get_ptr_mut((vec), (i)))
#define VEC_FRONT(vec, T)     (*(T*)genVec_front((vec)))
#define VEC_BACK(vec, T)      (*(T*)genVec_back((vec)))


// ── Mutate ─────────────────────────────────────────────────────────────────

/* VEC_SET — replace element at i. del_fn on old, copy_fn on new.
 * typeof deduces type from val — no T param needed.
 *
 *   VEC_SET(v, 0, 99);         // int
 *   VEC_SET(v, 0, my_string);  // String: del + copy called
 */
#define VEC_SET(vec, i, val)                       \
    ({                                             \
        typeof(val) wvs_tmp = (val);               \
        genVec_replace((vec), (i), (u8*)&wvs_tmp); \
    })


// ── Pop ────────────────────────────────────────────────────────────────────

/* VEC_POP — pop and return value as an expression.
 *
 *   int x    = VEC_POP(v, int);
 *   String* p = VEC_POP(v, String*);
 */
// NOTE: can't use for String, as String needs a malloced data ptr
#define VEC_POP(vec, T)                 \
    ({                                  \
        T wvpop;                        \
        genVec_pop((vec), (u8*)&wvpop); \
        wvpop;                          \
    })



// ── Iterate ────────────────────────────────────────────────────────────────

/* VEC_FOREACH — typed mutable pointer to each element.
 *
 * Strategy A:  VEC_FOREACH(v, String, s)  { string_print(s); }
 *   s is String* — direct pointer into the slot
 *
 * Strategy B:  VEC_FOREACH(v, String*, sp) { string_print(*sp); }
 *   sp is String** — *sp is the stored String*
 *
* The inner loop's update is `name = NULL`, so after the body executes,
 * name becomes NULL, the inner condition fails, and we return to the outer
 * loop which increments i correctly. The body sees i and name as expected.
 */
#define VEC_FOREACH(vec, T, name)                        \
    for (u64 _wvf_i = 0; _wvf_i < (vec)->size; _wvf_i++) \
        for (T* name = (T*)genVec_get_ptr_mut((vec), _wvf_i); name; name = NULL)





/* ══════════════════════════════════════════════════════════════════════════
 * 5.  TYPED CONVENIENCE MACROS
 *
 * These fill gaps in wc_macros.h for common patterns:
 *   VEC_OF_INT      — quick int vector creation
 *   VEC_OF_STR      — String-by-value vector
 *   VEC_OF_STR_PTR  — String-by-pointer vector
 *   VEC_OF_VEC      — genVec-by-value vector (vec of vecs)
 *   VEC_OF_VEC_PTR  — genVec-by-pointer vector
 *
 *   VEC_PUSH_VEC      — push a heap genVec* by move into a vec-of-vecs (by val)
 *   VEC_PUSH_VEC_PTR  — push a heap genVec* by move into a vec-of-vecs (by ptr)
 *
 *   MAP_PUT_INT_STR  — one-liner: int key, cstr value (moves String in)
 *   MAP_PUT_STR_STR  — one-liner: cstr key, cstr value (both moved in)
 * ══════════════════════════════════════════════════════════════════════════ */


// ── Vector creation shorthands ───────────────────────────────────────────

#define VEC_OF_INT(cap) \
    genVec_init((cap), sizeof(int), NULL, NULL, NULL)

#define VEC_OF_STR(cap) \
    VEC_CX(String, (cap), str_copy, str_move, str_del)

#define VEC_OF_STR_PTR(cap) \
    VEC_CX(String*, (cap), str_copy_ptr, str_move_ptr, str_del_ptr)

#define VEC_OF_VEC(cap) \
    VEC_CX(genVec, (cap), vec_copy, vec_move, vec_del)

#define VEC_OF_VEC_PTR(cap) \
    VEC_CX(genVec*, (cap), vec_copy_ptr, vec_move_ptr, vec_del_ptr)


// ── Push shorthands ──────────────────────────────────────────────────────

/*
 * VEC_PUSH_VEC(outer, inner_ptr)
 * Push a heap-allocated genVec* by move into a by-value vec-of-vecs.
 * inner_ptr is nulled after the call.
 * The inner vec's struct is freed; its data buffer lives in the outer slot.
 */
#define VEC_PUSH_VEC(outer, inner_ptr) \
    VEC_PUSH_MOVE((outer), (inner_ptr))

/*
 * VEC_PUSH_VEC_PTR(outer, inner_ptr)
 * Push a heap-allocated genVec* by move into a by-pointer vec-of-vecs.
 * inner_ptr is nulled after the call.
 */
#define VEC_PUSH_VEC_PTR(outer, inner_ptr) \
    VEC_PUSH_MOVE((outer), (inner_ptr))


// ── Hashmap shorthands ───────────────────────────────────────────────────

/*
 * MAP_PUT_INT_STR(map, int_key, cstr_literal)
 * Inserts key=int_key, val=string_from_cstr(cstr_literal) with move semantics.
 * Map must have been created with int key, String val, str_copy/str_move/str_del.
 */
#define MAP_PUT_INT_STR(map, k, cstr_val)                           \
    do {                                                            \
        String* _v = string_from_cstr(cstr_val);                   \
        hashmap_put_val_move((map), (u8*)&(int){(k)}, (u8**)&_v);  \
    } while (0)

/*
 * MAP_PUT_STR_STR(map, cstr_key, cstr_val)
 * Inserts key=string_from_cstr(cstr_key), val=string_from_cstr(cstr_val).
 * Both moved in. Map must use str_copy/str_move/str_del for both key and val.
 */
#define MAP_PUT_STR_STR(map, cstr_key, cstr_val)                    \
    do {                                                            \
        String* _k = string_from_cstr(cstr_key);                   \
        String* _v = string_from_cstr(cstr_val);                   \
        hashmap_put_move((map), (u8**)&_k, (u8**)&_v);             \
    } while (0)


// ── Put (COPY semantics) ───────────────────────────────────────────────────

/* MAP_PUT — copy key + value
 *
 *   MAP_PUT(m, key, val);
 */
#define MAP_PUT(map, key, val)                \
    ({                                        \
        typeof(key) _mk = (key);              \
        typeof(val) _mv = (val);              \
        hashmap_put((map),                    \
                    (const u8*)&_mk,          \
                    (const u8*)&_mv);         \
    })


// ── Put (MOVE semantics) ───────────────────────────────────────────────────

/* MAP_PUT_MOVE — move key + value
 *
 *   String k, v;
 *   MAP_PUT_MOVE(m, k, v);   // k == NULL, v == NULL after
 */
#define MAP_PUT_MOVE(map, kptr, vptr)           \
    ({                                          \
        typeof(kptr) _mkp = (kptr);              \
        typeof(vptr) _mvp = (vptr);              \
        hashmap_put_move((map),                 \
                          (u8**)&_mkp,          \
                          (u8**)&_mvp);         \
        (kptr) = _mkp;                           \
        (vptr) = _mvp;                           \
    })


// Mixed ownership
#define MAP_PUT_KEY_MOVE(map, kptr, val)        \
    ({                                          \
        typeof(val) _mv = (val);                 \
        hashmap_put_key_move((map),              \
                              (u8**)&(kptr),    \
                              (const u8*)&_mv); \
    })

#define MAP_PUT_VAL_MOVE(map, key, vptr)        \
    ({                                          \
        typeof(key) _mk = (key);                 \
        hashmap_put_val_move((map),              \
                              (const u8*)&_mk,  \
                              (u8**)&(vptr));   \
    })


// ── Get ───────────────────────────────────────────────────────────────────

/* MAP_GET — copy value into expression
 *
 *   int x = MAP_GET(m, int, key);
 *   String s = MAP_GET(m, String, key); // you own s
 *
 * NOTE: Undefined if key does not exist (same as VEC_POP style)
 */
#define MAP_GET(map, V, key)                 \
    ({                                       \
        V _out;                              \
        typeof(key) _mk = (key);             \
        hashmap_get((map),                   \
                    (const u8*)&_mk,         \
                    (u8*)&_out);             \
        _out;                                \
    })





#endif // WC_MACROS_H
