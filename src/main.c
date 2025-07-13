/**
 * @file main.c
 * @author nick
 * @date 7/11/25
 * @brief
 */
#include "ezqueue.h"
#include "netio.h"
#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

/* COMMON STRUCTURES */
typedef struct conn_context
{
    int fd;
    int is_active;
};

/* GLOBAL VARIABLES AND VALUES */
#define INITIAL_Q_SIZE 16
#define SERVER_PORT    1337
volatile sig_atomic_t g_should_shutdown = 0;

/* STATIC FUNCTION DECLARATIONS */
static void sighandler(int signum);
static int  setup_signal_handlers(void);
static int  setup_conn_mgmt_queues(ezqueue_t ** pp_new_conns, ezqueue_t ** pp_del_conns);
static void teardown_conn_mgmt_queues(ezqueue_t * p_new_conns, ezqueue_t * p_del_conns);

int main(int argc, char * argv[])
{
    int           err            = 0;
    int           sfd            = -1;
    int           active_sockets = 0;
    struct pollfd pfd[10]        = {0};

    ezqueue_t * p_new_conns = NULL;
    ezqueue_t * p_del_conns = NULL;

    // TODO: Start a single thread to handle signals

    /* Setup the signal handler to attempt a graceful shutdown on SIGINT and SIGTERM and ignore SIGPIPE */
    err = setup_signal_handlers();
    if (0 != err)
    {
        LOG_FATAL("Failed to setup signal handlers");
        goto end;
    }

    /* Create queues the threadpool will use to mark new connections and connections for removal */
    err = setup_conn_mgmt_queues(&p_new_conns, &p_del_conns);
    if (0 != err)
    {
        LOG_FATAL("Failed to setup connection management queues");
        goto end;
    }

    /* Start the main server listening socket */
    sfd = nl_start_listener("127.0.0.1", "1337");
    if (-1 == sfd)
    {
        LOG_FATAL("Failed to start listening on socket");
        goto destroy_queues;
    }

    /* TODO: Add the listening socket to the poll set */
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

destroy_queues:
    teardown_conn_mgmt_queues(p_new_conns, p_del_conns);
    p_new_conns = NULL;
    p_del_conns = NULL;

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

static int setup_conn_mgmt_queues(ezqueue_t ** pp_new_conns, ezqueue_t ** pp_del_conns)
{
    assert(NULL != pp_new_conns);
    assert(NULL != pp_del_conns);

    int res = -1;

    ezqueue_t * p_new_conn = NULL;
    ezqueue_t * p_del_conn = NULL;

    p_new_conn = (ezqueue_t *)malloc(sizeof(ezqueue_t));
    if (NULL == p_new_conn)
    {
        LOG_ERROR("Failed to allocate memory for ezqueue_t for incoming conns");
        goto err;
    }

    res = ezq_init(p_new_conn, INITIAL_Q_SIZE);
    if (0 != res)
    {
        LOG_ERROR("Failed to initialize ezqueue for incoming conns");
        goto cleanup_new_conn;
    }

    p_del_conn = (ezqueue_t *)malloc(sizeof(ezqueue_t));
    if (NULL == p_del_conn)
    {
        LOG_ERROR("Failed to allocate memory for ezqueue_t for closed conns");
        goto destroy_new_conn;
    }

    res = ezq_init(p_del_conn, SERVER_PORT);
    if (0 != res)
    {
        LOG_ERROR("Failed to initialize ezqueue for closed conns");
        goto cleanup_del_conn;
    }

    *pp_new_conns = p_new_conn;
    *pp_del_conns = p_del_conn;
    return res;

cleanup_del_conn:
    free(p_del_conn);
    p_del_conn = NULL;

destroy_new_conn:
    (void)ezq_deinit(p_new_conn);

cleanup_new_conn:
    free(p_new_conn);
    p_new_conn = NULL;

err:
    return res;
}

static void teardown_conn_mgmt_queues(ezqueue_t * p_new_conns, ezqueue_t * p_del_conns)
{
    assert(NULL != p_new_conns);
    assert(NULL != p_del_conns);

    (void)ezq_deinit(p_new_conns);
    (void)ezq_deinit(p_del_conns);

    free(p_new_conns);
    free(p_del_conns);
}
