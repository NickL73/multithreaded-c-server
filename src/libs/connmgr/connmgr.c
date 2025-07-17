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


static int  create_mgmt_queue(conn_mgmt_queue_t ** pp_queue, uint16_t max_items);
static void destroy_mgmt_queue(conn_mgmt_queue_t * p_queue);
static int  add_to_pollfd(int fd, struct pollfd * p_pfds, uint16_t cur_size, uint16_t max_size);
static int  connmgr_add_new_connections(conn_mgr_t * p_mgr);
static int  connmgr_remove_closed_connections(conn_mgr_t * p_mgr);

int connmgr_init(conn_mgr_t * p_mgr, uint16_t initial_max_conns)
{
    int err = -1;

    ezarray_t *         p_conns        = NULL;
    struct pollfd *     p_pfds         = NULL;
    conn_mgmt_queue_t * p_new_conns    = NULL;
    conn_mgmt_queue_t * p_closed_conns = NULL;

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

    err = create_mgmt_queue(&p_new_conns, initial_max_conns);
    if (0 != err)
    {
        LOG_ERROR("Failed to create conn_mgmt_queue for new conns");
        goto cleanup_pfds;
    }

    err = create_mgmt_queue(&p_closed_conns, initial_max_conns);
    if (0 != err)
    {
        LOG_ERROR("Failed to create conn_mgmt_queue for closed conns");
        goto cleanup_add_queue;
    }

    p_mgr->p_conns = p_conns;
    p_conns        = NULL;

    p_mgr->p_pfds = p_pfds;
    p_pfds        = NULL;

    p_mgr->p_new_conns = p_new_conns;
    p_new_conns        = NULL;

    p_mgr->p_closed_conns = p_closed_conns;
    p_closed_conns        = NULL;

    p_mgr->max_conns        = initial_max_conns;
    p_mgr->num_active_conns = 0;

    return 0;

cleanup_add_queue:
    (void)destroy_mgmt_queue(p_new_conns);
    free(p_pfds);
    p_pfds = NULL;

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

    destroy_mgmt_queue(p_mgr->p_closed_conns);
    p_mgr->p_closed_conns = NULL;

    destroy_mgmt_queue(p_mgr->p_new_conns);
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

int connmgr_create_new_conn(int fd, conn_mgr_t * p_mgr);

int connmgr_mark_for_deletion(conn_mgr_t * p_mgr, uint16_t conn_idx);

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
    {
        LOG_ERROR("Failed to add new connections");
    }

end:
    return res;
}

/* STATIC FUNCTION DEFINITIONS */


static int create_mgmt_queue(conn_mgmt_queue_t ** pp_queue, uint16_t max_items)
{
    assert(NULL != pp_queue);
    assert(0 < max_items);

    int                 res     = -1;
    conn_mgmt_queue_t * p_queue = NULL;
    ezqueue_t *         p_ezq   = NULL;
    pthread_mutex_t *   p_mutex = NULL;

    p_queue = (conn_mgmt_queue_t *)malloc(sizeof(conn_mgmt_queue_t));
    if (NULL == p_queue)
    {
        LOG_ERROR("Failed to allocate memory for conn_mgmt_queue_t");
        goto err;
    }

    p_ezq = (ezqueue_t *)malloc(sizeof(ezqueue_t));
    if (NULL == p_ezq)
    {
        LOG_ERROR("Failed to allocate memory for ezqueue_t");
        goto cleanup_queue;
    }

    res = ezq_init(p_ezq, max_items);
    if (0 != res)
    {
        LOG_ERROR("Failed to initialize ezqueue");
        goto cleanup_ezq;
    }

    res = pthread_mutex_init(&(p_queue->mutex), NULL);
    if (0 != res)
    {
        LOG_ERROR("Failed to initialize mutex");
        goto deinit_ezq;
    }

    p_queue->p_queue = p_ezq;
    *pp_queue        = p_queue;

    return 0;

deinit_ezq:
    (void)ezq_deinit(p_ezq);

cleanup_ezq:
    free(p_ezq);
    p_ezq = NULL;

cleanup_queue:
    free(p_queue);
    p_queue = NULL;

err:
    return -1;
}

static void destroy_mgmt_queue(conn_mgmt_queue_t * p_queue)
{
    assert(NULL != p_queue);

    (void)pthread_mutex_lock(&(p_queue->mutex));
    (void)ezq_clear(p_queue->p_queue, free);
    (void)pthread_mutex_unlock(&(p_queue->mutex));

    (void)pthread_mutex_destroy(&(p_queue->mutex));

    (void)ezq_deinit(p_queue->p_queue);
    free(p_queue->p_queue);
    p_queue->p_queue = NULL;

    free(p_queue);
}

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

    p_new_conns_q = p_mgr->p_new_conns->p_queue;

    res = pthread_mutex_lock((&p_mgr->p_new_conns->mutex));
    if (0 != res)
    {
        LOG_ERROR("Failed to lock mutex");
        goto end;
    }

    while (0 != p_new_conns_q->num_items)
    {
        res = ezq_dequeue(p_new_conns_q, &p_new_conn);
        if (0 != res)
        {
            LOG_ERROR("Failed to dequeue from conn_mgmt_queue");
            (void)pthread_mutex_unlock(&(p_mgr->p_new_conns->mutex));
            goto end;
        }

        res = ezarr_push(p_mgr->p_conns, p_new_conn);
        if (0 != res)
        {
            LOG_ERROR("Failed to push to ezarray_t");
            (void)pthread_mutex_unlock(&(p_mgr->p_new_conns->mutex));
            goto end;
        }

        res = add_to_pollfd(((conn_ctx_t *)p_new_conn)->fd, p_mgr->p_pfds, p_mgr->num_active_conns, p_mgr->max_conns);
        if (0 != res)
        {
            LOG_ERROR("Failed to add to poll array");
            (void)pthread_mutex_unlock(&(p_mgr->p_new_conns->mutex));
        }

        /* The array may have resized, so we should just update the connmgr to reflect, just in case */
        p_mgr->max_conns = p_mgr->p_conns->max_items;
        p_mgr->num_active_conns++;
    }

    res = pthread_mutex_unlock(&(p_mgr->p_new_conns->mutex));
    if (0 != res)
    {
        LOG_ERROR("Failed to unlock mutex");
        // Fallthrough to return regardless
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
        if (p_mgr->p_pfds[read_idx].fd != -1)
        {
            if (write_idx != read_idx)
            {
                p_mgr->p_pfds[write_idx]   = p_mgr->p_pfds[read_idx];
                p_mgr->p_pfds[read_idx].fd = -1;
            }
            write_idx++;
        }
    }

    p_mgr->num_active_conns = write_idx;
    res                     = 0;

end:
    return res;
}

static int add_to_pollfd(int fd, struct pollfd * p_pfds, uint16_t cur_size, uint16_t max_size)
{
    assert(NULL != p_pfds);

    int             res     = -1;
    struct pollfd * p_tmp   = NULL;
    uint16_t        new_max = 0;

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

        p_pfds = p_tmp;
        p_tmp  = NULL;
    }

    p_pfds[cur_size].fd      = fd;
    p_pfds[cur_size].events  = (POLLIN | POLLHUP | POLLERR | POLLNVAL);
    p_pfds[cur_size].revents = 0;

    res = 0;

end:
    return res;
}
