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
 *   copy_fn(wc_allocator dst, void* dest, const void* src)
 *     dest  — raw bytes of the slot (uninitialised — treat as blank)
 *     job   — deep-copy src into dest, allocating owned resources from `dst`
 *             (the container's allocator). DO NOT free dest first.
 *
 *   move_fn(void* dest, void* src)
 *     job   — dest takes over everything src owned; leave src ZEROED.
 *             For plain structs this is memcpy + memset(0), which is also
 *             what containers do when move_fn is NULL.
 *
 *   del_fn(void* elm)
 *     job   — free owned resources (with the element's OWN stored allocator)
 *             but NOT elm itself → GenVec_destroy, String_destroy, ...
 *
 * BY POINTER  (slot holds T*, sizeof(T*) = 8 bytes)
 *   copy_fn: *(T**)dest = a new shell from `dst` holding a deep copy (WC_BOX_IN)
 *   move_fn: *(T**)dest = *(T**)src;  *(T**)src = NULL;
 *   del_fn : destroy the pointee, then free the shell with the pointee's own
 *            allocator (shell allocator == child allocator, plan D6)
 */

#include "common.h"
#include "gen_vector.h"
#include "wc_string.h"
#include <stdio.h>
#include <string.h>



/* ══════════════════════════════════════════════════════════════════════════
 * 1.  STRING BY VALUE
 *
 * String stores its own allocator since Phase 4.
 * str_copy  — copies using dst allocator (so a String follows its outer container)
 * str_move  — memcpy the struct, zero the source
 * str_del   — delegates to String_destroy() (frees heap buf via str->alloc, NOT the slot)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void str_copy(wc_allocator dst, void* dest, const void* src)
{
    *(String*)dest = String_copy(dst, (const String*)src);
}

static inline void str_move(void* dest, void* src)
{
    // Works for both SSO (copies stk[]) and heap mode (the heap pointer moves).
    // A zeroed String reads as heap mode with heap == NULL: destroy-safe.
    memcpy(dest, src, sizeof(String));
    memset(src, 0, sizeof(String));
}

static inline void str_del(void* elm)
{
    String_destroy((String*)elm); // free data buffer via str->alloc, NOT the slot
}

static inline void str_print(const void* elm)
{
    String_print((const String*)elm);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 2.  STRING BY POINTER
 *
 * str_copy_ptr — box a deep copy in allocator `dst` (shell + contents from dst)
 * str_move_ptr — pointer swap, nulls source
 * str_del_ptr  — destroy pointee via its own allocator, then free shell
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void str_copy_ptr(wc_allocator dst, void* dest, const void* src)
{
    // Box a deep copy: shell and contents both from dst (D6)
    *(String**)dest = (String*)WC_BOX_IN(dst, String, String_copy, *(const String* const*)src);
}

static inline void str_move_ptr(void* dest, void* src)
{
    *(String**)dest = *(String**)src;
    *(String**)src  = NULL;
}

static inline void str_del_ptr(void* elm)
{
    String* s = *(String**)elm;
    if (!s) {
        return;
    }
    // Read allocator before destroy zeroes the struct
    wc_allocator a = s->alloc;
    String_destroy(s);                              // free heap buffer via s->alloc
    wc_free(a, s, sizeof(String), alignof(String)); // free the shell
}

static inline void str_print_ptr(const void* elm)
{
    String_print(*(const String**)elm);
}

static inline int str_cmp(const void* a, const void* b, u64 size)
{
    (void)size;
    return String_compare((const String*)a, (const String*)b);
}

static inline int str_cmp_ptr(const void* a, const void* b, u64 size)
{
    (void)size;
    return String_compare(*(const String**)a, *(const String**)b);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 3.  GENVEC BY VALUE  (vec of vecs)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy(wc_allocator dst, void* dest, const void* src)
{
    *(GenVec*)dest = GenVec_copy(dst, (const GenVec*)src); // inner vec follows the outer container
}

static inline void vec_move(void* dest, void* src)
{
    GenVec_move((GenVec*)dest, (GenVec*)src); // memcpy + zero src
}

static inline void vec_del(void* elm)
{
    GenVec_destroy((GenVec*)elm); // frees data with the inner vec's own allocator, NOT the slot
}

static inline void vec_print_int(const void* elm)
{
    const GenVec* v = (const GenVec*)elm;
    printf("[");
    for (u64 i = 0; i < v->size; i++) {
        printf("%d", *(int*)GenVec_get_ptr(v, i));
        if (i + 1 < v->size) {
            printf(", ");
        }
    }
    printf("]");
}


/* ══════════════════════════════════════════════════════════════════════════
 * 4.  GENVEC BY POINTER  (slot holds GenVec*)
 * ══════════════════════════════════════════════════════════════════════════ */

static inline void vec_copy_ptr(wc_allocator dst, void* dest, const void* src)
{
    // shell and contents both from dst: the shell allocator equals the child's (D6)
    *(GenVec**)dest = WC_BOX_IN(dst, GenVec, GenVec_copy, *(const GenVec* const*)src);
}

static inline void vec_move_ptr(void* dest, void* src)
{
    *(GenVec**)dest = *(GenVec**)src;
    *(GenVec**)src  = NULL;
}

static inline void vec_del_ptr(void* elm)
{
    GenVec* v = *(GenVec**)elm;
    if (!v) {
        return;
    }
    wc_allocator a = v->alloc; // read before destroy zeroes it
    GenVec_destroy(v);
    wc_free(a, v, sizeof(GenVec), alignof(GenVec));
}

static inline void vec_print_int_ptr(const void* elm)
{
    vec_print_int(*(const GenVec**)elm);
}


/* ══════════════════════════════════════════════════════════════════════════
 * 5.  SHARED OPS INSTANCES
 *
 * Define once here as static const. Reference by address wherever needed.
 * No per-instance overhead — all vectors of the same type share the pointer.
 *
 * Usage:
 *   GenVec v = GenVec_create(WC_LIBC, 8, sizeof(String), &wc_str_ops);
 *   HashMap m = HashMap_create(WC_LIBC, ..., &wc_str_ops, &wc_str_ops);
 * ══════════════════════════════════════════════════════════════════════════ */

static const wc_container_ops wc_str_ops     = {str_copy, str_move, str_del};
static const wc_container_ops wc_str_ptr_ops = {str_copy_ptr, str_move_ptr, str_del_ptr};
static const wc_container_ops wc_vec_ops     = {vec_copy, vec_move, vec_del};
static const wc_container_ops wc_vec_ptr_ops = {vec_copy_ptr, vec_move_ptr, vec_del_ptr};

// NOTE: sizeof(String) == 64, sizeof(GenVec) == 56 (D9 detail):
// the old _Static_assert(sizeof(String) == sizeof(GenVec)) is intentionally removed.

#endif // HELPERS_H
