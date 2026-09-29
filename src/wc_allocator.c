#include "wc_allocator.h"

_Thread_local wc_allocator wc_default_allocator = WC_DEFAULT_ALLOCATOR;

const wc_allocator wc_libc_allocator = { 0 };
