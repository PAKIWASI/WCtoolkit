#include "wc_allocator.h"

_Thread_local wc_allocator_t wc_default_allocator = WC_DEFAULT_ALLOCATOR;

const wc_allocator_t wc_libc_allocator = { 0 };
