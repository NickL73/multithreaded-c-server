/**
 * @file connmgr.h
 * @author nick
 * @date 7/13/25
 * @brief
 */
#ifndef CONN_MGMT_H
#define CONN_MGMT_H

#include "connmgr.h"
#include "ezqueue.h"

#include <pthread.h>

typedef struct conn_ctx
{
    int idx;
    int fd;
    struct sockaddr_storage;
} conn_ctx_t;

typedef struct conn_mgmt_queue
{
    ezqueue_t *       p_queue;
    pthread_mutex_t * p_mutex;
} conn_mgmt_queue_t;


#endif // CONN_MGMT_H
