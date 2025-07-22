//
// Created by nick on 7/12/25.
//

#include "netio.h"

#include "connmgr.h"
#include "pong.h"
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

typedef enum nl_internal_err_t
{
    NL_IO_GENERIC_ERROR,
    NL_IO_SUCCESS,
    NL_IO_EOF,
    NL_IO_EWOULDBLOCK,
    NL_RECV_ERR,
    NL_SEND_ERR
} nl_internal_err_t;

/* STATIC FUNCTION DECLARATIONS */
static nl_internal_err_t nl_sendall(int fd, const void * p_buf, size_t len, size_t * p_bytes_sent);
static nl_internal_err_t nl_recvall(int fd, void * p_buf, size_t len, size_t * p_bytes_read);
static int               read_header(conn_ctx_t * p_ctx);
static int               read_content(conn_ctx_t * p_ctx);

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

int nl_accept(const int fd, ezqueue_t * p_new_conns)
{
    assert(NULL != p_new_conns);

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

        memcpy(&(p_ctx->addr), &addr, sizeof(addr));
        memset(&addr, 0, sizeof(addr));
        addrlen = sizeof(addr);

        err = nl_set_nonblocking(p_ctx->fd);
        if (-1 == err)
        {
            LOG_ERROR("Failed to set socket to non-blocking mode");
            goto cleanup_ctx;
        }

        err = ezq_enqueue(p_new_conns, p_ctx);
        if (0 != err)
        {
            LOG_ERROR("Failed to enqueue new connection");
            goto cleanup_ctx;
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

int nl_handle_sock_data_in(conn_ctx_t * p_ctx)
{
    int res = -1;

    if (NULL == p_ctx)
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    switch (p_ctx->state)
    {
        case READ_HEADER:
            res = read_header(p_ctx);
            break;
        case READ_CONTENT:
            res = read_content(p_ctx);
            break;
        default:
            res = -1;
            LOG_ERROR("Invalid state on client read");
    }

end:
    return res;
}

int nl_handle_sock_data_out(conn_ctx_t * p_ctx)
{
    int               res        = -1;
    nl_internal_err_t err        = NL_IO_GENERIC_ERROR;
    size_t            bytes_sent = 0;
    if (NULL == p_ctx)
    {
        LOG_ERROR("Invalid argument");
        goto end;
    }

    err = nl_sendall(p_ctx->fd, p_ctx->p_send_buf, p_ctx->bytes_to_send, &bytes_sent);
    switch (err)
    {
        case NL_IO_SUCCESS:
            LOG_INFO("Successful response write on fd %d", p_ctx->fd);
            if (0 == p_ctx->bytes_to_send)
            {
                p_ctx->bytes_to_send = 0;
                p_ctx->bytes_sent    = 0;
                memset(p_ctx->p_send_buf, 0, IO_BUF_SIZE);

                p_ctx->p_fd->events  = (POLLIN | POLLHUP | POLLERR | POLLNVAL);
                p_ctx->state         = READ_HEADER;
                p_ctx->bytes_to_read = HEADER_SIZE;
            }
            res = 0;
            break;
        case NL_IO_EWOULDBLOCK:
            LOG_INFO("Connection on fd %d would block. Will poll again when ready.", p_ctx->fd);
            p_ctx->bytes_to_send -= bytes_sent;
            p_ctx->bytes_sent += bytes_sent;
            res = 0;
            break;
        case NL_SEND_ERR:
            LOG_ERROR("Failed to write to socket on fd %d.", p_ctx->fd);
            break;
        default:
            LOG_ERROR("Unknown error writing to socket on fd %d.", p_ctx->fd);
            break;
    }

end:
    return res;
}

/* STATIC FUNCTION DEFINITIONS */

// T: 1 byte
// L: 2 bytes
// V: determined by L

static nl_internal_err_t nl_sendall(int fd, const void * p_buf, size_t len, size_t * p_bytes_sent)
{
    assert(NULL != p_buf);
    assert(NULL != p_bytes_sent);
    assert(0 < len);

    nl_internal_err_t res        = NL_IO_GENERIC_ERROR;
    ssize_t           bytes_sent = 0;
    size_t            total_sent = 0;

    while (total_sent < len)
    {
        errno      = 0;
        bytes_sent = send(fd, (char *)p_buf + total_sent, len - total_sent, 0);
        if (0 > bytes_sent)
        {
            if (EINTR == errno)
            {
                continue;
            }

            res = ((EWOULDBLOCK == errno) || (EAGAIN == errno)) ? NL_IO_EWOULDBLOCK : NL_SEND_ERR;
            break;
        }

        total_sent += (size_t)bytes_sent;
    }

    if ((NL_IO_EWOULDBLOCK != res) && (NL_SEND_ERR != res))
    {
        res = NL_IO_SUCCESS;
    }

    return res;
}

static nl_internal_err_t nl_recvall(int fd, void * p_buf, size_t len, size_t * p_bytes_read)
{
    assert(NULL != p_buf);
    assert(NULL != p_bytes_read);

    nl_internal_err_t res        = NL_IO_GENERIC_ERROR;
    ssize_t           bytes_read = 0;
    size_t            total_read = 0;

    while (total_read < len)
    {
        errno      = 0;
        bytes_read = recv(fd, (char *)p_buf + total_read, len - total_read, 0);
        if (0 > bytes_read)
        {
            if (EINTR == errno)
            {
                continue;
            }

            res = ((EWOULDBLOCK == errno) || (EAGAIN == errno)) ? NL_IO_EWOULDBLOCK : NL_RECV_ERR;
            break;
        }

        if (0 == bytes_read)
        {
            res = NL_IO_EOF;
            break;
        }

        total_read += (size_t)bytes_read;
    }

    if ((NL_IO_EWOULDBLOCK != res) && (NL_RECV_ERR != res) && (NL_IO_EOF != res))
    {
        res = NL_IO_SUCCESS;
    }

    *p_bytes_read = total_read;
    return res;
}

static int read_header(conn_ctx_t * p_ctx)
{
    assert(NULL != p_ctx);

    int               res          = -1;
    size_t            bytes_read   = 0;
    size_t            incoming_len = 0;
    nl_internal_err_t err          = nl_recvall(p_ctx->fd, (p_ctx->p_recv_buf + p_ctx->bytes_read),
                                                (p_ctx->bytes_to_read - p_ctx->bytes_read), &bytes_read);

    p_ctx->bytes_read += bytes_read;
    p_ctx->bytes_to_read -= bytes_read;

    switch (err)
    {
        case NL_IO_SUCCESS:
            if (p_ctx->bytes_read == HEADER_SIZE)
            {
                memcpy(&incoming_len, p_ctx->p_recv_buf, HEADER_SIZE);
                incoming_len = ntohs(incoming_len);

                LOG_INFO("Received header and expecting message of %lu bytes.", incoming_len);
                p_ctx->bytes_to_read = incoming_len;
                p_ctx->bytes_read    = 0;
                p_ctx->state         = READ_CONTENT;
            }
            res = 0;
            break;
        case NL_IO_EWOULDBLOCK:
            LOG_INFO("Connection on fd %d would block. Will poll again when ready.", p_ctx->fd);
            res = 0;
            break;
        case NL_RECV_ERR:
            LOG_ERROR("Failed to read from socket on fd %d.", p_ctx->fd);
            break;
        case NL_IO_EOF:
            LOG_INFO("Connection on fd %d closed. Marking for deletion.", p_ctx->fd);
            p_ctx->b_marked_for_deletion = true;
            res                          = 0;
            break;
        default:
            LOG_ERROR("Unknown error reading from socket on fd %d.", p_ctx->fd);
            break;
    }

    return res;
}

static int read_content(conn_ctx_t * p_ctx)
{
    assert(NULL != p_ctx);

    int               res        = -1;
    size_t            bytes_read = 0;
    nl_internal_err_t err        = nl_recvall(p_ctx->fd, (p_ctx->p_recv_buf + p_ctx->bytes_read),
                                              (p_ctx->bytes_to_read - p_ctx->bytes_read), &bytes_read);

    p_ctx->bytes_read += bytes_read;
    p_ctx->bytes_to_read -= bytes_read;

    switch (err)
    {
        case NL_IO_SUCCESS:
            if (0 == p_ctx->bytes_to_read)
            {
                LOG_INFO("Received all content for message. Will send response.");
                (void)proto_pingpong_create_response(p_ctx->p_recv_buf, p_ctx->p_send_buf,
                                                     (HEADER_SIZE + p_ctx->bytes_read), &p_ctx->bytes_to_send);

                p_ctx->bytes_to_read = 0;
                p_ctx->bytes_read    = 0;
                p_ctx->state         = WRITE_RESPONSE;
            }
            res = 0;
            break;
        case NL_IO_EWOULDBLOCK:
            LOG_INFO("Connection on fd %d would block. Will poll again when ready.", p_ctx->fd);
            res = 0;
            break;
        case NL_RECV_ERR:
            LOG_ERROR("Failed to read from socket on fd %d.", p_ctx->fd);
            break;
        case NL_IO_EOF:
            LOG_INFO("Connection on fd %d closed. Marking for deletion.", p_ctx->fd);
            p_ctx->b_marked_for_deletion = true;
            res                          = 0;
            break;
        default:
            LOG_ERROR("Unknown error reading from socket on fd %d.", p_ctx->fd);
            break;
    }

    return res;
}
