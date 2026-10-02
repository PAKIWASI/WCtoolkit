// examples/allocators.c
//
// Three allocator patterns, end to end:
//   1. Arena_create_buf: a container that lives entirely in a stack buffer.
//   2. ARENA_SCOPE: per-task scratch memory with nested containers, no destroys.
//   3. Escaping a scope: deep-copy the result out of the arena into libc.
//
// Build: cmake --build build --target example_allocators && ./build/example_allocators
// Exits non-zero if any result is wrong.

#include "arena.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_helpers.h"
#include "wc_macros.h"
#include "wc_string.h"

#include <stdio.h>
#include <string.h>

#define EXPECT(cond)                                                            \
    do {                                                                        \
        if (!(cond)) {                                                          \
            fprintf(stderr, "%s:%d: expected %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                           \
        }                                                                       \
    } while (0)


// 1. No heap at all: an Arena over a stack array backs a GenVec.
static int fixed_buffer(void)
{
    u8    buf[256];
    Arena arena;
    Arena_create_buf(&arena, buf, sizeof(buf)); // pinned: never copy `arena`

    GenVec squares = VEC_IN(Arena_allocator(&arena), int, 4);
    for (int i = 1; i <= 10; i++) {
        VEC_PUSH(&squares, i * i); // growth reallocs inside buf, in place when it's the last block
    }

    int sum = 0;
    VEC_FOREACH (&squares, int, x) {
        sum += *x;
    }
    printf("1. stack-buffer vector: %llu squares, sum %d, arena used %llu/%zu bytes\n",
           (unsigned long long)GenVec_size(&squares), sum, (unsigned long long)arena.idx, sizeof(buf));
    EXPECT(sum == 385);

    GenVec_destroy(&squares); // rewinds the arena (last block); optional here
    Arena_destroy(&arena);    // frees nothing: the arena doesn't own buf
    return 0;
}


// 2 + 3. Group words by length inside a scratch arena, then copy the result out.
//   groups: GenVec<GenVec<String>>, every level allocated from the same arena,
//   because nested containers and VEC_PUSH_CSTR follow the outer allocator.
static int scoped_then_escape(GenVec* out_libc)
{
    const char* words[] = {"arena", "vec", "map", "string", "scope", "copy", "move", "box"};
    enum { MAX_LEN = 8 };

    ARENA_SCOPE (tmp, nKB(16)) {
        GenVec groups = VEC_CX_IN(tmp, GenVec, MAX_LEN + 1, &wc_vec_ops);
        for (int len = 0; len <= MAX_LEN; len++) {
            GenVec bucket = VEC_CX_IN(tmp, String, 2, &wc_str_ops);
            VEC_PUSH_VEC(&groups, bucket); // moved in; `bucket` is now zeroed
        }

        for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
            GenVec* bucket = VEC_AT_MUT(&groups, GenVec, strlen(words[i]));
            VEC_PUSH_CSTR(bucket, words[i]); // String allocated from bucket->alloc == tmp
        }

        GenVec* threes = VEC_AT_MUT(&groups, GenVec, 3);
        EXPECT(GenVec_size(threes) == 3); // vec, map, box
        EXPECT(VEC_AT_MUT(threes, String, 0)->alloc.ctx == tmp.ctx);

        // Escape: deep copy every level into libc. Nothing in the copy points
        // into the arena, so it survives the scope.
        *out_libc = GenVec_copy(WC_LIBC, &groups);

        // No GenVec_destroy(&groups): the whole tree dies with `tmp`.
    }

    // The arena is gone here; the copy is intact and owns libc memory.
    GenVec* fives = VEC_AT_MUT(out_libc, GenVec, 5);
    printf("2. scratch arena: grouped %zu words by length, arena destroyed at scope exit\n",
           sizeof(words) / sizeof(words[0]));
    printf("3. escaped copy (libc): length 5 ->");
    VEC_FOREACH (fives, String, s) {
        printf(" %s", String_data_ptr(s));
    }
    putchar('\n');

    EXPECT(GenVec_size(fives) == 2); // arena, scope
    EXPECT(String_equals_cstr(VEC_AT_MUT(fives, String, 0), "arena"));
    EXPECT(fives->alloc.vt == NULL);                        // libc
    EXPECT(VEC_AT_MUT(fives, String, 1)->alloc.vt == NULL); // children follow the copy
    return 0;
}


int main(void)
{
    if (fixed_buffer() != 0) {
        return 1;
    }

    GenVec groups = {0};
    if (scoped_then_escape(&groups) != 0) {
        return 1;
    }
    GenVec_destroy(&groups); // libc copy: must be destroyed (ASan/LSan check this in Debug)

    puts("ok");
    return 0;
}
