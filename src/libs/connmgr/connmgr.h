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

typedef enum conn_type
{
    INTERNAL_CONN,
    INBOUND_CONN
} conn_type_t;

typedef struct conn_ctx
{
    conn_type_t             type;
    pthread_mutex_t         mutex;
    int                     ref_count;
    int                     fd;
    struct sockaddr_storage addr;

    unsigned char * p_recv_buf;
    size_t          bytes_read;
    size_t          bytes_to_read;

    char * p_send_buf;
    size_t bytes_sent;
    size_t bytes_to_send;

    enum
    {
        READ_HEADER,
        READ_CONTENT,
        WRITE_RESPONSE,
        PENDING_CLOSE,
        SPECIAL_CONN // For the listener and self-pipe
    } state;

} conn_ctx_t;

typedef struct conn_mgr
{
    ezarray_t *     p_conns;
    struct pollfd * p_pfds;
    ezqueue_t *     p_new_conns;
    uint16_t        max_conns;
    uint16_t        num_active_conns;
} conn_mgr_t;

int connmgr_init(conn_mgr_t * p_mgr, uint16_t initial_max_conns);
int connmgr_deinit(conn_mgr_t * p_mgr);
int connmgr_destroy_all_conns(conn_mgr_t * p_mgr);

int  connmgr_create_new_conn(int fd, conn_mgr_t * p_mgr, conn_type_t type);
void connmgr_destroy_conn(void * p_arg);

int connmgr_update_connections(conn_mgr_t * p_mgr);


#endif // CONN_MGMT_H
