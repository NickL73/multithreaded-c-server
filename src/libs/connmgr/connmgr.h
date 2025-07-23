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
    pthread_mutex_t         mutex;
    int                     ref_count;
    bool                    b_marked_for_deletion;
    int                     fd;
    struct sockaddr_storage addr;

    struct pollfd * p_fd;

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

int connmgr_create_new_conn(int fd, conn_mgr_t * p_mgr);

int connmgr_check_active_connection(conn_ctx_t * p_ctx);
int connmgr_attempt_deletion(conn_mgr_t * p_mgr, uint16_t conn_idx);

int connmgr_update_connections(conn_mgr_t * p_mgr);


#endif // CONN_MGMT_H
