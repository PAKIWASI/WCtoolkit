#ifndef WC_COMP_H
#define WC_COMP_H

/*
 * wc_comp.h — compiler extensions, unified
 *
 * Every non-standard construct the kit relies on (attributes, typeof, inline
 * hints, overflow/expect builtins) is wrapped here.
 *
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
    /* res = a * b, evaluates to nonzero on overflow (division check). */
    #define wc_mul_overflow(a, b, res)                                              \
        ((res) = (unsigned long long)(a) * (unsigned long long)(b),                 \
         (unsigned long long)(a) != 0ull &&                                         \
             (res) / (unsigned long long)(a) != (unsigned long long)(b))
    /* u64 trailing zeros (x must be nonzero). */
    static inline unsigned wc_ctz(unsigned long long x)
    {
        unsigned long i;
        _BitScanForward64(&i, x);
        return (unsigned)i;
    }
#endif

#endif // WC_COMP_H
