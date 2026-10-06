#ifndef WC_SPECIALIZE_H
#define WC_SPECIALIZE_H

#include "common.h"

#include <string.h>

/*
 * SIZE SPECIALISATION
 * ===================
 *
 * Containers store elements as raw bytes and learn the element size at runtime
 * (a GenVec has a data_size field, a HashMap has key_size / val_size fields).
 * The copy routine is therefore memcpy(d, s, size) with `size` a variable, and
 * that is a call into libc: the compiler cannot see how many bytes move, so it
 * emits a call instead of a few plain loads and stores.
 *
 * This header turns those runtime sizes into compile-time constants, in two
 * levels:
 *
 *   1. wc_copy_n / wc_equal_n
 *      Drop-in for memcpy / (memcmp == 0). A switch sends the common sizes to
 *      a fixed-size copy. Use them anywhere: no setup, no change to the caller.
 *      On their own they only replace the libc call with a switch. They become
 *      free when the size is a constant at the call site, because then the
 *      switch folds away entirely (see level 2).
 *
 *   2. WC_SPECIALIZE(size, N, call)          - one runtime size
 *      WC_SPECIALIZE_2(s1, N1, s2, N2, call) - two runtime sizes
 *      A poor man's template. Each macro runs `call` once, with N (or N1/N2) a
 *      compile-time constant for each listed size, and with N bound to the
 *      runtime size otherwise. Use it to wrap a hot loop that lives in an
 *      always_inline function taking the size as a parameter. Each case gets
 *      its own inlined copy of the loop, so every wc_copy_n inside folds to a
 *      single fixed-size move, and the size switch runs once per call instead
 *      of once per element.
 *
 *      static inline __attribute__((always_inline))
 *      void fill_k(u8* dst, const u8* src, u64 n, u32 size)
 *      {
 *          for (u64 i = 0; i < n; i++) {
 *              wc_copy_n(dst + (i * size), src, size);
 *          }
 *      }
 *
 *      void fill(GenVec* v, const void* val, u64 n)
 *      {
 *          WC_SPECIALIZE(v->data_size, SZ, fill_k(v->data, val, n, SZ));
 *      }
 *
 * SIZE LISTS
 * ----------
 * Which sizes get their own copy is set by two lists (X-macros, see below):
 *
 *   WC_SPECIALIZE_SIZES      WC_SPECIALIZE, and the FIRST size of WC_SPECIALIZE_2
 *                            default: 4 8 16 48
 *   WC_SPECIALIZE_SIZES_2ND  the SECOND size of WC_SPECIALIZE_2
 *                            default: 0 4 8 16 48
 *
 *   0  - no value at all (a HashMap set): only in the second list
 *   4  - int, u32, float, small enum
 *   8  - u64, pointer, double
 *   16 - two words: a string view (ptr + len), a pair of u64
 *   48 - String, GenVec
 *
 * The defaults are measured. Widening
 * both lists to 1 2 4 8 16 24 32 48 64 cut instructions 8% for a 24-byte
 * value, but cost up to 6% more on EVERY hashmap lookup (a wider switch runs
 * per call) and doubled the hashmap's code. A list entry pays off only for a size that
 * is common in real use.
 *
 * Override either list for a translation unit by defining it before including
 * this header. Every size you add is one more copy of every wrapped body:
 *
 *   #define WC_SPECIALIZE_SIZES(X, ...) X(4, __VA_ARGS__) X(8, __VA_ARGS__)
 *   #include "wc_specialize.h"
 *
 * Keep every listed size also handled by wc_copy_n's switch. A listed size that
 * wc_copy_n does not fast-path still folds (the default memcpy gets a constant
 * size), so this is about clarity, not correctness.
 *
 * CODE SIZE
 * ---------
 * Every listed size is one more inlined copy of the wrapped code. That is the
 * entire trade: one switch per call and N copies of the body, in exchange for
 * removing a per-element branch or libc call.
 *
 *   - Wrap only hot loops. A function called once per program does not care.
 *   - WC_SPECIALIZE   makes |SIZES| + 1 copies of `call` (default: 5).
 *   - WC_SPECIALIZE_2 makes |SIZES| x |SIZES_2ND| + |SIZES| + 1 copies
 *     (default: 4 x 5 + 4 + 1 = 25): every listed pair, every listed first
 *     size with a runtime second size, and the fully runtime arm.
 *   - Use WC_SPECIALIZE_2 only where both sizes gate a copy inside the loop (a
 *     hashmap insert copies key AND value per slot). A lookup that only touches
 *     keys uses WC_SPECIALIZE on the key size.
 *   - Never put a WC_SPECIALIZE inside another one's body. WC_SPECIALIZE_2 is
 *     the 2-D form: one flat switch, one default arm.
 */


/*
 * wc_copy_n - fixed-size copy for common element sizes.
 *
 * Drop-in for memcpy(dst, src, n). Semantics match memcpy exactly, including
 * the restrict contract: dst and src must not overlap. If they may overlap,
 * use memmove instead; this function deliberately does not.
 *
 * The switch sends the common sizes to a memcpy with a literal size. Those
 * memcpy calls are themselves inlined by the compiler (a 1/2/4/8 byte memcpy
 * is just a load and a store), so the switch is really "pick a fixed-size move
 * or fall back to libc". When `n` is a compile-time constant (inside every
 * WC_SPECIALIZE / WC_SPECIALIZE_2 body) the whole switch folds away and only
 * the matching arm survives.
 *
 * n == 0 is a no-op on purpose. A set (HashMap with val_size 0) stores no
 * values and may pass a NULL src for its value copies. memcpy(dst, NULL, 0) is
 * UB (the standard requires valid pointers even for zero bytes, and glibc
 * declares memcpy nonnull, so the compiler may assume src != NULL afterwards).
 * The explicit 0 case avoids the call entirely.
 */
static inline __attribute__((always_inline)) void wc_copy_n(void* restrict dst, const void* restrict src, u64 n)
{
    switch (n) {
    case 0:
        break; // a set's value: src may be NULL, so no memcpy at all
    case 1:
        memcpy(dst, src, 1);
        break;
    case 2:
        memcpy(dst, src, 2);
        break;
    case 4:
        memcpy(dst, src, 4);
        break;
    case 8:
        memcpy(dst, src, 8);
        break;
    case 16:
        memcpy(dst, src, 16);
        break;
    case 24:
        memcpy(dst, src, 24);
        break;
    case 32:
        memcpy(dst, src, 32);
        break;
    case 48:
        memcpy(dst, src, 48);
        break;
    case 64:
        memcpy(dst, src, 64);
        break;
    default:
        memcpy(dst, src, n);
        break;
    }
}

/*
 * wc_equal_n - byte equality for common element sizes.
 *
 * Drop-in for (memcmp(a, b, n) == 0), returning true/false instead of the
 * memcmp tri-state. Use it in key comparisons and anywhere a memcmp would
 * otherwise be a libc call.
 *
 * 4 and 8 bytes compare as one integer instead of a memcmp. That is the common
 * case for hash keys (an int, a u64, a pointer) and the reason this function
 * exists: a memcmp call costs more than the comparison it performs. The memcpy
 * into a local u32/u64 is the standard type-punning idiom: it compiles to a
 * plain load and does not violate strict aliasing.
 *
 * 16 bytes is two 8-byte compares (a string view, a pair of u64).
 *
 * Other sizes fall back to memcmp. With a constant n the compiler already
 * turns a small memcmp into the same chain of loads and compares, so writing
 * them out gains nothing and keeps this function small enough to inline.
 *
 * n == 0 is true without touching either pointer, so both may be NULL.
 */
static inline __attribute__((always_inline)) bool wc_equal_n(const void* a, const void* b, u64 n)
{
    switch (n) {
    case 0:
        return true;
    case 4: {
        u32 x;
        u32 y;
        memcpy(&x, a, 4);
        memcpy(&y, b, 4);
        return x == y;
    }
    case 8: {
        u64 x;
        u64 y;
        memcpy(&x, a, 8);
        memcpy(&y, b, 8);
        return x == y;
    }
    case 16: {
        u64 x0;
        u64 x1;
        u64 y0;
        u64 y1;
        memcpy(&x0, a, 8);
        memcpy(&x1, (const u8*)a + 8, 8);
        memcpy(&y0, b, 8);
        memcpy(&y1, (const u8*)b + 8, 8);
        return (x0 == y0 && x1 == y1) != 0;
    }
    default:
        return memcmp(a, b, n) == 0;
    }
}


/*
 * SIZE LISTS (X-macros)
 *
 * A list is a macro that calls X once per size: X(size, args...). The case
 * macros below are the X. Define either list before including this header to
 * override it for that translation unit.
 */
#ifndef WC_SPECIALIZE_SIZES
#define WC_SPECIALIZE_SIZES(X, ...) X(4, __VA_ARGS__) X(8, __VA_ARGS__) X(16, __VA_ARGS__) X(48, __VA_ARGS__)
#endif

#ifndef WC_SPECIALIZE_SIZES_2ND
#define WC_SPECIALIZE_SIZES_2ND(X, ...) \
    X(0, __VA_ARGS__) X(4, __VA_ARGS__) X(8, __VA_ARGS__) X(16, __VA_ARGS__) X(48, __VA_ARGS__)
#endif


/*
 * MACRO MACHINERY
 * ==================
 * Call sites use only WC_SPECIALIZE and WC_SPECIALIZE_2 (plus wc_copy_n and
 * wc_equal_n above). Names ending in `_` are internal.
 *
 * `call` is variadic (...) in every macro, so code with unparenthesised commas
 * (a compound literal, a designated initialiser) passes through intact.
 */

/*
 * One switch arm: N is an enum constant equal to `value` inside `call`.
 *
 * An enum constant, not a `const u32`: only an enum constant is a true
 * compile-time constant in C. A `const u32` is a read-only variable whose value
 * the compiler MAY propagate. The enum guarantees wc_copy_n(..., N) folds.
 *
 * The braces are load-bearing: they scope the enum, so the next case can
 * declare its own N. N must stay unparenthesised: `enum { (N) = 4 }` is a
 * syntax error, whatever a macro-parentheses linter suggests.
 */
// NOLINTBEGIN(bugprone-macro-parentheses)
#define WC_SPECIALIZE_CASE_(value, N, ...) \
    case (value): {                        \
        enum { N = (value) };              \
        __VA_ARGS__;                       \
        break;                             \
    }

/*
 * WC_SPECIALIZE - run `call` with one runtime size turned into a constant.
 *
 * Usage:
 *   WC_SPECIALIZE(size_expr, NAME, code_using_NAME);
 *
 * Expands to a switch on `size_expr`. Each arm binds NAME to a compile-time
 * constant equal to that case's size, then runs the code. The default arm binds
 * NAME to the runtime size, so the macro is correct for any size, just not
 * faster for unlisted ones.
 *
 * Why the wrapped code must be always_inline
 * ------------------------------------------
 * The macro only makes NAME a constant inside the arm. For the constant to
 * reach the copies in the loop, the loop's function must be inlined into the
 * arm. If it is not inlined, NAME is passed as an ordinary argument and nothing
 * folds. For a data_size of 8 the fill() example above becomes, in effect:
 *
 *   case 8: {
 *       enum { SZ = 8 };
 *       for (u64 i = 0; i < n; i++) {
 *           wc_copy_n(v->data + i * 8, val, 8);   // one 8-byte move
 *       }
 *       break;
 *   }
 *
 * Pitfalls
 *   - `size_expr` is evaluated twice (switch and default). Pass a plain field
 *     or a local, never an expression with side effects.
 *   - NAME must not collide with any identifier in `call`.
 *   - The macro adds `break` after `call`. A bare `break` or `continue` inside
 *     `call` hits the switch, not an outer loop. `return` works.
 *   - It is a statement, not an expression.
 *   - The default arm casts `size_expr` to u32. Element sizes never exceed it.
 */
#define WC_SPECIALIZE(size, N, ...)                              \
    switch (size) {                                              \
        WC_SPECIALIZE_SIZES(WC_SPECIALIZE_CASE_, N, __VA_ARGS__) \
    default: {                                                   \
        const u32 N = (u32)(size);                               \
        __VA_ARGS__;                                             \
        break;                                                   \
    }                                                            \
    }


/*
 * One row of WC_SPECIALIZE_2: N1 is the constant v1, then an inner switch on
 * size2 over WC_SPECIALIZE_SIZES_2ND. The inner default keeps N1 constant and
 * binds N2 to the runtime size2: the one-dimension fallback.
 */
#define WC_SPECIALIZE_2_ROW_(v1, N1, size2, N2, ...)                      \
    case (v1): {                                                          \
        enum { N1 = (v1) };                                               \
        switch (size2) {                                                  \
            WC_SPECIALIZE_SIZES_2ND(WC_SPECIALIZE_CASE_, N2, __VA_ARGS__) \
        default: {                                                        \
            const u32 N2 = (u32)(size2);                                  \
            __VA_ARGS__;                                                  \
            break;                                                        \
        }                                                                 \
        }                                                                 \
        break;                                                            \
    }

/*
 * WC_SPECIALIZE_2 - run `call` with two runtime sizes turned into constants.
 *
 * Usage:
 *   WC_SPECIALIZE_2(size1_expr, NAME1, size2_expr, NAME2, code_using_both);
 *
 * The 2-D form of WC_SPECIALIZE, for a hot loop that copies two things whose
 * sizes are both runtime. A hashmap rehash copies a key of key_size and a value
 * of val_size for every element:
 *
 *   static void map_resize(HashMap* map, u64 new_capacity)
 *   {
 *       ...
 *       WC_SPECIALIZE_2(map->key_size, KS, map->val_size, VS,
 *                       map_rehash_k(map, old_keys, old_vals, old_psls, old_cap, KS, VS));
 *   }
 *
 * For key_size 8, val_size 4 that is, in effect:
 *
 *   case 8: {
 *       enum { KS = 8 };
 *       switch (map->val_size) {
 *       case 4: {
 *           enum { VS = 4 };
 *           map_rehash_k(..., KS, VS);   // 8- and 4-byte moves per element
 *           break;
 *       }
 *       ...
 *
 * Fallback, one dimension at a time
 *   size1 and size2 both listed  -> N1 and N2 constant
 *   only size1 listed            -> N1 constant, N2 runtime (the row's default)
 *   size1 not listed             -> both runtime
 *   The middle step matters: without it an int key with an unlisted 24-byte
 *   value lost the key constant too and ran 21% more instructions than
 *   WC_SPECIALIZE on the key alone, as many as no specialisation at all
 *   (docs/specialization.md). Put the size that matters more first. For a
 *   hashmap that is the key: it is hashed, compared AND copied.
 *
 * Dispatch
 *   Two small switches on 32-bit sizes, once per call. An earlier version
 *   switched once on the pair packed into a u64; its sparse labels compiled to
 *   a compare tree that cost about 18 more instructions per hashmap insert
 *   than the nested form, which made 2-D slower than 1-D for single inserts.
 *   Dispatch runs once per call, so wrap the function that owns the loop.
 *
 * Pitfalls: everything WC_SPECIALIZE warns about, for both sizes. size2 is
 *   evaluated once per row: pass a plain field. Do not nest.
 */
#define WC_SPECIALIZE_2(size1, N1, size2, N2, ...)                            \
    switch (size1) {                                                          \
        WC_SPECIALIZE_SIZES(WC_SPECIALIZE_2_ROW_, N1, size2, N2, __VA_ARGS__) \
    default: {                                                                \
        const u32 N1 = (u32)(size1);                                          \
        const u32 N2 = (u32)(size2);                                          \
        __VA_ARGS__;                                                          \
        break;                                                                \
    }                                                                         \
    }
// NOLINTEND(bugprone-macro-parentheses)


#endif // WC_SPECIALIZE_H
