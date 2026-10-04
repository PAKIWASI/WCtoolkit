#ifndef BENCH_SUPPORT_H
#define BENCH_SUPPORT_H

// Shared by every benchmark file.
// Each benchmark iteration works on N elements, so results read as "per N".
#ifndef BENCH_N
#define BENCH_N 100000
#endif
#define N BENCH_N

// A string long enough to live on the heap (String keeps 31 chars inline).
#define LONG_STR "a string that is long enough to need the heap"

#endif // BENCH_SUPPORT_H
