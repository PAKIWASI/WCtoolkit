#include "wc_errno.h"
#include "common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>



/* One definition of the thread-local error variable.
 * Every translation unit that includes wc_error.h sees the extern declaration.
 * This file provides the actual storage.
 */
_Thread_local wc_err wc_errno = WC_OK;


/* Diagnostics reporters (common.h). Defined once, out of line and `cold`:
 * the compiler places them, and every branch that calls them, away from hot
 * code. Both write to stderr so diagnostics never mix into program output. */

// NOLINTBEGIN(clang-analyzer-valist.Uninitialized): false positive: va_start precedes vfprintf
void wc_fatal_report(const char* file, int line, const char* func, const char* fmt, ...)
{
    fflush(stdout); // keep ordering with anything already printed
    fprintf(stderr, WC_COLOR_RED "[FATAL] %s:%d:%s(): ", file, line, func);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n" WC_COLOR_RESET);
    exit(EXIT_FAILURE);
}
// NOLINTEND(clang-analyzer-valist.Uninitialized)

// NOLINTBEGIN(clang-analyzer-valist.Uninitialized): as above
void wc_warn_report(const char* file, int line, const char* func, const char* fmt, ...)
{
    fflush(stdout);
    fprintf(stderr, WC_COLOR_YELLOW "[WARN] %s:%d:%s(): ", file, line, func);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n" WC_COLOR_RESET);
}
// NOLINTEND(clang-analyzer-valist.Uninitialized)
