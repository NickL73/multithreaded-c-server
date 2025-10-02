/**
 * @file coin.c
 * @author nick
 * @date 5/7/25
 * @brief
 */

#include "concurinc/coin.h"

#include "cards/queue.h"

#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define INITIAL_WORK_QUEUE_CAPACITY 64

typedef enum thread_status_t
{
    COIN_THREAD_STARTED,
    COIN_THREAD_TERMINATED,
    COIN_THREAD_JOINED
} thread_status_t;

typedef struct coin_tpool_thread
{
    pthread_t       tid;
    thread_status_t status;
    void *          p_retval;
} coin_tpool_thread_t;

typedef struct tpool_ctx
{
    bool b_shutdown;

    cards_queue_s * p_work_queue;
    pthread_mutex_t tpool_queue_mutex;
    pthread_cond_t  tpool_queue_cond;

    size_t          active_tasks;
    pthread_mutex_t active_tasks_mutex;
    pthread_cond_t  active_tasks_cond;
} tpool_ctx_t;

struct coin_threadpool
{
    coin_tpool_thread_t * p_threads;
    tpool_ctx_t *         p_ctx;
    size_t                num_workers;
};

struct coin_task
{
    coin_task_func_t          coin_task_func;
    void *                    coin_task_arg;
    coin_task_free_arg_func_t coin_task_free_arg_func;
};

/* STATIC FUNCTION DECLARATIONS */
static void *        coin_thread_worker(void * p_ctx);
static coin_status_t coin_tpool_ctx_init(tpool_ctx_t ** pp_ctx);
static coin_status_t coin_tpool_ctx_destroy(tpool_ctx_t * p_ctx);
static coin_status_t coin_initialize_workers(tpool_ctx_t * p_ctx, coin_tpool_thread_t ** pp_threads,
                                             size_t num_workers);
static coin_status_t coin_tpool_stop_workers(const coin_threadpool_t * p_tpool);

/* PUBLIC FUNCTION DEFINITIONS */

/**
 * @brief Initializes a threadpool with the specified number of worker threads.
 *
 * This function creates and initializes a threadpool structure that manages
 * worker threads. It allocates memory for the threadpool container, initializes
 * its internal context, and spawns the required number of worker threads.
 *
 * @param[out] pp_tpool A double pointer to the threadpool structure. The caller
 *                      provides a pointer that will be updated with the created
 *                      threadpool instance. Must not be NULL.
 * @param[in]  num_workers The number of worker threads to create. Must be greater than zero.
 *
 * @return Returns a status code indicating the outcome of the operation:
 *         - COIN_SUCCESS: The threadpool was successfully created and initialized.
 *         - COIN_INVALID_INPUT: The provided input parameters were invalid.
 *         - COIN_ALLOCATION_FAILURE: Memory allocation for the threadpool container failed.
 *         - Any status returned by `coin_tpool_ctx_init` or `coin_initialize_workers`.
 *
 * @note It is the caller's responsibility to destroy the threadpool by calling
 *       an appropriate cleanup function, such as `coin_tpool_destroy`, when it is
 *       no longer necessary.
 *
 * @warning This function uses dynamically allocated memory. If the function fails,
 *          ensure proper cleanup of partially allocated resources (if applicable).
 */
coin_status_t coin_tpool_init(coin_threadpool_t ** pp_tpool, const size_t num_workers)
{
    coin_status_t status = COIN_GENERIC_FAILURE;

    coin_threadpool_t *   p_threadpool  = NULL;
    coin_tpool_thread_t * p_threads_arr = NULL;
    tpool_ctx_t *         p_ctx         = NULL;


    if ((NULL == pp_tpool) || (0 == num_workers))
    {
        status = COIN_INVALID_INPUT;
        goto end;
    }

    /* Initialize the threadpool container */
    p_threadpool = calloc(1, sizeof(coin_threadpool_t));
    if (NULL == p_threadpool)
    {
        status = COIN_ALLOCATION_FAILURE;
        goto end;
    }

    status = coin_tpool_ctx_init(&p_ctx);
    if (COIN_SUCCESS != status)
    {
        goto cleanup_container;
    }

    p_threadpool->p_ctx = p_ctx;
    p_ctx               = NULL;

    status = coin_initialize_workers(p_threadpool->p_ctx, &p_threads_arr, num_workers);
    if (COIN_SUCCESS != status)
    {
        goto cleanup_ctx;
    }

    p_threadpool->num_workers = num_workers;
    p_threadpool->p_threads   = p_threads_arr;
    p_threads_arr             = NULL;

    *pp_tpool    = p_threadpool;
    p_threadpool = NULL;

    status = COIN_SUCCESS;
    return status;

cleanup_ctx:
    (void)coin_tpool_ctx_destroy(p_threadpool->p_ctx);
    p_threadpool->p_ctx = NULL;

cleanup_container:
    free(p_threadpool);
    p_threadpool = NULL;

end:
    return status;
}

/**
 * @brief Waits for all tasks in the threadpool to complete.
 *
 * This function blocks execution until all active tasks in the specified
 * threadpool have been completed and the queue of pending tasks is empty.
 * It ensures that no tasks are currently being executed and that no work
 * remains in the task queue before returning.
 *
 * @param[in] p_tpool A pointer to the threadpool structure to wait on.
 *                    Must not be NULL and must point to a valid threadpool
 *                    instance created using `coin_tpool_init`.
 *
 * @return Returns a status code indicating the outcome of the operation:
 *         - COIN_SUCCESS: All tasks have been successfully completed, and the
 *           task queue is empty.
 *         - COIN_PTHREAD_MUTEX_ERROR: A mutex operation (lock or unlock) failed.
 *         - COIN_PTHREAD_COND_ERROR: A condition variable operation failed.
 *         - COIN_GENERIC_FAILURE: A general failure occurred.
 *
 * @note This function interacts with internal threadpool synchronization mechanisms
 *       (mutexes and condition variables) to check and wait for task completion.
 *
 * @warning Ensure the threadpool remains valid and active during this operation.
 *          Undefined behavior may occur if the threadpool is destroyed or corrupted
 *          while this function is executing.
 */
coin_status_t coin_tpool_wait(const coin_threadpool_t * p_tpool)
{
    coin_status_t status    = COIN_GENERIC_FAILURE;
    int           err       = 0;
    bool          b_isempty = false;

    while (!(b_isempty))
    {
        err = pthread_mutex_lock(&(p_tpool->p_ctx->active_tasks_mutex));
        if (0 != err)
        {
            status = COIN_PTHREAD_MUTEX_ERROR;
            break;
        }

        while (0 < p_tpool->p_ctx->active_tasks)
        {
            err = pthread_cond_wait(&(p_tpool->p_ctx->active_tasks_cond), &(p_tpool->p_ctx->active_tasks_mutex));
            if (0 != err)
            {
                (void)pthread_mutex_unlock(&(p_tpool->p_ctx->active_tasks_mutex));
                status = COIN_PTHREAD_COND_ERROR;
                goto end;
            }
        }

        if (0 < p_tpool->p_ctx->p_work_queue->num_items)
        {
            err = pthread_mutex_unlock(&(p_tpool->p_ctx->active_tasks_mutex));
            if (0 != err)
            {
                status = COIN_PTHREAD_MUTEX_ERROR;
                goto end;
            }

            continue;
        }

        err = pthread_mutex_unlock(&(p_tpool->p_ctx->active_tasks_mutex));
        if (0 != err)
        {
            status = COIN_PTHREAD_MUTEX_ERROR;
            break;
        }

        err = pthread_mutex_lock(&(p_tpool->p_ctx->tpool_queue_mutex));
        if (0 != err)
        {
            status = COIN_PTHREAD_MUTEX_ERROR;
            break;
        }

        b_isempty = (0 == p_tpool->p_ctx->p_work_queue->num_items);

        err = pthread_mutex_unlock(&(p_tpool->p_ctx->tpool_queue_mutex));
        if (0 != err)
        {
            status = COIN_PTHREAD_MUTEX_ERROR;
            break;
        }
    }

    if (b_isempty)
    {
        status = COIN_SUCCESS;
    }

end:
    return status;
}

/**
 * @brief Destroys a threadpool and releases all associated resources.
 *
 * This function stops all worker threads in the threadpool, collects their
 * returned values if requested, cleans up the threadpool's context, and
 * frees all memory allocated for the threadpool and its components.
 *
 * @param[in,out] p_tpool A pointer to the threadpool structure to be destroyed.
 *                        Must not be NULL. On successful destruction, the pointer
 *                        is set to NULL.
 * @param[out]    retvals An optional array to store the return values of the worker threads.
 *                        If not NULL, this array must have a size equal to the number
 *                        of worker threads in the threadpool. Can be NULL if return values
 *                        are not needed.
 *
 * @return Returns a status code indicating the outcome of the operation:
 *         - COIN_SUCCESS: The threadpool was successfully destroyed.
 *         - COIN_INVALID_INPUT: Invalid input parameters were provided.
 *         - COIN_GENERIC_FAILURE: A failure occurred during the destruction process.
 *         - Any status returned by `coin_tpool_stop_workers` or `coin_tpool_ctx_destroy`.
 *
 * @note The caller must ensure that no new tasks are submitted to the threadpool
 *       before invoking this function. Proper synchronization mechanisms may be required
 *       if the threadpool is shared across threads.
 *
 * @warning This function must be called to avoid memory leaks. If the destruction fails,
 *          ensure proper cleanup of any partially destroyed resources if applicable.
 */
coin_status_t coin_tpool_destroy(coin_threadpool_t * p_tpool, int retvals[])
{
    coin_status_t status = COIN_GENERIC_FAILURE;

    if (NULL == p_tpool)
    {
        status = COIN_INVALID_INPUT;
        goto end;
    }

    status = coin_tpool_stop_workers(p_tpool);
    if (COIN_SUCCESS != status)
    {
        goto end;
    }

    if (NULL != retvals)
    {
        for (size_t i = 0; i < p_tpool->num_workers; i++)
        {
            retvals[i] = *(int *)(p_tpool->p_threads[i].p_retval);
        }
    }

    status = coin_tpool_ctx_destroy(p_tpool->p_ctx);
    if (COIN_SUCCESS != status)
    {
        goto end;
    }

    free(p_tpool->p_threads);
    p_tpool->p_threads = NULL;

    free(p_tpool);
    p_tpool = NULL;

end:
    return status;
}

/**
 * @brief Submits a task to the threadpool for execution.
 *
 * This function adds a task to the threadpool's work queue and signals
 * worker threads to execute it. The task is defined by a function pointer
 * and its associated argument. Memory cleanup for the argument can be handled
 * by providing an optional cleanup callback.
 *
 * @param[in] p_tpool A pointer to the threadpool instance. Must not be NULL and
 *                    must point to a valid threadpool with an initialized context.
 * @param[in] task_func A pointer to the task function to be executed. Must not be NULL.
 * @param[in] p_task_arg A pointer to the argument that will be passed to the task function.
 *                       Must not be NULL.
 * @param[in] free_arg_func Optional callback function to release memory for the task argument.
 *                          Can be NULL if no cleanup is needed.
 *
 * @return Returns a status code indicating the outcome of the operation:
 *         - COIN_SUCCESS: The task was successfully added to the queue.
 *         - COIN_INVALID_INPUT: One or more input parameters were invalid.
 *         - COIN_ALLOCATION_FAILURE: Memory allocation for the task failed.
 *         - COIN_PTHREAD_MUTEX_ERROR: A mutex lock or unlock operation failed.
 *         - COIN_PTHREAD_COND_ERROR: A condition variable signal operation failed.
 *         - Any other status returned by `coin_queue_push`.
 *
 * @note It is the caller's responsibility to ensure the provided task function and
 *       its argument remain valid for the lifetime of task execution.
 * */
coin_status_t coin_tpool_submit(const coin_threadpool_t * p_tpool, const coin_task_func_t task_func, void * p_task_arg,
                                const coin_task_free_arg_func_t free_arg_func)
{
    coin_status_t status    = COIN_GENERIC_FAILURE;
    cards_err_e   cards_err = CARDS_GENERIC_ERROR;
    coin_task_t * p_task    = NULL;
    int           err       = -1;

    if ((NULL == p_tpool) || (NULL == p_tpool->p_ctx) || (NULL == task_func) || (NULL == p_task_arg))
    {
        status = COIN_INVALID_INPUT;
        goto end;
    }

    p_task = calloc(1, sizeof(coin_task_t));
    if (NULL == p_task)
    {
        status = COIN_ALLOCATION_FAILURE;
        goto end;
    }

    p_task->coin_task_func          = task_func;
    p_task->coin_task_arg           = p_task_arg;
    p_task->coin_task_free_arg_func = free_arg_func;

    err = pthread_mutex_lock(&(p_tpool->p_ctx->tpool_queue_mutex));
    if (0 != err)
    {
        status = COIN_PTHREAD_MUTEX_ERROR;
        goto cleanup_task;
    }

    cards_err = cards_queue_enqueue(p_tpool->p_ctx->p_work_queue, p_task);
    if (CARDS_SUCCESS != cards_err)
    {
        (void)pthread_mutex_unlock(&(p_tpool->p_ctx->tpool_queue_mutex));
        if (CARDS_MAX_CAPACITY_ERROR == cards_err)
        {
            status = COIN_CAPACITY_ERROR;
        }
        else
        {
            status = COIN_QUEUE_SYSTEM_ERR;
        }
        goto cleanup_task;
    }

    p_task = NULL; /* Ownership semantics. The thread pool owns the task pointer now */

    err = pthread_cond_signal(&(p_tpool->p_ctx->tpool_queue_cond));
    if (0 != err)
    {
        /* The task is already on the queue, so not doing cleanup. It'll be cleaned up as the thread
         * pool shuts down. We don't have ownership anymore.
         */
        (void)pthread_mutex_unlock(&(p_tpool->p_ctx->tpool_queue_mutex));
        status = COIN_PTHREAD_COND_ERROR;
        goto end;
    }

    err = pthread_mutex_unlock(&(p_tpool->p_ctx->tpool_queue_mutex));
    if (0 != err)
    {
        status = COIN_PTHREAD_MUTEX_ERROR;
        goto end;
    }

    status = COIN_SUCCESS;
    return status;

cleanup_task:
    free(p_task);
    p_task = NULL;

end:
    return status;
}

/* STATIC FUNCTION DEFINITIONS */

/**
 * @brief Worker function for threads in the threadpool to execute tasks from the task queue.
 *
 * Each thread in the threadpool runs this function, which continuously fetches and executes
 * tasks from the work queue. The function also handles synchronization, task processing,
 * and shutdown signaling.
 *
 * @param[in] p_ctx A void pointer to the threadpool context (`tpool_ctx_t`). This context
 *                  contains the task queue, mutex, and condition variables required for
 *                  thread synchronization. It must not be NULL.
 *
 * @return A void pointer containing the thread exit status. On successful completion, it
 *         returns a value consistent with POSIX thread return values (typically 0). In
 *         case of fatal errors, the return value may indicate a non-zero error code.
 *
 * @note The function runs in an infinite loop to process tasks unless it is signaled to
 *       shut down via the `b_shutdown` flag in the threadpool context. Proper synchronization
 *       mechanisms (mutexes and condition variables) are used to coordinate access to shared
 *       resources.
 *
 * @warning The behavior of the thread may become undefined if the provided context (`p_ctx`)
 *          is invalid or if the work queue is in an inconsistent state. Ensure proper
 *          initialization and shutdown procedures for the threadpool to avoid resource
 *          leaks or deadlocks.
 */
static void * coin_thread_worker(void * p_ctx)
{
    int           err       = 0;
    cards_err_e   cards_err = CARDS_GENERIC_ERROR;
    coin_task_t * p_task    = NULL;

    tpool_ctx_t * p_tpool_ctx = (tpool_ctx_t *)p_ctx;
    assert(p_tpool_ctx);
    assert(p_tpool_ctx->p_work_queue);

    while (true)
    {
        /* Get the lock before checking any of the conditions */
        err = pthread_mutex_lock(&p_tpool_ctx->tpool_queue_mutex);
        if (0 != err)
        {
            break;
        }

        /* If still running, wait until there's some work to do. Release the lock and wait. */
        while ((!p_tpool_ctx->b_shutdown) && (0 == p_tpool_ctx->p_work_queue->num_items))
        {
            err = pthread_cond_wait(&p_tpool_ctx->tpool_queue_cond, &p_tpool_ctx->tpool_queue_mutex);
            if (0 != err)
            {
                break;
            }
        }

        /* If we're shutting down, drop the mutex and break out of the loop */
        if (p_tpool_ctx->b_shutdown)
        {
            /* Not reading the error code here because it doesn't change the behavior of breaking out of the loop, but
             * the logic after the loop will check it and act appropriately
             */
            err = pthread_mutex_unlock(&p_tpool_ctx->tpool_queue_mutex);
            break;
        }

        /* Try to pop a work item */
        cards_err = cards_queue_dequeue(p_tpool_ctx->p_work_queue, (void **)&p_task);
        if ((CARDS_SUCCESS != cards_err) || (NULL == p_task))
        {
            /* The state of the queue is corrupted. This shouldn't happen. Bail out. Don't bother checking return
             * values, because we're already on the way out for an error condition.
             */
            p_tpool_ctx->b_shutdown = true;
            (void)pthread_cond_broadcast(&(p_tpool_ctx->tpool_queue_cond));
            (void)pthread_mutex_unlock(&(p_tpool_ctx->tpool_queue_mutex));
            err = 1; // Try to keep consistent with POSIX thread function return values
            break;
        }


        /* Check once more if there's work on the queue, and if so, let somebody know about it */
        if (0 != p_tpool_ctx->p_work_queue->num_items)
        {
            err = pthread_cond_signal(&(p_tpool_ctx->tpool_queue_cond));
            if (0 != err)
            {
                (void)pthread_mutex_unlock(&(p_tpool_ctx->tpool_queue_mutex));
                break;
            }
        }

        err = pthread_mutex_unlock(&(p_tpool_ctx->tpool_queue_mutex));
        if (0 != err)
        {
            break;
        }

        /* Increment counter of active work (used by boss thread) */
        err = pthread_mutex_lock(&p_tpool_ctx->active_tasks_mutex);
        if (0 != err)
        {
            break;
        }
        p_tpool_ctx->active_tasks++;

        err = pthread_mutex_unlock(&p_tpool_ctx->active_tasks_mutex);
        if (0 != err)
        {
            break;
        }

        err = pthread_cond_broadcast(&(p_tpool_ctx->active_tasks_cond));
        if (0 != err)
        {
            (void)pthread_mutex_unlock(&(p_tpool_ctx->active_tasks_mutex));
            break;
        }


        /* I control this, but let's do a sanity check in case. Allowing NULL args, because it might be relevant to the
         * user's provided function
         */
        assert(p_task->coin_task_func);

        /* Actually do the work */
        p_task->coin_task_func(p_task->coin_task_arg);

        /* If the argument was complex and user was kind enough to tell us how to free it, free it. Otherwise, user can
         * RTFM and free it themselves if they keep a copy of the pointer
         */
        if (NULL != p_task->coin_task_free_arg_func)
        {
            p_task->coin_task_free_arg_func(p_task->coin_task_arg);
            p_task->coin_task_arg = NULL;
        }

        /* We allocate this when we submit the work, so let's free it now that the work is done */
        free(p_task);
        p_task = NULL;

        /* Decrement counter of active work (used by the wait function) */
        err = pthread_mutex_lock(&p_tpool_ctx->active_tasks_mutex);
        if (0 != err)
        {
            break;
        }
        p_tpool_ctx->active_tasks--;

        err = pthread_cond_broadcast(&(p_tpool_ctx->active_tasks_cond));
        if (0 != err)
        {
            (void)pthread_mutex_unlock(&(p_tpool_ctx->active_tasks_mutex));
            break;
        }

        err = pthread_mutex_unlock(&p_tpool_ctx->active_tasks_mutex);
        if (0 != err)
        {
            break;
        }
    }

    /* If exiting the main loop, I don't really care about error conditions. It's a moot point */
    (void)pthread_mutex_lock(&p_tpool_ctx->tpool_queue_mutex);

    /* The main thread loop only breaks on two conditions: first if the shutdown flag was given, and second, on a fatal
     * state for the thread. If one thread gets into a fatal error condition, there's not much realistic action to take.
     * The safest best is to just shut everything down, so set the global shutdown flag and let everybody else know
     * to bail out too.
     */
    if (!p_tpool_ctx->b_shutdown)
    {
        p_tpool_ctx->b_shutdown = true;
        pthread_cond_broadcast(&p_tpool_ctx->tpool_queue_cond);
    }

    (void)pthread_mutex_unlock(&p_tpool_ctx->tpool_queue_mutex);
    return (void *)(intptr_t)(err);
}

/**
 * @brief Initializes a threadpool context structure.
 *
 * This function allocates and initializes a threadpool context used to manage
 * synchronization constructs and a queue of work items. It sets up the required
 * mutex, condition variable, and initializes the associated work queue.
 *
 * @param[out] pp_ctx A double pointer to the threadpool context structure. The caller
 *                    provides a pointer that will be updated with the created context
 *                    instance. Must not be NULL.
 *
 * @return Returns a status code indicating the outcome of the operation:
 *         - COIN_SUCCESS: The threadpool context was successfully created and initialized.
 *         - COIN_ALLOCATION_FAILURE: Memory allocation for the context structure failed.
 *         - COIN_PTHREAD_MUTEX_ERROR: Initialization of the mutex failed.
 *         - COIN_PTHREAD_COND_ERROR: Initialization of the condition variable failed.
 *         - Any status returned by `coin_queue_init`.
 *
 * @note The caller is responsible for releasing resources associated with the
 *       threadpool context by calling a proper cleanup function, such as
 *       `coin_tpool_ctx_destroy`, when it is no longer needed.
 *
 * @warning If this function fails, ensure proper cleanup of partially allocated resources
 *          to avoid memory leaks or resource contention.
 */
static coin_status_t coin_tpool_ctx_init(tpool_ctx_t ** pp_ctx)
{
    assert(pp_ctx);

    coin_status_t   status    = COIN_GENERIC_FAILURE;
    cards_err_e     cards_err = CARDS_GENERIC_ERROR;
    cards_queue_s * p_queue   = NULL;
    int             err       = 1;

    p_queue = calloc(1, sizeof(cards_queue_s));
    if (NULL == p_queue)
    {
        status = COIN_ALLOCATION_FAILURE;
        goto end;
    }

    tpool_ctx_t * p_ctx = calloc(1, sizeof(tpool_ctx_t));
    if (NULL == p_ctx)
    {
        status = COIN_ALLOCATION_FAILURE;
        goto cleanup_queue_allocation;
    }

    err = pthread_mutex_init(&(p_ctx->tpool_queue_mutex), NULL);
    if (0 != err)
    {
        status = COIN_PTHREAD_MUTEX_ERROR;
        goto cleanup_ctx;
    }

    err = pthread_cond_init(&(p_ctx->tpool_queue_cond), NULL);
    if (0 != err)
    {
        status = COIN_PTHREAD_COND_ERROR;
        goto cleanup_queue_mutex;
    }

    cards_err = cards_queue_init(p_queue, INITIAL_WORK_QUEUE_CAPACITY, true);
    if (CARDS_SUCCESS != cards_err)
    {
        status = COIN_QUEUE_SYSTEM_ERR;
        goto cleanup_queue_cond;
    }

    err = pthread_mutex_init(&(p_ctx->active_tasks_mutex), NULL);
    if (0 != err)
    {
        status = COIN_PTHREAD_MUTEX_ERROR;
        goto deinitialize_queue;
    }

    err = pthread_cond_init(&(p_ctx->active_tasks_cond), NULL);
    if (0 != err)
    {
        status = COIN_PTHREAD_COND_ERROR;
        goto cleanup_tasks_mutex;
    }

    p_ctx->p_work_queue = p_queue;
    p_queue             = NULL;

    p_ctx->b_shutdown   = false;
    p_ctx->active_tasks = 0;

    *pp_ctx = p_ctx;
    p_ctx   = NULL;

    status = COIN_SUCCESS;
    return status;

cleanup_tasks_mutex:
    (void)pthread_mutex_destroy(&(p_ctx->active_tasks_mutex));

deinitialize_queue:
    (void)cards_queue_deinit(p_queue);

cleanup_queue_cond:
    (void)pthread_cond_destroy(&(p_ctx->tpool_queue_cond));

cleanup_queue_mutex:
    (void)pthread_mutex_destroy(&(p_ctx->tpool_queue_mutex));

cleanup_ctx:
    free(p_ctx);
    p_ctx = NULL;

cleanup_queue_allocation:
    free(p_queue);
    p_queue = NULL;

end:
    return status;
}

/**
 * @brief Destroys the threadpool context, freeing its resources.
 *
 * This function is responsible for cleaning up and releasing all resources
 * associated with a threadpool context. It deallocates memory, destroys the
 * work queue, and cleans up synchronization primitives such as mutexes and
 * condition variables. It ensures proper cleanup in case of errors during the
 * destruction process.
 *
 * @param[in,out] p_ctx A pointer to the threadpool context that needs to be destroyed.
 *                      Must not be NULL. The pointer itself is set to NULL after
 *                      successful destruction.
 *
 * @return Returns a status code indicating the outcome of the operation:
 *         - COIN_SUCCESS: The context was successfully destroyed.
 *         - COIN_PTHREAD_MUTEX_ERROR: Failed to destroy the mutex.
 *         - COIN_PTHREAD_COND_ERROR: Failed to destroy the condition variable.
 *         - Any status returned by `coin_queue_destroy`.
 *
 * @note It is the caller's responsibility to ensure that no threads are actively
 *       using the context when calling this function.
 *
 * @warning Improper usage of this function (e.g., passing an invalid pointer or
 *          using a context after destruction) leads to undefined behavior.
 */
static coin_status_t coin_tpool_ctx_destroy(tpool_ctx_t * p_ctx)
{
    assert(p_ctx);

    coin_status_t status    = COIN_GENERIC_FAILURE;
    cards_err_e   cards_err = CARDS_GENERIC_ERROR;
    int           err       = 0;

    err = pthread_cond_destroy(&(p_ctx->tpool_queue_cond));
    if (0 != err)
    {
        status = COIN_PTHREAD_COND_ERROR;
        goto end;
    }

    err = pthread_mutex_destroy(&(p_ctx->tpool_queue_mutex));
    if (0 != err)
    {
        status = COIN_PTHREAD_MUTEX_ERROR;
        goto end;
    }


    cards_err = cards_queue_deinit(p_ctx->p_work_queue);
    if (CARDS_SUCCESS != cards_err)
    {
        status = COIN_QUEUE_SYSTEM_ERR;
        goto end;
    }


    err = pthread_cond_destroy(&(p_ctx->tpool_queue_cond));
    if (0 != err)
    {
        status = COIN_PTHREAD_COND_ERROR;
        goto end;
    }

    err = pthread_mutex_destroy(&(p_ctx->tpool_queue_mutex));
    if (0 != err)
    {
        status = COIN_PTHREAD_MUTEX_ERROR;
        goto end;
    }

    free(p_ctx->p_work_queue);
    p_ctx->p_work_queue = NULL;

    free(p_ctx);
    p_ctx = NULL;

    status = COIN_SUCCESS;
    return status;

end:
    return status;
}

/**
 * @brief Initializes worker threads for a threadpool context.
 *
 * This function creates and initializes an array of worker threads that will
 * handle tasks within the specified threadpool context. Each thread is created
 * with a signal mask inherited from the calling thread. It ensures proper
 * allocation of resources and handles cleanup on failure.
 *
 * @param[in]  p_ctx A pointer to the threadpool context. Must not be NULL.
 * @param[out] pp_threads A double pointer to the array of worker threads. The
 *                        caller provides a pointer that will be updated with the
 *                        created worker thread instances. Must not be NULL.
 * @param[in]  num_workers The number of worker threads to create. Must be greater than zero.
 *
 * @return Returns a status code indicating the outcome of the operation:
 *         - COIN_SUCCESS: The threads were successfully created and initialized.
 *         - COIN_ALLOCATION_FAILURE: Memory allocation for the threads array failed.
 *         - COIN_PTHREAD_SIGMASK_ERROR: An error occurred while setting or restoring the signal mask.
 *         - COIN_PTHREAD_CREATE_ERROR: An error occurred while attempting to create threads.
 *
 * @note It is the caller's responsibility to properly handle and clean up the created
 *       threads, including joining and freeing the memory, if the function partially
 *       succeeds or fails.
 *
 * @warning This function modifies the signal mask for the calling thread during
 *          thread creation. Ensure it is compatible with the application’s signal
 *          handling requirements.
 */
static coin_status_t coin_initialize_workers(tpool_ctx_t * p_ctx, coin_tpool_thread_t ** pp_threads, size_t num_workers)
{
    assert(pp_threads);
    assert(p_ctx);
    assert(0 < num_workers);

    coin_status_t status      = COIN_GENERIC_FAILURE;
    size_t        num_created = 0;
    int           err         = 1;

    /* Setup signal mask for each thread created (inherited from calling thread) */
    pthread_attr_t attr   = {0};
    sigset_t       sigset = {0};
    sigset_t       oldset = {0};

    err = pthread_attr_init(&attr);
    if (0 != err)
    {
        status = COIN_PTHREAD_SIGMASK_ERROR;
        goto end;
    }

    err = sigfillset(&sigset);
    if (0 != err)
    {
        status = COIN_PTHREAD_SIGMASK_ERROR;
        goto cleanup_attr;
    }

    err = pthread_sigmask(SIG_BLOCK, &sigset, &oldset);
    if (0 != err)
    {
        status = COIN_PTHREAD_SIGMASK_ERROR;
        goto cleanup_attr;
    }

    coin_tpool_thread_t * p_threads = calloc(num_workers, sizeof(coin_tpool_thread_t));
    if (NULL == p_threads)
    {
        status = COIN_ALLOCATION_FAILURE;
        goto end;
    }

    for (size_t i = 0; i < num_workers; i++)
    {
        err = pthread_create(&(p_threads[i].tid), &attr, coin_thread_worker, p_ctx);
        if (0 != err)
        {
            status = COIN_PTHREAD_CREATE_ERROR;
            goto cleanup_threads;
        }

        p_threads[i].status   = COIN_THREAD_STARTED;
        p_threads[i].p_retval = NULL;

        num_created++;
    }

    /* Restore the signal mask */
    err = pthread_sigmask(SIG_SETMASK, &oldset, NULL);
    if (0 != err)
    {
        status = COIN_PTHREAD_SIGMASK_ERROR;
        goto cleanup_threads;
    }

    *pp_threads = p_threads;
    p_threads   = NULL;

    status = COIN_SUCCESS;
    return status;

cleanup_threads:
    for (size_t i = 0; i < num_created; i++)
    {
        (void)pthread_join(p_threads[i].tid, NULL);
    }

    free(p_threads);
    p_threads = NULL;

cleanup_attr:
    (void)pthread_attr_destroy(&attr);

end:
    return status;
}

/**
 * @brief Stops all worker threads in the specified threadpool.
 *
 * This function signals all worker threads in the threadpool to terminate and joins
 * them to ensure proper synchronization. It sets the shutdown flag, notifies all
 * worker threads via the condition variable, and waits for their termination. The
 * thread states are updated after joining.
 *
 * @param[in] p_tpool A pointer to the threadpool structure whose workers are to be
 *                    stopped. Must not be NULL and must contain a valid context
 *                    and worker thread array.
 *
 * @return Returns a status code indicating the outcome of the operation:
 *         - COIN_SUCCESS: The workers were successfully stopped and joined.
 *         - COIN_PTHREAD_MUTEX_ERROR: An error occurred while locking or unlocking
 *           the mutex.
 *         - COIN_PTHREAD_COND_ERROR: An error occurred while broadcasting the
 *           condition variable.
 *         - COIN_PTHREAD_JOIN_ERROR: A pthread join operation failed for one or
 *           more threads.
 */
static coin_status_t coin_tpool_stop_workers(const coin_threadpool_t * p_tpool)
{
    assert(p_tpool);
    assert(p_tpool->p_threads);
    assert(p_tpool->p_ctx);

    coin_status_t status = COIN_GENERIC_FAILURE;
    int           err    = 0;

    err = pthread_mutex_lock(&(p_tpool->p_ctx->tpool_queue_mutex));
    if (0 != err)
    {
        status = COIN_PTHREAD_MUTEX_ERROR;
        goto end;
    }

    p_tpool->p_ctx->b_shutdown = true;

    err = pthread_cond_broadcast(&(p_tpool->p_ctx->tpool_queue_cond));
    if (0 != err)
    {
        /* Already in an error state, not much I can do if this errors too. Just ignore return value */
        (void)pthread_mutex_unlock(&(p_tpool->p_ctx->tpool_queue_mutex));
        status = COIN_PTHREAD_COND_ERROR;
        goto end;
    }

    err = pthread_mutex_unlock(&(p_tpool->p_ctx->tpool_queue_mutex));
    if (0 != err)
    {
        status = COIN_PTHREAD_MUTEX_ERROR;
        goto end;
    }

    for (size_t i = 0; i < p_tpool->num_workers; i++)
    {
        err = pthread_join(p_tpool->p_threads[i].tid, &(p_tpool->p_threads[i].p_retval));
        if (0 != err)
        {
            status = COIN_PTHREAD_JOIN_ERROR;
        }

        else
        {
            p_tpool->p_threads[i].status = COIN_THREAD_JOINED;
        }
    }

    status = (status == COIN_PTHREAD_JOIN_ERROR ? COIN_PTHREAD_JOIN_ERROR : COIN_SUCCESS);

end:
    return status;
}

/* END OF FILE coin.c */
