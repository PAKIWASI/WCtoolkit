#ifndef HELPERS_H
#define HELPERS_H

/*
 * wc_helpers.h: Generic container callbacks and typed macros for WCtoolkit
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
 * RULES FOR WRITING copy/del CALLBACKS
 * -------------------------------------
 *
 * BY VALUE  (slot holds the full struct, sizeof(T) bytes)
 *   copy_fn(const wc_allocator* dst, void* dest, const void* src)
 *     dest  — raw bytes of the slot (uninitialised — treat as blank)
 *     job   — deep-copy src into dest, allocating owned resources from `dst`
 *             (the container's allocator). DO NOT free dest first.
 *
 *   del_fn(void* elm)
 *     job   — free owned resources (with the element's OWN stored allocator)
 *             but NOT elm itself → GenVec_destroy, String_destroy, ...
 *
 * BY POINTER  (slot holds T*, sizeof(T*) = 8 bytes)
 *   copy_fn: *(T**)dest = a new shell from `dst` holding a deep copy (WC_BOX_IN)
 *   del_fn : destroy the pointee, then free the shell with the pointee's own
 *            allocator (shell allocator == child allocator, rule B12)
 */

#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_string.h"
#include <stdalign.h>
#include <stdio.h>



/* 
 * 1.  STRING BY VALUE
 *
 * String stores its own allocator.
 * str_copy  — copies using dst allocator (so a String follows its outer container)
 * str_del   — delegates to String_destroy() (frees heap buf via str->alloc, NOT the slot)
*/

static inline void str_copy(const wc_allocator* dst, void* dest, const void* src)
{
    *(String*)dest = String_copy(dst, (const String*)src);
}

static inline void str_del(void* elm)
{
    String_destroy((String*)elm); // free data buffer via str->alloc, NOT the slot
}

static inline void str_print(const void* elm)
{
    String_print((const String*)elm);
}


/*
 * 2.  STRING BY POINTER
 *
 * str_copy_ptr — box a deep copy in allocator `dst` (shell + contents from dst)
 * str_del_ptr  — destroy pointee via its own allocator, then free shell
*/

static inline void str_copy_ptr(const wc_allocator* dst, void* dest, const void* src)
{
    // Box a deep copy: shell and contents both from dst
    *(String**)dest = (String*)WC_BOX_IN(dst, String, String_copy, *(const String* const*)src);
}

static inline void str_del_ptr(void* elm)
{
    String* s = *(String**)elm;
    if (!s) {
        return;
    }
    // Read allocator before destroy zeroes the struct
    const wc_allocator* a = s->alloc;
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


/*
 * 3.  GENVEC BY VALUE  (vec of vecs)
*/ 

static inline void vec_copy(const wc_allocator* dst, void* dest, const void* src)
{
    *(GenVec*)dest = GenVec_copy(dst, (const GenVec*)src); // inner vec follows the outer container
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


/*
 * 4.  GENVEC BY POINTER  (slot holds GenVec*)
*/

static inline void vec_copy_ptr(const wc_allocator* dst, void* dest, const void* src)
{
    // shell and contents both from dst: the shell allocator equals the child's (D6)
    *(GenVec**)dest = WC_BOX_IN(dst, GenVec, GenVec_copy, *(const GenVec* const*)src);
}

static inline void vec_del_ptr(void* elm)
{
    GenVec* v = *(GenVec**)elm;
    if (!v) {
        return;
    }
    const wc_allocator* a = v->alloc; // read before destroy zeroes it
    GenVec_destroy(v);
    wc_free(a, v, sizeof(GenVec), alignof(GenVec));
}

static inline void vec_print_int_ptr(const void* elm)
{
    vec_print_int(*(const GenVec**)elm);
}


/*
 * 5.  SHARED OPS INSTANCES
 *
 * Define once here as static const. Reference by address wherever needed.
 * No per-instance overhead — all vectors of the same type share the pointer.
 *
 * Usage:
 *   GenVec v = GenVec_create(WC_LIBC, 8, sizeof(String), &wc_str_ops);
 *   HashMap m = HashMap_create(WC_LIBC, ..., &wc_str_ops, &wc_str_ops);
*/

static const wc_container_ops wc_str_ops     = {.copy_fn = str_copy, .del_fn = str_del};
static const wc_container_ops wc_str_ptr_ops = {.copy_fn = str_copy_ptr, .del_fn = str_del_ptr};
static const wc_container_ops wc_vec_ops     = {.copy_fn = vec_copy, .del_fn = vec_del};
static const wc_container_ops wc_vec_ptr_ops = {.copy_fn = vec_copy_ptr, .del_fn = vec_del_ptr};


#endif // HELPERS_H
