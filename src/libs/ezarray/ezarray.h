//
// Created by nick on 7/15/25.
//

#ifndef EZARRAY_H
#define EZARRAY_H

#include <stdint.h>

typedef struct ezarray_t
{
    void **  pp_buf;
    uint16_t num_items;
    uint16_t max_items;
} ezarray_t;

int ezarr_init(ezarray_t * p_ezarray, uint16_t max_items);
int ezarr_deinit(ezarray_t * p_ezarray);

int ezarr_push(ezarray_t * p_ezarray, void * p_item);

int ezarr_set_at(ezarray_t * p_ezarray, uint16_t idx, void * p_item);
int ezarr_get_at(ezarray_t * p_ezarray, uint16_t idx, void ** pp_out);

int ezarr_compact(ezarray_t * p_ezarray, void * p_sentinel);
#endif // EZARRAY_H
