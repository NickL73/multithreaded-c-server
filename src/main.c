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
    int err = 0;
    int sfd = -1;

    conn_mgr_t conn_mgr = {0};

    // TODO: Start a single thread to handle signals

    /* Setup the signal handler to attempt a graceful shutdown on SIGINT and SIGTERM and ignore SIGPIPE */
    err = setup_signal_handlers();
    if (0 != err)
    {
        LOG_FATAL("Failed to setup signal handlers");
        goto end;
    }

    /* Setup the connection manager that will handle new and closed connections */
    err = connmgr_init(&conn_mgr, INITIAL_MAX_CONNS);
    if (0 != err)
    {
        LOG_FATAL("Failed to initialize connection manager");
        goto end;
    }

    /* Start the main server listening socket */
    sfd = nl_start_listener("127.0.0.1", "1337");
    if (-1 == sfd)
    {
        LOG_FATAL("Failed to start listening on socket");
        goto destroy_connmgr;
    }


    /* TODO: Add the listening socket to the poll set */

    /* TODO: Create conn_ctx_t for listener */
    /* TODO: Add conn_ctx_t for listener to new connections queue */
    /* TODO: connmgr_add_new */

    while (!g_should_shutdown)
    {
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
            /* Check if the connection has been marked for deletion before tasking anything to the threadpool */
            err = connmgr_check_active_connection((conn_ctx_t *)(conn_mgr.p_conns->pp_buf[conn]));

            if (-1 == err)
            {
                LOG_FATAL("Failed to check active connection");
                goto destroy_connmgr;
            }

            /* Client connection is no longer active, if no more references free resources and set sentinel values  */
            if (0 == err)
            {
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
                if (0 == conn)
                {
                    err = nl_accept(conn_mgr.p_pfds[conn].fd, conn_mgr.p_new_conns);
                }

                else
                {
                    err = nl_handle_sock_data_in((conn_ctx_t *)(&(conn_mgr.p_conns[conn])));
                }
            }

            if (conn_mgr.p_pfds[conn].revents & (POLLHUP | POLLERR | POLLNVAL))
            {
                LOG_INFO("Connection %d closed", conn);
                err = connmgr_mark_for_deletion((conn_ctx_t *)(&conn_mgr.p_conns[conn]));
            }

            if (conn_mgr.p_pfds[conn].revents & POLLOUT)
            {
                LOG_INFO("Connection %d is ready for writing", conn);
                err = nl_handle_sock_data_out((conn_ctx_t *)(&(conn_mgr.p_conns[conn])));
            }
        }

        err = connmgr_update_connections(&conn_mgr);
        if (0 != err)
        {
            LOG_FATAL("Failed to update connections");
            break;
        }
    }

destroy_connmgr:
    (void)connmgr_deinit(&conn_mgr);

end:
    return 0;
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
    sa.sa_flags          = 0;
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
