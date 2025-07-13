/**
 * @file main.c
 * @author nick
 * @date 7/11/25
 * @brief
 */
#include "netio.h"
#include "utils.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>


/* GLOBAL DEFINITIONS AND VALUES */
#define SERVER_PORT 1337
volatile sig_atomic_t g_should_shutdown = 0;

/* STATIC FUNCTION DECLARATIONS */
static void sighandler(int signum);
static int  setup_signal_handlers(void);

int main(int argc, char * argv[])
{
    int           err            = 0;
    int           sfd            = -1;
    int           active_sockets = 0;
    struct pollfd pfd[10]        = {0};

    /* Setup the signal handler to attempt a graceful shutdown on SIGINT and SIGTERM and ignore SIGPIPE */
    err = setup_signal_handlers();
    if (0 != err)
    {
        LOG_FATAL("Failed to setup signal handlers");
        goto end;
    }

    /* Start the main server listening socket */
    sfd = nl_start_listener("127.0.0.1", "1337");
    if (-1 == sfd)
    {
        LOG_FATAL("Failed to start listening on socket");
        goto end;
    }

    /* Add the listening socket to the poll set */
    active_sockets += 1;

    while (!g_should_shutdown)
    {
        err = poll(pfd, active_sockets, -1);
        if (-1 == err)
        {
            LOG_ERROR("poll() failed with errno %d (%s)", errno, strerror(errno));
            if (EINTR == errno)
            {
                continue;
            }

            break;
        }

        for (int conn = 0; conn < active_sockets; conn++)
        {
            if (pfd[conn].revents & POLLIN)
            {
                LOG_INFO("Received data on connection %d", conn);
                err = nl_handle_incoming_data(pfd[conn].fd);
            }

            if (pfd[conn].revents & (POLLHUP | POLLERR | POLLNVAL))
            {
                LOG_INFO("Connection %d closed", conn);
                // TODO: Mark as ready for removal and close the socket
            }

            if (pfd[conn].revents & POLLOUT)
            {
                LOG_INFO("Connection %d is ready for writing", conn);
                // TODO: Send the data
            }
        }

        // TODO: Compact array down for closed connections
    }


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
