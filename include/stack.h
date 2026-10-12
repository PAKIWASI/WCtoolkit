#ifndef STACK_H
#define STACK_H

#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"


// Stack is just a thin wrapper around GenVec
typedef GenVec Stack;

Stack Stack_create(const wc_allocator* a, u64 n, u32 data_size, const wc_container_ops* ops)
    __attribute__((warn_unused_result));
Stack Stack_create_val(const wc_allocator* a, u64 n, const void* val, u32 data_size, const wc_container_ops* ops)
    __attribute__((nonnull(3), warn_unused_result));

void  Stack_destroy(Stack* stk) WC_NONULL(1);
Stack Stack_copy(const wc_allocator* a, const Stack* src) __attribute__((nonnull(2), warn_unused_result));
void  Stack_move(Stack* dest, Stack* src) WC_NONULL(1, 2);

void Stack_clear(Stack* stk) WC_NONULL(1);
void Stack_reset(Stack* stk) WC_NONULL(1);

void        Stack_push(Stack* stk, const void* x) WC_NONULL(1, 2);
void        Stack_push_move(Stack* stk, void* x) WC_NONULL(1, 2); // *x is moved in and zeroed
void        Stack_pop(Stack* stk, void* popped) WC_NONULL(1);
void        Stack_peek(Stack* stk, void* peek) WC_NONULL(1, 2);
const void* Stack_peek_ptr(const Stack* stk) WC_NONULL(1);

static inline WC_NONULL(1) u64 Stack_size(const Stack* stk)
{
    return GenVec_size(stk);
}
static inline WC_NONULL(1) bool Stack_empty(const Stack* stk)
{
    return GenVec_empty(stk);
}
static inline WC_NONULL(1) u64 Stack_capacity(const Stack* stk)
{
    return GenVec_capacity(stk);
}

void Stack_print(Stack* stk, wc_print_fn print_fn) WC_NONULL(1, 2);

#endif // STACK_H
