/**
 * @file ezqueue.h
 * @author nick
 * @date 7/13/25
 * @brief
 */
#ifndef EZQUEUE_H
#define EZQUEUE_H

#include <stdint.h>

typedef struct ezqueue_t
{
    void **  pp_buf;
    uint16_t num_items;
    uint16_t max_items;
    uint16_t idx_head;
    uint16_t idx_tail;
} ezqueue_t;

int ezq_init(ezqueue_t * p_ezqueue, uint16_t max_items);
int ezq_deinit(ezqueue_t * p_ezqueue);

int ezq_enqueue(ezqueue_t * p_ezqueue, void * item);
int ezq_dequeue(ezqueue_t * p_ezqueue, void ** pitem);

int ezq_clear(ezqueue_t * p_ezqueue);
#endif // EZQUEUE_H
