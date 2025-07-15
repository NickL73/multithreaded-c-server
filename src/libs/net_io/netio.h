//
// Created by nick on 7/12/25.
//

#ifndef NETIO_H
#define NETIO_H

#include "connmgr.h"

typedef struct socket_ctx
{
    int idx;
    int fd;
} socket_ctx_t;

int nl_start_listener(char * p_host, char * p_service);

int nl_set_nonblocking(const int fd);

int nl_handle_sock_data_in(socket_ctx_t * p_ctx, conn_mgmt_queue_t * p_new_conns);

#endif // NETIO_H
