/**
 * @file connmgr.h
 * @author nick
 * @date 7/13/25
 * @brief
 */
#ifndef CONN_MGMT_H
#define CONN_MGMT_H

#include "ezarray.h"
#include "ezqueue.h"

#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>

typedef struct conn_ctx
{
    int                     idx;
    int                     fd;
    struct sockaddr_storage addr;
} conn_ctx_t;

typedef struct conn_mgmt_queue
{
    ezqueue_t *       p_queue;
    pthread_mutex_t * p_mutex;
} conn_mgmt_queue_t;

typedef struct conn_mgr_t
{
    ezarray_t *         p_conns;
    struct pollfd *     p_pfds;
    conn_mgmt_queue_t * p_new_conns;
    uint16_t            max_conns;
    uint16_t            num_active_conns;
} conn_mgr_t;

int connmgr_init(conn_mgr_t * p_mgr, uint16_t initial_max_conns);
int connmgr_deinit(conn_mgr_t * p_mgr);

int connmgr_add_new_connections(conn_mgr_t * p_mgr);
int connmgr_remove_closed_connections(conn_mgr_t * p_mgr);

#endif // CONN_MGMT_H
