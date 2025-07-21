//
// Created by nick on 7/12/25.
//

#include "netio.h"

#include "connmgr.h"
#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define CONNECTION_BACKLOG 100
#define LISTENER_IDX       0

/* STATIC FUNCTION DECLARATIONS */
static int nl_read(conn_ctx_t * p_ctx);

/* PUBLIC FUNCTION DEFINITONS */
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

int nl_accept(const int fd, const conn_mgmt_queue_t * p_new_conns)
{
    assert(NULL != p_new_conns);
    assert(NULL != p_new_conns->p_queue);

    int                     clifd   = -1;
    int                     err     = -1;
    int                     res     = -1;
    struct sockaddr_storage addr    = {0};
    socklen_t               addrlen = sizeof(addr);
    conn_ctx_t *            p_ctx   = NULL;

    while (1)
    {
        clifd = accept(fd, (struct sockaddr *)&addr, &addrlen);
        if (-1 == clifd)
        {
            break;
        }

        p_ctx = (conn_ctx_t *)malloc(sizeof(conn_ctx_t));
        if (NULL == p_ctx)
        {
            LOG_ERROR("Failed to allocate memory for conn_ctx_t");
            close(clifd);
            clifd = -1;
            goto end;
        }

        p_ctx->fd = clifd;
        clifd     = -1;

        p_ctx->idx = -1;

        memcpy(&(p_ctx->addr), &addr, sizeof(addr));
        memset(&addr, 0, sizeof(addr));
        addrlen = sizeof(addr);

        err = nl_set_nonblocking(p_ctx->fd);
        if (-1 == err)
        {
            LOG_ERROR("Failed to set socket to non-blocking mode");
            goto cleanup_ctx;
        }

        err = pthread_mutex_lock(p_new_conns->p_mutex);
        if (0 != err)
        {
            LOG_ERROR("Failed to lock mutex");
            goto cleanup_ctx;
        }

        err = ezq_enqueue(p_new_conns->p_queue, p_ctx);
        if (0 != err)
        {
            LOG_ERROR("Failed to enqueue new connection");
            (void)pthread_mutex_unlock(p_new_conns->p_mutex);
            goto cleanup_ctx;
        }

        err = pthread_mutex_unlock(p_new_conns->p_mutex);
        if (0 != err)
        {
            LOG_ERROR("Failed to unlock mutex");
            goto end; // No real remediation I can do here. No sense in cleaning up the p_ctx - it's already on the Q
        }

        LOG_INFO("Accepted new connection on fd %d.", p_ctx->fd);
        p_ctx = NULL;
    }

    if ((EAGAIN == errno) || (EWOULDBLOCK == errno))
    {
        res = 0;
    }

end:
    return res;

cleanup_ctx:
    close(p_ctx->fd);
    p_ctx->fd = -1;

    free(p_ctx);
    p_ctx = NULL;
    return -1;
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
        res = nl_accept(p_ctx->fd, p_new_conns);
    }

    else
    {
        res = nl_read(p_ctx);
    }


end:
    return res;
}

/* STATIC FUNCTION DEFINITIONS */


static int nl_read(conn_ctx_t * p_ctx)
{
    return 0;
}
