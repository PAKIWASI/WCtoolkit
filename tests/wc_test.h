#ifndef WC_TEST_H
#define WC_TEST_H

/*
 * wc_test.h — Minimal test framework for WCtoolkit
 * =================================================
 *
 * USAGE
 * -----
 *   // In a test file (e.g. tests/string_test.c):
 *   #include "wc_test.h"
 *   #include "String.h"
 *
 *   static void test_append(void) {
 *       String* s = string_from_cstr("hello");
 *       string_append_cstr(s, " world");
 *       WC_ASSERT_EQ_INT(string_len(s), 11);
 *       WC_ASSERT(string_equals_cstr(s, "hello world"));
 *       string_destroy(s);
 *   }
 *
 *   void string_suite(void) {
 *       WC_SUITE("String");
 *       WC_RUN(test_append);
 *   }
 *
 *   // In tests/test_main.c:
 *   void string_suite(void);
 *   int main(void) {
 *       string_suite();
 *       WC_REPORT();
 *   }
 *
 * ASSERT MACROS
 * -------------
 *   WC_ASSERT(cond)              — generic condition
 *   WC_ASSERT_EQ_INT(a, b)      — integer equality, prints values on fail
 *   WC_ASSERT_EQ_U64(a, b)      — u64 equality
 *   WC_ASSERT_EQ_STR(a, b)      — cstr equality (strcmp)
 *   WC_ASSERT_NULL(p)            — pointer is NULL
 *   WC_ASSERT_NOT_NULL(p)        — pointer is not NULL
 *   WC_ASSERT_TRUE(cond)         — alias for WC_ASSERT
 *   WC_ASSERT_FALSE(cond)        — asserts condition is false
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


/* ── Internal state ──────────────────────────────────────────────────────── */

static int wc_total  = 0;
static int wc_passed = 0;
static int wc_failed = 0;

/* Per-test tracking: reset at the start of each WC_RUN */
static int wc_test_failed = 0;

/* ANSI colours (same palette as common.h) */
#define WC_RED    "\033[1;31m"
#define WC_GREEN  "\033[1;32m"
#define WC_YELLOW "\033[1;33m"
#define WC_CYAN   "\033[1;36m"
#define WC_RESET  "\033[0m"


/* ── Core assert ─────────────────────────────────────────────────────────── */

/* All asserts funnel through this so failure tracking is in one place */
#define WC_ASSERT_CORE(cond, msg)                         \
    do {                                                  \
        if (!(cond)) {                                    \
            fprintf(stderr,                               \
                    "    " WC_RED "FAIL" WC_RESET " %s\n" \
                    "         at %s:%d\n",                \
                    (msg), __FILE__, __LINE__);           \
            wc_test_failed++;                             \
        }                                                 \
    } while (0)


/* ── Public assert macros ────────────────────────────────────────────────── */

#define WC_ASSERT(cond) WC_ASSERT_CORE((cond), #cond)

#define WC_ASSERT_TRUE(cond) WC_ASSERT_CORE((cond), #cond " is true")

#define WC_ASSERT_FALSE(cond) WC_ASSERT_CORE(!(cond), #cond " is false")

#define WC_ASSERT_NULL(p) WC_ASSERT_CORE((p) == NULL, #p " == NULL")

#define WC_ASSERT_NOT_NULL(p) WC_ASSERT_CORE((p) != NULL, #p " != NULL")

/* Integer — prints actual vs expected on failure */
#define WC_ASSERT_EQ_INT(a, b)                                                                \
    do {                                                                                      \
        long long _a = (long long)(a);                                                        \
        long long _b = (long long)(b);                                                        \
        if (_a != _b) {                                                                       \
            char _msg[256];                                                                   \
            snprintf(_msg, sizeof(_msg), #a " == " #b "  (got %lld, expected %lld)", _a, _b); \
            WC_ASSERT_CORE(0, _msg);                                                          \
        }                                                                                     \
    } while (0)

#define WC_ASSERT_NEQ_INT(a, b)                                                 \
    do {                                                                        \
        long long _a = (long long)(a);                                          \
        long long _b = (long long)(b);                                          \
        if (_a == _b) {                                                         \
            char _msg[256];                                                     \
            snprintf(_msg, sizeof(_msg), #a " != " #b "  (both are %lld)", _a); \
            WC_ASSERT_CORE(0, _msg);                                            \
        }                                                                       \
    } while (0)

#define WC_ASSERT_EQ_U64(a, b)                                                                \
    do {                                                                                      \
        unsigned long long _a = (unsigned long long)(a);                                      \
        unsigned long long _b = (unsigned long long)(b);                                      \
        if (_a != _b) {                                                                       \
            char _msg[256];                                                                   \
            snprintf(_msg, sizeof(_msg), #a " == " #b "  (got %llu, expected %llu)", _a, _b); \
            WC_ASSERT_CORE(0, _msg);                                                          \
        }                                                                                     \
    } while (0)

/* C-string equality */
#define WC_ASSERT_EQ_STR(a, b)                                                                    \
    do {                                                                                          \
        const char* _a = (const char*)(a);                                                        \
        const char* _b = (const char*)(b);                                                        \
        if (strcmp(_a, _b) != 0) {                                                                \
            char _msg[256];                                                                       \
            snprintf(_msg, sizeof(_msg), #a " == " #b "  (got \"%s\", expected \"%s\")", _a, _b); \
            WC_ASSERT_CORE(0, _msg);                                                              \
        }                                                                                         \
    } while (0)


/* ── Suite and runner macros ─────────────────────────────────────────────── */

/* Print a suite header */
#define WC_SUITE(name) printf("\n" WC_CYAN "══ %s ══" WC_RESET "\n", (name))

/*
 * WC_RUN(fn)
 * Run a single test function. Tracks pass/fail per test, not per assert,
 * so you see "test_foo ... FAIL (2 assertion(s))" rather than a wall of lines.
 */
#define WC_RUN(fn)                                                                 \
    do {                                                                           \
        wc_test_failed = 0;                                                        \
        wc_total++;                                                                \
        printf("  %-48s", #fn);                                                    \
        fflush(stdout);                                                            \
        fn();                                                                      \
        if (wc_test_failed == 0) {                                                 \
            printf(WC_GREEN "OK" WC_RESET "\n");                                   \
            wc_passed++;                                                           \
        } else {                                                                   \
            printf(WC_RED "FAIL" WC_RESET " (%d assertion(s))\n", wc_test_failed); \
            wc_failed++;                                                           \
        }                                                                          \
    } while (0)

/*
 * WC_REPORT()
 * Print summary and return exit code.
 * Put this as the last statement in main():  return WC_REPORT();
 */
#define WC_REPORT()                                                                                          \
    ({                                                                                                       \
        printf("\n");                                                                                        \
        if (wc_failed == 0) {                                                                                \
            printf(WC_GREEN "All %d tests passed." WC_RESET "\n", wc_total);                                 \
        } else {                                                                                             \
            printf(WC_RED "%d/%d tests FAILED." WC_RESET "  (%d passed)\n", wc_failed, wc_total, wc_passed); \
        }                                                                                                    \
        (wc_failed > 0) ? 1 : 0;                                                                             \
    })


#endif /* WC_TEST_H */
