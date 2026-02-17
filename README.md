# WCtoolkit

A C toolkit providing generic data structures with explicit ownership, arena allocation, and performance-conscious design. No dependencies beyond the C standard library.

---

## Table of Contents

- [Quick Start](#quick-start)
- [The Ownership Model](#the-ownership-model)
- [Components](#components)
  - [Arena Allocator](#arena-allocator)
  - [Generic Vector](#generic-vector)
  - [String](#string)
  - [Stack](#stack)
  - [Queue](#queue)
  - [HashMap](#hashmap)
  - [HashSet](#hashset)
  - [Matrix](#matrix)
  - [Bit Vector](#bit-vector)
  - [Random](#random)
  - [Fast Math](#fast-math)
- [Callback Reference](#callback-reference)
- [Building](#building)
- [Single-Header Distribution](#single-header-distribution)
- [Type Reference](#type-reference)

---

## Quick Start

```c
#include "gen_vector.h"
#include "helpers.h"   // str_copy, str_del, VEC_PUSH_SIMP, etc.

int main(void) {
    // --- Vector of ints (no callbacks needed for POD) ---
    genVec vec;
    genVec_init_stk(16, sizeof(int), NULL, NULL, NULL, &vec);

    VEC_PUSH_SIMP(&vec, int, 10);
    VEC_PUSH_SIMP(&vec, int, 20);
    VEC_PUSH_SIMP(&vec, int, 30);

    genVec_print(&vec, int_print);
    genVec_destroy_stk(&vec);

    // --- Vector of Strings (with copy/del callbacks) ---
    genVec* svec = genVec_init(8, sizeof(String), str_copy, str_move, str_del);

    VEC_PUSH_CSTR(svec, "hello");
    VEC_PUSH_CSTR(svec, "world");

    genVec_print(svec, str_print);
    genVec_destroy(svec);

    return 0;
}
```

---

## The Ownership Model

This is the central concept in the library. Every container that holds elements with heap resources needs three callbacks:

```c
void my_copy(u8* dest, const u8* src);   // deep copy src into dest
void my_move(u8* dest, u8** src);        // transfer ownership, null src
void my_del(u8* elm);                    // free resources owned by elm
```

Pass `NULL` for any callback you don't need. POD types (int, float, structs without pointers) work with all three as `NULL`.

### By Value vs By Pointer

You have two storage strategies. **By value** stores the struct inline:

```c
// sizeof(String) = 40 bytes stored per element
genVec* vec = genVec_init(8, sizeof(String), str_copy, str_move, str_del);

String s;
string_create_stk(&s, "hello");
genVec_push(vec, cast(s));       // deep copies s into the vector
string_destroy_stk(&s);          // safe to destroy, vector has its own copy
```

**By pointer** stores a pointer to a heap-allocated struct:

```c
// sizeof(String*) = 8 bytes stored per element
genVec* vec = genVec_init(8, sizeof(String*), str_copy_ptr, str_move_ptr, str_del_ptr);

String* s = string_from_cstr("hello");
genVec_push_move(vec, (u8**)&s); // transfers ownership, s is now NULL
```

By value is better for cache locality. By pointer is useful when elements are large or shared.

### Copy vs Move

```c
String* s = string_from_cstr("hello");

// Copy: s is unaffected, vector gets its own deep copy
genVec_push(vec, castptr(s));
string_destroy(s);    // still need to free

// Move: vector takes ownership, s is nulled
String* s2 = string_from_cstr("world");
genVec_push_move(vec, (u8**)&s2);
// s2 == NULL now, don't free it
```

---

## Components

### Arena Allocator

O(1) bump-pointer allocator. Allocations are free, cleanup is a single `free()`.

```c
// Heap arena
Arena* arena = arena_create(nKB(4));   // 0 = use ARENA_DEFAULT_SIZE (4KB)

// Stack arena (no heap)
Arena stack_arena;
ARENA_CREATE_STK_ARR(&stack_arena, 4); // 4KB on the stack
```

**Typed allocation macros:**

```c
int*        nums = ARENA_ALLOC_N(arena, int, 100);
MyStruct*   s    = ARENA_ALLOC_ZERO(arena, MyStruct);   // zeroed
char*       buf  = ARENA_ALLOC_N(arena, char, 256);
```

**Scratch regions — temporary allocations that auto-rollback:**

```c
// Automatic (preferred)
ARENA_SCRATCH(temp, arena) {
    float* scratch = ARENA_ALLOC_N(arena, float, 1024);
    // ... do work with scratch ...
}   // scratch memory is reclaimed here

// Manual
arena_scratch sc = arena_scratch_begin(arena);
u8* tmp = arena_alloc(arena, nKB(1));
// ... work ...
arena_scratch_end(&sc);   // rolls back to before tmp was allocated
```

**Arena-allocated matrices** (great for avoiding malloc in hot loops):

```c
// From matrix_test_7 in the test suite:
Arena* arena = arena_create(nKB(1));
Matrix* result = matrix_arena_alloc(arena, 4, 4);

ARENA_SCRATCH(temp, arena) {
    Matrix* t1 = matrix_arena_arr_alloc(arena, 4, 4, (float*)(float[4][4]){
        {1,2,3,4}, {1,2,3,4}, {1,2,3,4}, {1,2,3,4}
    });
    Matrix* t2 = matrix_arena_arr_alloc(arena, 4, 4, (float*)(float[4][4]){
        {1,2,3,4}, {1,2,3,4}, {1,2,3,4}, {1,2,3,4}
    });
    matrix_xply(result, t1, t2);
}   // t1 and t2 freed; result survives

matrix_print(result);
arena_release(arena);    // single cleanup for everything
```

**Query arena state:**

```c
u64 used      = arena_used(arena);
u64 remaining = arena_remaining(arena);
arena_clear(arena);     // reset without freeing (reuse the memory)
arena_release(arena);   // free everything
```

---

### Generic Vector

The foundation of the library. Stack, Queue, String, and BitVec all build on top of it.

**Initialization:**

```c
// Heap (returns pointer)
genVec* vec = genVec_init(initial_capacity, sizeof(MyType), copy, move, del);

// Stack (you provide the struct, data still on heap)
genVec vec;
genVec_init_stk(initial_capacity, sizeof(MyType), copy, move, del, &vec);

// Stack struct AND stack data (fixed size - will crash if you push past capacity)
int buf[8] = {0};
genVec vec;
genVec_init_arr(8, (u8*)buf, sizeof(int), NULL, NULL, NULL, &vec);

// Pre-filled with a value
genVec* vec = genVec_init_val(10, cast(my_val), sizeof(MyType), copy, move, del);
```

**Push / Pop:**

```c
// POD push using the VEC_PUSH_SIMP macro (from helpers.h)
VEC_PUSH_SIMP(&vec, float, 3.14f);
VEC_PUSH_SIMP(&vec, int,   42);

// Push by copy
genVec_push(vec, cast(my_string));

// Push by move (src is nulled)
genVec_push_move(vec, (u8**)&heap_string_ptr);

// Pop (pass NULL if you don't need the value)
genVec_pop(vec, cast(out_val));
genVec_pop(vec, NULL);
```

**Access:**

```c
// Get a copy
MyType out;
genVec_get(vec, i, cast(out));

// Get a pointer (invalidated by any push/insert/remove)
const MyType* p     = (MyType*)genVec_get_ptr(vec, i);
MyType*       p_mut = (MyType*)genVec_get_ptr_mut(vec, i);

// Front / back
const MyType* first = (MyType*)genVec_front(vec);
const MyType* last  = (MyType*)genVec_back(vec);
```

**Insert / Remove:**

```c
genVec_insert(vec, i, cast(val));            // shift right, insert copy
genVec_insert_move(vec, i, (u8**)&ptr);      // shift right, insert move
genVec_insert_multi(vec, i, (u8*)arr, n);    // insert n elements at once

genVec_remove(vec, i, cast(out));            // shift left, copy out (or NULL)
genVec_remove_range(vec, l, r);              // remove [l, r] inclusive

genVec_replace(vec, i, cast(val));           // overwrite element i
```

**Utilities:**

```c
u64 n   = genVec_size(vec);
u64 cap = genVec_capacity(vec);
b8  emp = genVec_empty(vec);

genVec_for_each(vec, my_fn);       // apply fn to every element
genVec_print(vec, my_print_fn);

genVec_reserve(vec, new_cap);      // pre-allocate (never shrinks)
genVec_shrink_to_fit(vec);         // reallocate to exact size
genVec_clear(vec);                 // call del_fn on all, keep capacity
genVec_reset(vec);                 // call del_fn on all, free memory
genVec_copy(dest, src);            // deep copy (cleans up dest first)
genVec_move(dest, &src);           // transfer ownership, src nulled

// Heap-allocated vector:
genVec_destroy(vec);
// Stack-allocated vector:
genVec_destroy_stk(&vec);
```

**Configurable growth/shrink** (define before including the header):

```c
#define GENVEC_GROWTH    1.5F   // capacity multiplier on grow
#define GENVEC_SHRINK_AT 0.25F  // shrink when < 25% full
#define GENVEC_SHRINK_BY 0.5F   // halve capacity on shrink
```

---

### String

A length-based dynamic string, not null-terminated internally. Wraps `genVec` of `char`.

**Creation:**

```c
String* s1 = string_create();                    // empty, heap
String* s2 = string_from_cstr("hello");          // from literal, heap
String* s3 = string_from_string(s2);             // deep copy, heap

String s4;
string_create_stk(&s4, "stack string");          // struct on stack, data on heap
```

**Modification:**

```c
string_append_cstr(s1, " world");
string_append_string(s1, s2);
string_append_char(s1, '!');
string_append_string_move(s1, &s2);   // consumes and frees s2

string_insert_cstr(s1, 0, "prefix: ");
string_insert_char(s1, 5, '-');

char c = string_pop_char(s1);
string_remove_char(s1, 3);
string_remove_range(s1, 2, 5);    // removes [2, 5] inclusive
string_clear(s1);
```

**Access:**

```c
char   c    = string_char_at(s1, 0);
char*  data = string_data_ptr(s1);   // raw buffer, NO null terminator
u64    len  = string_len(s1);
b8     emp  = string_empty(s1);

string_set_char(s1, 0, 'H');

// Get a null-terminated copy (caller must free)
char* cstr = string_to_cstr(s1);
printf("%s\n", cstr);
free(cstr);

// Temporarily null-terminate for a read-only call (no alloc)
TEMP_CSTR_READ(s1) {
    printf("%s\n", string_data_ptr(s1));
}
```

**Search and compare:**

```c
u64 pos = string_find_char(s1, 'e');     // (u64)-1 if not found
u64 idx = string_find_cstr(s1, "llo");   // (u64)-1 if not found

String* sub = string_substr(s1, 2, 4);   // heap copy of chars [2..5]

b8 eq  = string_equals(s1, s2);
b8 eq2 = string_equals_cstr(s1, "hello");
int cmp = string_compare(s1, s2);        // like memcmp
```

**Cleanup:**

```c
string_destroy(s1);           // heap string (string_from_cstr etc.)
string_destroy_stk(&s4);      // stack string (string_create_stk)
```

**Using String as a hashmap key** — include `str_setup.h` for the hash and compare functions:

```c
#include "str_setup.h"

hashmap* map = hashmap_create(
    sizeof(String), sizeof(int),
    murmurhash3_str, str_cmp,
    str_copy, NULL, str_move, NULL, str_del, NULL
);
```

---

### Stack

LIFO wrapper around `genVec`. Exactly the same callback rules apply.

```c
Stack* stk = stack_create(16, sizeof(int), NULL, NULL, NULL);

int val = 42;
stack_push(stk, cast(val));
stack_push_move(stk, (u8**)&ptr);    // move semantics

int top;
stack_peek(stk, cast(top));
const int* top_ptr = (int*)stack_peek_ptr(stk);

stack_pop(stk, cast(top));           // copy out then remove
stack_pop(stk, NULL);                // just discard

u64 n  = stack_size(stk);
b8  ep = stack_empty(stk);

stack_clear(stk);
stack_destroy(stk);
```

---

### Queue

Circular buffer FIFO with O(1) enqueue/dequeue and automatic grow/shrink.

```c
Queue* q = queue_create(16, sizeof(int), NULL, NULL, NULL);

int val = 5;
enqueue(q, cast(val));
enqueue_move(q, (u8**)&ptr);

int out;
dequeue(q, cast(out));    // copies front to out then removes
dequeue(q, NULL);         // just discard front

queue_peek(q, cast(out));
const int* front = (int*)queue_peek_ptr(q);

u64 n  = queue_size(q);
b8  ep = queue_empty(q);
u64 cp = queue_capacity(q);

queue_shrink_to_fit(q);
queue_clear(q);     // empty queue, keep capacity
queue_reset(q);     // empty queue, free memory
queue_destroy(q);
```

---

### HashMap

Open-addressing hash table with linear probing. Grows at 70% load, shrinks at 20%.

**Creation:**

```c
// int -> int  (no callbacks)
hashmap* map = hashmap_create(
    sizeof(int), sizeof(int),
    NULL, NULL,        // hash_fn, cmp_fn  (NULL = FNV-1a, memcmp)
    NULL, NULL,        // key_copy, val_copy
    NULL, NULL,        // key_move, val_move
    NULL, NULL         // key_del, val_del
);

// int -> String  (String values need copy/move/del)
hashmap* map = hashmap_create(
    sizeof(int), sizeof(String),
    NULL, NULL,
    NULL,     str_copy,
    NULL,     str_move,
    NULL,     str_del
);

// String -> String  (both sides need callbacks, plus custom hash/compare)
#include "str_setup.h"
hashmap* map = hashmap_create(
    sizeof(String), sizeof(String),
    murmurhash3_str, str_cmp,
    str_copy, str_copy,
    str_move, str_move,
    str_del,  str_del
);
```

**Insertion:**

```c
int k = 42;
String v;
string_create_stk(&v, "hello");

// Copy both
hashmap_put(map, cast(k), cast(v));

// Move value only (common pattern)
String* heap_val = string_from_cstr("world");
hashmap_put_val_move(map, cast(k), (u8**)&heap_val);  // heap_val = NULL

// Move both (when key is also heap-allocated, e.g. String key)
String* key = string_from_cstr("mykey");
String* val = string_from_cstr("myval");
hashmap_put_move(map, (u8**)&key, (u8**)&val);   // both nulled
```

All `put` variants return `1` if the key already existed (value was updated), `0` if it was a new insertion.

**Retrieval:**

```c
// Copy out
String out;
if (hashmap_get(map, cast(k), cast(out))) {
    string_print(&out);
    string_destroy_stk(&out);
}

// Direct pointer (invalidated by any put/del)
String* ptr = (String*)hashmap_get_ptr(map, cast(k));
if (ptr) {
    string_append_cstr(ptr, "_modified");   // modifies in place
}

b8 exists = hashmap_has(map, cast(k));
```

**Deletion:**

```c
// Delete and discard
hashmap_del(map, cast(k), NULL);

// Delete and get the value back
String deleted;
if (hashmap_del(map, cast(k), cast(deleted))) {
    string_print(&deleted);
    string_destroy_stk(&deleted);
}
```

**Utilities:**

```c
u64 n   = hashmap_size(map);
u64 cap = hashmap_capacity(map);
b8  emp = hashmap_empty(map);

hashmap_print(map, key_print_fn, val_print_fn);
hashmap_destroy(map);
```

---

### HashSet

Same implementation strategy as HashMap but stores only elements, no values.

```c
hashset* set = hashset_create(sizeof(int), NULL, NULL, NULL, NULL, NULL);

int x = 42;
b8 existed = hashset_insert(set, cast(x));        // 0 = inserted, 1 = already existed
b8 existed = hashset_insert_move(set, (u8**)&p);  // move semantics

b8 found   = hashset_has(set, cast(x));
b8 removed = hashset_remove(set, cast(x));

hashset_clear(set);    // empty, keep capacity
hashset_reset(set);    // empty, reset to initial capacity
hashset_destroy(set);
```

**String set example (from the test suite):**

```c
#include "str_setup.h"

hashset* set = hashset_create(sizeof(String),
    murmurhash3_str, str_cmp, str_copy, str_move, str_del);

String* s = string_from_cstr("hello");
hashset_insert_move(set, (u8**)&s);   // s is now NULL

// Check membership with a temporary stack string
String query;
string_create_stk(&query, "hello");
b8 found = hashset_has(set, cast(query));
string_destroy_stk(&query);

hashset_destroy(set);
```

---

### Matrix

Row-major 2D float matrix with cache-optimized operations.

**Creation:**

```c
// Heap
Matrix* mat = matrix_create(3, 4);
Matrix* mat = matrix_create_arr(3, 3, (float*)(float[3][3]){
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9}
});

// Stack (matrix struct + data both on the stack)
Matrix mat;
matrix_create_stk(&mat, 3, 3, (float*)(float[3][3]){
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9}
});

// Stack with zero data
Matrix out;
matrix_create_stk(&out, 3, 3, (float*)ZEROS_2D(3, 3));
```

**Setting values:**

```c
// Preferred: explicit 2D array initializer
matrix_set_val_arr(mat, 9, (float*)(float[3][3]){
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9}
});

// Single element
matrix_set_elm(mat, 3.14f, 1, 2);   // row 1, col 2

// Direct macro access (no bounds checking)
MATRIX_AT(mat, 1, 2) = 42.0f;
float v = MATRIX_AT(mat, 1, 2);
```

**Operations:**

```c
matrix_add(out, a, b);        // out = a + b  (out may alias a or b)
matrix_sub(out, a, b);        // out = a - b  (out may alias a or b)
matrix_scale(mat, 2.0f);      // mat *= 2
matrix_div(mat, 2.0f);        // mat /= 2
matrix_copy(dest, src);

// Multiply: out CANNOT alias a or b
// (m×k) * (k×n) = (m×n)
matrix_xply(out, a, b);       // blocked ikj — good for small/medium
matrix_xply_2(out, a, b);     // transposes b internally — better for large

// Transpose: out CANNOT alias mat
matrix_T(out, mat);           // out = mat^T

// LU decomposition and determinant
matrix_LU_Decomp(L, U, mat);
float det = matrix_det(mat);

matrix_print(mat);
matrix_destroy(mat);   // only for heap-allocated matrices
```

**Generic matrix** (for other element types — compile-time instantiation):

```c
#include "matrix_generic.h"

// In a .c file, instantiate all functions for a type:
INSTANTIATE_MATRIX(int, "%d ");
INSTANTIATE_MATRIX(double, "%.6f ");

// Then use the type-suffixed functions:
Matrix_int* m = matrix_create_int(3, 3);
matrix_set_elm_int(m, 42, 0, 0);
double det = matrix_det_int(m);     // always returns double
matrix_print_int(m);
matrix_destroy_int(m);
```

---

### Bit Vector

Compact dynamic bit array backed by a `genVec` of bytes.

```c
bitVec* bv = bitVec_create();

bitVec_set(bv, 42);          // set bit 42 to 1
bitVec_clear(bv, 42);        // set bit 42 to 0
bitVec_toggle(bv, 42);       // flip bit 42
u8 b = bitVec_test(bv, 42);  // returns 0 or 1

bitVec_push(bv);             // append a 1 bit
bitVec_pop(bv);              // remove last bit

u64 nbits  = bitVec_size_bits(bv);   // logical bit count
u64 nbytes = bitVec_size_bytes(bv);  // backing byte count

bitVec_print(bv, 0);         // print byte 0 as binary
bitVec_destroy(bv);
```

---

### Random

PCG32 random number generator — fast, high quality, reproducible.

```c
// Seed with explicit values (same seed = same sequence)
pcg32_rand_seed(42, 1);

// Seed from system time (different each run, second precision)
pcg32_rand_seed_time();

// Raw 32-bit random
u32 r = pcg32_rand();

// Bounded: uniform in [0, bound), no modulo bias
u32 dice = pcg32_rand_bounded(6);

// Floating point in [0, 1)
float  f = pcg32_rand_float();
double d = pcg32_rand_double();  // 53-bit precision

// Range: [min, max)
float  fv = pcg32_rand_float_range(-1.0f, 1.0f);
double dv = pcg32_rand_double_range(0.0, 100.0);

// Gaussian (Box-Muller, standard normal N(0, 1))
float g = pcg32_rand_gaussian();

// Custom normal N(mean, stddev²)
float gv = pcg32_rand_gaussian_custom(0.0f, 1.0f);
```

---

### Fast Math

Approximate implementations of common math functions. Faster than `<math.h>` at the cost of some precision. Designed for use cases like RNG, simulations, or visualizations where exactness isn't critical.

```c
#include "fast_math.h"

float s = fast_sqrt(x);    // Newton-Raphson, 4 iterations
float l = fast_log(x);     // range reduction + Taylor series
float e = fast_exp(x);     // range reduction + repeated squaring
float sn = fast_sin(x);    // Taylor series, input reduced to [-π, π]
float cs = fast_cos(x);    // = fast_sin(x + π/2)
float c  = fast_ceil(x);
```

---

## Callback Reference

All callbacks follow the same signature conventions across the entire library.

### For By-Value storage (e.g. `genVec` of `String`)

```c
// str_copy, str_move, str_del from helpers.h

void str_copy(u8* dest, const u8* src) {
    String* d = (String*)dest;
    String* s = (String*)src;
    memcpy(d, s, sizeof(String));     // copy all fields
    u64 n = s->size * s->data_size;
    d->data = malloc(n);
    memcpy(d->data, s->data, n);      // deep copy data buffer
}

void str_move(u8* dest, u8** src) {
    memcpy(dest, *src, sizeof(String));
    free(*src);
    *src = NULL;
}

void str_del(u8* elm) {
    string_destroy_stk((String*)elm); // free data buffer, not the struct
}
```

### For By-Pointer storage (e.g. `genVec` of `String*`)

```c
// str_copy_ptr, str_move_ptr, str_del_ptr from helpers.h

void str_copy_ptr(u8* dest, const u8* src) {
    String* s = *(String**)src;    // src is String**
    String* d = malloc(sizeof(String));
    memcpy(d, s, sizeof(String));
    u64 n = s->size * s->data_size;
    d->data = malloc(n);
    memcpy(d->data, s->data, n);
    *(String**)dest = d;           // dest is String**
}

void str_move_ptr(u8* dest, u8** src) {
    *(String**)dest = *(String**)src;
    *src = NULL;
}

void str_del_ptr(u8* elm) {
    string_destroy(*(String**)elm); // free both data buffer and the struct
}
```

### For HashMap/HashSet — compare function

```c
// str_cmp from helpers.h (for String keys)
int str_cmp(const u8* a, const u8* b, u64 size) {
    (void)size;
    return string_compare((const String*)a, (const String*)b);
}
```

### Utility print functions (from helpers.h)

```c
void int_print(const u8* elm)    { printf("%d",  *(int*)elm); }
void float_print(const u8* elm)  { printf("%f",  *(float*)elm); }
void double_print(const u8* elm) { printf("%f",  *(double*)elm); }
void str_print(const u8* elm)    { string_print((const String*)elm); }
void str_print_ptr(const u8* elm){ string_print(*(const String**)elm); }
```

---

## Building

The project uses CMake with Clang:

```bash
# Debug build (ASan + UBSan enabled, -O0)
cmake -B build -G Ninja
ninja -C build
./build/main

# Release build (-O3, no sanitizers)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

All source files in `src/` are compiled automatically. Test headers live in `tests/`.

---

## Single-Header Distribution

Each component can be converted into a self-contained single-header file that requires no
separate compilation step — just drop it into your project and include it.

### Generating the headers

The script lives in `scripts/make_single_header.py`. Run it from the `scripts/` directory:

```bash
cd scripts

# Convert specific components (dependencies are pulled in automatically)
python make_single_header.py arena gen_vector string

# Convert everything
python make_single_header.py --all

# See all components and their dependency chains
python make_single_header.py --list
```

Output goes to `../single_header/` as `<component>_single.h`.

Custom paths (defaults are `../include`, `../src`, `../single_header`):

```bash
python make_single_header.py --all \
    --include-dir path/to/include \
    --src-dir path/to/src \
    --out-dir path/to/output
```

### Using a generated header

In **exactly one** `.c` file, define `WC_IMPLEMENTATION` before the include:

```c
// my_app.c  — only in this one file
#define WC_IMPLEMENTATION
#include "string_single.h"
```

In all other files, just include normally:

```c
// other.c
#include "string_single.h"
```

Dependencies are inlined automatically. Requesting `string_single.h` also brings in
`gen_vector` and `common` — you don't include those separately.

### Using multiple single-headers together

Each implementation block is guarded against being emitted more than once, so including
several single-headers in the same project is safe:

```c
#define WC_IMPLEMENTATION
#include "string_single.h"    // inlines common + gen_vector + string
#include "hashmap_single.h"   // inlines common + map_setup + hashmap
                              // common is NOT duplicated — guard prevents it
```

### Available components

| Component | Depends on |
|---|---|
| `common` | — |
| `fast_math` | `common` |
| `arena` | `common` |
| `gen_vector` | `common` |
| `bit_vector` | `common`, `gen_vector` |
| `String` | `common`, `gen_vector` |
| `Stack` | `common`, `gen_vector` |
| `Queue` | `common`, `gen_vector` |
| `map_setup` | `common` |
| `random` | `common`, `fast_math` |
| `hashmap` | `common`, `map_setup` |
| `hashset` | `common`, `map_setup` |
| `matrix` | `common`, `arena` |

> **Note:** `matrix_generic.h` is already header-only (macro-generated) and needs no
> conversion. Copy it directly.

---

## Type Reference

### Primitive types (`common.h`)

```c
typedef uint8_t  u8;
typedef uint8_t  b8;   // boolean: false (0), true (1)
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
```

### Casting helpers

```c
cast(x)     // (u8*)(&x)    — address of a local variable
castptr(x)  // (u8*)(x)     — cast a pointer directly
```

### Size helpers

```c
nKB(n)   // n * 1024
nMB(n)   // n * 1024 * 1024
```

### Diagnostics

```c
FATAL(fmt, ...)                      // print to stderr and exit
WARN(fmt, ...)                       // print to stdout and continue
LOG(fmt, ...)                        // info print

CHECK_FATAL(cond, fmt, ...)          // exit if cond is true
CHECK_WARN(cond, fmt, ...)           // warn if cond is true
CHECK_WARN_RET(cond, ret, fmt, ...)  // warn and return ret if cond is true
ASSERT_FATAL(cond, fmt, ...)         // exit if cond is false
ASSERT_WARN(cond, fmt, ...)          // warn if cond is false
```

---

## License

MIT — see `LICENSE`.

