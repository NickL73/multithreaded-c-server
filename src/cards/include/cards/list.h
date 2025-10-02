/**
 * @file list.h
 * @author nick
 * @date 9/30/25
 * @brief
 */

#ifndef CARDS_LIST_H
#define CARDS_LIST_H

#include "cards/cards.h"

#include <stdbool.h>
#include <stddef.h>

typedef cards_base_s cards_list_s;

static inline cards_err_e cards_list_init(cards_list_s * p_list, const size_t capacity, const bool b_enable_resize)
{
    return cards_init(p_list, capacity, b_enable_resize);
}

static inline cards_err_e cards_list_deinit(cards_list_s * p_list)
{
    return cards_deinit(p_list);
}

static inline cards_err_e cards_list_prepend(cards_list_s * p_list, void * p_item)
{
    return cards_insert_at(p_list, 0, p_item);
}

static inline cards_err_e cards_list_append(cards_list_s * p_list, void * p_item)
{
    return cards_insert_at(p_list, p_list->num_items, p_item);
}

static inline cards_err_e cards_list_insert_at(cards_list_s * p_list, const size_t idx, void * p_item)
{
    return cards_insert_at(p_list, idx, p_item);
}

static inline cards_err_e cards_list_remove(cards_list_s * p_list, const size_t idx, void ** pp_item)
{
    return cards_remove_at(p_list, idx, pp_item);
}

static inline cards_err_e cards_list_get(const cards_list_s * p_list, const size_t idx, void ** pp_item)
{
    return cards_peek_at(p_list, idx, pp_item);
}

#endif // CARD_LIST_H
