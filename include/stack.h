#ifndef STACK_H
#define STACK_H

#include "common.h"
#include "gen_vector.h"


// Stack is just a thin wrapper around GenVec
typedef GenVec Stack;


Stack* Stack_create(u64 n, u32 data_size, const wc_container_ops* ops) __attribute__((warn_unused_result));
Stack* Stack_create_val(u64 n, const u8* val, u32 data_size, const wc_container_ops* ops) __attribute__((nonnull(2), warn_unused_result));

void Stack_destroy(Stack* stk) __attribute__((nonnull(1)));
void Stack_clear(Stack* stk) __attribute__((nonnull(1)));
void Stack_reset(Stack* stk) __attribute__((nonnull(1)));

void      Stack_push(Stack* stk, const u8* x) __attribute__((nonnull(1, 2)));
void      Stack_push_move(Stack* stk, u8** x) __attribute__((nonnull(1, 2)));
void      Stack_pop(Stack* stk, u8* popped) __attribute__((nonnull(1)));
void      Stack_peek(Stack* stk, u8* peek) __attribute__((nonnull(1, 2)));
const u8* Stack_peek_ptr(const Stack* stk) __attribute__((nonnull(1)));

static inline __attribute__((nonnull(1))) u64 Stack_size(const Stack* stk) { return GenVec_size(stk);     }
static inline __attribute__((nonnull(1))) u8 Stack_empty(const Stack* stk) { return GenVec_empty(stk);    }
static inline __attribute__((nonnull(1))) u64 Stack_capacity(const Stack* stk) { return GenVec_capacity(stk); }

void Stack_print(Stack* stk, print_fn print_fn) __attribute__((nonnull(1, 2)));


#endif // STACK_H
