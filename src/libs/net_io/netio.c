//
// Created by nick on 7/12/25.
//

#include "netio.h"

#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define CONNECTION_BACKLOG 100
#define LISTENER_IDX       0

static int nl_accept_new(int fd, conn_mgmt_queue_t * p_new_conns);

int nl_start_listener(char * p_host, char * p_service)
{
    int fd     = -1;
    int err    = -1;
    int optval = 1;

    const struct addrinfo * p_check = NULL;
    struct addrinfo *       p_addr  = NULL;
    const struct addrinfo   hints   = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM, .ai_flags = AI_PASSIVE};

    /* Allowing NULL to for p_host to bind to all interfaces (0.0.0.0) */
    if (NULL == p_service)
    {
        LOG_ERROR("NULL p_service input to nl_start_listener");
        goto end;
    }

    err = getaddrinfo(p_host, p_service, &hints, &p_addr);
    if (0 != err)
    {
        LOG_ERROR("getaddrinfo() failed with error %d (%s)", err, gai_strerror(err));
        goto end;
    }

    for (p_check = p_addr; p_check != NULL; p_check = p_check->ai_next)
    {
        fd = socket(p_check->ai_family, p_check->ai_socktype, p_check->ai_protocol);
        if (-1 == fd)
        {
            LOG_WARN("Failed to create socket, will try again if possible.");
            continue;
        }

#ifndef NDEBUG
        err = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        if (0 != err)
        {
            LOG_ERROR("Failed to set SO_REUSEADDR on socket. Only attempted in DEBUG mode. Continuing.");
        }
#endif

        err = bind(fd, p_check->ai_addr, p_check->ai_addrlen);
        if (0 == err)
        {
            LOG_INFO("Successfully bound to %s:%s", p_host, p_service);
            break;
        }

        LOG_WARN("Failed to bind to %s:%s. Will try again if possible.", p_host, p_service);
        close(fd);
        fd = -1;
    }

    /* Need to know if we came out of the loop successfully or hit the backstop */
    if (NULL == p_check)
    {
        LOG_ERROR("Failed to bind to %s:%s", p_host, p_service);
        goto release_addrinfo;
    }

    err = nl_set_nonblocking(fd);
    if (-1 == err)
    {
        LOG_ERROR("Failed to set socket to non-blocking mode");
        close(fd);
        fd = -1;
        goto release_addrinfo;
    }

    err = listen(fd, CONNECTION_BACKLOG);
    if (-1 == err)
    {
        LOG_ERROR("Failed to start listening on socket");
        close(fd);
        fd = -1;
    }

release_addrinfo:
    freeaddrinfo(p_addr);
    p_addr = NULL;

end:
    return fd;
}

int nl_set_nonblocking(const int fd)
{
    int       res   = -1;
    const int flags = fcntl(fd, F_GETFL, 0);
    if (-1 != flags)
    {
        res = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    return res;
}

int nl_handle_sock_data_in(conn_ctx_t * p_ctx, conn_mgmt_queue_t * p_new_conns)
{
    int res = -1;

    if ((NULL == p_ctx) || (NULL == p_new_conns))
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    if (LISTENER_IDX == p_ctx->idx)
    {
        // TODO: Accept Connection
        // Create socket_ctx_t
        // Add to new_conns queue
        // Do all of that in a loop until -1 with EAGAIN/EWOULDBLOCK
    }

    else
    {
        // TODO: Read and act
    }


end:
    return res;
}

static int nl_accept(int fd, conn_mgmt_queue_t * p_new_conns)
{
    assert(NULL != p_new_conns);
    int clifd = -1;

    while (1)
    {
        clifd = accept(fd, NULL, NULL);
        if (-1 == clifd)
        {
            break;
        }

        // TODO: Save off the fd
    }

    if ((EAGAIN == errno) || (EWOULDBLOCK == errno))
    {
        return 0;
    }

    return -1;
}
