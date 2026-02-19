#ifndef HELPERS_H
#define HELPERS_H

/*
 * helpers.h — Generic container callbacks and typed macros for WCtoolkit
 * =======================================================================
 *
 * ALL functions are static inline to avoid multiple-definition linker
 * errors when this header is included in more than one translation unit.
 *
 * SECTIONS
 * --------
 *   1.  String by value  (sizeof(String) per slot)
 *   2.  String by pointer (sizeof(String*) per slot)
 *   3.  genVec by value  (sizeof(genVec) per slot)  — vec of vecs
 *   4.  genVec by pointer (sizeof(genVec*) per slot)
 *   5.  Typed convenience macros
 *
 * RULES FOR WRITING copy/move/del CALLBACKS
 * ------------------------------------------
 * These rules apply universally, whatever T you are storing.
 *
 * BY VALUE  (slot holds the full struct, sizeof(T) bytes)
 *   copy_fn(u8* dest, const u8* src)
 *     dest  — raw bytes of the slot (uninitialised / garbage — treat as blank)
 *     src   — raw bytes of the source element (T*)src is valid
 *     job   — deep-copy all owned resources into dest; DO NOT free dest first
 *
 *   move_fn(u8* dest, u8** src)
 *     dest  — raw bytes of the slot (uninitialised)
 *     *src  — heap pointer to the source T
 *     job   — memcpy the struct fields, free(*src), *src = NULL
 *             The data ptr moves; only the container struct is freed.
 *
 *   del_fn(u8* elm)
 *     elm   — raw bytes of the slot (T*)elm is valid
 *     job   — free owned resources (e.g. data buffer) but NOT elm itself
 *             (the slot memory is owned by the vector, not you)
 *             → call string_destroy_stk / genVec_destroy_stk etc.
 *
 * BY POINTER  (slot holds T*, sizeof(T*) = 8 bytes)
 *   copy_fn(u8* dest, const u8* src)
 *     *(T**)src  is the source pointer
 *     *(T**)dest must be set to a newly heap-allocated deep copy
 *
 *   move_fn(u8* dest, u8** src)
 *     *(T**)dest = *(T**)src;  *src = NULL;
 *     (transfer the pointer, null the source — no struct free needed)
 *
 *   del_fn(u8* elm)
 *     *(T**)elm  is the pointer stored in the slot
 *     job — fully destroy the heap object (free data + free struct)
 *           → call string_destroy / genVec_destroy etc.
 */

#include "String.h"
#include "gen_vector.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/* ══════════════════════════════════════════════════════════════════════════
 * 1.  STRING BY VALUE
 *     Slot stores the full String struct (sizeof(String) bytes).
 *     copy_fn deep-copies the data buffer.
 *     move_fn transfers the heap struct, nulls source.
 *     del_fn  frees only the data buffer (not the slot).
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void str_copy(u8* dest, const u8* src)
{
    const String* s = (const String*)src;
    String*       d = (String*)dest;

    memcpy(d, s, sizeof(String));       /* copy all fields (data ptr too)  */

    u64 n   = s->size * s->data_size;
    d->data = malloc(n);                /* independent data buffer          */
    memcpy(d->data, s->data, n);
}

static inline void str_move(u8* dest, u8** src)
{
    memcpy(dest, *src, sizeof(String)); /* copy all fields into slot        */
    free(*src);                         /* free heap container, not data    */
    *src = NULL;
}

static inline void str_del(u8* elm)
{
    string_destroy_stk((String*)elm);   /* free data buffer, NOT the slot   */
}

static inline void str_print(const u8* elm)
{
    string_print((const String*)elm);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 2.  STRING BY POINTER
 *     Slot stores String* (sizeof(String*) = 8 bytes).
 *     Each slot is a pointer to a heap-allocated String.
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void str_copy_ptr(u8* dest, const u8* src)
{
    const String* s = *(const String**)src;

    String* d = malloc(sizeof(String));
    memcpy(d, s, sizeof(String));

    u64 n   = s->size * s->data_size;
    d->data = malloc(n);
    memcpy(d->data, s->data, n);

    *(String**)dest = d;
}

static inline void str_move_ptr(u8* dest, u8** src)
{
    *(String**)dest = *(String**)src;   /* transfer pointer into slot       */
    *src            = NULL;
}

static inline void str_del_ptr(u8* elm)
{
    string_destroy(*(String**)elm);     /* free data buffer + struct        */
}

static inline void str_print_ptr(const u8* elm)
{
    string_print(*(const String**)elm);
}

static inline int str_cmp(const u8* a, const u8* b, u64 size)
{
    (void)size;
    return string_compare((const String*)a, (const String*)b);
}

static inline int str_cmp_ptr(const u8* a, const u8* b, u64 size)
{
    (void)size;
    return string_compare(*(const String**)a, *(const String**)b);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 3.  GENVEC BY VALUE  (vec of vecs)
 *     Slot stores the full genVec struct (sizeof(genVec) bytes).
 *
 *     WHY vec_move works the way it does:
 *       *src is a heap-allocated genVec*.
 *       We memcpy all fields (including the data ptr) into the slot.
 *       Then we free the heap container (*src) — NOT the data buffer.
 *       The data buffer now lives inside the slot's genVec.
 *       We null *src so the caller can't use the dangling pointer.
 *
 *     WHY vec_copy cannot call genVec_copy:
 *       genVec_copy first calls genVec_destroy_stk on dest, which would
 *       try to free garbage memory (dest is uninitialised). We do it
 *       manually: memcpy fields, then malloc + copy the data buffer.
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy(u8* dest, const u8* src)
{
    const genVec* s = (const genVec*)src;
    genVec*       d = (genVec*)dest;

    memcpy(d, s, sizeof(genVec));                           /* copy all fields  */
    d->data = malloc(s->capacity * (u64)s->data_size);      /* new data buffer  */

    if (s->copy_fn) {
        for (u64 i = 0; i < s->size; i++) {
            s->copy_fn(d->data + i * d->data_size,
                       genVec_get_ptr(s, i));
        }
    } else {
        memcpy(d->data, s->data, s->capacity * (u64)s->data_size);
    }
}

static inline void vec_move(u8* dest, u8** src)
{
    genVec* s = *(genVec**)src;
    genVec* d = (genVec*)dest;

    memcpy(d, s, sizeof(genVec));   /* transfer all fields (incl. data ptr) */
    free(*src);                     /* free container struct only           */
    *src = NULL;
}

static inline void vec_del(u8* elm)
{
    genVec_destroy_stk((genVec*)elm); /* free data buffer, NOT the slot     */
}

static inline void vec_print_int(const u8* elm)
{
    const genVec* v = (const genVec*)elm;
    printf("[");
    for (u64 i = 0; i < v->size; i++) {
        printf("%d", *(int*)genVec_get_ptr(v, i));
        if (i + 1 < v->size) printf(", ");
    }
    printf("]");
}


/* ══════════════════════════════════════════════════════════════════════════
 * 4.  GENVEC BY POINTER  (slot holds genVec*)
 *     Slot stores genVec* (sizeof(genVec*) = 8 bytes).
 *     The pointed-to genVec is fully heap-allocated.
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy_ptr(u8* dest, const u8* src)
{
    const genVec* s = *(const genVec**)src;

    genVec* d = malloc(sizeof(genVec));
    memcpy(d, s, sizeof(genVec));
    d->data   = malloc(s->capacity * (u64)s->data_size);

    if (s->copy_fn) {
        for (u64 i = 0; i < s->size; i++) {
            s->copy_fn(d->data + i * d->data_size,
                       genVec_get_ptr(s, i));
        }
    } else {
        memcpy(d->data, s->data, s->capacity * (u64)s->data_size);
    }

    *(genVec**)dest = d;
}

static inline void vec_move_ptr(u8* dest, u8** src)
{
    *(genVec**)dest = *(genVec**)src;  /* transfer pointer into slot        */
    *src            = NULL;
}

static inline void vec_del_ptr(u8* elm)
{
    genVec_destroy(*(genVec**)elm);    /* free data buffer + struct         */
}

static inline void vec_print_int_ptr(const u8* elm)
{
    vec_print_int((const u8*)*(const genVec**)elm);
}


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

#include "wc_macros.h"
#include "hashmap.h"

/* ── Vector creation shorthands ─────────────────────────────────────────── */

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


/* ── Push shorthands ────────────────────────────────────────────────────── */

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


/* ── Hashmap shorthands ─────────────────────────────────────────────────── */

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


#endif /* HELPERS_H */
