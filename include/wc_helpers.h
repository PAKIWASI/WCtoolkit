#ifndef HELPERS_H
#define HELPERS_H

/*
 * wc_helpers.h — Generic container callbacks and typed macros for WCtoolkit
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
 *   5.  Shared ops instances (define once, reference everywhere)
 *
 * RULES FOR WRITING copy/move/del CALLBACKS
 * ------------------------------------------
 *
 * BY VALUE  (slot holds the full struct, sizeof(T) bytes)
 *   copy_fn(u8* dest, const u8* src)
 *     dest  — raw bytes of the slot (uninitialised — treat as blank)
 *     src   — raw bytes of the source element
 *     job   — deep-copy all owned resources into dest; DO NOT free dest first
 *
 *   move_fn(u8* dest, u8** src)
 *     dest  — raw bytes of the slot (uninitialised)
 *     *src  — heap pointer to the source T
 *     job   — memcpy the struct fields, free(*src), *src = NULL
 *             The data ptr moves; only the container struct is freed.
 *
 *   del_fn(u8* elm)
 *     elm   — raw bytes of the slot
 *     job   — free owned resources (e.g. data buffer) but NOT elm itself
 *             → call string_destroy_stk / genVec_destroy_stk etc.
 *
 * BY POINTER  (slot holds T*, sizeof(T*) = 8 bytes)
 *   copy_fn(u8* dest, const u8* src)
 *     *(T**)src  is the source pointer
 *     *(T**)dest must be set to a newly heap-allocated deep copy
 *
 *   move_fn(u8* dest, u8** src)
 *     *(T**)dest = *(T**)src;  *src = NULL;
 *
 *   del_fn(u8* elm)
 *     *(T**)elm  is the pointer stored in the slot
 *     job — fully destroy the heap object
 *           → call string_destroy / genVec_destroy etc.
 */

#include "String.h"
#include <string.h>


/* ══════════════════════════════════════════════════════════════════════════
 * 1.  STRING BY VALUE
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void str_copy(u8* dest, const u8* src)
{
    const String* s = (const String*)src;
    String*       d = (String*)dest;

    memcpy(d, s, sizeof(String));       // copy all fields (data ptr too)

    u64 n   = s->size * s->data_size;
    d->data = malloc(n);                // independent data buffer
    memcpy(d->data, s->data, n);
}

static inline void str_move(u8* dest, u8** src)
{
    memcpy(dest, *src, sizeof(String)); // copy all fields into slot
    free(*src);                         // free heap container, not data
    *src = NULL;
}

static inline void str_del(u8* elm)
{
    string_destroy_stk((String*)elm);   // free data buffer, NOT the slot
}

static inline void str_print(const u8* elm)
{
    string_print((const String*)elm);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 2.  STRING BY POINTER
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
    *(String**)dest = *(String**)src;
    *src            = NULL;
}

static inline void str_del_ptr(u8* elm)
{
    string_destroy(*(String**)elm);
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
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy(u8* dest, const u8* src)
{
    const genVec* s = (const genVec*)src;
    genVec*       d = (genVec*)dest;

    memcpy(d, s, sizeof(genVec));                           // copy all fields (including ops ptr)
    d->data = malloc(s->capacity * (u64)s->data_size);      // new data buffer

    copy_fn copy = VEC_COPY_FN(s);                          // safe: handles NULL ops
    if (copy) {
        for (u64 i = 0; i < s->size; i++) {
            copy(d->data + (i * d->data_size), genVec_get_ptr(s, i));
        }
    } else {
        memcpy(d->data, s->data, s->capacity * (u64)s->data_size);
    }
}

static inline void vec_move(u8* dest, u8** src)
{
    memcpy(dest, *src, sizeof(genVec));  // transfer all fields (incl. data ptr and ops ptr)
    free(*src);                          // free container struct only
    *src = NULL;
}

static inline void vec_del(u8* elm)
{
    genVec_destroy_stk((genVec*)elm);    // free data buffer, NOT the slot
}

static inline void vec_print_int(const u8* elm)
{
    const genVec* v = (const genVec*)elm;
    printf("[");
    for (u64 i = 0; i < v->size; i++) {
        printf("%d", *(int*)genVec_get_ptr(v, i));
        if (i + 1 < v->size) { printf(", "); }
    }
    printf("]");
}


/* ══════════════════════════════════════════════════════════════════════════
 * 4.  GENVEC BY POINTER  (slot holds genVec*)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy_ptr(u8* dest, const u8* src)
{
    const genVec* s = *(const genVec**)src;

    genVec* d = malloc(sizeof(genVec));
    memcpy(d, s, sizeof(genVec));                           // copies ops ptr too
    d->data   = malloc(s->capacity * (u64)s->data_size);

    copy_fn copy = VEC_COPY_FN(s);
    if (copy) {
        for (u64 i = 0; i < s->size; i++) {
            copy(d->data + (i * d->data_size), genVec_get_ptr(s, i));
        }
    } else {
        memcpy(d->data, s->data, s->capacity * (u64)s->data_size);
    }

    *(genVec**)dest = d;
}

static inline void vec_move_ptr(u8* dest, u8** src)
{
    *(genVec**)dest = *(genVec**)src;
    *src            = NULL;
}

static inline void vec_del_ptr(u8* elm)
{
    genVec_destroy(*(genVec**)elm);
}

static inline void vec_print_int_ptr(const u8* elm)
{
    vec_print_int((const u8*)*(const genVec**)elm);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 5.  SHARED OPS INSTANCES
 *
 * Define once here as static const. Reference by address wherever needed.
 * No per-instance overhead — all vectors of the same type share the pointer.
 *
 * Usage:
 *   genVec* v = genVec_init(8, sizeof(String), &wc_str_ops);
 *   hashmap* m = hashmap_create(..., &wc_str_ops, &wc_str_ops);
 * ══════════════════════════════════════════════════════════════════════════ */

static const container_ops wc_str_ops     = { str_copy,     str_move,     str_del     };
static const container_ops wc_str_ptr_ops = { str_copy_ptr, str_move_ptr, str_del_ptr };
static const container_ops wc_vec_ops     = { vec_copy,     vec_move,     vec_del     };
static const container_ops wc_vec_ptr_ops = { vec_copy_ptr, vec_move_ptr, vec_del_ptr };


#endif // HELPERS_H
