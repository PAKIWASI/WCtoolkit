#ifndef WC_MACROS_H
#define WC_MACROS_H

// _Generic association expressions must name declared
// symbols in every TU that sees this header, even when the macro is never used.
#include "bit_vector.h"
#include "common.h"
#include "gen_vector.h"
#include "hashmap.h"
#include "hashset.h"
#include "map_setup.h"
#include "matrix.h"
#include "priority_queue.h"
#include "queue.h"
#include "stack.h"
#include "views.h"
#include "wc_allocator.h"
#include "wc_helpers.h"
#include "wc_string.h"

#include <string.h>


/* C11 + GNU extensions used here:
 *   typeof  (__typeof__)  — GNU ext, available with Clang/GCC + -std=c11
 *   ({ })   statement expressions — GNU ext, Clang/GCC only
 *
 * MEMORY RULES FOR THIS LAYER (see docs/memory-rules.md, group E)
 *   E1  A macro never allocates or frees anything you don't see.
 *   E2  Macros that take an element BY VALUE (VEC_PUSH, MAP_PUT, ...) accept
 *       plain data only, checked at compile time. An owning value passed by
 *       value would be deep-copied out of the macro's temporary, and the
 *       temporary would leak. Owning elements go in through:
 *         *_MOVE(lval)   the container takes it, lval is zeroed
 *         *_COPY(lval)   the container deep-copies it, you keep lval
 *       Both take an LVALUE: there is no hidden temporary.
 *   E3  Macros that return an element BY VALUE (VEC_AT, MAP_GET, ...) reject
 *       owning structs: a by-value String would share the container's buffer.
 *       Read those through pointers: VEC_REF, MAP_GET_PTR, ... Pointer
 *       elements (String*) are allowed: the returned pointer is a visible borrow.
 *       VEC_POP / QUEUE_POP / STACK_POP work for every type: popping MOVES the
 *       element out, so the returned value is yours (B7).
 *   E4  Typed macros check sizeof(T) against the slot in Debug.
 */

#define typeof __typeof__


/* WC_IS_OWNING_VALUE(T) — 1 for the toolkit's own types that own memory,
 * 0 otherwise. WC_IS_OWNING(T) — the same, plus pointers to them (by-pointer
 * elements: the slot owns the pointee). Both are integer constant expressions,
 * so they can drive _Static_assert. Your own owning types fall through to 0:
 * for those the runtime is_pod / size checks are the backstop.
 *
 * _Generic picks the branch whose type matches the controlling expression's
 * type, at compile time, without evaluating it. `(T*)0` is used so that any T
 * works, including arrays and incomplete types, without needing a value.
 */
#define WC_OWNING_VALUE_CASES_                                                                                       \
    String* : 1, const String* : 1, GenVec* : 1, const GenVec* : 1, HashMap* : 1, const HashMap* : 1, HashSet* : 1,  \
        const HashSet* : 1, Queue* : 1, const Queue* : 1, PriorityQueue* : 1, const PriorityQueue* : 1, BitVec* : 1, \
        const BitVec* : 1, Matrixf* : 1, const Matrixf* : 1, StringStore* : 1, const StringStore* : 1

#define WC_IS_OWNING_VALUE(T) _Generic((T*)0, WC_OWNING_VALUE_CASES_, default: 0)

#define WC_IS_OWNING(T)         \
    _Generic((T*)0,             \
        WC_OWNING_VALUE_CASES_, \
        String * *: 1,          \
        const String**: 1,      \
        GenVec**: 1,            \
        const GenVec**: 1,      \
        default: 0)

/* Compile-time guards.
 * WC_REQUIRE_POD (E2): a value macro takes the element BY VALUE into a hidden
 *   temporary. Owning structs AND owning pointers are rejected: an rvalue
 *   (String_from_cstr(..), WC_BOX_IN(..)) would be deep-copied and then leak.
 * WC_REQUIRE_NO_ALIAS (E3): a read macro returns the element BY VALUE. An
 *   owning struct is rejected: the copy would share the container's buffer.
 *   Pointer elements are allowed: a returned pointer is a visible borrow.
 */
#define WC_REQUIRE_POD(T, what) \
    _Static_assert(!WC_IS_OWNING(T), what ": owning element type, use the _MOVE or _COPY form")
#define WC_REQUIRE_NO_ALIAS(T, what)       \
    _Static_assert(!WC_IS_OWNING_VALUE(T), \
                   what ": owning struct by value would alias the container, use the pointer form")


/* WC_ASSERT_ELEM_SIZE — developer guard for typed macro layers.
 * Fires when a type-asserting macro (VEC_AT, VEC_POP, ...) is used on a vec
 * whose element size doesn't match sizeof(T) — i.e. the wrong T was passed.
 * The container never knows T, so this lives in the macro layer.
 */
#define WC_ASSERT_ELEM_SIZE(vec, T)                                                               \
    WC_ASSERT((vec)->data_size == sizeof(T),                                                      \
              "element size mismatch: " #vec " (data_size=%u), macro given " #T " (sizeof=%llu)", \
              (unsigned)(vec)->data_size, (unsigned long long)sizeof(T))


/* WC_ASSERT_SIZE — the value a macro was given must be exactly as big as the
 * slot it is copied into. Macros copy values through a temporary of the
 * VALUE's type, so a mismatch (an int literal into a double vector) would
 * read past the temporary. Debug-only, like every other WC_ASSERT.
 */
#define WC_ASSERT_SIZE(have, want, what)                                                    \
    WC_ASSERT((u64)(have) == (u64)(want), what ": value is %llu bytes, slot is %llu bytes", \
              (unsigned long long)(have), (unsigned long long)(want))


/* WC_OPS — pick the right container_ops for T at compile time.
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
// _IN form takes the allocator (a const wc_allocator*) first.

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

#define MAP_OF_IN(A, K, V) HashMap_create((A), sizeof(K), sizeof(V), WC_HASH_FN(K), WC_CMP_FN(K), WC_OPS(K), WC_OPS(V))
#define MAP_OF(K, V)       MAP_OF_IN(WC_LIBC, K, V)

#define VEC_MAKE_OPS(copy, del) \
    (wc_container_ops)          \
    {                           \
        (copy), (del)           \
    }

/* Create vector from an array of n elements. The elements are COPIED (the
 * array keeps its own): plain data is one memcpy, owning T goes through
 * WC_OPS(T)'s copy_fn.
Usage:
    GenVec v = VEC_FROM_ARR(int, 4, ((int[4]){1,2,3,4}));
*/
#define VEC_FROM_ARR_IN(A, T, n, arr)                                                     \
    ({                                                                                    \
        _Static_assert(sizeof((arr)[0]) == sizeof(T), "VEC_FROM_ARR: array element type " \
                                                      "must match T");                    \
        u64    _vfa_n = (u64)(n);                                                         \
        GenVec _vfa   = GenVec_create((A), _vfa_n, sizeof(T), WC_OPS(T));                 \
        if (_vfa_n > 0) {                                                                 \
            GenVec_insert_multi(&_vfa, 0, (const void*)(arr), _vfa_n);                    \
        }                                                                                 \
        _vfa;                                                                             \
    })
#define VEC_FROM_ARR(T, n, arr) VEC_FROM_ARR_IN(WC_LIBC, T, n, arr)


// Push

// VEC_PUSH — push a PLAIN-DATA value (E2: owning types use VEC_PUSH_MOVE / VEC_PUSH_COPY).
/* Hot path is inline: a POD element with spare capacity is one typed store, with no call
 * and no runtime-length memcpy (sizeof is a constant here). Everything else (grow, a size
 * mismatch) goes through GenVec_push unchanged. */
#define VEC_PUSH(vec, val)                                                                                      \
    ({                                                                                                          \
        WC_REQUIRE_POD(typeof(val), "VEC_PUSH");                                                                \
        typeof(val) wvp_tmp = (val);                                                                            \
        WC_ASSERT_ELEM_SIZE(vec, typeof(wvp_tmp));                                                              \
        if (WC_LIKELY((vec)->is_pod && (vec)->size < (vec)->capacity && (vec)->data_size == sizeof(wvp_tmp))) { \
            __builtin_memcpy((vec)->data + ((vec)->size * sizeof(wvp_tmp)), &wvp_tmp, sizeof(wvp_tmp));         \
            (vec)->size++;                                                                                      \
        } else {                                                                                                \
            GenVec_push(vec, &wvp_tmp);                                                                         \
        }                                                                                                       \
    })

// VEC_PUSH_MOVE — transfer ownership of an LVALUE element (struct or pointer).
// The source is left zeroed (a pointer source becomes NULL).
//   String s = ...;       VEC_PUSH_MOVE(&v, s);        // by-value vec
//   String* p = ...;      VEC_PUSH_MOVE(&v, p);        // by-pointer vec, p == NULL after
#define VEC_PUSH_MOVE(vec, lval)                  \
    ({                                            \
        WC_ASSERT_ELEM_SIZE((vec), typeof(lval)); \
        GenVec_push_move((vec), (void*)&(lval));  \
    })

// VEC_PUSH_COPY — deep-copy an LVALUE element in; you keep lval (B8).
#define VEC_PUSH_COPY(vec, lval)                  \
    ({                                            \
        WC_ASSERT_ELEM_SIZE((vec), typeof(lval)); \
        GenVec_push((vec), (const void*)&(lval)); \
    })

// VEC_PUSH_CSTR — build a String from a C string and move it into a String
// vector, by value (wc_str_ops) or by pointer (wc_str_ptr_ops).
// Uses the container's own allocator so strings follow their vector.
#define VEC_PUSH_CSTR(vec, cstr)                                                        \
    ({                                                                                  \
        if ((vec)->data_size == sizeof(String)) {                                       \
            /* by value: build on the vec's allocator, move the struct in */            \
            String _wpc_s = String_from_cstr((vec)->alloc, (cstr));                     \
            GenVec_push_move((vec), (void*)&_wpc_s); /* _wpc_s zeroed by move */        \
        } else {                                                                        \
            /* by pointer: box the String in the vec's allocator */                     \
            WC_ASSERT_ELEM_SIZE((vec), String*);                                        \
            String* _wpc_p = WC_BOX_IN((vec)->alloc, String, String_from_cstr, (cstr)); \
            GenVec_push_move((vec), (void*)&_wpc_p); /* slot owns the pointer */        \
        }                                                                               \
    })


// Access
// All type-asserting macros guard with WC_ASSERT_ELEM_SIZE, pass the right T.

// By value: plain data only (E3).
#define VEC_AT(vec, T, i)                 \
    ({                                    \
        WC_REQUIRE_NO_ALIAS(T, "VEC_AT"); \
        WC_ASSERT_ELEM_SIZE((vec), T);    \
        *(T*)GenVec_get_ptr((vec), (i));  \
    })

// Read-only pointer into the vector: any T (E3).
#define VEC_REF(vec, T, i)                    \
    ({                                        \
        WC_ASSERT_ELEM_SIZE((vec), T);        \
        (const T*)GenVec_get_ptr((vec), (i)); \
    })

#define VEC_AT_MUT(vec, T, i)               \
    ({                                      \
        WC_ASSERT_ELEM_SIZE((vec), T);      \
        (T*)GenVec_get_ptr_mut((vec), (i)); \
    })

#define VEC_FRONT(vec, T)                    \
    ({                                       \
        WC_REQUIRE_NO_ALIAS(T, "VEC_FRONT"); \
        WC_ASSERT_ELEM_SIZE((vec), T);       \
        *(const T*)GenVec_front((vec));      \
    })

#define VEC_BACK(vec, T)                    \
    ({                                      \
        WC_REQUIRE_NO_ALIAS(T, "VEC_BACK"); \
        WC_ASSERT_ELEM_SIZE((vec), T);      \
        *(const T*)GenVec_back((vec));      \
    })


// Mutate

// Replace element i with a plain-data value (E2).
#define VEC_SET(vec, i, val)                               \
    ({                                                     \
        WC_REQUIRE_POD(typeof(val), "VEC_SET");            \
        typeof(val) wvs_tmp = (val);                       \
        WC_ASSERT_ELEM_SIZE((vec), typeof(wvs_tmp));       \
        GenVec_replace((vec), (i), (const void*)&wvs_tmp); \
    })

// Replace element i by moving an lvalue in (old element deleted, lval zeroed).
#define VEC_SET_MOVE(vec, i, lval)                       \
    ({                                                   \
        WC_ASSERT_ELEM_SIZE((vec), typeof(lval));        \
        GenVec_replace_move((vec), (i), (void*)&(lval)); \
    })

// Replace element i with a deep copy of an lvalue (old element deleted, you keep lval).
#define VEC_SET_COPY(vec, i, lval)                        \
    ({                                                    \
        WC_ASSERT_ELEM_SIZE((vec), typeof(lval));         \
        GenVec_replace((vec), (i), (const void*)&(lval)); \
    })


// Pop

// Removes and returns the last element. It is MOVED out (B7): you own it now,
// for every T. The POD hot path is one typed load.
#define VEC_POP(vec, T)                                                                     \
    ({                                                                                      \
        WC_ASSERT_ELEM_SIZE(vec, T);                                                        \
        T wvpop;                                                                            \
        if (WC_LIKELY((vec)->is_pod && (vec)->size > 0 && (vec)->data_size == sizeof(T))) { \
            (vec)->size--;                                                                  \
            __builtin_memcpy(&wvpop, (vec)->data + ((vec)->size * sizeof(T)), sizeof(T));   \
        } else {                                                                            \
            memset(&wvpop, 0, sizeof(T));                                                   \
            GenVec_pop(vec, (void*)&wvpop);                                                 \
        }                                                                                   \
        wvpop;                                                                              \
    })


// Iterate
/* bounds hoisted into the outer loop (_wvf_n) and element fetch elided via
 * GenVec_get_ptr_mut_unsafe — the index is provably < size. NOTE: no size assert
 * here (a leading statement would break `if (x) VEC_FOREACH(...) ...` usage);
 * the T* assignment still gives compile-time type checking.
 */
/* All *_FOREACH macros are two nested loops sharing a flag `k`:
 *   normal pass : the inner loop flips k to 0, the outer flips it back to 1 and continues
 *   break       : skips the inner flip, so the outer flip sets k to 0 and the loop ends
 *   continue    : runs the inner flip, same as a normal pass
 * Loop variables get __LINE__-unique names, so FOREACHes nest without shadowing.
 * The optimizer removes the flag.
 */
#define WC_FE_(name, line)        WC_CAT(WC_CAT(_wfe_, name), line)
#define VEC_FOREACH(vec, T, name) VEC_FOREACH_(vec, T, name, __LINE__)
// NOLINTBEGIN(bugprone-macro-parentheses): T is a type; (T)* would parse as a cast
#define VEC_FOREACH_(vec, T, name, L)                                                                  \
    for (u64 WC_FE_(k, L) = 1, WC_FE_(i, L) = 0, WC_FE_(n, L) = (vec)->size;                           \
         WC_FE_(k, L) && WC_FE_(i, L) < WC_FE_(n, L); WC_FE_(k, L) = !WC_FE_(k, L), WC_FE_(i, L)++)    \
        for (T* name = (T*)GenVec_get_ptr_mut_unsafe((vec), WC_FE_(i, L));                             \
             WC_FE_(k, L); /* NOLINT(bugprone-macro-parentheses): T is a type: (T)* would be a cast */ \
             WC_FE_(k, L) = !WC_FE_(k, L), (void)(name))
// NOLINTEND(bugprone-macro-parentheses)



/*
 * TYPED CONVENIENCE MACROS
*/

// Vector creation shorthands

#define VEC_OF_INT(cap)     VEC(int, (cap))
#define VEC_OF_STR(cap)     VEC_CX(String, (cap), &wc_str_ops)
#define VEC_OF_STR_PTR(cap) VEC_CX(String*, (cap), &wc_str_ptr_ops)
#define VEC_OF_VEC(cap)     VEC_CX(GenVec, (cap), &wc_vec_ops)
#define VEC_OF_VEC_PTR(cap) VEC_CX(GenVec*, (cap), &wc_vec_ptr_ops)


// Push shorthands

#define VEC_PUSH_VEC(outer, inner)         VEC_PUSH_MOVE((outer), (inner))     // inner: GenVec lvalue
#define VEC_PUSH_VEC_PTR(outer, inner_ptr) VEC_PUSH_MOVE((outer), (inner_ptr)) // inner_ptr: GenVec* lvalue


// Hashmap: String-keyed lookups without building a String (rule E1)
/* A C string is wrapped in a borrowed, read-only String (String_borrow): it
 * hashes and compares exactly like an owning String with the same bytes, and
 * costs no allocation. The map must use &wc_str_ops for its keys (MAP_OF(String, V)). */

#define WC_ASSERT_STR_KEYS(map, what)                                                         \
    WC_ASSERT((map)->key_size == sizeof(String) && (map)->key_ops && (map)->key_ops->copy_fn, \
              what ": map keys must be String with wc_str_ops")

#define WC_CSTR_PROBE(cstr) String_borrow((cstr), strlen(cstr))

/*
 * MAP_PUT_INT_STR(map, int_key, cstr_val)
 * Map must have int key, String val, created with &wc_str_ops for val.
 * The value String is built in the map's allocator and moved in.
 */
#define MAP_PUT_INT_STR(map, k, cstr_val)                                              \
    ({                                                                                 \
        String _v = String_from_cstr((map)->alloc, (cstr_val));                        \
        bool   _r = HashMap_put_val_move((map), (const void*)&(int){(k)}, (void*)&_v); \
        _r;                                                                            \
    })

/*
 * MAP_PUT_STR_INT(map, cstr_key, int_val)
 * The key is COPIED from a borrowed probe: an existing key costs no allocation;
 * a new key is copied once into the map's allocator.
 */
#define MAP_PUT_STR_INT(map, cstr_key, int_val)                                           \
    ({                                                                                    \
        WC_ASSERT_STR_KEYS((map), "MAP_PUT_STR_INT");                                     \
        String _k = WC_CSTR_PROBE(cstr_key);                                              \
        bool   _r = HashMap_put((map), (const void*)&_k, (const void*)&(int){(int_val)}); \
        _r;                                                                               \
    })

/*
 * MAP_PUT_STR_STR(map, cstr_key, cstr_val)
 * Map must use &wc_str_ops for both key and val. Key as in MAP_PUT_STR_INT;
 * the value String is built in the map's allocator and moved in.
 */
#define MAP_PUT_STR_STR(map, cstr_key, cstr_val)                               \
    ({                                                                         \
        WC_ASSERT_STR_KEYS((map), "MAP_PUT_STR_STR");                          \
        String _k = WC_CSTR_PROBE(cstr_key);                                   \
        String _v = String_from_cstr((map)->alloc, (cstr_val));                \
        bool   _r = HashMap_put_val_move((map), (const void*)&_k, (void*)&_v); \
        _r;                                                                    \
    })

// bool: is the C string a key?
#define MAP_HAS_CSTR(map, cstr)                    \
    ({                                             \
        WC_ASSERT_STR_KEYS((map), "MAP_HAS_CSTR"); \
        String _k = WC_CSTR_PROBE(cstr);           \
        HashMap_has((map), (const void*)&_k);      \
    })

// const V*: the value under the C string key, or NULL. Any V.
// NOLINTBEGIN(bugprone-macro-parentheses): V is a type; (V)* would parse as a cast
#define MAP_GET_PTR_CSTR(map, V, cstr)                                             \
    ({                                                                             \
        WC_ASSERT_STR_KEYS((map), "MAP_GET_PTR_CSTR");                             \
        WC_ASSERT_SIZE(sizeof(V), (map)->val_size, "MAP_GET_PTR_CSTR value type"); \
        String _k = WC_CSTR_PROBE(cstr);                                           \
        (const V*)HashMap_get_ptr((map), (const void*)&_k);                        \
    })
// NOLINTEND(bugprone-macro-parentheses)

// V by value (plain data only, E3). A missing key aborts in Debug, zero V in Release.
#define MAP_GET_CSTR(map, V, cstr)                         \
    ({                                                     \
        WC_REQUIRE_NO_ALIAS(V, "MAP_GET_CSTR");            \
        const V* _mp = MAP_GET_PTR_CSTR((map), V, (cstr)); \
        WC_ASSERT(_mp, "MAP_GET_CSTR: key not found");     \
        _mp ? *_mp : (V){0};                               \
    })


// Put (COPY semantics): plain-data key and value (E2)

// TODO: isn't this a copy in here? typeof(key) _mk = (key);
#define MAP_PUT(map, key, val)                                         \
    ({                                                                 \
        WC_REQUIRE_POD(typeof(key), "MAP_PUT key");                    \
        WC_REQUIRE_POD(typeof(val), "MAP_PUT value");                  \
        typeof(key) _mk = (key);                                       \
        typeof(val) _mv = (val);                                       \
        WC_ASSERT_SIZE(sizeof(_mk), (map)->key_size, "MAP_PUT key");   \
        WC_ASSERT_SIZE(sizeof(_mv), (map)->val_size, "MAP_PUT value"); \
        HashMap_put((map), (const void*)&_mk, (const void*)&_mv);      \
    })

// Deep-copy both from LVALUES: any types, you keep both
#define MAP_PUT_COPY(map, klval, vlval)                                       \
    ({                                                                        \
        WC_ASSERT_SIZE(sizeof(klval), (map)->key_size, "MAP_PUT_COPY key");   \
        WC_ASSERT_SIZE(sizeof(vlval), (map)->val_size, "MAP_PUT_COPY value"); \
        HashMap_put((map), (const void*)&(klval), (const void*)&(vlval));     \
    })


// Put (MOVE semantics): key/val are LVALUE elements, left zeroed after the call.

#define MAP_PUT_MOVE(map, klval, vlval)                                       \
    ({                                                                        \
        WC_ASSERT_SIZE(sizeof(klval), (map)->key_size, "MAP_PUT_MOVE key");   \
        WC_ASSERT_SIZE(sizeof(vlval), (map)->val_size, "MAP_PUT_MOVE value"); \
        HashMap_put_move((map), (void*)&(klval), (void*)&(vlval));            \
    })

// key moved, plain-data value copied
#define MAP_PUT_KEY_MOVE(map, klval, val)                                       \
    ({                                                                          \
        WC_REQUIRE_POD(typeof(val), "MAP_PUT_KEY_MOVE value");                  \
        typeof(val) _mv = (val);                                                \
        WC_ASSERT_SIZE(sizeof(klval), (map)->key_size, "MAP_PUT_KEY_MOVE key"); \
        WC_ASSERT_SIZE(sizeof(_mv), (map)->val_size, "MAP_PUT_KEY_MOVE value"); \
        HashMap_put_key_move((map), (void*)&(klval), (const void*)&_mv);        \
    })

// plain-data key copied, value moved
#define MAP_PUT_VAL_MOVE(map, key, vlval)                                         \
    ({                                                                            \
        WC_REQUIRE_POD(typeof(key), "MAP_PUT_VAL_MOVE key");                      \
        typeof(key) _mk = (key);                                                  \
        WC_ASSERT_SIZE(sizeof(_mk), (map)->key_size, "MAP_PUT_VAL_MOVE key");     \
        WC_ASSERT_SIZE(sizeof(vlval), (map)->val_size, "MAP_PUT_VAL_MOVE value"); \
        HashMap_put_val_move((map), (const void*)&_mk, (void*)&(vlval));          \
    })


// Get (plain-data key; String keys use the _CSTR forms above)

// const V*: pointer to the value, or NULL. Any V. Valid until the next insert/delete (D1).
// NOLINTBEGIN(bugprone-macro-parentheses): V is a type; (V)* would parse as a cast
#define MAP_GET_PTR(map, V, key)                                              \
    ({                                                                        \
        WC_REQUIRE_POD(typeof(key), "MAP_GET_PTR key");                       \
        typeof(key) _mk = (key);                                              \
        WC_ASSERT_SIZE(sizeof(_mk), (map)->key_size, "MAP_GET_PTR key");      \
        WC_ASSERT_SIZE(sizeof(V), (map)->val_size, "MAP_GET_PTR value type"); \
        (const V*)HashMap_get_ptr((map), (const void*)&_mk);                  \
    })
// NOLINTEND(bugprone-macro-parentheses)

// V by value (plain data only, E3). Asserts the key is present: a missing key
// aborts in Debug and gives a zeroed V in Release. No copy beyond the load.
#define MAP_GET(map, V, key)                         \
    ({                                               \
        WC_REQUIRE_NO_ALIAS(V, "MAP_GET");           \
        const V* _mp = MAP_GET_PTR((map), V, (key)); \
        WC_ASSERT(_mp, "MAP_GET: key not found");    \
        _mp ? *_mp : (V){0};                         \
    })

// bool: 1 if found (and *out_ptr written), 0 if not. Plain-data V only (E3).
// NOLINTBEGIN(bugprone-macro-parentheses): V is a type; (V)* would parse as a cast
#define MAP_TRY_GET(map, V, key, out_ptr)                                   \
    ({                                                                      \
        WC_REQUIRE_NO_ALIAS(V, "MAP_TRY_GET");                              \
        V*       _mo = (out_ptr); /* compile-time check: out_ptr is a V* */ \
        const V* _mp = MAP_GET_PTR((map), V, (key));                        \
        if (_mp) {                                                          \
            *_mo = *_mp;                                                    \
        }                                                                   \
        _mp != NULL;                                                        \
    })
// NOLINTEND(bugprone-macro-parentheses)



// Iterate

#define MAP_FOREACH_KEY(c, T, name) MAP_FOREACH_KEY_(c, T, name, __LINE__)
#define MAP_FOREACH_KEY_(c, T, name, L)                                                             \
    for (u64 WC_FE_(k, L) = 1, WC_FE_(i, L) = 0, WC_FE_(n, L) = HashMap_bucket_count(c);            \
         WC_FE_(k, L) && WC_FE_(i, L) < WC_FE_(n, L); WC_FE_(k, L) = !WC_FE_(k, L), WC_FE_(i, L)++) \
        for (const T*(name)             = HashMap_bucket_occupied((c), WC_FE_(i, L))                \
                                              ? (const T*)HashMap_bucket_key_ptr((c), WC_FE_(i, L)) \
                                              : NULL;                                               \
             WC_FE_(k, L); WC_FE_(k, L) = !WC_FE_(k, L))                                            \
            if (!(name)) {                                                                          \
            } else

#define MAP_FOREACH_VAL(c, T, name) MAP_FOREACH_VAL_(c, T, name, __LINE__)
// NOLINTBEGIN(bugprone-macro-parentheses): T is a type; (T)* would parse as a cast
#define MAP_FOREACH_VAL_(c, T, name, L)                                                                              \
    for (u64 WC_FE_(k, L) = 1, WC_FE_(i, L) = 0, WC_FE_(n, L) = HashMap_bucket_count(c);                             \
         WC_FE_(k, L) && WC_FE_(i, L) < WC_FE_(n, L); WC_FE_(k, L) = !WC_FE_(k, L), WC_FE_(i, L)++)                  \
        for (T* name =                                                                                               \
                 HashMap_bucket_occupied((c), WC_FE_(i, L))                                                          \
                     ? (T*)HashMap_bucket_val_ptr(                                                                   \
                           (c),                                                                                      \
                           WC_FE_(i, L)) /* NOLINT(bugprone-macro-parentheses): T is a type: (T)* would be a cast */ \
                     : NULL;                                                                                         \
             WC_FE_(k, L); WC_FE_(k, L) = !WC_FE_(k, L))                                                             \
            if (!(name)) {                                                                                           \
            } else
// NOLINTEND(bugprone-macro-parentheses)


// Hashset shorthands

// A new set of vec's elements (deep copies), on vec's allocator. Sized once up
// front: no intermediate resizes.
#define SET_FROM_VEC(vec, hash_fn, cmp_fn) SET_FROM_VEC_((vec), hash_fn, cmp_fn, __LINE__)
#define SET_FROM_VEC_(vec, hash_fn, cmp_fn, L)                                                                     \
    ({                                                                                                             \
        HashSet WC_FE_(set, L) = HashSet_create((vec)->alloc, (vec)->data_size, hash_fn, cmp_fn, (vec)->ops);      \
        HashSet_reserve(&WC_FE_(set, L), GenVec_size(vec));                                                        \
        for (u64 WC_FE_(i, L) = 0, WC_FE_(n, L) = GenVec_size(vec); WC_FE_(i, L) < WC_FE_(n, L); WC_FE_(i, L)++) { \
            HashSet_insert(&WC_FE_(set, L), GenVec_get_ptr((vec), WC_FE_(i, L)));                                  \
        }                                                                                                          \
        WC_FE_(set, L);                                                                                            \
    })

// Plain-data value
#define SET_INSERT(set, elm)                                          \
    ({                                                                \
        WC_REQUIRE_POD(typeof(elm), "SET_INSERT");                    \
        typeof(elm) _temp = (elm);                                    \
        WC_ASSERT_SIZE(sizeof(_temp), (set)->elm_size, "SET_INSERT"); \
        HashSet_insert((set), (const void*)&(_temp));                 \
    })

// Deep-copy an lvalue in; you keep it (B8).
#define SET_INSERT_COPY(set, lval)                                        \
    ({                                                                    \
        WC_ASSERT_SIZE(sizeof(lval), (set)->elm_size, "SET_INSERT_COPY"); \
        HashSet_insert((set), (const void*)&(lval));                      \
    })

// lval is left zeroed (moved in, or destroyed if already present)
#define SET_INSERT_MOVE(set, lval)                                        \
    ({                                                                    \
        WC_ASSERT_SIZE(sizeof(lval), (set)->elm_size, "SET_INSERT_MOVE"); \
        HashSet_insert_move((set), (void*)&(lval));                       \
    })

#define WC_ASSERT_STR_SET(set, what)                                                  \
    WC_ASSERT((set)->elm_size == sizeof(String) && (set)->ops && (set)->ops->copy_fn, \
              what ": set elements must be String with wc_str_ops")

// Insert a C string into a String set. Probes with a borrowed String first: an
// existing string costs no allocation; a new one is copied once into the set's allocator.
#define SET_INSERT_CSTR(set, cstr)                   \
    ({                                               \
        WC_ASSERT_STR_SET((set), "SET_INSERT_CSTR"); \
        String _s = WC_CSTR_PROBE(cstr);             \
        HashSet_insert((set), (const void*)&_s);     \
    })

// bool: is the C string in a String set? No allocation.
#define SET_HAS_CSTR(set, cstr)                   \
    ({                                            \
        WC_ASSERT_STR_SET((set), "SET_HAS_CSTR"); \
        String _s = WC_CSTR_PROBE(cstr);          \
        HashSet_has((set), (const void*)&_s);     \
    })


#define SET_FOREACH(c, T, name) SET_FOREACH_(c, T, name, __LINE__)
#define SET_FOREACH_(c, T, name, L)                                                                 \
    for (u64 WC_FE_(k, L) = 1, WC_FE_(i, L) = 0, WC_FE_(n, L) = HashSet_bucket_count(c);            \
         WC_FE_(k, L) && WC_FE_(i, L) < WC_FE_(n, L); WC_FE_(k, L) = !WC_FE_(k, L), WC_FE_(i, L)++) \
        for (const T*(name)             = HashSet_bucket_occupied((c), WC_FE_(i, L))                \
                                              ? (const T*)HashSet_bucket_elm_ptr((c), WC_FE_(i, L)) \
                                              : NULL;                                               \
             WC_FE_(k, L); WC_FE_(k, L) = !WC_FE_(k, L))                                            \
            if (!(name)) {                                                                          \
            } else


// Stack macros

// Stack_create returns Stack by value. Use WC_LIBC for the common case.
// STACK_CREATE / STACK_CREATE_IN: by value on the stack (no heap shell).
#define STACK_CREATE(T, cap)               Stack_create(WC_LIBC, (cap), sizeof(T), NULL)
#define STACK_CREATE_CX(T, cap, ops)       Stack_create(WC_LIBC, (cap), sizeof(T), (ops))
#define STACK_CREATE_IN(A, T, cap)         Stack_create((A), (cap), sizeof(T), NULL)
#define STACK_CREATE_CX_IN(A, T, cap, ops) Stack_create((A), (cap), sizeof(T), (ops))
#define STACK_PUSH(stk, val)               VEC_PUSH((stk), (val))
#define STACK_PUSH_MOVE(stk, lval)         VEC_PUSH_MOVE((stk), lval)
#define STACK_PUSH_COPY(stk, lval)         VEC_PUSH_COPY((stk), lval)
#define STACK_POP(stk, T)                  VEC_POP((stk), T)
#define STACK_AT(stk, T, i)                VEC_AT((stk), T, (i))
#define STACK_REF(stk, T, i)               VEC_REF((stk), T, (i))
#define STACK_FOREACH(stk, T, name)        VEC_FOREACH ((stk), T, name)


// Queue macros

// Queue_create returns Queue by value. Use WC_LIBC for the common case.
#define QUEUE_CREATE(T, cap)               Queue_create(WC_LIBC, (cap), sizeof(T), NULL)
#define QUEUE_CREATE_CX(T, cap, ops)       Queue_create(WC_LIBC, (cap), sizeof(T), (ops))
#define QUEUE_CREATE_IN(A, T, cap)         Queue_create((A), (cap), sizeof(T), NULL)
#define QUEUE_CREATE_CX_IN(A, T, cap, ops) Queue_create((A), (cap), sizeof(T), (ops))

// Plain-data value (E2).
#define QUEUE_PUSH(q, val)                               \
    ({                                                   \
        WC_REQUIRE_POD(typeof(val), "QUEUE_PUSH");       \
        typeof(val) _qp_tmp = (val);                     \
        WC_ASSERT_ELEM_SIZE(&(q)->arr, typeof(_qp_tmp)); \
        Queue_push((q), (const void*)&_qp_tmp);          \
    })

#define QUEUE_PUSH_MOVE(q, lval)                      \
    ({                                                \
        WC_ASSERT_ELEM_SIZE(&(q)->arr, typeof(lval)); \
        Queue_push_move((q), (void*)&(lval));         \
    })

#define QUEUE_PUSH_COPY(q, lval)                      \
    ({                                                \
        WC_ASSERT_ELEM_SIZE(&(q)->arr, typeof(lval)); \
        Queue_push((q), (const void*)&(lval));        \
    })

#define QUEUE_PUSH_CSTR(q, cstr)                               \
    ({                                                         \
        String _qp_s = String_from_cstr((q)->arr.alloc, cstr); \
        Queue_push_move((q), (void*)&_qp_s);                   \
    })

// Front element MOVED out (B7): yours, for every T.
#define QUEUE_POP(q, T)                    \
    ({                                     \
        WC_ASSERT_ELEM_SIZE(&(q)->arr, T); \
        T _qp_out;                         \
        memset(&_qp_out, 0, sizeof(T));    \
        Queue_pop((q), (void*)&_qp_out);   \
        _qp_out;                           \
    })

// Front element by value: plain data only (E3).
#define QUEUE_PEEK(q, T)                      \
    ({                                        \
        WC_REQUIRE_NO_ALIAS(T, "QUEUE_PEEK"); \
        WC_ASSERT_ELEM_SIZE(&(q)->arr, T);    \
        *(const T*)Queue_peek_ptr(q);         \
    })

// Read-only pointer to the front element: any T.
#define QUEUE_PEEK_REF(q, T)               \
    ({                                     \
        WC_ASSERT_ELEM_SIZE(&(q)->arr, T); \
        (const T*)Queue_peek_ptr(q);       \
    })


#endif // WC_MACROS_H
