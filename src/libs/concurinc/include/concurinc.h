/**
 * @file concurinc.h
 * @author nick
 * @date 5/6/25
 * @brief
 */
#ifndef CONCURINC_H
#define CONCURINC_H

#include <stddef.h>

typedef struct coin_threadpool coin_threadpool_t;
typedef struct coin_task       coin_task_t;

typedef void (*coin_task_func_t)(void *);

typedef void (*coin_task_free_arg_func_t)(void *);

typedef enum coin_status_t
{
    COIN_SUCCESS,
    COIN_GENERIC_FAILURE,
    COIN_INVALID_INPUT,
    COIN_ALLOCATION_FAILURE,
    COIN_THREAD_CREATION_FAILURE,
    COIN_PTHREAD_MUTEX_ERROR,
    COIN_PTHREAD_COND_ERROR,
    COIN_PTHREAD_JOIN_ERROR,
    COIN_PTHREAD_CREATE_ERROR,
    COIN_PTHREAD_SIGMASK_ERROR,
    COIN_EMPTY_QUEUE_ERROR,
} coin_status_t;

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
 *       no longer needed.
 *
 * @warning This function uses dynamically allocated memory. If the function fails,
 *          ensure proper cleanup of partially allocated resources (if applicable).
 */
coin_status_t coin_tpool_init(coin_threadpool_t ** pp_tpool, size_t num_workers);

/**
 * @brief Waits for all tasks in the threadpool to complete.
 *
 * This function blocks the calling thread until all active tasks in the threadpool
 * have finished executing and the work queue is empty. It ensures that all ongoing
 * work is completed before the function returns.
 *
 * @param[in] p_tpool A pointer to the threadpool structure. Must not be NULL.
 *
 * @return Returns a status code indicating the outcome of the operation:
 *         - COIN_SUCCESS: All tasks have completed successfully, and the threadpool
 *           is idle.
 *         - COIN_INVALID_INPUT: The provided threadpool pointer is NULL.
 *         - COIN_PTHREAD_MUTEX_ERROR: An error occurred while locking or
 *           unlocking the mutex.
 *         - COIN_PTHREAD_COND_ERROR: An error occurred while waiting on the condition
 *           variable.
 *
 * @note This function should be called only after the threadpool has been initialized.
 *       It ensures thread-safe behavior by utilizing a mutex and a condition variable
 *       to manage synchronization.
 *
 * @warning If an error occurs during the wait operation, it is the caller's
 *          responsibility to handle any necessary cleanup or recovery actions.
 */
coin_status_t coin_tpool_wait(const coin_threadpool_t * p_tpool);

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
coin_status_t coin_tpool_destroy(coin_threadpool_t * p_tpool, int retvals[]);

/**
 * @brief Submits a task to the specified threadpool for execution.
 *
 * This function creates a task object encapsulating the provided task function,
 * its arguments, and an optional cleanup function. The task is then pushed
 * onto the threadpool's work queue, which is subsequently processed by worker
 * threads.
 *
 * @param[in] p_tpool A pointer to the threadpool structure. Must not be NULL.
 * @param[in] task_func A pointer to the function that defines the task to be executed. Must not be NULL.
 * @param[in] p_task_arg A pointer to the arguments for the task function. Must not be NULL.
 * @param[in] free_arg_func A pointer to a function that frees the resources
 *                          associated with `p_task_arg`. Can be NULL if no cleanup is required.
 *
 * @return Returns a status code indicating the outcome of the operation:
 *         - COIN_SUCCESS: The task was successfully submitted.
 *         - COIN_INVALID_INPUT: One or more input parameters were invalid.
 *         - COIN_ALLOCATION_FAILURE: Memory allocation for the task object failed.
 *         - COIN_PTHREAD_MUTEX_ERROR: Mutex locking or unlocking encountered an error.
 *         - COIN_PTHREAD_COND_ERROR: Condition variable signaling encountered an error.
 *         - Any status returned by `coin_queue_push`.
 *
 * @note The caller is responsible for ensuring that the threadpool is properly
 *       initialized before invoking this function. If `p_task_arg` requires cleanup,
 *       ensure to provide a valid `free_arg_func` to handle resource deallocation.
 *
 * @warning If the function encounters an error, the task is not submitted to the
 *          threadpool. Any allocated resources for the task will be cleaned up,
 *          except for the provided `p_task_arg`, which remains the responsibility
 *          of the caller unless a `free_arg_func` is provided.
 */
coin_status_t coin_tpool_submit(const coin_threadpool_t * p_tpool, const coin_task_func_t task_func, void * task_arg,
                                const coin_task_free_arg_func_t task_free_arg_func);


#endif // CONCURINC_H
