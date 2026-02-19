#ifndef WC_ERRNO_H
#define WC_ERRNO_H

#include <stdio.h>


/* wc_errno.h — Error reporting for WCtoolkit
 * ============================================
 *
 * Two tiers:
 *
 *   CHECK_FATAL  Programmer errors: null pointer, out of bounds, OOM.
 *                Crashes with a message. These are bugs, not conditions.
 *
 *   wc_errno     Expected conditions: pop on empty, arena full.
 *                Function returns NULL / 0 / void. wc_errno says why.
 *                Ignore it if you don't care. Check it if you do.
 *
 *
 * USAGE
 * -----
 *   // Check a single call:
 *   wc_errno = WC_OK;
 *   u8* p = arena_alloc(arena, size);
 *   if (!p && wc_errno == WC_ERR_FULL) { ... }
 *
 *   // Check a batch — wc_errno stays set if any call failed:
 *   wc_errno = WC_OK;
 *   float* a = (float*)arena_alloc(arena, 256);
 *   float* b = (float*)arena_alloc(arena, 256);
 *   if (wc_errno) { wc_perror("alloc"); }
 *
 *
 * RULES
 * -----
 *   1. Successful calls do NOT clear wc_errno — clear it yourself.
 *   2. Check the return value first. wc_errno tells you WHY, not WHETHER.
 *   3. wc_errno is thread-local. Each thread has its own copy.
 *
 *
 * WHAT SETS wc_errno
 * ------------------
 *   arena_alloc, arena_alloc_aligned      WC_ERR_FULL    arena exhausted
 *   genVec_pop, genVec_front, genVec_back WC_ERR_EMPTY   vec is empty
 *   dequeue, queue_peek, queue_peek_ptr   WC_ERR_EMPTY   queue is empty
 *   stack_pop, stack_peek                 WC_ERR_EMPTY   stack is empty
 */


typedef enum {
    WC_OK        = 0,
    WC_ERR_FULL,       // arena exhausted / container at capacity
    WC_ERR_EMPTY,      // pop or peek on empty container
} wc_err;

static inline const char* wc_strerror(wc_err e)
{
    switch (e) {
        case WC_OK:        return "ok";
        case WC_ERR_FULL:  return "full";
        case WC_ERR_EMPTY: return "empty";
        default:           return "unknown";
    }
}

/* Defined in wc_errno.c:
 *   _Thread_local wc_err wc_errno = WC_OK;
 */
extern _Thread_local wc_err wc_errno;

/* Print last error — same pattern as perror(3).
 *   wc_perror("arena_alloc");  ->  "arena_alloc: full"
 */
static inline void wc_perror(const char* prefix)
{
    if (prefix && prefix[0]) {
        fprintf(stderr, "%s: %s\n", prefix, wc_strerror(wc_errno));
    } else {
        fprintf(stderr, "%s\n", wc_strerror(wc_errno));
    }
}


/* Internal macros (library use only)
 * ------------------------------------
 * WC_SET_RET — replaces CHECK_WARN_RET at expected-condition sites.
 * Sets wc_errno silently and returns. No print.
 *
 *   WC_SET_RET(WC_ERR_EMPTY, vec->size == 0, );     void return
 *   WC_SET_RET(WC_ERR_FULL,  cond,           NULL); pointer return
 */
#define WC_SET_RET(err_code, cond, ret) \
    do {                                \
        if (cond) {                     \
            wc_errno = (err_code);      \
            return ret;                 \
        }                               \
    } while (0)

/* WC_PROPAGATE_RET — exit immediately if a callee already set wc_errno.
 *
 *   some_internal_fn(vec);
 *   WC_PROPAGATE_RET( );   // exits if some_internal_fn set wc_errno
 */
#define WC_PROPAGATE_RET(ret)    \
    do {                         \
        if (wc_errno != WC_OK) { \
            return ret;          \
        }                        \
    } while (0)

#endif // WC_ERRNO_H

