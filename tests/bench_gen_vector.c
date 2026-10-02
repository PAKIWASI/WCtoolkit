#include "bench_support.h"

#include <string.h>

// Plain data (no ops, memcpy paths) vs owning elements (String, per-element ops).

UBENCH(gen_vector, push_int)
{
    GenVec v = VEC(int, 0);
    for (int i = 0; i < N; i++) { VEC_PUSH(&v, i); }
    UBENCH_DO_NOTHING(v.data);
    GenVec_destroy(&v);
}

UBENCH(gen_vector, push_string)
{
    GenVec v = VEC_OF(String, 0);
    for (int i = 0; i < N; i++) { VEC_PUSH_CSTR(&v, LONG_STR); }
    UBENCH_DO_NOTHING(v.data);
    GenVec_destroy(&v);
}

UBENCH_EX(gen_vector, copy_int)
{
    GenVec src = VEC(int, N);
    for (int i = 0; i < N; i++) { VEC_PUSH(&src, i); }
    UBENCH_DO_BENCHMARK()
    {
        GenVec c = GenVec_copy(WC_LIBC, &src);   // one memcpy
        UBENCH_DO_NOTHING(c.data);
        GenVec_destroy(&c);
    }
    GenVec_destroy(&src);
}

UBENCH_EX(gen_vector, copy_string)
{
    GenVec src = VEC_OF(String, N);
    for (int i = 0; i < N; i++) { VEC_PUSH_CSTR(&src, LONG_STR); }
    UBENCH_DO_BENCHMARK()
    {
        GenVec c = GenVec_copy(WC_LIBC, &src);   // copy_fn per element
        UBENCH_DO_NOTHING(c.data);
        GenVec_destroy(&c);
    }
    GenVec_destroy(&src);
}

UBENCH(gen_vector, pop_int)
{
    GenVec v = VEC(int, N);
    for (int i = 0; i < N; i++) { VEC_PUSH(&v, i); }
    int out = 0;
    for (int i = 0; i < N; i++) { GenVec_pop(&v, &out); }
    UBENCH_DO_NOTHING(&out);
    GenVec_destroy(&v);
}

UBENCH(gen_vector, remove_range_string)
{
    GenVec v = VEC_OF(String, N);
    for (int i = 0; i < N; i++) { VEC_PUSH_CSTR(&v, LONG_STR); }
    GenVec_remove_range(&v, N / 4, N / 2);       // del_fn on the middle half
    UBENCH_DO_NOTHING(v.data);
    GenVec_destroy(&v);
}

UBENCH(gen_vector, create_val_int)
{
    int    zero = 0;
    GenVec v    = GenVec_create_val(WC_LIBC, N, &zero, sizeof(int), NULL);
    UBENCH_DO_NOTHING(v.data);
    GenVec_destroy(&v);
}


// A struct owning a String and a GenVec: copying it deep-copies both,
// moving it is a memcpy.

typedef struct {
    String name;
    GenVec scores;
} Person;

static void person_copy(wc_allocator dst, void* dest, const void* src)
{
    const Person* s = src;
    Person*       d = dest;
    d->name   = String_copy(dst, &s->name);
    d->scores = GenVec_copy(dst, &s->scores);
}

static void person_del(void* elm)
{
    Person* p = elm;
    String_destroy(&p->name);
    GenVec_destroy(&p->scores);
}

static const wc_container_ops person_ops = { person_copy, NULL, person_del };

static Person person_make(int i)
{
    Person p = { String_from_cstr(WC_LIBC, LONG_STR), VEC(int, 64) };
    for (int j = 0; j < 64; j++) { VEC_PUSH(&p.scores, i + j); }
    return p;
}

UBENCH(gen_vector, push_struct_copy)
{
    GenVec v = VEC_CX(Person, N, &person_ops);
    for (int i = 0; i < N; i++) {
        Person p = person_make(i);
        GenVec_push(&v, &p);                     // deep copy...
        person_del(&p);                          // ...then free the original
    }
    GenVec_destroy(&v);
}

UBENCH(gen_vector, push_struct_move)
{
    GenVec v = VEC_CX(Person, N, &person_ops);
    for (int i = 0; i < N; i++) {
        Person p = person_make(i);
        VEC_PUSH_MOVE(&v, p);                    // ownership moves, no copy
    }
    GenVec_destroy(&v);
}
