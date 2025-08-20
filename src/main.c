/**
 * @file main.c
 * @author nick
 * @date 7/11/25
 * @brief
 */
#include "common.h"
#include "concurinc.h"
#include "connmgr.h"
#include "netio.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* GLOBAL VARIABLES AND VALUES */
#define INITIAL_MAX_CONNS 16
#define SERVER_PORT       1337
#define NUM_THREADS       8
volatile sig_atomic_t g_should_shutdown = 0;

typedef struct sig_thread_args_t
{
    sigset_t * p_sigset;
    int        sigpipe_fd;
} sig_thread_args_t;

/* STATIC FUNCTION DECLARATIONS */
static int  setup_signal_handler_thread(pthread_t * p_thread, sig_thread_args_t * p_args);
void *      signal_thread_fn(void * p_args);
static void task_io_read(void * p_arg);
static void task_io_send(void * p_arg);

int main(void)
{
    int err = 0;
    int res = -1;
    int sfd = -1;

    coin_status_t       tp_status = COIN_GENERIC_FAILURE;
    coin_threadpool_t * p_tp      = NULL;

    sigset_t          sigset     = {0};
    pthread_t         sig_thread = {0};
    int               sigpipe_fds[2]; // [0] = read end, [1] = write end
    sig_thread_args_t sig_args = {0};

    conn_mgr_t   conn_mgr  = {0};
    conn_ctx_t * p_cur_ctx = NULL;
    int          cur_refs  = -1;
    int          cur_state = -1;


    /* Setup the signal handler to attempt a graceful shutdown on SIGINT and SIGTERM and ignore SIGPIPE */
    LOG_INFO("Setting up thread to handle SIGINT and SIGTERM.");
    err = pipe(sigpipe_fds);
    if (-1 == err)
    {
        LOG_FATAL("Failed to create pipe for signal handling");
        goto end;
    }

    (void)fcntl(sigpipe_fds[0], F_SETFL, O_NONBLOCK);
    (void)fcntl(sigpipe_fds[1], F_SETFL, O_NONBLOCK);

    sig_args.p_sigset   = &sigset;
    sig_args.sigpipe_fd = sigpipe_fds[1];
    err                 = setup_signal_handler_thread(&sig_thread, &sig_args);
    if (0 != err)
    {
        LOG_FATAL("Failed to setup signal handlers");
        goto close_pipefds;
    }

    /* Setup the connection manager that will handle new and closed connections */
    LOG_INFO("Setting up connection manager.");
    err = connmgr_init(&conn_mgr, INITIAL_MAX_CONNS);
    if (0 != err)
    {
        LOG_FATAL("Failed to initialize connection manager");
        goto join_sig_thread;
    }

    /* Setup the threadpool that will handle the I/O for each connection */
    tp_status = coin_tpool_init(&p_tp, NUM_THREADS);
    if (COIN_SUCCESS != tp_status)
    {
        LOG_FATAL("Failed to initialize threadpool");
        goto destroy_connmgr;
    }

    /* Start the main server listening socket and make its connection context */
    LOG_INFO("Starting listening socket.");
    sfd = nl_start_listener("127.0.0.1", "1337");
    if (-1 == sfd)
    {
        LOG_FATAL("Failed to start listening on socket");
        goto destroy_tpool;
    }

    /* Add the signal pipe fds and listening socket as new connections */
    err = connmgr_create_new_conn(sfd, &conn_mgr, INTERNAL_CONN);
    if (0 != err)
    {
        LOG_FATAL("Failed to create connection structure for listener.");
        goto cleanup_connections;
    }

    err = connmgr_create_new_conn(sigpipe_fds[0], &conn_mgr, INTERNAL_CONN);
    if (0 != err)
    {
        LOG_FATAL("Failed to create connection structure for listener.");
        goto cleanup_connections;
    }

    while (!g_should_shutdown)
    {
        /* Update the connection manager to account for recently added connections and handle closing ones */
        for (uint16_t conn = 0; conn < conn_mgr.num_active_conns; conn++)
        {
            err = ezarr_get_at(conn_mgr.p_conns, conn, (void **)&p_cur_ctx);
            if (0 != err)
            {
                LOG_FATAL("Failed to get connection context");
                goto cleanup_connections;
            }

            /* No need to lock here because this value is only ever set once and is then read-only. These connections
             * only ever close at server shutdown, so there's little sense in checking them.
             */
            if (INTERNAL_CONN == p_cur_ctx->type)
            {
                continue;
            }

            /* Save off the context's state and reference count */
            err = pthread_mutex_lock(&(p_cur_ctx->mutex));
            if (0 != err)
            {
                LOG_ERROR("Failed to lock mutex when updating events");
                continue;
            }

            cur_refs  = p_cur_ctx->ref_count;
            cur_state = p_cur_ctx->state;

            err = pthread_mutex_unlock(&(p_cur_ctx->mutex));
            if (0 != err)
            {
                LOG_ERROR("Failed to unlock mutex after updating events");
                continue;
            }

            /* If no more threads have a reference to the connection and its ready to close, close it */
            if ((cur_state == PENDING_CLOSE) && (0 == cur_refs))
            {
                LOG_INFO("Connection on fd %d is PENDING CLOSE and will be deleted.", p_cur_ctx->fd);

                connmgr_destroy_conn(p_cur_ctx);

                err = ezarr_set_at(conn_mgr.p_conns, conn, NULL);
                if (0 == err)
                {
                    /* Only set the p_fd sentinel value if p_conn set successful, otherwise they'll be out of sync */
                    memset(conn_mgr.p_pfds + conn, 0, sizeof(struct pollfd));
                }

                else
                {
                    LOG_ERROR("Failed to set connection context to NULL. The array will have a stale entry.");
                }
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
        err = poll(conn_mgr.p_pfds, conn_mgr.num_active_conns, -1);
        if (-1 == err)
        {
            LOG_ERROR("poll() failed with errno %d (%s)", errno, strerror(errno));
            break;
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
                if (sfd == conn_mgr.p_pfds[conn].fd)
                {
                    err = nl_accept(conn_mgr.p_pfds[conn].fd, &conn_mgr);
                    if (-1 == err)
                    {
                        LOG_ERROR("Failed to accept new connections");
                        goto cleanup_connections;
                    }
                }

                /* If the read end of the self pipe is ready for reading, it means we need to shut down */
                else if (p_cur_ctx->fd == sigpipe_fds[0])
                {
                    LOG_INFO("Received data on self pipe. Shutting down.");
                    break;
                }

                else
                {
                    err = pthread_mutex_lock(&(p_cur_ctx->mutex));
                    if (0 != err)
                    {
                        LOG_ERROR("Failed to lock mutex");
                        goto cleanup_connections;
                    }

                    p_cur_ctx->ref_count += 1;

                    tp_status = coin_tpool_submit(p_tp, task_io_read, p_cur_ctx, NULL);
                    if (COIN_SUCCESS != tp_status)
                    {
                        LOG_ERROR("Failed to submit to coin_tpool");
                        (void)pthread_mutex_unlock(&(p_cur_ctx->mutex));
                        goto cleanup_connections;
                    }

                    err = pthread_mutex_unlock(&(p_cur_ctx->mutex));
                    if (0 != err)
                    {
                        LOG_ERROR("Failed to unlock mutex");
                        goto cleanup_connections;
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

                tp_status = coin_tpool_submit(p_tp, task_io_send, p_cur_ctx, NULL);
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

join_sig_thread:
    /* If exiting for non-signal reasons, need to tell the signal thread to break its loop - just send it a signal */
    if (0 == g_should_shutdown)
    {
        (void)pthread_kill(sig_thread, SIGUSR1);
    }
    (void)pthread_join(sig_thread, NULL);

close_pipefds:
    (void)close(sigpipe_fds[0]);
    (void)close(sigpipe_fds[1]);

end:
    return res;
}

/* STATIC FUNCTION DEFINITIONS */
static int setup_signal_handler_thread(pthread_t * p_thread, sig_thread_args_t * p_args)
{
    assert(NULL != p_thread);
    assert(NULL != p_args);

    int err = -1;

    err = sigemptyset(p_args->p_sigset);
    if (0 != err)
    {
        LOG_ERROR("Failed to empty signal set");
        return err;
    }

    err = sigaddset(p_args->p_sigset, SIGPIPE);
    if (0 != err)
    {
        LOG_ERROR("Failed to add SIGPIPE to signal set");
        return err;
    }

    err = sigaddset(p_args->p_sigset, SIGINT);
    if (0 != err)
    {
        LOG_ERROR("Failed to add SIGINT to signal set");
        return err;
    }

    err = sigaddset(p_args->p_sigset, SIGTERM);
    if (0 != err)
    {
        LOG_ERROR("Failed to add SIGTERM to signal set");
        return err;
    }

    err = pthread_sigmask(SIG_BLOCK, p_args->p_sigset, NULL);
    if (0 != err)
    {
        LOG_ERROR("Failed to block signals for business threads.");
        return err;
    }

    err = pthread_create(p_thread, NULL, signal_thread_fn, p_args);
    if (0 != err)
    {
        LOG_ERROR("Failed to create signal thread.");
        return err;
    }

    return 0;
}

void * signal_thread_fn(void * p_args)
{
    assert(NULL != p_args);
    sig_thread_args_t * p_sig_args = (sig_thread_args_t *)(p_args);
    sigset_t *          p_sigset   = (sigset_t *)(p_sig_args->p_sigset);
    int                 sigpipe_fd = p_sig_args->sigpipe_fd;
    int                 signal     = 0;
    int                 err        = -1;

    for (;;)
    {
        uint8_t byte = 1;

        err = sigwait(p_sigset, &signal);
        if (0 != err)
        {
            LOG_ERROR("Failed to wait for signal");
            break;
        }

        if ((SIGINT == signal) || (SIGTERM == signal))
        {
            LOG_INFO("Received signal %d. Shutting down.", signal);
            g_should_shutdown = 1;

            // Wake up poll() in the main thread
            write(sigpipe_fd, &byte, sizeof(byte));
            break;
        }

        else if (SIGUSR1 == signal)
        {
            LOG_INFO("Received SIGUSR1. Breaking loop.");
            break;
        }
    }

    return NULL;
}

static void task_io_read(void * p_arg)
{
    assert(NULL != p_arg);
    conn_ctx_t * p_ctx = (conn_ctx_t *)(p_arg);
    int          err   = 0;

    err = pthread_mutex_lock(&(p_ctx->mutex));
    if (0 != err)
    {
        LOG_ERROR("Failed to lock mutex for I/O");
    }

    if (((p_ctx->state == READ_CONTENT) || (p_ctx->state == READ_HEADER)) && !g_should_shutdown)
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

static void task_io_send(void * p_arg)
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
