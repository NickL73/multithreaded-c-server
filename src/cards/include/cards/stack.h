/**
* @file stack.h
 * @author nick
 * @date 9/30/25
 * @brief
 */

#ifndef CARDS_STACK_H
#define CARDS_STACK_H

#include <stdbool.h>
#include <stddef.h>

#include "cards/cards.h"

typedef cards_base_s cards_stack_s;

static inline cards_err_e cards_stack_init(cards_stack_s * p_stack, const size_t capacity, const bool b_enable_resize)
{
    return cards_init(p_stack, capacity, b_enable_resize);
}

static inline cards_err_e cards_stack_deinit(cards_stack_s * p_stack)
{
    return cards_deinit(p_stack);
}

static inline cards_err_e cards_stack_push(cards_stack_s * p_stack, void * p_item)
{
    return cards_insert_at(p_stack, p_stack->num_items, p_item);
}

static inline cards_err_e cards_stack_pop(cards_stack_s * p_stack, void ** pp_item)
{
    return cards_remove_at(p_stack, p_stack->num_items - 1, pp_item);
}

static inline cards_err_e cards_stack_peek(const cards_stack_s * p_stack, void ** pp_item)
{
    return cards_peek_at(p_stack, p_stack->num_items - 1, pp_item);
}


#endif // CARDS_STACK_H
