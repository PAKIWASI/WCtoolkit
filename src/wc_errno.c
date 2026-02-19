#include "wc_errno.h"



/* One definition of the thread-local error variable.
 * Every translation unit that includes wc_error.h sees the extern declaration.
 * This file provides the actual storage.
 */
_Thread_local wc_err wc_errno = WC_OK;
