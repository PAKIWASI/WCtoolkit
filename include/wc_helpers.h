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
 *   3.  GenVec by value  (sizeof(GenVec) per slot)  — vec of vecs
 *   4.  GenVec by pointer (sizeof(GenVec*) per slot)
 *   5.  Shared ops instances (define once, reference everywhere)
 *
 * RULES FOR WRITING copy/move/del CALLBACKS
 * ------------------------------------------
 *
 * BY VALUE  (slot holds the full struct, sizeof(T) bytes)
 *   copy_fn(wc_allocator dst, u8* dest, const u8* src)
 *     dest  — raw bytes of the slot (uninitialised — treat as blank)
 *     job   — deep-copy src into dest, allocating owned resources from `dst`
 *             (the container's allocator). DO NOT free dest first.
 *
 *   move_fn(u8* dest, u8* src)
 *     job   — dest takes over everything src owned; leave src ZEROED.
 *             For plain structs this is memcpy + memset(0), which is also
 *             what containers do when move_fn is NULL.
 *
 *   del_fn(u8* elm)
 *     job   — free owned resources (with the element's OWN stored allocator)
 *             but NOT elm itself → GenVec_destroy, String_destroy_stk, ...
 *
 * BY POINTER  (slot holds T*, sizeof(T*) = 8 bytes)
 *   copy_fn: *(T**)dest = a new shell from `dst` holding a deep copy (WC_BOX_IN)
 *   move_fn: *(T**)dest = *(T**)src;  *(T**)src = NULL;
 *   del_fn : destroy the pointee, then free the shell with the pointee's own
 *            allocator (shell allocator == child allocator, plan D6)
 *
 * TRANSITIONAL (until Phase 4): String does not store an allocator yet, so the
 * String callbacks ignore `dst` and use libc, exactly as String itself does.
 */

#include "wc_string.h"
#include "common.h"
#include "gen_vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* ══════════════════════════════════════════════════════════════════════════
 * 1.  STRING BY VALUE
 *
 * String is SSO-based (no data_size / data fields).
 * str_copy  — delegates to String_copy()  (handles SSO vs heap correctly)
 * str_move  — memcpy the struct, zero the source
 * str_del   — delegates to String_destroy_stk() (frees heap buf if any)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void str_copy(wc_allocator dst, u8* dest, const u8* src)
{
    (void)dst; // TRANSITIONAL: String allocates from libc until Phase 4
    String*       d = (String*)dest;
    const String* s = (const String*)src;
    memcpy(d, s, sizeof(String));

    if (String_is_sso(s)) {
        return; // str stored inline, we have everything
    }

    // src owns resources, copy them
    d->heap = malloc(s->capacity);
    CHECK_FATAL(!d->heap, "malloc failed");
    memcpy(d->heap, s->heap, s->capacity);
}

static inline void str_move(u8* dest, u8* src)
{
    // Works for both SSO (copies stk[]) and heap mode (the heap pointer moves).
    // A zeroed String reads as heap mode with heap == NULL: destroy-safe.
    memcpy(dest, src, sizeof(String));
    memset(src, 0, sizeof(String));
}

static inline void str_del(u8* elm)
{
    String_destroy_stk((String*)elm);   // free data buffer, NOT the slot
}

static inline void str_print(const u8* elm)
{
    String_print((const String*)elm);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 2.  STRING BY POINTER
 *
 * str_copy_ptr — delegates to String_from_String() for a full heap copy
 * str_move_ptr — pointer swap, nulls source
 * str_del_ptr  — delegates to String_destroy() (frees buf + struct)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void str_copy_ptr(wc_allocator dst, u8* dest, const u8* src)
{
    (void)dst; // TRANSITIONAL: heap String shells come from libc until Phase 4
    *(String**)dest = String_from_String(*(const String**)src);
}

static inline void str_move_ptr(u8* dest, u8* src)
{
    *(String**)dest = *(String**)src;
    *(String**)src  = NULL;
}

static inline void str_del_ptr(u8* elm)
{
    String_destroy(*(String**)elm);
}

static inline void str_print_ptr(const u8* elm)
{
    String_print(*(const String**)elm);
}

static inline int str_cmp(const u8* a, const u8* b, u64 size)
{
    (void)size;
    return String_compare((const String*)a, (const String*)b);
}

static inline int str_cmp_ptr(const u8* a, const u8* b, u64 size)
{
    (void)size;
    return String_compare(*(const String**)a, *(const String**)b);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 3.  GENVEC BY VALUE  (vec of vecs)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy(wc_allocator dst, u8* dest, const u8* src)
{
    *(GenVec*)dest = GenVec_copy(dst, (const GenVec*)src); // inner vec follows the outer container
}

static inline void vec_move(u8* dest, u8* src)
{
    GenVec_move((GenVec*)dest, (GenVec*)src); // memcpy + zero src
}

static inline void vec_del(u8* elm)
{
    GenVec_destroy((GenVec*)elm); // frees data with the inner vec's own allocator, NOT the slot
}

static inline void vec_print_int(const u8* elm)
{
    const GenVec* v = (const GenVec*)elm;
    printf("[");
    for (u64 i = 0; i < v->size; i++) {
        printf("%d", *(int*)GenVec_get_ptr(v, i));
        if (i + 1 < v->size) { printf(", "); }
    }
    printf("]");
}


/* ══════════════════════════════════════════════════════════════════════════
 * 4.  GENVEC BY POINTER  (slot holds GenVec*)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy_ptr(wc_allocator dst, u8* dest, const u8* src)
{
    // shell and contents both from dst: the shell allocator equals the child's (D6)
    *(GenVec**)dest = WC_BOX_IN(dst, GenVec, GenVec_copy, *(const GenVec* const*)src);
}

static inline void vec_move_ptr(u8* dest, u8* src)
{
    *(GenVec**)dest = *(GenVec**)src;
    *(GenVec**)src  = NULL;
}

static inline void vec_del_ptr(u8* elm)
{
    GenVec* v = *(GenVec**)elm;
    if (!v) {
        return;
    }
    wc_allocator a = v->alloc; // read before destroy zeroes it
    GenVec_destroy(v);
    wc_free(a, v, sizeof(GenVec), alignof(GenVec));
}

static inline void vec_print_int_ptr(const u8* elm)
{
    vec_print_int((const u8*)*(const GenVec**)elm);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 5.  SHARED OPS INSTANCES
 *
 * Define once here as static const. Reference by address wherever needed.
 * No per-instance overhead — all vectors of the same type share the pointer.
 *
 * Usage:
 *   GenVec v = GenVec_create(WC_LIBC, 8, sizeof(String), &wc_str_ops);
 *   HashMap* m = HashMap_create(..., &wc_str_ops, &wc_str_ops);
 * ══════════════════════════════════════════════════════════════════════════ */

static const wc_container_ops wc_str_ops     = { str_copy,     str_move,     str_del     };
static const wc_container_ops wc_str_ptr_ops = { str_copy_ptr, str_move_ptr, str_del_ptr };
static const wc_container_ops wc_vec_ops     = { vec_copy,     vec_move,     vec_del     };
static const wc_container_ops wc_vec_ptr_ops = { vec_copy_ptr, vec_move_ptr, vec_del_ptr };


#endif // HELPERS_H
