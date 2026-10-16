#ifndef WC_COMP_H
#define WC_COMP_H

/*
 * wc_comp.h — compiler extensions, unified
 *
 * Every non-standard construct the kit relies on (attributes, typeof, inline
 * hints, overflow/expect builtins) is wrapped here.
 *
 * Attribute placement standard:
 *   [attributes] [storage / linkage] [inlining] [return_type] func_name(params);
 * All wc_* attributes are placed in PREFIX position (before return type)
 * for full compatibility with GCC, Clang, clang-cl, and MSVC.
 */

#if defined(__clang__) || defined(__GNUC__)
    #define wc_likely(x)       __builtin_expect(!!(x), 1)
    #define wc_unlikely(x)     __builtin_expect(!!(x), 0)
    #define wc_nonull(...)     __attribute__((nonnull(__VA_ARGS__)))
    #define wc_nodiscard       __attribute__((warn_unused_result))
    #define wc_always_inline   inline __attribute__((always_inline))
    #define wc_cold            __attribute__((cold))
    #define wc_noinline        __attribute__((noinline))
    #define wc_noreturn        __attribute__((noreturn))
    #define wc_format(k, n, v) __attribute__((format(k, n, v)))
    #define wc_typeof(expr)    __typeof__(expr)
    #define wc_mul_overflow(a, b, res) (__builtin_mul_overflow((a), (b), (res)))
    /* u64 trailing zeros (x must be nonzero). */
    #define wc_ctz(x)          __builtin_ctzll(x)
    /* u64 leading zeros (x must be nonzero). */
    #define wc_clz(x)          __builtin_clzll(x)
    /* u64 population count (number of 1 bits). */
    #define wc_popcount(x)     __builtin_popcountll(x)
    #define wc_unreachable()   __builtin_unreachable()

    static wc_always_inline void wc_mum64(unsigned long long* a, unsigned long long* b)
    {
        __uint128_t r = (__uint128_t)*a * *b;
        *a            = (unsigned long long)r;
        *b            = (unsigned long long)(r >> 64);
    }
#else
    #if defined(_MSC_VER)
        #include <intrin.h>
        #include <sal.h>
        #define wc_likely(x)       (!!(x))
        #define wc_unlikely(x)     (x)
        #define wc_nonull(...)
        #define wc_nodiscard       _Check_return_
        #define wc_always_inline   __forceinline
        #define wc_cold
        #define wc_noinline        __declspec(noinline)
        #define wc_noreturn        __declspec(noreturn)
        #define wc_format(k, n, v)
        #define wc_typeof(expr)    __typeof__(expr)
        #define wc_unreachable()   __assume(0)

        #define wc_mul_overflow(a, b, res)                                              \
            ((res) = (unsigned long long)(a) * (unsigned long long)(b),                 \
             (unsigned long long)(a) != 0ull &&                                         \
                 (res) / (unsigned long long)(a) != (unsigned long long)(b))

        static inline unsigned wc_ctz(unsigned long long x)
        {
            unsigned long i;
            _BitScanForward64(&i, x);
            return (unsigned)i;
        }

        static inline unsigned wc_clz(unsigned long long x)
        {
            unsigned long i;
            _BitScanReverse64(&i, x);
            return (unsigned)(63 - i);
        }

        static inline unsigned wc_popcount(unsigned long long x)
        {
            return (unsigned)__popcnt64(x);
        }

        static inline void wc_mum64(unsigned long long* a, unsigned long long* b)
        {
        #if defined(_M_X64) || defined(_M_AMD64)
            unsigned __int64 hi;
            *a = _umul128(*a, *b, &hi);
            *b = hi;
        #else
            unsigned long long u1 = (unsigned long)(*a);
            unsigned long long v1 = (unsigned long)(*b);
            unsigned long long t = u1 * v1;
            unsigned long long w3 = t & 0xFFFFFFFFull;
            unsigned long long k = t >> 32;
            *a = (*a >> 32);
            t = (*a) * v1 + k;
            unsigned long long k1 = t & 0xFFFFFFFFull;
            unsigned long long w1 = t >> 32;
            *b = (*b >> 32);
            t = u1 * (*b) + k1;
            k = t >> 32;
            *a = (t << 32) + w3;
            *b = (*a) * (*b) + w1 + k;
        #endif
        }
    #else
        #define wc_likely(x)       (!!(x))
        #define wc_unlikely(x)     (x)
        #define wc_nonull(...)
        #define wc_nodiscard
        #define wc_always_inline   inline
        #define wc_cold
        #define wc_noinline
        #define wc_noreturn
        #define wc_format(k, n, v)
        #define wc_typeof(expr)    __typeof__(expr)
        #define wc_unreachable()   ((void)0)
        #define wc_mul_overflow(a, b, res)                                              \
            ((res) = (unsigned long long)(a) * (unsigned long long)(b),                 \
             (unsigned long long)(a) != 0ull &&                                         \
                 (res) / (unsigned long long)(a) != (unsigned long long)(b))

        static inline unsigned wc_ctz(unsigned long long x)
        {
            unsigned long long t = x;
            unsigned c = 0;
            if (!(t & 0xFFFFFFFFull)) { c += 32; t >>= 32; }
            if (!(t & 0xFFFFull))     { c += 16; t >>= 16; }
            if (!(t & 0xFFull))       { c += 8;  t >>= 8; }
            if (!(t & 0xFull))        { c += 4;  t >>= 4; }
            if (!(t & 0x3ull))        { c += 2;  t >>= 2; }
            if (!(t & 0x1ull))        { c += 1; }
            return c;
        }

        static inline unsigned wc_clz(unsigned long long x)
        {
            unsigned long long t = x;
            unsigned c = 0;
            if (!(t & 0xFFFFFFFF00000000ull)) { c += 32; t <<= 32; }
            if (!(t & 0xFFFF000000000000ull)) { c += 16; t <<= 16; }
            if (!(t & 0xFF00000000000000ull)) { c += 8;  t <<= 8; }
            if (!(t & 0xF000000000000000ull)) { c += 4;  t <<= 4; }
            if (!(t & 0xC000000000000000ull)) { c += 2;  t <<= 2; }
            if (!(t & 0x8000000000000000ull)) { c += 1; }
            return c;
        }

        static inline unsigned wc_popcount(unsigned long long x)
        {
            x = x - ((x >> 1) & 0x5555555555555555ull);
            x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
            x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Full;
            return (unsigned)((x * 0x0101010101010101ull) >> 56);
        }

        static inline void wc_mum64(unsigned long long* a, unsigned long long* b)
        {
            unsigned long long u0 = *a & 0xFFFFFFFFull;
            unsigned long long u1 = *a >> 32;
            unsigned long long v0 = *b & 0xFFFFFFFFull;
            unsigned long long v1 = *b >> 32;
            unsigned long long w0 = u0 * v0;
            unsigned long long t  = u1 * v0 + (w0 >> 32);
            unsigned long long w1 = t & 0xFFFFFFFFull;
            unsigned long long w2 = t >> 32;
            w1 += u0 * v1;
            *a = (w1 << 32) + (w0 & 0xFFFFFFFFull);
            *b = u1 * v1 + w2 + (w1 >> 32);
        }
    #endif
#endif

#endif // WC_COMP_H
