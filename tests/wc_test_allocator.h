#ifndef WC_TEST_ALLOCATOR_H
#define WC_TEST_ALLOCATOR_H

/*
 * wc_test_allocator: a checking allocator for tests (not part of the library).
 *
 * Wraps a backing allocator and records every live block with its size and
 * alignment. Every free/realloc is checked against that record.
 *
 * Detects (each is an "error"):
 *   - double free / realloc of a freed block
 *   - foreign pointer (never returned by this allocator)
 *   - size or align at free/realloc differs from the block's last alloc/realloc
 *   - zero-size alloc, non-power-of-two align
 *   - backend returned a block that is not aligned to `align` (address check)
 * Also provides:
 *   - alloc / realloc / free counters, live blocks, live and peak bytes
 *   - fail-on-Nth allocation (alloc and realloc both count), returns NULL
 *   - fill new bytes with 0xBE, poison freed bytes with 0xDD
 *   - leak report at destroy
 *
 * Error mode:
 *   WC_TA_ABORT  (default) print the error and abort(): a contract violation
 *                in library code must stop the test run immediately.
 *   WC_TA_RECORD count the error and continue: used to test the checker itself.
 *
 * Usage:
 *   wc_test_alloc ta;
 *   wc_test_alloc_init(&ta, WC_LIBC);               // or any backing allocator
 *   GenVec v = GenVec_create(wc_test_alloc_allocator(&ta), 4, sizeof(int), NULL);
 *   ...
 *   GenVec_destroy(&v);
 *   assert(wc_test_alloc_destroy(&ta) == 0);         // leak report
 */

#include "common.h"
#include "wc_allocator.h"
#include <stddef.h>

#define WC_TA_FILL   0xBE
#define WC_TA_POISON 0xDD

typedef enum {
    WC_TA_ABORT  = 0,
    WC_TA_RECORD = 1,
} wc_ta_error_mode;

typedef struct {
    void*  ptr; // NULL = empty slot, WC_TA_TOMBSTONE = deleted
    size_t size;
    size_t align;
} wc_ta_block;

typedef struct {
    const wc_allocator* backing; // where memory really comes from
    wc_allocator        self;    // this checker as an allocator: {vt, this}

    // live-block table (open addressing, libc-backed, never uses `backing`)
    wc_ta_block* blocks;
    u64          cap;
    u64          used; // live + tombstones
    u64          live_blocks;

    // stats
    u64 n_alloc;    // successful allocs
    u64 n_realloc;  // successful reallocs
    u64 n_free;     // successful frees
    u64 n_attempts; // alloc + realloc attempts (drives fail-Nth)
    u64 live_bytes;
    u64 peak_bytes;

    // fault injection: fail the Nth attempt (1-based). 0 = never.
    u64 fail_at;
    bool  fail_sticky; // 1: every attempt >= fail_at fails

    // errors
    wc_ta_error_mode mode;
    u64              n_errors;
    char             last_error[160];
} wc_test_alloc;


// backing: WC_LIBC, Arena_allocator(&a), ...
void wc_test_alloc_init(wc_test_alloc* ta, const wc_allocator* backing);

// Returns the number of leaked blocks (0 = clean) and prints a report of each.
// Releases leaked blocks through the backing allocator so ASAN stays quiet.
u64 wc_test_alloc_destroy(wc_test_alloc* ta);

// Points into `ta`: valid until wc_test_alloc_destroy.
static inline const wc_allocator* wc_test_alloc_allocator(wc_test_alloc* ta)
{
    return &ta->self;
}

static inline void wc_test_alloc_fail_at(wc_test_alloc* ta, u64 n, bool sticky)
{
    ta->fail_at     = n;
    ta->fail_sticky = sticky;
}

// Is `p` a live block of this allocator?
bool wc_test_alloc_owns(const wc_test_alloc* ta, const void* p);

// The three callbacks (exposed so tests can build custom vtables around them)
void* wc_test_alloc_cb_alloc(void* ctx, size_t size, size_t align);
void* wc_test_alloc_cb_realloc(void* ctx, void* p, size_t old_size, size_t new_size, size_t align);
void  wc_test_alloc_cb_free(void* ctx, void* p, size_t size, size_t align);

#endif // WC_TEST_ALLOCATOR_H
