//
// Created by nick on 7/12/25.
//

#ifndef NETIO_H
#define NETIO_H

int nl_start_listener(char * p_host, char * p_service);

int nl_handle_incoming_data(int fd);

int nl_set_nonblocking(const int fd);

#endif // NETIO_H
