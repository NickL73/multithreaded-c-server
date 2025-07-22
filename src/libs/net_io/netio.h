//
// Created by nick on 7/12/25.
//

#ifndef NETIO_H
#define NETIO_H

#include "connmgr.h"
#include "ezqueue.h"

int nl_start_listener(char * p_host, char * p_service);

int nl_accept(const int fd, conn_mgr_t * p_mgr);

int nl_set_nonblocking(const int fd);

int nl_handle_sock_data_in(conn_ctx_t * p_ctx);

int nl_handle_sock_data_out(conn_ctx_t * p_ctx);

#endif // NETIO_H
