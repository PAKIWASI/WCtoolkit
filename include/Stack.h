#ifndef STACK_H
#define STACK_H

#include "common.h"
#include "gen_vector.h"
#include "wc_errno.h"


// Stack is just a thin wrapper around genVec
typedef genVec Stack;


Stack* stack_create(u64 n, u32 data_size, const container_ops* ops);
Stack* stack_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops) __attribute__((nonnull(2)));

void stack_destroy(Stack* stk) __attribute__((nonnull(1)));
void stack_clear(Stack* stk) __attribute__((nonnull(1)));
void stack_reset(Stack* stk) __attribute__((nonnull(1)));

void      stack_push(Stack* stk, const u8* x) __attribute__((nonnull(1, 2)));
void      stack_push_move(Stack* stk, u8** x) __attribute__((nonnull(1, 2)));
void      stack_pop(Stack* stk, u8* popped) __attribute__((nonnull(1)));
void      stack_peek(Stack* stk, u8* peek) __attribute__((nonnull(1, 2)));
const u8* stack_peek_ptr(const Stack* stk) __attribute__((nonnull(1)));

static inline u64 stack_size(const Stack* stk) __attribute__((nonnull(1)))     { return genVec_size(stk);     }
static inline u8  stack_empty(const Stack* stk) __attribute__((nonnull(1)))    { return genVec_empty(stk);    }
static inline u64 stack_capacity(const Stack* stk) __attribute__((nonnull(1))) { return genVec_capacity(stk); }

void stack_print(Stack* stk, print_fn print_fn) __attribute__((nonnull(1, 2)));


#endif // STACK_H
