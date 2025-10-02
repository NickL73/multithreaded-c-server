/**
 * @file queue.h
 * @author nick
 * @date 9/30/25
 * @brief
 */

#ifndef CARDS_QUEUE_H
#define CARDS_QUEUE_H

#include "cards/cards.h"

#include <stdbool.h>
#include <stddef.h>

typedef cards_base_s cards_queue_s;

static inline cards_err_e cards_queue_init(cards_queue_s * p_queue, const size_t capacity, const bool b_enable_resize)
{
    return cards_init(p_queue, capacity, b_enable_resize);
}

static inline cards_err_e cards_queue_deinit(cards_queue_s * p_queue)
{
    return cards_deinit(p_queue);
}

static inline cards_err_e cards_queue_enqueue(cards_queue_s * p_queue, void * p_item)
{
    return cards_insert_at(p_queue, p_queue->num_items, p_item);
}

static inline cards_err_e cards_queue_dequeue(cards_queue_s * p_queue, void ** pp_item)
{
    return cards_remove_at(p_queue, 0, pp_item);
}

static inline cards_err_e cards_queue_peek(const cards_queue_s * p_queue, void ** pp_item)
{
    return cards_peek_at(p_queue, 0, pp_item);
}

#endif // CARDS_QUEUE_H
