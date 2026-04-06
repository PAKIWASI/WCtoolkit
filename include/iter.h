#ifndef ITER_H
#define ITER_H

/*
 * iter.h — Forward iterators for genVec, hashmap, and hashset
 * =============================================================
 *
 * Three iterator types, one consistent protocol for all of them:
 *
 *   iter_init   — point at the first valid element (or mark exhausted)
 *   iter_next   — advance to the next valid element
 *   iter_valid  — check whether the iterator still has an element
 *
 * All iterators are structs — stack-allocate them, pass by pointer.
 * They hold a pointer back to their container and a position index.
 * They do NOT own anything and do NOT need to be destroyed.
 *
 *
 * INVALIDATION
 * ------------
 * Any mutation of the container (push, insert, remove, resize, put, del,
 * clear, etc.) invalidates all live iterators on that container.
 * Using an iterator after mutation is undefined behaviour.
 * Read-only access during iteration is safe.
 *
 *
 * USAGE — genVec (forward, by pointer into slot)
 * -----------------------------------------------
 *   genVec* v = VEC_OF_INT(8);
 *   VEC_PUSH(v, 1); VEC_PUSH(v, 2); VEC_PUSH(v, 3);
 *
 *   // Low-level:
 *   vec_iter it;
 *   for (vec_iter_init(&it, v); vec_iter_valid(&it); vec_iter_next(&it)) {
 *       int* p = (int*)vec_iter_get(&it);
 *       printf("%d\n", *p);
 *   }
 *
 *   // Macro (typed pointer, supports break/continue/return safely):
 *   VEC_ITER(v, int, p) { printf("%d\n", *p); }
 *
 *   // Reverse:
 *   VEC_ITER_REV(v, int, p) { printf("%d\n", *p); }
 *
 *
 * USAGE — hashmap (yields key+value pair together)
 * -------------------------------------------------
 *   hashmap* m = hashmap_create(sizeof(int), sizeof(float), NULL, NULL, NULL, NULL);
 *   MAP_PUT(m, 1, 1.0f); MAP_PUT(m, 2, 2.0f);
 *
 *   // Low-level:
 *   map_iter it;
 *   for (map_iter_init(&it, m); map_iter_valid(&it); map_iter_next(&it)) {
 *       int*   k = (int*)  map_iter_key(&it);
 *       float* v = (float*)map_iter_val(&it);
 *       printf("%d => %.1f\n", *k, *v);
 *   }
 *
 *   // Macro (typed key AND value in one loop, no inner-for trick):
 *   MAP_ITER(m, int, k, float, v) { printf("%d => %.1f\n", *k, *v); }
 *
 *
 * USAGE — hashset (yields element pointer)
 * -----------------------------------------
 *   hashset* s = hashset_create(sizeof(int), NULL, NULL, NULL);
 *   SET_INSERT(s, 10); SET_INSERT(s, 20);
 *
 *   // Low-level:
 *   set_iter it;
 *   for (set_iter_init(&it, s); set_iter_valid(&it); set_iter_next(&it)) {
 *       int* p = (int*)set_iter_get(&it);
 *       printf("%d\n", *p);
 *   }
 *
 *   // Macro:
 *   SET_ITER(s, int, p) { printf("%d\n", *p); }
 *
 *
 * MUTATION DURING ITERATION
 * -------------------------
 * Deleting while iterating a hashmap or hashset is NOT safe in general —
 * backward-shift deletion can move a not-yet-visited entry into a slot that
 * was already visited, causing it to be skipped.
 *
 * If you need to delete while iterating, collect keys into a genVec first,
 * then delete from that list after the loop.
 *
 *
 * REVERSE ITERATION
 * -----------------
 * Only genVec supports reverse iteration (vec_iter_rev / VEC_ITER_REV).
 * Hash containers have no meaningful ordering, so reverse is not provided.
 */

#include "gen_vector.h"
#include "hashmap.h"
#include "hashset.h"


/* ══════════════════════════════════════════════════════════════════════════
 * 1. VEC ITERATOR
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    genVec* vec;
    u64     idx;
    b8      reverse; // 0 = forward, 1 = reverse
} vec_iter;


// Point at element 0, or mark exhausted if vec is empty.
static inline void vec_iter_init(vec_iter* it, genVec* vec)
{
    CHECK_FATAL(!it, "it is null");
    CHECK_FATAL(!vec, "vec is null");
    it->vec     = vec;
    it->idx     = 0;
    it->reverse = 0;
}

// Point at the last element, or mark exhausted if vec is empty.
static inline void vec_iter_init_rev(vec_iter* it, genVec* vec)
{
    CHECK_FATAL(!it, "it is null");
    CHECK_FATAL(!vec, "vec is null");
    it->vec     = vec;
    it->idx     = vec->size > 0 ? vec->size - 1 : 0;
    it->reverse = 1;
}

// Returns 1 if the iterator has a current element.
static inline b8 vec_iter_valid(const vec_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    if (it->vec->size == 0) {
        return 0;
    }
    if (it->reverse) {
        return it->idx < it->vec->size;
    } // wraps past 0 → UINT64_MAX
    return it->idx < it->vec->size;
}

// Advance to the next element.
static inline void vec_iter_next(vec_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    if (it->reverse) {
        // Unsigned underflow from 0 → UINT64_MAX signals exhaustion.
        // vec_iter_valid will see idx >= size and return 0.
        it->idx--;
    } else {
        it->idx++;
    }
}

// Return a mutable pointer to the current element's raw bytes.
// Returns NULL if the iterator is exhausted.
static inline u8* vec_iter_get(const vec_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    if (!vec_iter_valid(it)) {
        return NULL;
    }
    return genVec_get_ptr_mut(it->vec, it->idx);
}

// Return the current index.
static inline u64 vec_iter_index(const vec_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    return it->idx;
}


/* ══════════════════════════════════════════════════════════════════════════
 * 2. MAP ITERATOR
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const hashmap* map;
    u64            slot; // index into the backing arrays; capacity = exhausted sentinel
} map_iter;


// Advance slot to the first occupied bucket (helper, not for direct use).
static inline void map_iter_seek_(map_iter* it)
{
    while (it->slot < it->map->capacity && it->map->psls[it->slot] == 0) {
        it->slot++;
    }
}

// Point at the first occupied entry, or mark exhausted.
static inline void map_iter_init(map_iter* it, const hashmap* map)
{
    CHECK_FATAL(!it, "it is null");
    CHECK_FATAL(!map, "map is null");
    it->map  = map;
    it->slot = 0;
    map_iter_seek_(it);
}

// Returns 1 if the iterator has a current entry.
static inline b8 map_iter_valid(const map_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    return it->slot < it->map->capacity;
}

// Advance to the next occupied entry.
static inline void map_iter_next(map_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    it->slot++;
    map_iter_seek_(it);
}

// Return a const pointer to the current entry's key bytes.
// Returns NULL if exhausted.
static inline const u8* map_iter_key(const map_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    if (!map_iter_valid(it)) {
        return NULL;
    }
    return it->map->keys + ((u64)it->map->key_size * it->slot);
}

// Return a mutable pointer to the current entry's value bytes.
// Returns NULL if exhausted.
// NOTE: you may update the value in place; do NOT modify the key.
static inline u8* map_iter_val(const map_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    if (!map_iter_valid(it)) {
        return NULL;
    }
    return it->map->vals + ((u64)it->map->val_size * it->slot);
}

// Return the current slot index (bucket position, not insertion order).
static inline u64 map_iter_slot(const map_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    return it->slot;
}


/* ══════════════════════════════════════════════════════════════════════════
 * 3. SET ITERATOR
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const hashset* set;
    u64            slot;
} set_iter;


// Advance slot to the first occupied bucket (helper, not for direct use).
static inline void set_iter_seek_(set_iter* it)
{
    while (it->slot < it->set->capacity && it->set->psls[it->slot] == 0) {
        it->slot++;
    }
}

// Point at the first occupied element, or mark exhausted.
static inline void set_iter_init(set_iter* it, const hashset* set)
{
    CHECK_FATAL(!it, "it is null");
    CHECK_FATAL(!set, "set is null");
    it->set  = set;
    it->slot = 0;
    set_iter_seek_(it);
}

// Returns 1 if the iterator has a current element.
static inline b8 set_iter_valid(const set_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    return it->slot < it->set->capacity;
}

// Advance to the next occupied element.
static inline void set_iter_next(set_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    it->slot++;
    set_iter_seek_(it);
}

// Return a const pointer to the current element's raw bytes.
// Returns NULL if exhausted.
static inline const u8* set_iter_get(const set_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    if (!set_iter_valid(it)) {
        return NULL;
    }
    return it->set->elms + ((u64)it->set->elm_size * it->slot);
}

// Return the current slot index.
static inline u64 set_iter_slot(const set_iter* it)
{
    CHECK_FATAL(!it, "it is null");
    return it->slot;
}


/* ══════════════════════════════════════════════════════════════════════════
 * 4. CONVENIENCE MACROS
 * ══════════════════════════════════════════════════════════════════════════
 *
 * These use a plain for loop — break, continue, and return all work as
 * expected inside the body. No double-for trick, no hidden state.
 *
 * The iterator struct is declared in the for-init so its scope is the loop.
 * Naming it with a prefix avoids clashing with user variables.
 */


/* VEC_ITER(vec, Type, ptr)
 * Iterates forward. ptr is a Type* pointing into the slot.
 * Example:
 *   VEC_ITER(v, int, p) { printf("%d\n", *p); }
 */
#define VEC_ITER(vec, T, ptr)                                                              \
    for (vec_iter _vi_ = {0}; vec_iter_init(&_vi_, (vec)), vec_iter_valid(&_vi_); (void)0) \
        for (T* ptr = (T*)vec_iter_get(&_vi_); ptr;                                        \
             vec_iter_next(&_vi_), ptr = vec_iter_valid(&_vi_) ? (T*)vec_iter_get(&_vi_) : NULL)


/* VEC_ITER_REV(vec, Type, ptr)
 * Iterates in reverse (last element first).
 * Example:
 *   VEC_ITER_REV(v, int, p) { printf("%d\n", *p); }
 */
#define VEC_ITER_REV(vec, T, ptr)                                                                 \
    for (vec_iter _vir_ = {0}; vec_iter_init_rev(&_vir_, (vec)), vec_iter_valid(&_vir_); (void)0) \
        for (T* ptr = (T*)vec_iter_get(&_vir_); ptr;                                              \
             vec_iter_next(&_vir_), ptr = vec_iter_valid(&_vir_) ? (T*)vec_iter_get(&_vir_) : NULL)


/* MAP_ITER(map, KeyType, kptr, ValType, vptr)
 * Yields a typed const key pointer and a typed mutable value pointer
 * together in one loop body — the main advantage over the old
 * MAP_FOREACH_KEY / MAP_FOREACH_VAL split.
 *
 * Example:
 *   MAP_ITER(m, int, k, float, v) { printf("%d => %.1f\n", *k, *v); }
 */
#define MAP_ITER(map, KT, kptr, VT, vptr)                                                                \
    for (map_iter _mi_ = {0}; map_iter_init(&_mi_, (map)), map_iter_valid(&_mi_); (void)0)               \
        for (const KT *kptr = (const KT*)map_iter_key(&_mi_), *_mi_ksentinel_ = kptr; _mi_ksentinel_;    \
             map_iter_next(&_mi_), kptr = map_iter_valid(&_mi_) ? (const KT*)map_iter_key(&_mi_) : NULL, \
                      _mi_ksentinel_ = kptr)                                                             \
            for (VT* vptr = (VT*)map_iter_val(&_mi_); vptr; vptr = NULL)


/* SET_ITER(set, Type, ptr)
 * Yields a typed const element pointer.
 * Example:
 *   SET_ITER(s, int, p) { printf("%d\n", *p); }
 */
#define SET_ITER(set, T, ptr)                                                              \
    for (set_iter _si_ = {0}; set_iter_init(&_si_, (set)), set_iter_valid(&_si_); (void)0) \
        for (const T* ptr = (const T*)set_iter_get(&_si_); ptr;                            \
             set_iter_next(&_si_), ptr = set_iter_valid(&_si_) ? (const T*)set_iter_get(&_si_) : NULL)


#endif // ITER_H
