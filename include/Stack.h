#ifndef STACK_H
#define STACK_H

#include "gen_vector.h"


// Stack is just a thin wrapper around genVec
typedef genVec Stack;


Stack* stack_create(u64 n, u32 data_size, const genVec_ops* ops);
Stack* stack_create_val(u64 n, const u8* val, u32 data_size, const genVec_ops* ops);

void stack_destroy(Stack* stk);
void stack_clear(Stack* stk);
void stack_reset(Stack* stk);

void      stack_push(Stack* stk, const u8* x);
void      stack_push_move(Stack* stk, u8** x);
void      stack_pop(Stack* stk, u8* popped);
void      stack_peek(Stack* stk, u8* peek);
const u8* stack_peek_ptr(Stack* stk);

static inline u64 stack_size(Stack* stk)     { return genVec_size(stk);     }
static inline u8  stack_empty(Stack* stk)    { return genVec_empty(stk);    }
static inline u64 stack_capacity(Stack* stk) { return genVec_capacity(stk); }

void stack_print(Stack* stk, print_fn print_fn);


#endif // STACK_H
