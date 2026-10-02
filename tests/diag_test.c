// Diagnostics macro contract (common.h).
//
// The suite runs in every build type, and the #ifdef NDEBUG branches pin the
// release/debug split: run it in Debug AND Release to cover both sides.
//
//   all builds : FATAL, FATAL_IF, WARN, WARN_IF, WARN_IF_RET, LOG, LOG_IF
//   debug only : WC_ASSERT (condition not even evaluated under NDEBUG)

#include "common.h"
#include "wc_test.h"
#include "wc_test_fatal.h"

#include <stdio.h>
#include <unistd.h>

static int g_evals;

static int count(int v)
{
    g_evals++;
    return v;
}

// Silence stdout/stderr around calls that print on purpose.
static int quiet_begin(int* saved_out, int* saved_err)
{
    fflush(stdout);
    fflush(stderr);
    *saved_out = dup(STDOUT_FILENO);
    *saved_err = dup(STDERR_FILENO);
    FILE* null = freopen("/dev/null", "w", stdout);
    (void)null;
    dup2(STDOUT_FILENO, STDERR_FILENO);
    return 0;
}

static void quiet_end(int saved_out, int saved_err)
{
    fflush(stdout);
    fflush(stderr);
    dup2(saved_out, STDOUT_FILENO);
    dup2(saved_err, STDERR_FILENO);
    close(saved_out);
    close(saved_err);
}


/* ── survives every build ──────────────────────────────────────────── */

static void fatal_if_true(void) { FATAL_IF(count(1), "boom"); }

static void test_fatal_if_dies_in_every_build(void)
{
    WC_EXPECT_DIES(fatal_if_true);
}

static void test_fatal_if_false_evaluates_once_and_continues(void)
{
    g_evals = 0;
    FATAL_IF(count(0), "never");
    WC_EXPECT_EQ_INT(g_evals, 1);
}

static void test_warn_if_and_log_if_evaluate_once_and_continue(void)
{
    int so, se;
    quiet_begin(&so, &se);
    g_evals = 0;
    WARN_IF(count(1), "printed");
    WARN_IF(count(0), "not printed");
    LOG_IF(count(1), "printed");
    LOG_IF(count(0), "not printed");
    LOG("unconditional %d", 42);
    WARN("unconditional %d", 42);
    quiet_end(so, se);
    WC_EXPECT_EQ_INT(g_evals, 4); // each condition exactly once, in every build
}

static int warn_ret_helper(int fail)
{
    WARN_IF_RET(fail, -1, "returning early");
    return 7;
}

static void test_warn_if_ret_returns_from_caller(void)
{
    int so, se;
    quiet_begin(&so, &se);
    int r1 = warn_ret_helper(1);
    int r2 = warn_ret_helper(0);
    quiet_end(so, se);
    WC_EXPECT_EQ_INT(r1, -1);
    WC_EXPECT_EQ_INT(r2, 7);
}


/* ── stripped under NDEBUG ─────────────────────────────────────────── */

static void assert_false(void) { WC_ASSERT(count(0), "invariant broken"); }

static void test_assert_true_passes(void)
{
    g_evals = 0;
    WC_ASSERT(count(1) == 1, "never fires");
#ifdef NDEBUG
    WC_EXPECT_EQ_INT(g_evals, 0); // release: not evaluated, side effects do not run
#else
    WC_EXPECT_EQ_INT(g_evals, 1); // debug: evaluated exactly once
#endif
}

static void test_assert_false(void)
{
#ifdef NDEBUG
    g_evals = 0;
    assert_false(); // compiled out: must NOT die, must NOT evaluate
    WC_EXPECT_EQ_INT(g_evals, 0);
#else
    WC_EXPECT_DIES(assert_false);
#endif
}


/* ── every macro is an expression ──────────────────────────────────── */

static void test_macros_are_expressions(void)
{
    int so, se;
    quiet_begin(&so, &se);
    int x = 3;
    // comma operator, ternary arms, and a statement-expression value
    int y = (WC_ASSERT(x == 3, "x"), FATAL_IF(x != 3, "x"), WARN_IF(0, "w"), LOG_IF(0, "l"), x + 1);
    x > 0 ? WC_ASSERT(x > 0, "pos") : FATAL_IF(1, "neg");
    int z = ({ WC_ASSERT(y == 4, "y"); y * 2; });
    // unbraced if/else: no dangling-else or stray-semicolon issues
    if (z == 8)
        WARN_IF(0, "no");
    else
        FATAL_IF(1, "unreachable");
    quiet_end(so, se);
    WC_EXPECT_EQ_INT(y, 4);
    WC_EXPECT_EQ_INT(z, 8);
}


void diag_suite(void);
void diag_suite(void)
{
    WC_SUITE("Diagnostics macros");
    WC_RUN(test_fatal_if_dies_in_every_build);
    WC_RUN(test_fatal_if_false_evaluates_once_and_continues);
    WC_RUN(test_warn_if_and_log_if_evaluate_once_and_continue);
    WC_RUN(test_warn_if_ret_returns_from_caller);
    WC_RUN(test_assert_true_passes);
    WC_RUN(test_assert_false);
    WC_RUN(test_macros_are_expressions);
}
