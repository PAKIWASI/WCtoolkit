// PRIVATE to the library sources. Never include from include/ or user code.
//
// Bans raw libc allocation in library code: every allocation must go through
// a wc_allocator (plan Phase 7, Definition of Done #1). The libc backend in
// wc_allocator.h is the single exception; it is defined before the poison.
//
// Include this LAST in every src/*.c. GCC/Clang poison also fires on system
// header declarations, so every system header a source file needs must come
// before it (the common ones are pulled in here).
//
// Poison does not see macros defined earlier (e.g. in include/), so the
// `no_raw_alloc` ctest greps src/ and include/ as the second layer.
#ifndef WC_POISON_H
#define WC_POISON_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc_allocator.h"

#pragma GCC poison malloc calloc realloc free aligned_alloc

#endif // WC_POISON_H
