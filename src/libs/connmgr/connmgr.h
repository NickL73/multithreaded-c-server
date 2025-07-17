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

typedef struct conn_ctx
{
    pthread_mutex_t         mutex;
    int                     idx;
    int                     fd;
    struct sockaddr_storage addr;
} conn_ctx_t;

typedef struct conn_mgmt_queue
{
    ezqueue_t *     p_queue;
    pthread_mutex_t mutex;
} conn_mgmt_queue_t;

typedef struct conn_mgr
{
    ezarray_t *         p_conns;
    struct pollfd *     p_pfds;
    conn_mgmt_queue_t * p_new_conns;
    conn_mgmt_queue_t * p_closed_conns;
    uint16_t            max_conns;
    uint16_t            num_active_conns;
} conn_mgr_t;

int connmgr_init(conn_mgr_t * p_mgr, uint16_t initial_max_conns);
int connmgr_deinit(conn_mgr_t * p_mgr);

int connmgr_create_new_conn(int fd, conn_mgr_t * p_mgr);
int connmgr_mark_for_deletion(conn_mgr_t * p_mgr, uint16_t conn_idx);

int connmgr_update_connections(conn_mgr_t * p_mgr);


#endif // CONN_MGMT_H
