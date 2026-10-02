#include "wc_errno.h"
#include "wc_poison.h" // must stay last: bans raw malloc/free below



/* One definition of the thread-local error variable.
 * Every translation unit that includes wc_error.h sees the extern declaration.
 * This file provides the actual storage.
 */
_Thread_local wc_err wc_errno = WC_OK;
