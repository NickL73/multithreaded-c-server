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
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

/* SERVER STRUCTURES */
typedef struct conn_context
{
    int fd;
    int is_active;
};

typedef struct conn_mgmt_queue
{
    ezqueue_t *       p_queue;
    pthread_mutex_t * p_mutex;
} conn_mgmt_queue_t;

/* GLOBAL VARIABLES AND VALUES */
#define INITIAL_Q_SIZE 16
#define SERVER_PORT    1337
volatile sig_atomic_t g_should_shutdown = 0;

/* STATIC FUNCTION DECLARATIONS */
static void sighandler(int signum);
static int  setup_signal_handlers(void);
static int  create_mgmt_queue(conn_mgmt_queue_t ** pp_queue);
static void destroy_mgmt_queue(conn_mgmt_queue_t * p_queue);
static int  setup_conn_mgmt_queues(conn_mgmt_queue_t ** pp_new_conns, conn_mgmt_queue_t ** pp_del_conns);
static void teardown_conn_mgmt_queues(conn_mgmt_queue_t * p_new_conns, conn_mgmt_queue_t * p_del_conns);

int main(int argc, char * argv[])
{
    int           err            = 0;
    int           sfd            = -1;
    int           active_sockets = 0;
    struct pollfd pfd[10]        = {0};

    conn_mgmt_queue_t * p_new_conns = NULL;
    conn_mgmt_queue_t * p_del_conns = NULL;

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

static int create_mgmt_queue(conn_mgmt_queue_t ** pp_queue)
{
    assert(NULL != pp_queue);

    int                 res     = -1;
    conn_mgmt_queue_t * p_queue = NULL;
    ezqueue_t *         p_ezq   = NULL;
    pthread_mutex_t *   p_mutex = NULL;

    p_queue = (conn_mgmt_queue_t *)malloc(sizeof(conn_mgmt_queue_t));
    if (NULL == p_queue)
    {
        LOG_ERROR("Failed to allocate memory for conn_mgmt_queue_t");
        goto err;
    }

    p_ezq = (ezqueue_t *)malloc(sizeof(ezqueue_t));
    if (NULL == p_ezq)
    {
        LOG_ERROR("Failed to allocate memory for ezqueue_t");
        goto cleanup_queue;
    }

    res = ezq_init(p_ezq, INITIAL_Q_SIZE);
    if (0 != res)
    {
        LOG_ERROR("Failed to initialize ezqueue");
        goto cleanup_ezq;
    }

    p_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (NULL == p_mutex)
    {
        LOG_ERROR("Failed to allocate memory for mutex");
        goto deinit_ezq;
    }

    res = pthread_mutex_init(p_mutex, NULL);
    if (0 != res)
    {
        LOG_ERROR("Failed to initialize mutex");
        goto cleanup_mutex;
    }

    p_queue->p_queue = p_ezq;
    p_queue->p_mutex = p_mutex;
    *pp_queue        = p_queue;

    return 0;

cleanup_mutex:
    free(p_mutex);
    p_mutex = NULL;

deinit_ezq:
    (void)ezq_deinit(p_ezq);

cleanup_ezq:
    free(p_ezq);
    p_ezq = NULL;

cleanup_queue:
    free(p_queue);
    p_queue = NULL;

err:
    return -1;
}

static void destroy_mgmt_queue(conn_mgmt_queue_t * p_queue)
{
    assert(NULL != p_queue);

    (void)pthread_mutex_destroy(p_queue->p_mutex);
    free(p_queue->p_queue);
    p_queue->p_mutex = NULL;

    (void)ezq_deinit(p_queue->p_queue);
    free(p_queue->p_queue);
    p_queue->p_queue = NULL;

    free(p_queue);
}

static int setup_conn_mgmt_queues(conn_mgmt_queue_t ** pp_new_conns, conn_mgmt_queue_t ** pp_del_conns)
{
    assert(NULL != pp_new_conns);
    assert(NULL != pp_del_conns);

    int res = -1;

    conn_mgmt_queue_t * p_new_conn = NULL;
    conn_mgmt_queue_t * p_del_conn = NULL;

    res = create_mgmt_queue(&p_new_conn);
    if (0 != res)
    {
        LOG_ERROR("Failed to create conn_mgmt_queue for new conns");
        goto err;
    }

    res = create_mgmt_queue(&p_del_conn);
    if (0 != res)
    {
        LOG_ERROR("Failed to create conn_mgmt_queue for closed conns");
        goto cleanup_new_conn;
    }

    *pp_new_conns = p_new_conn;
    *pp_del_conns = p_del_conn;
    return 0;

cleanup_new_conn:
    destroy_mgmt_queue(p_new_conn);
    p_new_conn = NULL;

err:
    return -1;
}

static void teardown_conn_mgmt_queues(conn_mgmt_queue_t * p_new_conns, conn_mgmt_queue_t * p_del_conns)
{
    assert(NULL != p_new_conns);
    assert(NULL != p_del_conns);

    destroy_mgmt_queue(p_new_conns);
    destroy_mgmt_queue(p_del_conns);
}
