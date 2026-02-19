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


/* ── Creation ─────────────────────────────────────────────────────────────── */

#define VEC(T, cap)                      genVec_init((cap), sizeof(T), NULL, NULL, NULL)
#define VEC_CX(T, cap, copy, move, del)  genVec_init((cap), sizeof(T), (copy), (move), (del))
#define VEC_EMPTY(T)                     VEC(T, 0)
#define VEC_EMPTY_CX(T, copy, move, del) VEC_CX(T, 0, copy, move, del)


/* ── Push ─────────────────────────────────────────────────────────────────── */

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
#define VEC_PUSH(vec, val)                  \
    ({                                      \
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
#define VEC_PUSH_COPY(vec, val)             \
    ({                                      \
        typeof(val) wpc_tmp = (val);       \
        genVec_push((vec), (u8*)&wpc_tmp); \
    })

/* VEC_PUSH_MOVE — transfer ownership. Source becomes NULL.
 * Works for both strategies — the registered move_fn handles the difference.
 *
 *   String* s = string_from_cstr("hello");
 *   VEC_PUSH_MOVE(v, s);    // s == NULL after this
 *
 * Do NOT pass a stack String — strategy A's move_fn calls free(*src).
 */
#define VEC_PUSH_MOVE(vec, ptr)                   \
    ({                                            \
        typeof(ptr) wvm_p = (ptr);               \
        genVec_push_move((vec), (u8**)&wvm_p);   \
        (ptr) = wvm_p; /* propagate NULL back */ \
    })

/* VEC_PUSH_CSTR — allocate a heap String and move it in. One line.
 * Works for both strategies.
 */
#define VEC_PUSH_CSTR(vec, cstr)                 \
    ({                                           \
        String* wpc_s = string_from_cstr(cstr); \
        genVec_push_move((vec), (u8**)&wpc_s);  \
    })


/* ── Access ───────────────────────────────────────────────────────────────── */

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


/* ── Mutate ───────────────────────────────────────────────────────────────── */

/* VEC_SET — replace element at i. del_fn on old, copy_fn on new.
 * typeof deduces type from val — no T param needed.
 *
 *   VEC_SET(v, 0, 99);         // int
 *   VEC_SET(v, 0, my_string);  // String: del + copy called
 */
#define VEC_SET(vec, i, val)                        \
    ({                                              \
        typeof(val) wvs_tmp = (val);               \
        genVec_replace((vec), (i), (u8*)&wvs_tmp); \
    })


/* ── Pop ──────────────────────────────────────────────────────────────────── */

/* VEC_POP — pop and return value as an expression.
 *
 *   int x    = VEC_POP(v, int);
 *   String s = VEC_POP(v, String);  // you own s, must destroy
 *   String* p = VEC_POP(v, String*);
 */
#define VEC_POP(vec, T)                  \
    ({                                   \
        T wvpop;                        \
        genVec_pop((vec), (u8*)&wvpop); \
        wvpop;                          \
    })


/* ── Iterate ──────────────────────────────────────────────────────────────── */

/* VEC_FOREACH — typed mutable pointer to each element.
 *
 * Strategy A:  VEC_FOREACH(v, String, s)  { string_print(s); }
 *   s is String* — direct pointer into the slot
 *
 * Strategy B:  VEC_FOREACH(v, String*, sp) { string_print(*sp); }
 *   sp is String** — *sp is the stored String*
 *
 * Double-for trick: outer increments i, inner declares name and runs once,
 * then sets i = size so the outer condition fails and the loop ends cleanly.
 */
#define VEC_FOREACH(vec, T, name)                                                   \
    for (u64 wvf_i = 0; wvf_i < (vec)->size; wvf_i++)                           \
        for (T* name = (T*)genVec_get_ptr_mut((vec), wvf_i); wvf_i < (vec)->size; \
             wvf_i  = (vec)->size)


#endif /* WC_MACROS_H */
