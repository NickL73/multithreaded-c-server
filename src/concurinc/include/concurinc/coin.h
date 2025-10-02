/**
 * @file concurinc.h
 * @author nick
 * @date 5/6/25
 * @brief
 */
#ifndef COIN_CONCURINC_H
#define COIN_CONCURINC_H

#include <stddef.h>

typedef struct coin_threadpool coin_threadpool_t;
typedef struct coin_task       coin_task_t;

typedef void (*coin_task_func_t)(void *);

typedef void (*coin_task_free_arg_func_t)(void *);

typedef enum coin_status_t
{
    COIN_SUCCESS                 = 0,
    COIN_GENERIC_FAILURE         = 1,
    COIN_INVALID_INPUT           = 2,
    COIN_ALLOCATION_FAILURE      = 3,
    COIN_THREAD_CREATION_FAILURE = 4,
    COIN_PTHREAD_MUTEX_ERROR     = 5,
    COIN_PTHREAD_COND_ERROR      = 6,
    COIN_PTHREAD_JOIN_ERROR      = 7,
    COIN_PTHREAD_CREATE_ERROR    = 8,
    COIN_PTHREAD_SIGMASK_ERROR   = 9,
    COIN_QUEUE_SYSTEM_ERR        = 10,
    COIN_CAPACITY_ERROR          = 11
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
 */
coin_status_t coin_tpool_submit(const coin_threadpool_t * p_tpool, const coin_task_func_t task_func, void * p_task_arg,
                                const coin_task_free_arg_func_t free_arg_func);


#endif // COIN_CONCURINC_H
