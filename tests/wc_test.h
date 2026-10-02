#ifndef WC_TEST_H
#define WC_TEST_H

/*
 * wc_test.h — Minimal test framework for WCtoolkit
 * =================================================
 *
 * USAGE
 * -----
 *   // In a test file (e.g. tests/String_test.c):
 *   #include "wc_test.h"
 *   #include "String.h"
 *
 *   static void test_append(void) {
 *       String* s = String_from_cstr("hello");
 *       String_append_cstr(s, " world");
 *       WC_EXPECT_EQ_INT(String_len(s), 11);
 *       WC_EXPECT(String_equals_cstr(s, "hello world"));
 *       String_destroy(s);
 *   }
 *
 *   void String_suite(void) {
 *       WC_SUITE("String");
 *       WC_RUN(test_append);
 *   }
 *
 *   // In tests/test_main.c:
 *   void String_suite(void);
 *   int main(void) {
 *       String_suite();
 *       WC_REPORT();
 *   }
 *
 * EXPECT MACROS
 * -------------
 * Non-fatal: a failure is counted and the test keeps running. (WC_ASSERT is
 * the library's debug-only aborting invariant check; see common.h.)
 *   WC_EXPECT(cond)             — generic condition
 *   WC_EXPECT_EQ_INT(a, b)      — integer equality, prints values on fail
 *   WC_EXPECT_EQ_U64(a, b)      — u64 equality
 *   WC_EXPECT_EQ_STR(a, b)      — cstr equality (strcmp)
 *   WC_EXPECT_NULL(p)           — pointer is NULL
 *   WC_EXPECT_NOT_NULL(p)       — pointer is not NULL
 *   WC_EXPECT_TRUE(cond)        — alias for WC_EXPECT
 *   WC_EXPECT_FALSE(cond)       — expects condition is false
 *
 * RULES
 * -----
 *   - All asserts accumulate: a failed assert does NOT stop the test.
 *     This lets you see all failures in one run.
 *   - Each test function is void and takes no args.
 *   - WC_REPORT() returns 0 on success, 1 on any failure.
 *     Use it as your main() return value so ctest / CI sees failures.
 */

#include <stdio.h>
#include <string.h>


#ifdef WC_TEST_MAIN
// Defined ex _MAINonce, in the file that #define WC_TEST_MAIN
int wc_total       = 0;
int wc_passed      = 0;
int wc_failed      = 0;
int wc_test_failed = 0;
#else
// All other translation units: just extern declarations
extern int wc_total;
extern int wc_passed;
extern int wc_failed;
extern int wc_test_failed;
#endif

// ANSI colours (same palette as common.h)
#define WC_RED    "\033[1;31m"
#define WC_GREEN  "\033[1;32m"
#define WC_YELLOW "\033[1;33m"
#define WC_CYAN   "\033[1;36m"
#define WC_RESET  "\033[0m"


// Core assert 

// All asserts funnel through this so failure tracking is in one place
static inline void wc_expect_failed(const char* msg, const char* file, int line)
{
    fprintf(stderr, "    " WC_RED "FAIL" WC_RESET " %s\n         at %s:%d\n", msg, file, line);
    wc_test_failed++;
}

#define WC_EXPECT_CORE(cond, msg) ((void)((cond) || (wc_expect_failed((msg), __FILE__, __LINE__), 0)))


// Public assert macros

#define WC_EXPECT(cond) WC_EXPECT_CORE((cond), #cond)

#define WC_EXPECT_TRUE(cond) WC_EXPECT_CORE((cond), #cond " is true")

#define WC_EXPECT_FALSE(cond) WC_EXPECT_CORE(!(cond), #cond " is false")

#define WC_EXPECT_NULL(p) WC_EXPECT_CORE((p) == NULL, #p " == NULL")

#define WC_EXPECT_NOT_NULL(p) WC_EXPECT_CORE((p) != NULL, #p " != NULL")

// Integer — prints actual vs expected on failure
#define WC_EXPECT_EQ_INT(a, b)                                                                   \
    ({                                                                                           \
        long long _a = (long long)(a);                                                           \
        long long _b = (long long)(b);                                                           \
        if (_a != _b) {                                                                          \
            char _msg[256];                                                                      \
            snprintf(_msg, sizeof(_msg), "%s == %s  (got %lld, expected %lld)", #a, #b, _a, _b); \
            WC_EXPECT_CORE(0, _msg);                                                             \
        }                                                                                        \
    })

#define WC_EXPECT_NEQ_INT(a, b)                                                    \
    ({                                                                             \
        long long _a = (long long)(a);                                             \
        long long _b = (long long)(b);                                             \
        if (_a == _b) {                                                            \
            char _msg[256];                                                        \
            snprintf(_msg, sizeof(_msg), "%s != %s  (both are %lld)", #a, #b, _a); \
            (void)_b;                                                              \
            WC_EXPECT_CORE(0, _msg);                                               \
        }                                                                          \
    })

#define WC_EXPECT_EQ_U64(a, b)                                                                   \
    ({                                                                                           \
        unsigned long long _a = (unsigned long long)(a);                                         \
        unsigned long long _b = (unsigned long long)(b);                                         \
        if (_a != _b) {                                                                          \
            char _msg[256];                                                                      \
            snprintf(_msg, sizeof(_msg), "%s == %s  (got %llu, expected %llu)", #a, #b, _a, _b); \
            WC_EXPECT_CORE(0, _msg);                                                             \
        }                                                                                        \
    })

// C-String equality
#define WC_EXPECT_EQ_STR(a, b)                                                                       \
    ({                                                                                               \
        const char* _a = (const char*)(a);                                                           \
        const char* _b = (const char*)(b);                                                           \
        if (strcmp(_a, _b) != 0) {                                                                   \
            char _msg[256];                                                                          \
            snprintf(_msg, sizeof(_msg), "%s == %s  (got \"%s\", expected \"%s\")", #a, #b, _a, _b); \
            WC_EXPECT_CORE(0, _msg);                                                                 \
        }                                                                                            \
    })


// Suite and runner macros 

// Print a suite header
#define WC_SUITE(name) printf("\n" WC_CYAN "══ %s ══" WC_RESET "\n", (name))

/*
 * WC_RUN(fn)
 * Run a single test function. Tracks pass/fail per test, not per assert,
 * so you see "test_foo ... FAIL (2 assertion(s))" rather than a wall of lines.
 */
static inline void wc_run(void (*fn)(void), const char* name, int expect_fail)
{
    wc_test_failed = 0;
    wc_total++;
    printf("  %-48s", name);
    fflush(stdout);
    fn();
    if (!expect_fail) {
        if (wc_test_failed == 0) {
            printf(WC_GREEN "OK" WC_RESET "\n");
            wc_passed++;
        } else {
            printf(WC_RED "FAIL" WC_RESET " (%d assertion(s))\n", wc_test_failed);
            wc_failed++;
        }
    } else if (wc_test_failed != 0) {
        printf(WC_YELLOW "XFAIL" WC_RESET " (known defect, %d assertion(s))\n", wc_test_failed);
        wc_passed++;
    } else {
        printf(WC_RED "XPASS" WC_RESET " (defect fixed: switch to WC_RUN)\n");
        wc_failed++;
    }
}

#define WC_RUN(fn) wc_run((fn), #fn, 0)

/*
 * WC_RUN_XFAIL(fn)
 * Run a test that is EXPECTED to fail (a known defect with a regression test
 * written before the fix). A failure prints XFAIL and counts as passed.
 * A pass prints XPASS and counts as FAILED: the defect is fixed, so switch
 * the call to WC_RUN so the test guards the fix from now on.
 * Assertion output is still printed so the failure mode stays visible.
 */
#define WC_RUN_XFAIL(fn) wc_run((fn), #fn, 1)

/*
 * WC_REPORT()
 * Print summary and return exit code.
 * Put this as the last statement in main():  return WC_REPORT();
 */
static inline int wc_report(void)
{
    printf("\n");
    if (wc_failed == 0) {
        printf(WC_GREEN "All %d tests passed." WC_RESET "\n", wc_total);
    } else {
        printf(WC_RED "%d/%d tests FAILED." WC_RESET "  (%d passed)\n", wc_failed, wc_total, wc_passed);
    }
    return (wc_failed > 0) ? 1 : 0;
}
#define WC_REPORT() wc_report()


#endif // WC_TEST_H
