/**
 * @file ezqueue.c
 * @author nick
 * @date 7/13/25
 * @brief
 */
#include "ezqueue.h"

#include "utils.h"

#include <stdint.h>
#include <stdlib.h>

static int ezq_resize(ezqueue_t * p_queue);

int ezq_init(ezqueue_t * p_ezqueue, uint16_t max_items)
{
    int     res   = -1;
    void ** p_buf = NULL;

    if ((NULL == p_ezqueue) || (0 == max_items))
    {
        LOG_ERROR("Invalid arguments");
        goto end;
    }

    p_buf = malloc(max_items * sizeof(void *));
    if (NULL == p_buf)
    {
        LOG_ERROR("Failed to allocate memory");
        goto end;
    }

    p_ezqueue->pp_buf = p_buf;
    p_buf             = NULL;

    p_ezqueue->max_items = max_items;
    p_ezqueue->idx_head  = -1;
    p_ezqueue->idx_tail  = -1;
    p_ezqueue->num_items = 0;

    res = 0;

end:
    return res;
}

int ezq_deinit(ezqueue_t * p_ezqueue)
{
    int res = -1;
    if (NULL != p_ezqueue)
    {
        free(p_ezqueue->pp_buf);
        p_ezqueue->pp_buf    = NULL;
        p_ezqueue->num_items = 0;
        p_ezqueue->idx_head  = -1;
        p_ezqueue->idx_tail  = -1;
        p_ezqueue->max_items = 0;

        res = 0;
    }

    else
    {
        LOG_ERROR("Invalid arguments");
    }

    return res;
}

int ezq_enqueue(ezqueue_t * p_ezqueue, void * p_item)
{
    int res = -1;

    if (NULL == p_ezqueue)
    {
        LOG_ERROR("Invalid arguments");
        goto end;
    }

    if (p_ezqueue->num_items == p_ezqueue->max_items)
    {
        res = ezq_resize(p_ezqueue);
        if (0 != res)
        {
            LOG_ERROR("Failed to resize ezqueue");
            goto end;
        }
    }

    if (0 == p_ezqueue->num_items)
    {
        p_ezqueue->idx_head = 0;
        p_ezqueue->idx_tail = 0;
    }

    else
    {
        p_ezqueue->idx_tail = (p_ezqueue->idx_tail + 1) % (p_ezqueue->max_items);
    }

    p_ezqueue->pp_buf[p_ezqueue->idx_tail] = p_item;
    p_ezqueue->num_items++;

    res = 0;

end:
    return res;
}

int ezq_dequeue(ezqueue_t * p_ezqueue, void ** pp_out)
{
    int res = -1;
    if ((NULL == p_ezqueue) || (NULL == pp_out))
    {
        LOG_ERROR("Invalid arguments");
        goto end;
    }

    if (0 == p_ezqueue->num_items)
    {
        LOG_ERROR("Trying to dequeue from an empty ezqueue");
        goto end;
    }

    *pp_out = p_ezqueue->pp_buf[p_ezqueue->idx_head];
    if (1 == p_ezqueue->num_items)
    {
        p_ezqueue->idx_head = -1;
        p_ezqueue->idx_tail = -1;
    }

    else
    {
        p_ezqueue->idx_head = (p_ezqueue->idx_head + 1) % (p_ezqueue->max_items);
    }

    p_ezqueue->num_items--;
    res = 0;

end:
    return res;
}

int ezq_clear(ezqueue_t * p_ezqueue, void (*free_func)(void *))
{
    int    res    = -1;
    void * p_item = NULL;
    if (NULL == p_ezqueue)
    {
        LOG_ERROR("Invalid arguments");
        goto end;
    }

    while (0 < p_ezqueue->num_items)
    {
        (void)ezq_dequeue(p_ezqueue, &p_item);
        if ((NULL != p_item) && (NULL != free_func))
        {
            free_func(p_item);
        }

        p_item = NULL;
    }

    res = 0;

end:
    return res;
}

static int ezq_resize(ezqueue_t * p_queue)
{
    int      res       = -1;
    void **  pp_newbuf = NULL;
    uint16_t new_max   = 0;

    if (NULL == p_queue)
    {
        LOG_ERROR("Invalid arguments");
        goto end;
    }

    if (UINT16_MAX == p_queue->max_items)
    {
        LOG_ERROR("Queue at max size.");
        goto end;
    }

    if ((UINT16_MAX / 2) < p_queue->max_items)
    {
        LOG_WARN(
          "Resizing queue by double would overflow. Will attempt to resize to UINT16_MAX. Future resizes will fail.");
        new_max = UINT16_MAX;
    }

    else
    {
        new_max = p_queue->max_items * 2;
    }

    pp_newbuf = malloc(new_max * sizeof(void *));
    if (NULL == pp_newbuf)
    {
        LOG_ERROR("Failed to allocate memory");
        goto end;
    }

    for (int idx = 0; idx < p_queue->num_items; idx++)
    {
        pp_newbuf[idx] = p_queue->pp_buf[(p_queue->idx_head + idx) % (p_queue->max_items)];
    }

    free(p_queue->pp_buf);
    p_queue->pp_buf = pp_newbuf;
    pp_newbuf       = NULL;

    p_queue->max_items = new_max;
    p_queue->idx_head  = 0;
    p_queue->idx_tail  = p_queue->num_items - 1;

    res = 0;

end:
    return res;
}
