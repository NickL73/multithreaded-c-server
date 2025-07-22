/**
 * @file main.c
 * @author nick
 * @date 7/11/25
 * @brief
 */
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
volatile sig_atomic_t g_should_shutdown = 0;

/* STATIC FUNCTION DECLARATIONS */
static void sighandler(int signum);
static int  setup_signal_handlers(void);

int main(int argc, char * argv[])
{
    int          err       = 0;
    int          res       = -1;
    int          sfd       = -1;
    conn_ctx_t * p_cur_ctx = NULL;
    conn_mgr_t   conn_mgr  = {0};

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

    /* Start the main server listening socket */
    LOG_INFO("Starting listening socket.");
    sfd = nl_start_listener("127.0.0.1", "1337");
    if (-1 == sfd)
    {
        LOG_FATAL("Failed to start listening on socket");
        goto destroy_connmgr;
    }

    err = connmgr_create_new_conn(sfd, &conn_mgr);
    if (0 != err)
    {
        LOG_FATAL("Failed to create connection structure for listener.");
        goto cleanup_connections;
    }

    err = connmgr_update_connections(&conn_mgr);
    if (0 != err)
    {
        LOG_FATAL("Failed to update connection manager for listener.");
        // TODO: Is this the right action? I'm not sure yet.
        goto cleanup_connections;
    }

    while (!g_should_shutdown)
    {
        LOG_INFO("Polling connections for activity.");
        err = poll(conn_mgr.p_pfds, conn_mgr.num_active_conns, -1);
        if (-1 == err)
        {
            LOG_ERROR("poll() failed with errno %d (%s)", errno, strerror(errno));
            if (EINTR == errno)
            {
                continue;
            }

            break;
        }

        for (uint16_t conn = 0; conn < conn_mgr.num_active_conns; conn++)
        {
            err = ezarr_get_at(conn_mgr.p_conns, conn, (void **)&p_cur_ctx);
            if (0 != err)
            {
                LOG_FATAL("Failed to get connection context");
                goto destroy_connmgr;
            }

            /* Check if the connection has been marked for deletion before tasking anything to the threadpool */
            LOG_INFO("Checking status of connection.");
            err = connmgr_check_active_connection((conn_ctx_t *)(conn_mgr.p_conns->pp_buf[conn]));
            if (-1 == err)
            {
                LOG_FATAL("Failed to check active connection");
                goto destroy_connmgr;
            }

            /* Client connection is no longer active, if no more references free resources and set sentinel values  */
            if (0 == err)
            {
                LOG_INFO("Connection no longer active. Will attempt to remove.");
                err = connmgr_attempt_deletion(&conn_mgr, conn);
                if (-1 == err)
                {
                    LOG_FATAL("Failed to attempt deletion");
                    goto destroy_connmgr;
                }

                /* Nothing else to do for a connection pending deletion so move on */
                continue;
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
                        goto destroy_connmgr;
                    }
                }

                else
                {
                    err = nl_handle_sock_data_in(p_cur_ctx);
                }
            }

            if (conn_mgr.p_pfds[conn].revents & (POLLHUP | POLLERR | POLLNVAL))
            {
                LOG_INFO("Connection on fd %d closed. Marking for deletion.", conn_mgr.p_pfds[conn].fd);
                // err = pthread_mutex_lock(&((conn_ctx_t *)&(conn_mgr.p_conns->pp_buf[conn]))->mutex);
                // if (0 != err)
                // {
                //     LOG_FATAL("Failed to lock mutex on dead connection.");
                //     goto destroy_connmgr;
                // }

                ((conn_ctx_t *)&(conn_mgr.p_conns->pp_buf[conn]))->b_marked_for_deletion = true;

                // err = pthread_mutex_unlock(&((conn_ctx_t *)&(conn_mgr.p_conns->pp_buf[conn]))->mutex);
                // if (0 != err)
                // {
                //     LOG_FATAL("Failed to unlock mutex on dead connection.");
                //     goto destroy_connmgr;
                // }
            }

            if (conn_mgr.p_pfds[conn].revents & POLLOUT)
            {
                LOG_INFO("Connection %d is ready for writing", conn);
                err = nl_handle_sock_data_out(p_cur_ctx);
            }
        }

        err = connmgr_update_connections(&conn_mgr);
        if (0 != err)
        {
            LOG_FATAL("Failed to update connections");
            break;
        }

        p_cur_ctx = NULL;
    }

    /* Exiting cleanly from the loop */
    LOG_INFO("Exiting cleanly from the main loop.");
    res = 0;

cleanup_connections:
    // TODO: Close everything tracked by conn mgr
    close(sfd);

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
