/**
 * @file connmgr.c
 * @author nick
 * @date 7/13/25
 * @brief
 */

#include "connmgr.h"

#include "utils.h"

#include <assert.h>
#include <stdlib.h>


static int  create_mgmt_queue(conn_mgmt_queue_t ** pp_queue, uint16_t max_items);
static void destroy_mgmt_queue(conn_mgmt_queue_t * p_queue);

int connmgr_init(conn_mgr_t * p_mgr, uint16_t initial_max_conns)
{
    int err = -1;

    ezarray_t *         p_conns     = NULL;
    struct pollfd *     p_pfds      = NULL;
    conn_mgmt_queue_t * p_new_conns = NULL;

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

    p_mgr->p_conns = p_conns;
    p_conns        = NULL;

    p_mgr->p_pfds = p_pfds;
    p_pfds        = NULL;

    p_mgr->p_new_conns = p_new_conns;
    p_new_conns        = NULL;

    p_mgr->max_conns        = initial_max_conns;
    p_mgr->num_total_conns  = 0;
    p_mgr->num_active_conns = 0;

    return 0;

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

    p_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (NULL == p_mutex)
    {
        LOG_ERROR("Failed to allocate memory for mutex");
        goto deinit_ezq;
    }

    res = pthread_mutex_init(p_mutex, NULL);
    if (0 != res)
    {
        LOG_ERROR("Failed to initialize mutex");
        goto cleanup_mutex;
    }

    p_queue->p_queue = p_ezq;
    p_queue->p_mutex = p_mutex;
    *pp_queue        = p_queue;

    return 0;

cleanup_mutex:
    free(p_mutex);
    p_mutex = NULL;

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

    (void)pthread_mutex_destroy(p_queue->p_mutex);
    free(p_queue->p_queue);
    p_queue->p_mutex = NULL;

    (void)ezq_deinit(p_queue->p_queue);
    free(p_queue->p_queue);
    p_queue->p_queue = NULL;

    free(p_queue);
}
