/**
 * @file main.c
 * @author nick
 * @date 7/11/25
 * @brief
 */
#include "debug_print.h"

#include <signal.h>


/* GLOBAL DEFINITIONS AND VALUES */
#define SERVER_PORT 1337
volatile sig_atomic_t should_shutdown = 0;

/* STATIC FUNCTION DECLARATIONS */
static void sighandler(int signum);

int main(int argc, char * argv[])
{
    int err = 0;

    /* Setup the signal handler to attempt a graceful shutdown on SIGINT and SIGTERM */
    struct sigaction sa = {0};
    sa.sa_handler       = sighandler;
    sa.sa_flags         = 0;
    if (0 != sigemptyset(&sa.sa_mask))
    {
        LOG_FATAL("Failed to empty server's signal set");
        goto end;
    }

    if ((0 != sigaction(SIGINT, &sa, NULL)) || (0 != sigaction(SIGTERM, &sa, NULL)))
    {
        LOG_FATAL("Failed to set server's signal handler");
        goto end;
    }

    /* Don't let SIGPIPE break the server either, just ignore it */
    (void)signal(SIGPIPE, SIG_IGN);

end:
    return 0;
}

/* STATIC FUNCTION DEFINITIONS */
static void sighandler(int signum)
{
    should_shutdown = 1;
}
