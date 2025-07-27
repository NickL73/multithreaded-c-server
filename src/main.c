/**
 * @file main.c
 * @author nick
 * @date 7/11/25
 * @brief
 */
#include "concurinc.h"
#include "connmgr.h"
#include "netio.h"
#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>


/* GLOBAL VARIABLES AND VALUES */
#define INITIAL_MAX_CONNS 16
#define SERVER_PORT       1337
#define NUM_THREADS       8
volatile sig_atomic_t g_should_shutdown = 0;

/* STATIC FUNCTION DECLARATIONS */
static void sighandler(int signum);
static int  setup_signal_handlers(void);
static void coin_io_read(void * p_arg);
static void coin_io_send(void * p_arg);

int main(void)
{
    int                 err       = 0;
    int                 res       = -1;
    int                 sfd       = -1;
    coin_status_t       tp_status = COIN_GENERIC_FAILURE;
    coin_threadpool_t * p_tp      = NULL;
    conn_ctx_t *        p_cur_ctx = NULL;
    conn_mgr_t          conn_mgr  = {0};

    // TODO: Start a single thread to handle signals

    /* Setup the signal handler to attempt a graceful shutdown on SIGINT and SIGTERM and ignore SIGPIPE */
    LOG_INFO("Setting up signal handlers.");
    err = setup_signal_handlers();
    if (0 != err)
    {
        LOG_FATAL("Failed to setup signal handlers");
        goto end;
    }

    /* Setup the connection manager that will handle new and closed connections */
    LOG_INFO("Setting up connection manager.");
    err = connmgr_init(&conn_mgr, INITIAL_MAX_CONNS);
    if (0 != err)
    {
        LOG_FATAL("Failed to initialize connection manager");
        goto end;
    }

    /* Setup the threadpool that will handle the I/O for each connection */
    tp_status = coin_tpool_init(&p_tp, NUM_THREADS);
    if (COIN_SUCCESS != tp_status)
    {
        LOG_FATAL("Failed to initialize threadpool");
        goto destroy_connmgr;
    }

    /* Start the main server listening socket */
    LOG_INFO("Starting listening socket.");
    sfd = nl_start_listener("127.0.0.1", "1337");
    if (-1 == sfd)
    {
        LOG_FATAL("Failed to start listening on socket");
        goto destroy_tpool;
    }

    err = connmgr_create_new_conn(sfd, &conn_mgr);
    if (0 != err)
    {
        LOG_FATAL("Failed to create connection structure for listener.");
        goto cleanup_connections;
    }

    while (!g_should_shutdown)
    {
        for (uint16_t conn = 0; conn < conn_mgr.num_active_conns; conn++)
        {
            err = ezarr_get_at(conn_mgr.p_conns, conn, (void **)&p_cur_ctx);
            if (0 != err)
            {
                LOG_FATAL("Failed to get connection context");
                goto cleanup_connections;
            }

            err = pthread_mutex_lock(&(p_cur_ctx->mutex));
            if (0 != err)
            {
                LOG_ERROR("Failed to lock mutex when updating events");
                continue;
            }

            switch (p_cur_ctx->state)
            {
                LOG_DEBUG("Client on fd %d at state %d", p_cur_ctx->fd, p_cur_ctx->state);
                case READ_HEADER:
                case READ_CONTENT:
                    LOG_DEBUG("Setting events for fd %d to POLLIN | POLLHUP | POLLERR | POLLNVAL", p_cur_ctx->fd);
                    conn_mgr.p_pfds[conn].events = POLLIN | POLLOUT | POLLHUP | POLLERR | POLLNVAL;
                    break;
                case WRITE_RESPONSE:
                    LOG_DEBUG("Setting events for fd %d to POLLOUT | POLLHUP | POLLERR | POLLNVAL", p_cur_ctx->fd);
                    conn_mgr.p_pfds[conn].events = POLLOUT | POLLIN | POLLHUP | POLLERR | POLLNVAL;
                    break;
                case PENDING_CLOSE:
                    LOG_DEBUG("Conn %d is PENDING CLOSE. Trying to delete", conn);
                    conn_mgr.p_pfds[conn].fd = -1;
                    if (0 == p_cur_ctx->ref_count)
                    {
                        LOG_DEBUG("Deleting.");
                        close(p_cur_ctx->fd);
                        p_cur_ctx->fd = -1;
                        free(p_cur_ctx->p_recv_buf);
                        p_cur_ctx->p_recv_buf = NULL;
                        free(p_cur_ctx->p_send_buf);
                        p_cur_ctx->p_send_buf = NULL;
                        (void)pthread_mutex_unlock(&(p_cur_ctx->mutex));
                        (void)pthread_mutex_destroy(&(p_cur_ctx->mutex));
                        free(p_cur_ctx);

                        err = ezarr_set_at(conn_mgr.p_conns, conn, NULL);
                        memset(conn_mgr.p_pfds + conn, 0, sizeof(struct pollfd));

                        p_cur_ctx = NULL;
                        continue;
                    }
                    break;
                default:
                    conn_mgr.p_pfds[conn].events = 0;
                    break;
            }

            err = pthread_mutex_unlock(&(p_cur_ctx->mutex));
            if (0 != err)
            {
                LOG_ERROR("Failed to unlock mutex after updating events");
            }

            p_cur_ctx = NULL;
        }

        LOG_INFO("Updating connections.");
        err = connmgr_update_connections(&conn_mgr);
        if (0 != err)
        {
            LOG_FATAL("Failed to update connection manager for listener.");
            goto cleanup_connections;
        }

        LOG_INFO("Polling connections for activity.");
        err = poll(conn_mgr.p_pfds, conn_mgr.num_active_conns, -1); // 15000);
        if (-1 == err)
        {
            LOG_ERROR("poll() failed with errno %d (%s)", errno, strerror(errno));
            if (EINTR == errno)
            {
                continue;
            }

            break;
        }

        if (0 == err)
        {
            LOG_INFO("Poll timed out, going to recycle to cleanup connections as required.");
            continue;
        }

        for (uint16_t conn = 0; conn < conn_mgr.num_active_conns; conn++)
        {
            err = ezarr_get_at(conn_mgr.p_conns, conn, (void **)&p_cur_ctx);
            if (0 != err)
            {
                LOG_FATAL("Failed to get connection context");
                goto cleanup_connections;
            }

            if (conn_mgr.p_pfds[conn].revents & POLLIN)
            {
                LOG_INFO("Received data on connection %d", conn);
                // TODO: Check if we're at the maximum number of connections (this is really impractical)
                if (sfd == conn_mgr.p_pfds[conn].fd)
                {
                    err = nl_accept(conn_mgr.p_pfds[conn].fd, &conn_mgr);
                    if (-1 == err)
                    {
                        LOG_ERROR("Failed to accept new connections");
                        goto cleanup_connections;
                    }
                }

                else
                {
                    err = pthread_mutex_lock(&(p_cur_ctx->mutex));
                    if (0 != err)
                    {
                        LOG_ERROR("Failed to lock mutex");
                        continue;
                    }

                    p_cur_ctx->ref_count += 1;

                    tp_status = coin_tpool_submit(p_tp, coin_io_read, p_cur_ctx, NULL);
                    if (COIN_SUCCESS != tp_status)
                    {
                        LOG_ERROR("Failed to submit to coin_tpool");
                    }

                    err = pthread_mutex_unlock(&(p_cur_ctx->mutex));
                    if (0 != err)
                    {
                        LOG_ERROR("Failed to unlock mutex");
                    }
                }
            }

            if (conn_mgr.p_pfds[conn].revents & (POLLHUP | POLLERR | POLLNVAL))
            {
                LOG_INFO("Connection on fd %d closed. Marking for deletion.", conn_mgr.p_pfds[conn].fd);
                err = pthread_mutex_lock(&(p_cur_ctx->mutex));
                if (0 != err)
                {
                    LOG_FATAL("Failed to lock mutex on dead connection.");
                    goto cleanup_connections;
                }

                close(p_cur_ctx->fd);
                p_cur_ctx->fd    = -1;
                p_cur_ctx->state = PENDING_CLOSE;

                err = pthread_mutex_unlock(&(p_cur_ctx->mutex));
                if (0 != err)
                {
                    LOG_FATAL("Failed to unlock mutex on dead connection.");
                    goto cleanup_connections;
                }
            }

            if (conn_mgr.p_pfds[conn].revents & POLLOUT)
            {
                err = pthread_mutex_lock(&(p_cur_ctx->mutex));
                if (0 != err)
                {
                    LOG_ERROR("Failed to lock mutex");
                    continue;
                }

                p_cur_ctx->ref_count += 1;

                tp_status = coin_tpool_submit(p_tp, coin_io_send, p_cur_ctx, NULL);
                if (COIN_SUCCESS != tp_status)
                {
                    LOG_ERROR("Failed to submit to coin_tpool");
                }

                err = pthread_mutex_unlock(&(p_cur_ctx->mutex));
                if (0 != err)
                {
                    LOG_ERROR("Failed to unlock mutex");
                }
            }
            p_cur_ctx = NULL;
        }
    }

    LOG_INFO("Exiting cleanly from the main loop.");

    // TODO: Wait for enqueued jobs to complete or clear them all out (shut down the threadpool)
    tp_status = coin_tpool_wait(p_tp);
    if (COIN_SUCCESS != tp_status)
    {
        LOG_ERROR("Failed to wait on coin_tpool");
    }

    res = 0;

cleanup_connections:
    (void)connmgr_destroy_all_conns(&conn_mgr);

destroy_tpool:
    (void)coin_tpool_destroy(p_tp, NULL);
    p_tp = NULL;

destroy_connmgr:
    (void)connmgr_deinit(&conn_mgr);

end:
    return res;
}

/* STATIC FUNCTION DEFINITIONS */
static void sighandler(int signum)
{
    (void)signum;
    g_should_shutdown = 1;
}

static int setup_signal_handlers(void)
{
    int              res = -1;
    struct sigaction sa  = {0};
    sa.sa_handler        = sighandler;
    sa.sa_flags          = SA_RESTART;
    if (0 != sigemptyset(&sa.sa_mask))
    {
        LOG_ERROR("Failed to empty signal set");
        return res;
    }

    if ((0 != sigaction(SIGINT, &sa, NULL)) || (0 != sigaction(SIGTERM, &sa, NULL)))
    {
        LOG_ERROR("Failed to set sigaction on SIGINT or SIGTERM");
        return res;
    }

    /* Don't let SIGPIPE break the server either, just ignore it */
    (void)signal(SIGPIPE, SIG_IGN);
    res = 0;
    return res;
}

static void coin_io_read(void * p_arg)
{
    assert(NULL != p_arg);
    conn_ctx_t * p_ctx = (conn_ctx_t *)(p_arg);
    int          err   = 0;

    err = pthread_mutex_lock(&(p_ctx->mutex));
    if (0 != err)
    {
        LOG_ERROR("Failed to lock mutex for I/O");
    }

    if ((p_ctx->state != PENDING_CLOSE) && (p_ctx->state != WRITE_RESPONSE) && !g_should_shutdown)
    {
        err = nl_handle_sock_data_in(p_ctx);
        if (0 != err)
        {
            LOG_ERROR("Failed to read data on fd %d", p_ctx->fd);
        }
    }

    else
    {
        LOG_INFO("Socket state not ready for reading.");
    }

    p_ctx->ref_count -= 1;

    err = pthread_mutex_unlock(&(p_ctx->mutex));
    if (0 != err)
    {
        LOG_ERROR("Failed to unlock mutex for I/O");
    }
}

static void coin_io_send(void * p_arg)
{
    assert(NULL != p_arg);
    conn_ctx_t * p_ctx = (conn_ctx_t *)(p_arg);
    int          err   = 0;

    err = pthread_mutex_lock(&(p_ctx->mutex));
    if (0 != err)
    {
        LOG_ERROR("Failed to lock mutex for I/O");
    }

    if ((p_ctx->state == WRITE_RESPONSE) && !g_should_shutdown)
    {
        err = nl_handle_sock_data_out(p_ctx);
        if (0 != err)
        {
            LOG_ERROR("Failed to read data on fd %d", p_ctx->fd);
        }
    }

    else
    {
        LOG_INFO("Socket state not ready for writing.");
    }

    p_ctx->ref_count -= 1;

    err = pthread_mutex_unlock(&(p_ctx->mutex));
    if (0 != err)
    {
        LOG_ERROR("Failed to unlock mutex for I/O");
    }
}
