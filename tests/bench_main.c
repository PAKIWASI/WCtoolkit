// The benchmark binary: every tests/bench_*.c registers itself via UBENCH().
// Build in Release for meaningful numbers. Run with --help for options,
// e.g. --filter=hashmap.* or --output=results.csv.
//
// ubench.h  https://github.com/sheredom/ubench.h  (public domain), vendored as-is.
#include "ubench.h"

UBENCH_MAIN()
