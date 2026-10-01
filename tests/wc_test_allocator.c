#include "wc_test_allocator.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WC_TA_TOMBSTONE ((void*)(uintptr_t)1)
#define WC_TA_INIT_CAP  64


// Error reporting

__attribute__((format(printf, 2, 3))) static void ta_error(wc_test_alloc* ta, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(ta->last_error, sizeof(ta->last_error), fmt, args);
    va_end(args);

    ta->n_errors++;
    if (ta->mode == WC_TA_ABORT) {
        fprintf(stderr, WC_COLOR_RED "[wc_test_alloc] %s" WC_COLOR_RESET "\n", ta->last_error);
        abort();
    }
}


// Block table (libc-backed on purpose: must never recurse into `backing`)

static inline u64 ta_hash(const void* p)
{
    u64 x = (u64)(uintptr_t)p;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    return x;
}

static wc_ta_block* ta_find(const wc_test_alloc* ta, const void* p)
{
    u64 mask = ta->cap - 1;
    for (u64 i = ta_hash(p) & mask, n = 0; n < ta->cap; i = (i + 1) & mask, n++) {
        wc_ta_block* b = &ta->blocks[i];
        if (b->ptr == NULL) {
            return NULL;
        }
        if (b->ptr == p) {
            return b;
        }
    }
    return NULL;
}

static void ta_insert_raw(wc_ta_block* blocks, u64 cap, wc_ta_block blk)
{
    u64 mask = cap - 1;
    for (u64 i = ta_hash(blk.ptr) & mask;; i = (i + 1) & mask) {
        if (blocks[i].ptr == NULL || blocks[i].ptr == WC_TA_TOMBSTONE) {
            blocks[i] = blk;
            return;
        }
    }
}

static void ta_rehash(wc_test_alloc* ta, u64 new_cap)
{
    wc_ta_block* nb = calloc(new_cap, sizeof(wc_ta_block));
    if (!nb) {
        FATAL("wc_test_alloc: table calloc failed");
    }
    for (u64 i = 0; i < ta->cap; i++) {
        void* p = ta->blocks[i].ptr;
        if (p != NULL && p != WC_TA_TOMBSTONE) {
            ta_insert_raw(nb, new_cap, ta->blocks[i]);
        }
    }
    free(ta->blocks);
    ta->blocks = nb;
    ta->cap    = new_cap;
    ta->used   = ta->live_blocks;
}

static void ta_track(wc_test_alloc* ta, void* p, size_t size, size_t align)
{
    if ((ta->used + 1) * 4 >= ta->cap * 3) { // load factor 0.75, tombstones included
        u64 want = ta->cap;
        while ((ta->live_blocks + 1) * 2 >= want) {
            want *= 2;
        }
        ta_rehash(ta, want);
    }
    ta_insert_raw(ta->blocks, ta->cap, (wc_ta_block){.ptr = p, .size = size, .align = align});
    ta->used++;
    ta->live_blocks++;

    ta->live_bytes += size;
    if (ta->live_bytes > ta->peak_bytes) {
        ta->peak_bytes = ta->live_bytes;
    }
}

static void ta_untrack(wc_test_alloc* ta, wc_ta_block* b)
{
    ta->live_bytes -= b->size;
    ta->live_blocks--;
    b->ptr   = WC_TA_TOMBSTONE; // keep `used` as-is: tombstone still occupies the probe chain
    b->size  = 0;
    b->align = 0;
}


// Backing

static inline void* ta_b_alloc(wc_test_alloc* ta, size_t size, size_t align)
{
    return wc_alloc(ta->backing, size, align);
}

static inline void ta_b_free(wc_test_alloc* ta, void* p, size_t size, size_t align)
{
    wc_free(ta->backing, p, size, align);
}


// Contract checks shared by alloc and realloc

static b8 ta_check_request(wc_test_alloc* ta, const char* op, size_t size, size_t align)
{
    if (size == 0) {
        ta_error(ta, "%s: size == 0 (wrappers must never pass 0 to a backend)", op);
        return 0;
    }
    if (align == 0 || (align & (align - 1)) != 0) {
        ta_error(ta, "%s: align %zu is not a power of two", op, align);
        return 0;
    }
    return 1;
}

static b8 ta_should_fail(wc_test_alloc* ta)
{
    ta->n_attempts++;
    if (ta->fail_at == 0) {
        return 0;
    }
    return ta->fail_sticky ? (ta->n_attempts >= ta->fail_at) : (ta->n_attempts == ta->fail_at);
}

static b8 ta_check_block(wc_test_alloc* ta, const char* op, void* p, wc_ta_block** out, size_t size, size_t align)
{
    wc_ta_block* b = ta_find(ta, p);
    if (!b) {
        // freed blocks leave a tombstone, but the pointer is gone from the table:
        // double free and foreign pointer look the same from here
        ta_error(ta, "%s: %p is not a live block (double free or foreign pointer)", op, p);
        return 0;
    }
    if (b->size != size) {
        ta_error(ta, "%s: %p size mismatch (block has %zu, caller passed %zu)", op, p, b->size, size);
        return 0;
    }
    if (b->align != align) {
        ta_error(ta, "%s: %p align mismatch (block has %zu, caller passed %zu)", op, p, b->align, align);
        return 0;
    }
    *out = b;
    return 1;
}


// Callbacks

void* wc_test_alloc_cb_alloc(void* ctx, size_t size, size_t align)
{
    wc_test_alloc* ta = ctx;
    if (!ta_check_request(ta, "alloc", size, align) || ta_should_fail(ta)) {
        return NULL;
    }

    void* p = ta_b_alloc(ta, size, align);
    if (!p) {
        return NULL;
    }
    if (((uintptr_t)p & (align - 1)) != 0) {
        ta_error(ta, "alloc: backend returned %p, not aligned to %zu", p, align);
    }

    memset(p, WC_TA_FILL, size);
    ta_track(ta, p, size, align);
    ta->n_alloc++;
    return p;
}

void* wc_test_alloc_cb_realloc(void* ctx, void* p, size_t old_size, size_t new_size, size_t align)
{
    wc_test_alloc* ta = ctx;
    wc_ta_block*   b  = NULL;

    if (!ta_check_block(ta, "realloc", p, &b, old_size, align)) {
        return NULL;
    }
    if (!ta_check_request(ta, "realloc", new_size, align) || ta_should_fail(ta)) {
        return NULL; // p stays valid and tracked
    }

    // Always move: alloc new, copy, poison + free old. Catches callers that
    // keep using the old pointer, which an in-place libc realloc would hide.
    void* q = ta_b_alloc(ta, new_size, align);
    if (!q) {
        return NULL;
    }
    if (((uintptr_t)q & (align - 1)) != 0) {
        ta_error(ta, "realloc: backend returned %p, not aligned to %zu", q, align);
    }

    size_t keep = old_size < new_size ? old_size : new_size;
    memcpy(q, p, keep);
    if (new_size > keep) {
        memset((u8*)q + keep, WC_TA_FILL, new_size - keep);
    }

    memset(p, WC_TA_POISON, old_size);
    ta_untrack(ta, b);
    ta_b_free(ta, p, old_size, align);

    ta_track(ta, q, new_size, align);
    ta->n_realloc++;
    return q;
}

void wc_test_alloc_cb_free(void* ctx, void* p, size_t size, size_t align)
{
    wc_test_alloc* ta = ctx;
    wc_ta_block*   b  = NULL;

    if (!ta_check_block(ta, "free", p, &b, size, align)) {
        return; // leave the memory alone: it is not ours or already gone
    }

    memset(p, WC_TA_POISON, size);
    ta_untrack(ta, b);
    ta_b_free(ta, p, size, align);
    ta->n_free++;
}


// Lifecycle

void wc_test_alloc_init(wc_test_alloc* ta, wc_allocator backing)
{
    memset(ta, 0, sizeof(*ta));
    ta->backing = backing;
    ta->cap     = WC_TA_INIT_CAP;
    ta->blocks  = calloc(ta->cap, sizeof(wc_ta_block));
    if (!ta->blocks) {
        FATAL("wc_test_alloc: table calloc failed");
    }
}

u64 wc_test_alloc_destroy(wc_test_alloc* ta)
{
    u64 leaks = ta->live_blocks;

    if (leaks) {
        fprintf(stderr, WC_COLOR_YELLOW "[wc_test_alloc] %llu leaked block(s), %llu byte(s):" WC_COLOR_RESET "\n",
                (unsigned long long)leaks, (unsigned long long)ta->live_bytes);
    }
    for (u64 i = 0; i < ta->cap; i++) {
        wc_ta_block* b = &ta->blocks[i];
        if (b->ptr != NULL && b->ptr != WC_TA_TOMBSTONE) {
            fprintf(stderr, "    leak: %p size=%zu align=%zu\n", b->ptr, b->size, b->align);
            ta_b_free(ta, b->ptr, b->size, b->align);
        }
    }

    free(ta->blocks);
    ta->blocks      = NULL;
    ta->cap         = 0;
    ta->used        = 0;
    ta->live_blocks = 0;
    return leaks;
}

static const wc_alloc_vtable wc_test_alloc_vt = {
    .alloc   = wc_test_alloc_cb_alloc,
    .realloc = wc_test_alloc_cb_realloc,
    .free    = wc_test_alloc_cb_free,
};

wc_allocator wc_test_alloc_allocator(wc_test_alloc* ta)
{
    return (wc_allocator){.vt = &wc_test_alloc_vt, .ctx = ta};
}

b8 wc_test_alloc_owns(const wc_test_alloc* ta, const void* p)
{
    return p != NULL && ta_find(ta, p) != NULL;
}
