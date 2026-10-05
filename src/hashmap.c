#include "hashmap.h"
#include "common.h"
#include "gen_vector.h"
#include "map_setup.h"
#include "wc_allocator.h"
#include "wc_string.h"

#include <stdio.h>
#include <string.h>


#define GET_KEY(map, i) ((map)->keys + ((u64)(map)->key_size * (i)))
#define GET_PSL(map, i) ((map)->psls + (i))
#define GET_VAL(map, i) ((map)->vals + ((u64)(map)->val_size * (i)))

// capacity is always power-of-2 — use bitmask instead of %
#define MAP_MASK(map)    ((map)->capacity - 1)
#define MAP_NEXT(map, i) (((i) + 1) & MAP_MASK(map))

// PSL 0 == empty bucket; stored PSL is (real_psl + 1), starting at 1
#define BUCKET_EMPTY 0

// Single block layout, every region aligned for what it holds:
//   [keys: cap * key_size][vals: cap * val_size][scratch][psls: cap bytes]
// keys/vals/scratch start on MAP_ALIGN; psls are bytes and go last.
//
// scratch layout (KPAD/VPAD = key/val size rounded up to MAP_ALIGN):
//   STAGE region: STAGE_KEY [0, KPAD), STAGE_VAL [KPAD, KPAD + VPAD)
//     used by put to stage the incoming key/val
//   SWAP region:  SWAP_KEY, SWAP_VAL, the same shape right after STAGE
//     used by map_insert for Robin Hood evictions
// The two regions must NOT overlap: map_insert is called with pointers INTO
// the STAGE region, and it writes displaced residents into the SWAP region.

_Static_assert((sizeof(String) == 48 && sizeof(GenVec) == 48) != 0, "update the 48-byte fast paths below");

static inline u64 map_align_up(u64 x, u64 a)
{
    return (x + (a - 1)) & ~(a - 1);
}

static inline u64 map_align(u32 key_size, u32 val_size)
{
    u64 ka = wc_align_for_size(key_size);
    u64 va = wc_align_for_size(val_size);
    return ka > va ? ka : va;
}

#define MAP_ALIGN(map) map_align((map)->key_size, (map)->val_size)
#define KPAD(map)      map_align_up((map)->key_size, MAP_ALIGN(map))
#define VPAD(map)      map_align_up((map)->val_size, MAP_ALIGN(map))

#define STAGE_KEY(map) ((map)->scratch)
#define STAGE_VAL(map) ((map)->scratch + KPAD(map))
#define SWAP_KEY(map)  ((map)->scratch + KPAD(map) + VPAD(map))
#define SWAP_VAL(map)  ((map)->scratch + KPAD(map) + VPAD(map) + KPAD(map))

static inline u64 map_scratch_size(u32 key_size, u32 val_size)
{
    u64 al = map_align(key_size, val_size);
    return 2 * (map_align_up(key_size, al) + map_align_up(val_size, al));
}

typedef struct {
    u64 vals, scratch, psls, total; // byte offsets from the block start, and its size
} map_layout;

static inline map_layout map_layout_for(u64 cap, u32 key_size, u32 val_size)
{
    u64        al = map_align(key_size, val_size);
    map_layout l;
    l.vals    = map_align_up(cap * key_size, al);
    l.scratch = map_align_up(l.vals + (cap * val_size), al);
    l.psls    = l.scratch + map_scratch_size(key_size, val_size);
    l.total   = l.psls + cap;
    return l;
}

// One allocation for the whole table; psls zeroed (all buckets empty).
static void map_alloc_block(HashMap* map, u64 cap)
{
    map_layout l = map_layout_for(cap, map->key_size, map->val_size);
    u8*        b = wc_alloc(map->alloc, l.total, MAP_ALIGN(map));
    FATAL_IF(!b, "HashMap: table allocation of %llu bytes failed", (unsigned long long)l.total);

    map->keys     = b;
    map->vals     = b + l.vals;
    map->scratch  = b + l.scratch;
    map->psls     = b + l.psls;
    map->capacity = cap;
    memset(map->psls, 0, cap);
}

static void map_free_block(const HashMap* map, u8* block, u64 cap)
{
    wc_free(map->alloc, block, map_layout_for(cap, map->key_size, map->val_size).total, MAP_ALIGN(map));
}

// Move one element into `dest` (memcpy). `src` is left zeroed.
static inline void map_move_into(u32 size, u8* dest, u8* src)
{
    memcpy(dest, src, size);
    memset(src, 0, size);
}

// Copy one element into raw `dest`: copy_fn if any, else memcpy
static inline void map_copy_into(const HashMap* map, const wc_container_ops* ops, u32 size, u8* dest, const void* src)
{
    wc_copy_fn cp = ops ? ops->copy_fn : NULL;
    if (cp) {
        cp(map->alloc, dest, src);
    } else {
        memcpy(dest, src, size);
    }
}

// Delete one element in place (if it owns anything).
static inline void map_delete_elm(const wc_container_ops* ops, u8* elm)
{
    wc_delete_fn del = ops ? ops->del_fn : NULL;
    if (del) {
        del(elm);
    }
}

// Destroy an incoming duplicate the container will not keep, and zero it.
static inline void map_consume_dup(const wc_container_ops* ops, u32 size, u8* elm)
{
    map_delete_elm(ops, elm);
    memset(elm, 0, size);
}


// A stored PSL is a u8, and 0 means empty. If an insert would push a probe length
// past PSL_LIMIT the hash function is degenerate (a healthy hash at load <= 0.75
// stays under ~30). Die loudly instead of letting the u8 wrap to BUCKET_EMPTY and
// corrupt the table.
#define PSL_LIMIT      250
#define PSL_OVERFLOW() FATAL("HashMap: probe length overflow (degenerate hash function)")

// Fixed-size copies for the common element sizes: the compiler turns the constant
// memcpy into plain loads/stores instead of a libc call with a runtime length.
static inline void map_cpy(void* d, const void* s, u32 n)
{
    switch (n) {
    case 1:
        memcpy(d, s, 1);
        break;
    case 2:
        memcpy(d, s, 2);
        break;
    case 4:
        memcpy(d, s, 4);
        break;
    case 8:
        memcpy(d, s, 8);
        break;
    case 16:
        memcpy(d, s, 16);
        break;
    case 24:
        memcpy(d, s, 24);
        break;
    case 32:
        memcpy(d, s, 32);
        break;
    case 48:
        memcpy(d, s, 48); // sizeof(String), sizeof(GenVec)
        break;
    case 64:
        memcpy(d, s, 64);
        break;
    default:
        memcpy(d, s, n);
        break;
    }
}

// TODO: change the default hash func and don't do this
//
// Default hash is called directly (inlinable) instead of through the pointer.
// The result is ALWAYS multiplied by the golden-ratio constant (Fibonacci hashing),
// including the default wyhash: the bucket index below comes from the TOP bits, and
// wyhash's top bits are weak on short sequential keys (a 4-byte key 0,1,2,... clusters
// badly). The multiply folds every input bit into the top bits. Custom hashes with
// weak high bits (small integers, shifted ids) are covered by the same step.
// `ks` is the key size: passed in so a specialised caller folds it to a constant.
static inline __attribute__((always_inline)) u64 map_hash_k(const HashMap* map, const void* key, u32 ks)
{
    u64 h = (map->hash_fn == wyhash) ? wyhash(key, ks) : map->hash_fn(key, ks);
    return h * 0x9E3779B97F4A7C15ULL;
}

// Home bucket = top log2(capacity) bits of the hash. Doubling the table then sends the
// element at old index i to new index 2i or 2i+1, so a resize writes the new table
// (nearly) sequentially instead of scattering random writes across it. capacity is a
// power of two >= HASHMAP_INIT_CAPACITY here (callers reject the zeroed state first).
static inline __attribute__((always_inline)) u64 map_idx_k(const HashMap* map, const void* key, u32 ks)
{
    return map_hash_k(map, key, ks) >> (64 - __builtin_ctzll(map->capacity));
}

// Default compare on 4/8 byte keys is a single integer compare.
static inline __attribute__((always_inline)) bool map_keyeq_k(const HashMap* map, const u8* a, const u8* b, u32 ks)
{
    if (map->cmp_fn == default_compare) {
        switch (ks) {
        case 4: {
            u32 x, y;
            memcpy(&x, a, 4);
            memcpy(&y, b, 4);
            return x == y;
        }
        case 8: {
            u64 x, y;
            memcpy(&x, a, 8);
            memcpy(&y, b, 8);
            return x == y;
        }
        default:
            return memcmp(a, b, ks) == 0;
        }
    }
    return map->cmp_fn(a, b, ks) == 0;
}

#define MAP_IDX(map, key) map_idx_k((map), (key), (map)->key_size)

// Run `body` once per common (key_size, val_size) pair with KS/VS as compile-time
// constants, and once generically. kinda like a template: the body is an
// always_inline function, so each arm is a fully specialised copy.
#define MAP_DISPATCH(map, call)                                    \
    switch (((u64)(map)->key_size << 32) | (u64)(map)->val_size) { \
    case ((u64)4 << 32) | 4: {                                     \
        enum { KS = 4, VS = 4 };                                   \
        call;                                                      \
        break;                                                     \
    }                                                              \
    case ((u64)8 << 32) | 8: {                                     \
        enum { KS = 8, VS = 8 };                                   \
        call;                                                      \
        break;                                                     \
    }                                                              \
    case ((u64)4 << 32) | 8: {                                     \
        enum { KS = 4, VS = 8 };                                   \
        call;                                                      \
        break;                                                     \
    }                                                              \
    case ((u64)8 << 32) | 4: {                                     \
        enum { KS = 8, VS = 4 };                                   \
        call;                                                      \
        break;                                                     \
    }                                                              \
    case ((u64)48 << 32) | 4: {                                    \
        enum { KS = 48, VS = 4 };                                  \
        call;                                                      \
        break;                                                     \
    }                                                              \
    case ((u64)48 << 32) | 48: {                                   \
        enum { KS = 48, VS = 48 };                                 \
        call;                                                      \
        break;                                                     \
    }                                                              \
    default: {                                                     \
        const u32 KS = (map)->key_size, VS = (map)->val_size;      \
        call;                                                      \
        break;                                                     \
    }                                                              \
    }



/*
====================PRIVATE DECLARATIONS====================
*/

static u64         map_lookup(const HashMap* map, const u8* key, LOOKUP_RES* res, u8* out_psl);
static void        map_insert(HashMap* map, u8* key, u8* val, u8 psl, u64 idx);
static inline void map_maybe_resize(HashMap* map);
static void        map_resize(HashMap* map, u64 new_capacity);


/*
====================PUBLIC FUNCTIONS====================
*/

HashMap HashMap_create(const wc_allocator* a, u32 key_size, u32 val_size, custom_hash_fn hash_fn, wc_compare_fn cmp_fn,
                       const wc_container_ops* key_ops, const wc_container_ops* val_ops)
{
    FATAL_IF(key_size == 0 || val_size == 0, "key/val size can't be 0");

    HashMap map = {
        .size     = 0,
        .key_size = key_size,
        .val_size = val_size,
        .hash_fn  = hash_fn ? hash_fn : wyhash,
        .cmp_fn   = cmp_fn ? cmp_fn : default_compare,
        .key_ops  = key_ops,
        .val_ops  = val_ops,
        .alloc    = a,
    };
    map_alloc_block(&map, HASHMAP_INIT_CAPACITY); // the one allocation
    return map;
}

void HashMap_destroy(HashMap* map)
{
    if (map->capacity == 0) {
        return;
    }

    wc_delete_fn k_del = MAP_DEL(map->key_ops);
    wc_delete_fn v_del = MAP_DEL(map->val_ops);
    if (k_del || v_del) {
        for (u64 i = 0; i < map->capacity; i++) {
            if (*GET_PSL(map, i) == BUCKET_EMPTY) {
                continue;
            }
            if (k_del) {
                k_del(GET_KEY(map, i));
            }
            if (v_del) {
                v_del(GET_VAL(map, i));
            }
        }
    }

    map_free_block(map, map->keys, map->capacity);
    memset(map, 0, sizeof(HashMap));
}

void HashMap_move(HashMap* dest, HashMap* src)
{
    if (dest == src) {
        return;
    }
    *dest = *src;
    memset(src, 0, sizeof(HashMap));
}


// Insert the staged key/val (already owned by the map) at the lookup result.
static inline void map_commit(HashMap* map, u8 psl, u64 slot)
{
    map_insert(map, STAGE_KEY(map), STAGE_VAL(map), psl, slot);
    map_maybe_resize(map);
}

// TODO: do proper boolean returns!

// Insert or update — COPY semantics (B8: both sides keep the value).
// The map deep-copies key and val; the caller keeps its own.
// Returns 1 if key existed (updated), 0 if new key inserted.
bool HashMap_put(HashMap* map, const void* key, const void* val)
{
    FATAL_IF(map->capacity == 0, "HashMap mutation on zeroed/moved-from table");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res == FOUND) {
        map_delete_elm(map->val_ops, GET_VAL(map, slot));
        map_copy_into(map, map->val_ops, map->val_size, GET_VAL(map, slot), val);
        return 1;
    }

    map_copy_into(map, map->key_ops, map->key_size, STAGE_KEY(map), key);
    map_copy_into(map, map->val_ops, map->val_size, STAGE_VAL(map), val);
    map_commit(map, out_psl, slot);
    return 0;
}


// Insert or update — MOVE semantics: the map takes *key and *val, both zeroed.
// A duplicate key is destroyed (the map keeps its own, B10).
bool HashMap_put_move(HashMap* map, void* key, void* val)
{
    FATAL_IF(map->capacity == 0, "HashMap mutation on zeroed/moved-from table");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res == FOUND) {
        map_delete_elm(map->val_ops, GET_VAL(map, slot));
        map_move_into(map->val_size, GET_VAL(map, slot), val);
        map_consume_dup(map->key_ops, map->key_size, key);
        return 1;
    }

    map_move_into(map->key_size, STAGE_KEY(map), key);
    map_move_into(map->val_size, STAGE_VAL(map), val);
    map_commit(map, out_psl, slot);
    return 0;
}


// Mixed: key is COPIED, val is MOVED (*val zeroed).
bool HashMap_put_val_move(HashMap* map, const void* key, void* val)
{
    FATAL_IF(map->capacity == 0, "HashMap mutation on zeroed/moved-from table");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res == FOUND) {
        map_delete_elm(map->val_ops, GET_VAL(map, slot));
        map_move_into(map->val_size, GET_VAL(map, slot), val);
        return 1;
    }

    map_copy_into(map, map->key_ops, map->key_size, STAGE_KEY(map), key);
    map_move_into(map->val_size, STAGE_VAL(map), val);
    map_commit(map, out_psl, slot);
    return 0;
}


// Mixed: key is MOVED (*key zeroed, or destroyed if already present), val is COPIED.
bool HashMap_put_key_move(HashMap* map, void* key, const void* val)
{
    FATAL_IF(map->capacity == 0, "HashMap mutation on zeroed/moved-from table");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res == FOUND) {
        map_delete_elm(map->val_ops, GET_VAL(map, slot));
        map_copy_into(map, map->val_ops, map->val_size, GET_VAL(map, slot), val);
        map_consume_dup(map->key_ops, map->key_size, key);
        return 1;
    }

    map_move_into(map->key_size, STAGE_KEY(map), key);
    map_copy_into(map, map->val_ops, map->val_size, STAGE_VAL(map), val);
    map_commit(map, out_psl, slot);
    return 0;
}


// Get value for key — deep COPY into val (B8: the map keeps its own).
// Returns 1 if found, 0 if not. Caller owns the copy.
bool HashMap_get(const HashMap* map, const void* key, void* val)
{
    if (map->capacity == 0) {
        return 0;
    }

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    map_copy_into(map, map->val_ops, map->val_size, val, GET_VAL(map, slot));
    return 1;
}


// Get pointer to value in-place (read-only). Returns NULL if not found.
// The pointer is valid until the next mutation (put/del/resize).
// Do NOT free the returned pointer — the map owns it.
const void* HashMap_get_ptr(const HashMap* map, const void* key)
{
    if (map->capacity == 0) {
        return NULL;
    }

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    return (res == FOUND) ? GET_VAL(map, slot) : NULL;
}

bool HashMap_bucket_occupied(const HashMap* map, u64 i)
{
    WC_ASSERT(i < map->capacity, "index out of bounds");
    return *GET_PSL(map, i) != BUCKET_EMPTY;
}

const void* HashMap_bucket_key_ptr(const HashMap* map, u64 i)
{
    WC_ASSERT(i < map->capacity, "index out of bounds");
    return GET_KEY(map, i);
}

void* HashMap_bucket_val_ptr(HashMap* map, u64 i)
{
    WC_ASSERT(i < map->capacity, "index out of bounds");
    return GET_VAL(map, i);
}


// Delete key.
// If out != NULL, the value is MOVED into it before deletion (caller takes ownership).
// If out == NULL, the value is destroyed via del_fn (or simply discarded for POD).
// Returns 1 if found and deleted, 0 if not found.
//
// Uses Robin Hood backward-shift deletion to maintain the probe-sequence invariant
// without tombstones: after removing a slot, we shift subsequent entries back one
// position as long as they have PSL > 1 (i.e. they are not sitting at their home slot).
bool HashMap_del(HashMap* map, const void* key, void* out)
{
    FATAL_IF(map->capacity == 0, "HashMap mutation on zeroed/moved-from table");

    LOOKUP_RES res;
    u8         out_psl;
    u64        slot = map_lookup(map, key, &res, &out_psl);

    if (res != FOUND) {
        return 0;
    }

    // B7: the value is MOVED into out (caller owns it), or deleted.
    if (out) {
        memcpy(out, GET_VAL(map, slot), map->val_size);
    } else {
        map_delete_elm(map->val_ops, GET_VAL(map, slot));
    }
    map_delete_elm(map->key_ops, GET_KEY(map, slot));

    // Backward-shift deletion: pull subsequent entries one slot back as long as
    // they have PSL > 1.  Entries at their home slot (PSL == 1) must not move.
    // This restores the Robin Hood invariant without tombstones.
    u64 cur = slot;
    for (;;) {
        u64 next     = MAP_NEXT(map, cur);
        u8  next_psl = *GET_PSL(map, next);

        // Stop if next slot is empty or the next entry is already at its home slot.
        if (next_psl <= 1) {
            *GET_PSL(map, cur) = BUCKET_EMPTY;
            break;
        }

        // Shift next entry one slot back; its PSL decreases by 1.
        *GET_PSL(map, cur) = next_psl - 1;
        memcpy(GET_KEY(map, cur), GET_KEY(map, next), map->key_size);
        memcpy(GET_VAL(map, cur), GET_VAL(map, next), map->val_size);

        cur = next;
    }

    map->size--;
    return 1;
}


// Check if key exists.
bool HashMap_has(const HashMap* map, const void* key)
{
    if (map->capacity == 0) {
        return 0;
    }

    LOOKUP_RES res;
    u8         out_psl;
    map_lookup(map, key, &res, &out_psl);
    return res == FOUND;
}


// Print all key-value pairs.
void HashMap_print(const HashMap* map, wc_print_fn key_print, wc_print_fn val_print)
{
    printf("\t=========\n");
    printf("\tSize: %lu / Capacity: %lu\n", (unsigned long)map->size, (unsigned long)map->capacity);
    printf("\t=========\n");

    for (u64 i = 0; i < map->capacity; i++) {
        if (*GET_PSL(map, i) == BUCKET_EMPTY) {
            continue;
        }
        putchar('\t');
        key_print(GET_KEY(map, i));
        printf(" => ");
        val_print(GET_VAL(map, i));
        putchar('\n');
    }

    printf("\t=========\n");
}


// Remove all elements, keep capacity.
// Destroys all keys and values via their del_fn callbacks, then zeroes the arrays.
void HashMap_clear(HashMap* map)
{
    FATAL_IF(map->capacity == 0, "HashMap_clear on zeroed/moved-from table");

    wc_delete_fn k_del = MAP_DEL(map->key_ops);
    wc_delete_fn v_del = MAP_DEL(map->val_ops);
    if (k_del || v_del) {
        for (u64 i = 0; i < map->capacity; i++) {
            if (*GET_PSL(map, i) == BUCKET_EMPTY) {
                continue;
            }
            if (k_del) {
                k_del(GET_KEY(map, i));
            }
            if (v_del) {
                v_del(GET_VAL(map, i));
            }
        }
    }

    memset(map->psls, 0, map->capacity);
    map->size = 0;
}


// Make room for n elements total without any further resize.
void HashMap_reserve(HashMap* map, u64 n)
{
    FATAL_IF(map->capacity == 0, "HashMap_reserve on zeroed/moved-from table");
    FATAL_IF(n > ((u64)1 << 56), "HashMap_reserve: n too large");

    u64 need = map->capacity;
    while (n * 4 >= need * 3) {
        need *= 2;
    }
    if (need > map->capacity) {
        map_resize(map, need);
    }
}


// Deep copy src into a new map allocated from `a` (A14). Same capacity, so
// every element keeps its bucket: no rehash.
HashMap HashMap_copy(const wc_allocator* a, const HashMap* src)
{
    if (src->capacity == 0) {
        return (HashMap){0};
    }

    HashMap dest = *src; // sizes, functions, ops
    dest.alloc   = a;
    map_alloc_block(&dest, src->capacity);

    wc_copy_fn k_cp = MAP_COPY(src->key_ops);
    wc_copy_fn v_cp = MAP_COPY(src->val_ops);

    if (!k_cp && !v_cp) {
        // Nothing to deep-copy: the layout is identical, so one memcpy of the block.
        memcpy(dest.keys, src->keys, map_layout_for(src->capacity, src->key_size, src->val_size).total);
        return dest;
    }

    for (u64 i = 0; i < src->capacity; i++) {
        u8 psl = *GET_PSL(src, i);
        if (psl == BUCKET_EMPTY) {
            continue;
        }
        *GET_PSL(&dest, i) = psl;
        map_copy_into(&dest, src->key_ops, src->key_size, GET_KEY(&dest, i), GET_KEY(src, i));
        map_copy_into(&dest, src->val_ops, src->val_size, GET_VAL(&dest, i), GET_VAL(src, i));
    }

    return dest;
}


/*
====================PRIVATE FUNCTIONS====================
*/

static inline void map_maybe_resize(HashMap* map)
{
    // integer multiply avoids float — equivalent to load > 0.75
    if (map->size * 4 >= map->capacity * 3) {
        map_resize(map, map->capacity * 2);
    }
}

static inline __attribute__((always_inline)) u64 map_lookup_k(const HashMap* map, const u8* key, LOOKUP_RES* res,
                                                              u8* out_psl, u32 ks)
{
    u64 idx = map_idx_k(map, key, ks);
    u8  psl = 1; // stored PSL=1 means real probe distance 0 (home slot)

    for (u64 i = idx;; i = MAP_NEXT(map, i)) {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == BUCKET_EMPTY) {
            *res     = NOT_FOUND;
            *out_psl = psl;
            return i;
        }

        if (slot_psl < psl) {
            // The resident has a lower PSL — it's closer to its home than we are.
            // Under Robin Hood, our key would have displaced this resident on insert,
            // so our key cannot exist at or beyond this slot.
            *res     = ROBINHOOD_EXIT;
            *out_psl = psl;
            return i;
        }

        if (map_keyeq_k(map, GET_KEY(map, i), key, ks)) {
            *res     = FOUND;
            *out_psl = psl;
            return i;
        }

        psl++;
    }
}

static u64 map_lookup(const HashMap* map, const u8* key, LOOKUP_RES* res, u8* out_psl)
{
    switch (map->key_size) {
    case 4:
        return map_lookup_k(map, key, res, out_psl, 4);
    case 8:
        return map_lookup_k(map, key, res, out_psl, 8);
    case 48:
        return map_lookup_k(map, key, res, out_psl, 48); // String keys
    default:
        return map_lookup_k(map, key, res, out_psl, map->key_size);
    }
}

// Insert key/val with the given starting psl at slot idx.
// key and val are already OWNED by the caller (staged copy or moved pointer).
// This function never calls copy/del: it only shuffles raw bytes between slots.
//
// Phase 1 walks to the first empty slot or the first resident we out-rank, with no
// copying. An empty slot is filled straight from key/val. Only when a resident must
// be displaced do we stage the in-hand element in scratch (Robin Hood, phase 2).
// Displaced residents are buffered in the SWAP half of scratch, disjoint from STAGE.
static inline __attribute__((always_inline)) void map_insert_k(HashMap* map, u8* key, u8* val, u8 psl0, u64 idx, u32 ks,
                                                               u32 vs)
{
    u32 psl = psl0;
    u64 i   = idx;

    for (;; i = MAP_NEXT(map, i)) {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == BUCKET_EMPTY) {
            *GET_PSL(map, i) = (u8)psl;
            map_cpy(GET_KEY(map, i), key, ks);
            map_cpy(GET_VAL(map, i), val, vs);
            map->size++;
            return;
        }
        if (slot_psl < psl) {
            break;
        }
        if (++psl > PSL_LIMIT) {
            PSL_OVERFLOW();
        }
    }

    u8* cur_key = STAGE_KEY(map);
    u8* cur_val = STAGE_VAL(map);
    u8* swp_key = SWAP_KEY(map);
    u8* swp_val = SWAP_VAL(map);

    // key/val may already be STAGE: only copy if not already there
    if (key != cur_key) {
        map_cpy(cur_key, key, ks);
    }
    if (val != cur_val) {
        map_cpy(cur_val, val, vs);
    }

    for (;; i = MAP_NEXT(map, i)) {
        u8 slot_psl = *GET_PSL(map, i);

        if (slot_psl == BUCKET_EMPTY) {
            *GET_PSL(map, i) = (u8)psl;
            map_cpy(GET_KEY(map, i), cur_key, ks);
            map_cpy(GET_VAL(map, i), cur_val, vs);
            map->size++;
            return;
        }

        if (slot_psl < psl) {
            // Evict into swp (disjoint from cur), put cur in the slot
            map_cpy(swp_key, GET_KEY(map, i), ks);
            map_cpy(swp_val, GET_VAL(map, i), vs);

            *GET_PSL(map, i) = (u8)psl;
            map_cpy(GET_KEY(map, i), cur_key, ks);
            map_cpy(GET_VAL(map, i), cur_val, vs);

            // Evicted resident becomes the in-hand element
            u8* tmp = cur_key;
            cur_key = swp_key;
            swp_key = tmp;
            tmp     = cur_val;
            cur_val = swp_val;
            swp_val = tmp;
            psl     = (u32)slot_psl;
        }

        if (++psl > PSL_LIMIT) {
            PSL_OVERFLOW();
        }
    }
}

static void map_insert(HashMap* map, u8* key, u8* val, u8 psl0, u64 idx)
{
    MAP_DISPATCH(map, map_insert_k(map, key, val, psl0, idx, KS, VS));
}

static inline __attribute__((always_inline)) void map_rehash_k(HashMap* map, const u8* old_keys, const u8* old_psls,
                                                               const u8* old_vals, u64 old_cap, u32 ks, u32 vs)
{
    for (u64 i = 0; i < old_cap; i++) {
        if (old_psls[i] == BUCKET_EMPTY) {
            continue;
        }
        u8* old_key = (u8*)old_keys + ((u64)ks * i);
        u8* old_val = (u8*)old_vals + ((u64)vs * i);
        map_insert_k(map, old_key, old_val, 1, map_idx_k(map, old_key, ks), ks, vs);
    }
}

// Rehash into a new array of new_capacity (must be power-of-2).
// Ownership transfers as raw bytes: no copy/del callbacks are invoked.
// Keys are already unique, so there is no lookup and no compare: each element goes
// straight to its home slot and Robin Hood insertion does the rest.
static void map_resize(HashMap* map, u64 new_capacity)
{
    if (new_capacity < HASHMAP_INIT_CAPACITY) {
        new_capacity = HASHMAP_INIT_CAPACITY;
    }

    u8* old_block = map->keys;
    u8* old_vals  = map->vals;
    u8* old_psls  = map->psls;
    u64 old_cap   = map->capacity;

    map_alloc_block(map, new_capacity); // new keys/vals/scratch/psls in one allocation
    map->size = 0;

    MAP_DISPATCH(map, map_rehash_k(map, old_block, old_psls, old_vals, old_cap, KS, VS));

    map_free_block(map, old_block, old_cap);
}
