#include "Stack.h"
#include "common.h"
#include "gen_vector.h"



Stack* stack_create(u64 n, u32 data_size, const container_ops* ops)
{
    return genVec_create(n, data_size, ops);
}

Stack* stack_create_val(u64 n, const u8* val, u32 data_size, const container_ops* ops)
{
    return genVec_create_val(n, val, data_size, ops);
}


void stack_destroy(Stack* stk)
{
    genVec_destroy(stk);
}

void stack_clear(Stack* stk)
{
    genVec_clear(stk);
}

void stack_reset(Stack* stk)
{
    genVec_reset(stk);
}

void stack_push(Stack* stk, const u8* x)
{
    genVec_push(stk, x);
}

void stack_push_move(Stack* stk, u8** x)
{
    genVec_push_move(stk, x);
}

void stack_pop(Stack* stk, u8* popped)
{
    genVec_pop(stk, popped);
}

void stack_peek(Stack* stk, u8* peek)
{
    CHECK_FATAL(!stk, "stack is null");
    CHECK_FATAL(!peek, "peek is null");
    WC_SET_RET(WC_ERR_EMPTY, stack_empty(stk), );
    genVec_get(stk, genVec_size(stk) - 1, peek);
}

const u8* stack_peek_ptr(const Stack* stk)
{
    CHECK_FATAL(!stk, "stack is null");
    WC_SET_RET(WC_ERR_EMPTY, stack_empty(stk), NULL);
    return genVec_get_ptr(stk, genVec_size(stk) - 1);
}

void stack_print(Stack* stk, print_fn print)
{
    genVec_print(stk, print);
}