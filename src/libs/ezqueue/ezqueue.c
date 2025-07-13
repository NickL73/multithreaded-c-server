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

int ezq_init(ezqueue_t * p_ezqueue, uint16_t max_items)
{
    int     res   = -1;
    void ** p_buf = NULL;

    if ((NULL == p_ezqueue) || (0 == max_items))
    {
        LOG_ERROR("Invalid arguments\n");
        goto end;
    }

    p_buf = malloc(max_items * sizeof(void *));
    if (NULL == p_buf)
    {
        LOG_ERROR("Failed to allocate memory\n");
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
        LOG_ERROR("Invalid arguments\n");
    }

    return res;
}

int ezq_enqueue(ezqueue_t * p_ezqueue, void * item)
{
    return -1;
}

int ezq_dequeue(ezqueue_t * p_ezqueue, void ** pitem)
{
    return -1;
}

int ezq_clear(ezqueue_t * p_ezqueue, void (*free_func)(void *))
{
    int res = -1;
    if (NULL == p_ezqueue)
    {
        LOG_ERROR("Invalid arguments\n");
        goto end;
    }

end:
    return res;
}
