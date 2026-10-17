#ifndef STACK_H
#define STACK_H

#include "common.h"
#include "gen_vector.h"
#include "wc_allocator.h"
#include "wc_comp.h"


// Stack is just a thin wrapper around GenVec
typedef GenVec Stack;

wc_nodiscard wc_nonull(1) Stack Stack_create(const wc_allocator* a, u64 n, u32 data_size, const wc_container_ops* ops);
wc_nodiscard wc_nonull(1, 3) Stack Stack_create_val(const wc_allocator* a, u64 n, const void* val, u32 data_size, const wc_container_ops* ops);

wc_nonull(1) void  Stack_destroy(Stack* stk);
wc_nodiscard wc_nonull(1, 2) Stack Stack_copy(const wc_allocator* a, const Stack* src);
wc_nonull(1, 2) void  Stack_move(Stack* dest, Stack* src);

wc_nonull(1) void Stack_clear(Stack* stk);
wc_nonull(1) void Stack_reset(Stack* stk);

wc_nonull(1, 2) void        Stack_push(Stack* stk, const void* x);
wc_nonull(1, 2) void        Stack_push_move(Stack* stk, void* x); // *x is moved in and zeroed
wc_nonull(1) void        Stack_pop(Stack* stk, void* popped);
wc_nonull(1, 2) void        Stack_peek(Stack* stk, void* peek);
wc_nodiscard wc_nonull(1) const void* Stack_peek_ptr(const Stack* stk);

wc_nodiscard wc_nonull(1) static wc_always_inline u64 Stack_size(const Stack* stk)
{
    return GenVec_size(stk);
}
wc_nodiscard wc_nonull(1) static wc_always_inline bool Stack_empty(const Stack* stk)
{
    return GenVec_empty(stk);
}
wc_nodiscard wc_nonull(1) static wc_always_inline u64 Stack_capacity(const Stack* stk)
{
    return GenVec_capacity(stk);
}

wc_nonull(1, 2) void Stack_print(Stack* stk, wc_print_fn print_fn);

#endif // STACK_H
