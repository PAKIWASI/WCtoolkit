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

// The installed fatal handler; NULL means the default (print + exit).
static wc_fatal_fn g_fatal_handler = NULL;

wc_fatal_fn wc_set_fatal_handler(wc_fatal_fn fn)
{
    wc_fatal_fn prev = g_fatal_handler;
    g_fatal_handler  = fn;
    return prev;
}

void wc_fatal_report(const char* file, int line, const char* func, const char* fmt, ...)
{
    fflush(stdout); // keep ordering with anything already printed

    char    msg[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (g_fatal_handler) {
        g_fatal_handler(file, line, func, msg); // must not return
    } else {
        fprintf(stderr, WC_COLOR_RED "[FATAL] %s:%d:%s(): %s\n" WC_COLOR_RESET, file, line, func, msg);
    }
    exit(EXIT_FAILURE); // default, and the backstop for a handler that returns
}

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
