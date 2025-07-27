/**
 * @file connmgr.c
 * @author nick
 * @date 7/13/25
 * @brief
 */

#include "connmgr.h"

#include "utils.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

static int add_to_pollfd(conn_ctx_t * p_ctx, struct pollfd ** pp_pfds, uint16_t cur_size, uint16_t max_size);
static int connmgr_add_new_connections(conn_mgr_t * p_mgr);
static int connmgr_remove_closed_connections(conn_mgr_t * p_mgr);

int connmgr_init(conn_mgr_t * p_mgr, uint16_t initial_max_conns)
{
    int err = -1;

    ezarray_t *     p_conns     = NULL;
    struct pollfd * p_pfds      = NULL;
    ezqueue_t *     p_new_conns = NULL;

    if ((NULL == p_mgr) || (0 == initial_max_conns))
    {
        LOG_ERROR("Invalid arguments");
        goto end;
    }

    p_conns = malloc(sizeof(ezarray_t));
    if (NULL == p_conns)
    {
        LOG_ERROR("Failed to allocate memory for ezarray_t");
        goto end;
    }

    err = ezarr_init(p_conns, initial_max_conns);
    if (0 != err)
    {
        LOG_ERROR("Failed to initialize ezarray_t");
        goto cleanup_ezarray;
    }

    p_pfds = malloc(sizeof(struct pollfd) * initial_max_conns);
    if (NULL == p_pfds)
    {
        LOG_ERROR("Failed to allocate memory for struct pollfd");
        goto deinit_eza;
    }

    memset(p_pfds, 0, sizeof(struct pollfd) * initial_max_conns);

    p_new_conns = malloc(sizeof(ezqueue_t));
    if (NULL == p_new_conns)
    {
        LOG_ERROR("Failed to allocate memory for ezqueue_t");
        goto cleanup_pfds;
    }

    err = ezq_init(p_new_conns, initial_max_conns);
    if (0 != err)
    {
        LOG_ERROR("Failed to initialize ezqueue_t");
        goto destroy_queue;
    }

    p_mgr->p_conns = p_conns;
    p_conns        = NULL;

    p_mgr->p_pfds = p_pfds;
    p_pfds        = NULL;

    p_mgr->p_new_conns = p_new_conns;
    p_new_conns        = NULL;

    p_mgr->max_conns        = initial_max_conns;
    p_mgr->num_active_conns = 0;

    return 0;

destroy_queue:
    free(p_new_conns);
    p_new_conns = NULL;

cleanup_pfds:
    free(p_pfds);
    p_pfds = NULL;

deinit_eza:
    (void)ezarr_deinit(p_conns);

cleanup_ezarray:
    free(p_conns);
    p_conns = NULL;

end:
    return -1;
}

int connmgr_deinit(conn_mgr_t * p_mgr)
{
    int res = -1;

    if (NULL == p_mgr)
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    (void)ezq_deinit(p_mgr->p_new_conns);
    free(p_mgr->p_new_conns);
    p_mgr->p_new_conns = NULL;

    free(p_mgr->p_pfds);
    p_mgr->p_pfds = NULL;

    (void)ezarr_deinit(p_mgr->p_conns);
    free(p_mgr->p_conns);
    p_mgr->p_conns = NULL;

    res = 0;

end:
    return res;
}

int connmgr_destroy_all_conns(conn_mgr_t * p_mgr)
{
    int          res    = -1;
    conn_ctx_t * p_conn = NULL;

    if (NULL == p_mgr)
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    /* By the time this function is called, the threadpool has either been waited on or shut down so no need to lock */
    for (int idx = 0; idx < p_mgr->num_active_conns; idx++)
    {
        (void)ezarr_get_at(p_mgr->p_conns, idx, (void **)&p_conn);
        if (NULL == p_conn)
        {
            continue;
        }

        res = connmgr_destroy_conn(p_conn);
        if (0 != res)
        {
            LOG_ERROR("Failed to destroy connection at index %d", idx);
            continue;
        }

        res = ezarr_set_at(p_mgr->p_conns, idx, NULL);
        if (0 != res)
        {
            LOG_ERROR("Failed to set connection at index %d to NULL", idx);
        }

        p_conn = NULL;
    }

    res = ezq_clear(p_mgr->p_new_conns, connmgr_destroy_conn);
    if (0 != res)
    {
        LOG_ERROR("Failed to clear new connection queue");
    }

end:
    return res;
}

int connmgr_create_new_conn(int fd, conn_mgr_t * p_mgr, conn_type_t type)
{
    int          res    = -1;
    conn_ctx_t * p_conn = NULL;

    if ((NULL == p_mgr) || (fd < 0))
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    LOG_INFO("Creating new connection context.");
    p_conn = malloc(sizeof(conn_ctx_t));
    if (NULL == p_conn)
    {
        LOG_ERROR("Failed to allocate memory for conn_ctx_t");
        goto end;
    }

    if (type == INBOUND_CONN)
    {
        LOG_INFO("Adding inbound connection.");
        res = pthread_mutex_init(&p_conn->mutex, NULL);
        if (0 != res)
        {
            LOG_ERROR("Failed to initialize mutex");
            goto destroy_conn_ctx;
        }

        p_conn->p_recv_buf = malloc(IO_BUF_SIZE * sizeof(unsigned char));
        if (NULL == p_conn->p_recv_buf)
        {
            LOG_ERROR("Failed to allocate memory for recv buffer");
            goto destroy_mutex;
        }

        p_conn->p_send_buf = malloc(IO_BUF_SIZE * sizeof(char));
        if (NULL == p_conn->p_send_buf)
        {
            LOG_ERROR("Failed to allocate memory for send buffer");
            goto destroy_recv_buf;
        }

        p_conn->ref_count = 0;
        p_conn->fd        = fd;

        p_conn->bytes_read    = 0;
        p_conn->bytes_to_read = HEADER_SIZE;

        p_conn->bytes_sent    = 0;
        p_conn->bytes_to_send = 0;

        p_conn->state = READ_HEADER;
    }

    else
    {
        LOG_INFO("Adding internal connection.");
        p_conn->state     = SPECIAL_CONN;
        p_conn->fd        = fd;
        p_conn->ref_count = 0;
    }

    /* Same for any connection type */
    p_conn->type = type;

    res = ezq_enqueue(p_mgr->p_new_conns, p_conn);
    if (0 != res)
    {
        LOG_ERROR("Failed to enqueue new connection");
        goto destroy_send_buf;
    }

    return res;

destroy_send_buf:
    free(p_conn->p_send_buf);
    p_conn->p_send_buf = NULL;

destroy_recv_buf:
    free(p_conn->p_recv_buf);
    p_conn->p_recv_buf = NULL;

destroy_mutex:
    (void)pthread_mutex_destroy(&p_conn->mutex);

destroy_conn_ctx:
    free(p_conn);
    p_conn = NULL;

end:
    return res;
}

int connmgr_destroy_conn(conn_ctx_t * p_conn)
{
    int res = -1;

    if (NULL == p_conn)
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    close(p_conn->fd);
    p_conn->fd = 0;

    if (INBOUND_CONN == p_conn->type)
    {
        free(p_conn->p_recv_buf);
        p_conn->p_recv_buf = NULL;
        free(p_conn->p_send_buf);
        p_conn->p_send_buf = NULL;

        (void)pthread_mutex_destroy(&p_conn->mutex);
        free(p_conn);
    }

    res = 0;

end:
    return res;
}

int connmgr_update_connections(conn_mgr_t * p_mgr)
{
    int res = -1;
    if (NULL == p_mgr)
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    res = connmgr_remove_closed_connections(p_mgr);
    if (0 != res)
    {
        LOG_ERROR("Failed to remove closed connections");
        goto end;
    }

    res = connmgr_add_new_connections(p_mgr);
    if (0 != res)
    {
        LOG_ERROR("Failed to add new connections");
    }

end:
    return res;
}

/* STATIC FUNCTION DEFINITIONS */

static int connmgr_add_new_connections(conn_mgr_t * p_mgr)
{
    int         res           = -1;
    ezqueue_t * p_new_conns_q = NULL;
    void *      p_new_conn    = NULL;

    if ((NULL == p_mgr) || (NULL == p_mgr->p_new_conns) || (NULL == p_mgr->p_conns) || (NULL == p_mgr->p_pfds))
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    p_new_conns_q = p_mgr->p_new_conns;

    if (0 == p_new_conns_q->num_items)
    {
        LOG_INFO("No new connections to add.");
        res = 0;
        goto end;
    }

    while (0 != p_new_conns_q->num_items)
    {
        res = ezq_dequeue(p_new_conns_q, &p_new_conn);
        if (0 != res)
        {
            LOG_ERROR("Failed to dequeue from conn_mgmt_queue");
            break;
        }

        res = ezarr_push(p_mgr->p_conns, p_new_conn);
        if (0 != res)
        {
            LOG_ERROR("Failed to push to ezarray_t");
            break;
        }

        res = add_to_pollfd((conn_ctx_t *)p_new_conn, &(p_mgr->p_pfds), p_mgr->num_active_conns, p_mgr->max_conns);
        if (0 != res)
        {
            LOG_ERROR("Failed to add to poll array");
            break;
        }

        /* The array may have resized, so we should just update the connmgr to reflect, just in case */
        p_mgr->max_conns = p_mgr->p_conns->max_items;
        p_mgr->num_active_conns++;
    }

end:
    return res;
}

static int connmgr_remove_closed_connections(conn_mgr_t * p_mgr)
{
    int res       = -1;
    int write_idx = 0;
    if ((NULL == p_mgr) || (NULL == p_mgr->p_conns) || (NULL == p_mgr->p_pfds))
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    res = ezarr_compact(p_mgr->p_conns, NULL);
    if (0 != res)
    {
        LOG_ERROR("Failed to compact ezarray_t");
        goto end;
    }

    /* This logic is already contained in ezarray.c, but not generalized enough to use it for struct pollfd */
    for (int read_idx = 0; read_idx < p_mgr->max_conns; read_idx++)
    {
        if (p_mgr->p_pfds[read_idx].fd != 0)
        {
            if (write_idx != read_idx)
            {
                memcpy(p_mgr->p_pfds + write_idx, p_mgr->p_pfds + read_idx, sizeof(struct pollfd));
                memset(p_mgr->p_pfds + read_idx, 0, sizeof(struct pollfd));
            }
            write_idx++;
        }
    }

    p_mgr->num_active_conns = write_idx;

    /* Have to reset the pointers to pollfd array in each context after shuffling */
    // for (int idx = 0; idx < p_mgr->num_active_conns; idx++)
    // {
    //     res = ezarr_get_at(p_mgr->p_conns, idx, (void **)&p_conn);
    //     if (0 != res)
    //     {
    //         LOG_ERROR("Failed to get connection at index %d", idx);
    //         goto end;
    //     }
    //     pthread_mutex_lock(&p_conn->mutex);
    //     p_conn->p_fd = p_mgr->p_pfds + idx;
    //     pthread_mutex_unlock(&p_conn->mutex);
    //     p_conn = NULL;
    // }

    res = 0;

end:
    return res;
}

static int add_to_pollfd(conn_ctx_t * p_ctx, struct pollfd ** pp_pfds, uint16_t cur_size, uint16_t max_size)
{
    assert(NULL != pp_pfds);
    assert(NULL != p_ctx);
    assert(0 < max_size);

    int             res     = -1;
    struct pollfd * p_tmp   = NULL;
    uint16_t        new_max = 0;
    struct pollfd * p_pfds  = *pp_pfds;

    if (cur_size == max_size)
    {
        LOG_INFO("Poll array needs to be resized. Attempting.");

        if (UINT16_MAX == cur_size)
        {
            LOG_ERROR("Poll array is full and cannot be resized");
            goto end;
        }

        if ((UINT16_MAX / 2) < cur_size)
        {
            LOG_WARN("Resizing poll array by double would overflow. Will attempt to resize to UINT16_MAX. Future "
                     "resizes will fail.");
            new_max = UINT16_MAX;
        }

        else
        {
            new_max = cur_size * 2;
        }

        p_tmp = realloc(p_pfds, sizeof(struct pollfd) * new_max);
        if (NULL == p_tmp)
        {
            LOG_ERROR("Failed to resize poll array");
            goto end;
        }

        *pp_pfds = p_tmp;
        p_pfds   = *pp_pfds;
        p_tmp    = NULL;
    }

    p_pfds[cur_size].fd      = p_ctx->fd;
    p_pfds[cur_size].events  = (POLLIN | POLLOUT | POLLHUP | POLLERR | POLLNVAL);
    p_pfds[cur_size].revents = 0;

    res = 0;

end:
    return res;
}
