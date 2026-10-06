#ifndef WC_HASH_H
#define WC_HASH_H

#include "common.h"
#include <string.h>


// Hash `size` bytes at `key`
typedef u64 (*wc_hash_fn)(const void* key, u64 size);

/*
====================HASH FUNCTIONS====================
*/

/* wc_hash: rapidhash V3, "nano" variant (the one tuned for short inputs).
 *
 * Based on wyhash by Wang Yi. Source: https://github.com/Nicoshev/rapidhash
 *
 * Copyright (C) 2025 Nicolas De Carli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Changes from upstream: C only, default seed and secrets folded in, reads in
 * native byte order (hash values are for in-memory tables, never stored or
 * sent, so big-endian hosts need no byte swap). Output matches upstream
 * rapidhashNano() on little-endian hosts; tests/test_hashmap.c checks it.
 */

// 64 x 64 -> 128 bit multiply. *a gets the low half, *b the high half.
static inline __attribute__((always_inline)) void wc_rapid_mum(u64* a, u64* b)
{
    __uint128_t r = (__uint128_t)*a * *b;
    *a            = (u64)r;
    *b            = (u64)(r >> 64);
}

// Multiply, then fold the 128-bit product to 64 bits.
static inline __attribute__((always_inline)) u64 wc_rapid_mix(u64 a, u64 b)
{
    wc_rapid_mum(&a, &b);
    return a ^ b;
}

static inline __attribute__((always_inline)) u64 wc_rapid_read64(const u8* p)
{
    u64 v;
    memcpy(&v, p, 8);
    return v;
}

static inline __attribute__((always_inline)) u64 wc_rapid_read32(const u8* p)
{
    u32 v;
    memcpy(&v, p, 4);
    return v;
}

#define WC_RAPID_S0   0x2d358dccaa6c78a5ULL
#define WC_RAPID_S1   0x8bb84b93962eacc9ULL
#define WC_RAPID_S2   0x4b33a62ed433d4a3ULL
#define WC_RAPID_S7   0xaaaaaaaaaaaaaaaaULL
#define WC_RAPID_SEED 0ULL

// Hash `len` bytes. With a constant len (keys of a known size) every length
// branch folds away: a 4 or 8 byte key costs two 128-bit multiplies.
static inline __attribute__((always_inline)) u64 wc_hash(const void* key, u64 len)
{
    const u8* p    = key;
    u64       seed = WC_RAPID_SEED ^ wc_rapid_mix(WC_RAPID_SEED ^ WC_RAPID_S2, WC_RAPID_S1);
    u64       a    = 0;
    u64       b    = 0;
    u64       i    = len;

    if (WC_LIKELY(len <= 16)) {
        if (len >= 4) {
            seed ^= len;
            if (len >= 8) {
                a = wc_rapid_read64(p);
                b = wc_rapid_read64(p + len - 8);
            } else {
                a = wc_rapid_read32(p);
                b = wc_rapid_read32(p + len - 4);
            }
        } else if (len > 0) {
            a = ((u64)p[0] << 45) | p[len - 1];
            b = p[len >> 1];
        }
    } else {
        if (i > 48) {
            u64 see1 = seed;
            u64 see2 = seed;
            do {
                seed = wc_rapid_mix(wc_rapid_read64(p) ^ WC_RAPID_S0, wc_rapid_read64(p + 8) ^ seed);
                see1 = wc_rapid_mix(wc_rapid_read64(p + 16) ^ WC_RAPID_S1, wc_rapid_read64(p + 24) ^ see1);
                see2 = wc_rapid_mix(wc_rapid_read64(p + 32) ^ WC_RAPID_S2, wc_rapid_read64(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (i > 48);
            seed ^= see1;
            seed ^= see2;
        }
        if (i > 16) {
            seed = wc_rapid_mix(wc_rapid_read64(p) ^ WC_RAPID_S2, wc_rapid_read64(p + 8) ^ seed);
            if (i > 32) {
                seed = wc_rapid_mix(wc_rapid_read64(p + 16) ^ WC_RAPID_S2, wc_rapid_read64(p + 24) ^ seed);
            }
        }
        a = wc_rapid_read64(p + i - 16) ^ i;
        b = wc_rapid_read64(p + i - 8);
    }

    a ^= WC_RAPID_S1;
    b ^= seed;
    wc_rapid_mum(&a, &b);
    return wc_rapid_mix(a ^ WC_RAPID_S7, b ^ WC_RAPID_S1 ^ i);
}


#endif // WC_HASH_H
