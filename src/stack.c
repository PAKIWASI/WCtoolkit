#include "stack.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_errno.h"



Stack Stack_create(wc_allocator a, u64 n, u32 data_size, const wc_container_ops* ops)
{
    return GenVec_create(a, n, data_size, ops);
}

Stack Stack_create_val(wc_allocator a, u64 n, const u8* val, u32 data_size, const wc_container_ops* ops)
{
    return GenVec_create_val(a, n, val, data_size, ops);
}

void Stack_destroy(Stack* stk)
{
    GenVec_destroy(stk);
}

Stack Stack_copy(wc_allocator a, const Stack* src)
{
    return GenVec_copy(a, src);
}

void Stack_move(Stack* dest, Stack* src)
{
    GenVec_move(dest, src);
}

void Stack_clear(Stack* stk)
{
    GenVec_clear(stk);
}

void Stack_reset(Stack* stk)
{
    GenVec_reset(stk);
}

void Stack_push(Stack* stk, const u8* x)
{
    GenVec_push(stk, x);
}

void Stack_push_move(Stack* stk, u8* x)
{
    GenVec_push_move(stk, x);
}

void Stack_pop(Stack* stk, u8* popped)
{
    GenVec_pop(stk, popped);
}

void Stack_peek(Stack* stk, u8* peek)
{
    WC_SET_RET(WC_ERR_EMPTY, Stack_empty(stk), );
    GenVec_get(stk, GenVec_size(stk) - 1, peek);
}

const u8* Stack_peek_ptr(const Stack* stk)
{
    WC_SET_RET(WC_ERR_EMPTY, Stack_empty(stk), NULL);
    return GenVec_get_ptr(stk, GenVec_size(stk) - 1);
}

void Stack_print(Stack* stk, wc_print_fn print)
{
    GenVec_print(stk, print);
}
