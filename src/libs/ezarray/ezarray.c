//
// Created by nick on 7/15/25.
//

#include "ezarray.h"

#include "utils.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

static int ezarr_resize(ezarray_t * p_ezarray);

int ezarr_init(ezarray_t * p_ezarray, uint16_t max_items)
{
    int     res    = -1;
    void ** pp_buf = NULL;

    if ((NULL == p_ezarray) || (0 == max_items))
    {
        LOG_ERROR("Invalid arguments");
        goto end;
    }

    pp_buf = calloc(max_items, sizeof(void *));
    if (NULL == pp_buf)
    {
        LOG_ERROR("Failed to allocate memory");
        goto end;
    }

    p_ezarray->pp_buf    = pp_buf;
    p_ezarray->max_items = max_items;
    p_ezarray->num_items = 0;

    res = 0;

end:
    return res;
}

int ezarr_deinit(ezarray_t * p_ezarray)
{
    int res = -1;
    if (NULL != p_ezarray)
    {
        free(p_ezarray->pp_buf);
        p_ezarray->pp_buf    = NULL;
        p_ezarray->num_items = 0;
        p_ezarray->max_items = 0;

        res = 0;
    }

    else
    {
        LOG_ERROR("Invalid arguments");
    }

    return res;
}

int ezarr_push(ezarray_t * p_ezarray, void * p_item)
{
    int res = -1;

    if (NULL == p_ezarray)
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    if (p_ezarray->num_items == p_ezarray->max_items)
    {
        res = ezarr_resize(p_ezarray);
        if (0 != res)
        {
            LOG_ERROR("Failed to resize ezarray");
            goto end;
        }
    }

    p_ezarray->pp_buf[p_ezarray->num_items] = p_item;
    p_ezarray->num_items++;

    res = 0;

end:
    return res;
}

int ezarr_set_at(ezarray_t * p_ezarray, uint16_t idx, void * p_item)
{
    int res = -1;

    if (NULL == p_ezarray)
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    if (idx >= p_ezarray->max_items)
    {
        LOG_ERROR("Index out of bounds");
        goto end;
    }

    p_ezarray->pp_buf[idx] = p_item;
    res                    = 0;

end:
    return res;
}

int ezarr_get_at(ezarray_t * p_ezarray, uint16_t idx, void ** pp_out)
{
    int res = -1;

    if ((NULL == p_ezarray) || (NULL == pp_out))
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    if (idx >= p_ezarray->max_items)
    {
        LOG_ERROR("Index out of bounds");
        goto end;
    }

    *pp_out = p_ezarray->pp_buf[idx];
    res     = 0;

end:
    return res;
}

int ezarr_compact(ezarray_t * p_ezarray, void * p_sentinel)
{
    int      res       = -1;
    uint16_t write_idx = 0;

    if ((NULL == p_ezarray) || (NULL == p_ezarray->pp_buf) || (0 == p_ezarray->max_items))
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    for (uint16_t read_idx = 0; read_idx < p_ezarray->max_items; read_idx++)
    {
        if (p_ezarray->pp_buf[read_idx] != p_sentinel)
        {
            if (write_idx != read_idx)
            {
                p_ezarray->pp_buf[write_idx] = p_ezarray->pp_buf[read_idx];
                p_ezarray->pp_buf[read_idx]  = p_sentinel;
            }
            write_idx++;
        }
    }

    p_ezarray->num_items = write_idx;
    res                  = 0;

end:
    return res;
}

/* STATIC FUNCTION DEFINITIONS */
static int ezarr_resize(ezarray_t * p_ezarray)
{
    assert(p_ezarray);

    int      res     = -1;
    void **  pp_tmp  = NULL;
    uint16_t new_max = 0;

    if (UINT16_MAX == p_ezarray->num_items)
    {
        LOG_ERROR("Array at max size.");
        goto end;
    }

    if ((UINT16_MAX / 2) < p_ezarray->num_items)
    {
        LOG_WARN(
          "Resizing array by double would overflow. Will attempt to resize to UINT16_MAX. Future resizes will fail.");
        new_max = UINT16_MAX;
    }

    else
    {
        new_max = p_ezarray->num_items * 2;
    }

    pp_tmp = realloc(p_ezarray->pp_buf, new_max * sizeof(void *));
    if (NULL == pp_tmp)
    {
        LOG_ERROR("Failed to allocate memory");
        goto end;
    }

    p_ezarray->pp_buf    = pp_tmp;
    p_ezarray->max_items = new_max;

    res = 0;

end:
    return res;
}
