#include "Stack.h"
#include "common.h"
#include "gen_vector.h"
#include "wc_errno.h"



Stack* stack_create(u64 n, u32 data_size, const container_ops* ops)
{
    return GenVec_create(n, data_size, ops);
}

Stack* stack_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops)
{
    return GenVec_create_val(n, val, data_size, ops);
}


void stack_destroy(Stack* stk)
{
    GenVec_destroy(stk);
}

void stack_clear(Stack* stk)
{
    GenVec_clear(stk);
}

void stack_reset(Stack* stk)
{
    GenVec_reset(stk);
}

void stack_push(Stack* stk, const u8* x)
{
    GenVec_push(stk, x);
}

void stack_push_move(Stack* stk, u8** x)
{
    GenVec_push_move(stk, x);
}

void stack_pop(Stack* stk, u8* popped)
{
    GenVec_pop(stk, popped);
}

void stack_peek(Stack* stk, u8* peek)
{
    WC_SET_RET(WC_ERR_EMPTY, stack_empty(stk), );
    GenVec_get(stk, GenVec_size(stk) - 1, peek);
}

const u8* stack_peek_ptr(const Stack* stk)
{
    WC_SET_RET(WC_ERR_EMPTY, stack_empty(stk), NULL);
    return GenVec_get_ptr(stk, GenVec_size(stk) - 1);
}

void stack_print(Stack* stk, print_fn print)
{
    GenVec_print(stk, print);
}
