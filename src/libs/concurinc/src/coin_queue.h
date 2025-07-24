/**
 * @file coin_queue.h
 * @author nick
 * @date 5/6/25
 * @brief
 */
#ifndef COIN_QUEUE_H
#define COIN_QUEUE_H

#include "concurinc.h"

/**
 * Represents a node in a thread-safe task queue. Each node encapsulates
 * a task to be processed and a pointer to the next node in the queue.
 *
 * Members:
 * - p_task: A pointer to the task associated with this node. This task
 *           is processed as part of the queue management.
 * - p_next: A pointer to the next node in the queue. This linkage is
 *           used to maintain the structure of the linked list.
 *
 * This structure serves as the building block for the task queue, enabling
 * linked list-based implementation for efficient task management in
 * concurrent systems.
 */
typedef struct coin_queue_node_t
{
    coin_task_t *              p_task;
    struct coin_queue_node_t * p_next;
} coin_queue_node_t;

/**
 * Represents a queue structure designed for handling tasks in a thread-safe
 * manner. The queue is implemented as a linked list, where each node encapsulates
 * a task to be processed.
 *
 * Members:
 * - p_head: Pointer to the first node in the queue. This represents the front of the queue.
 * - p_tail: Pointer to the last node in the queue. This represents the end of the queue.
 * - size: Maintains the current number of elements (tasks) in the queue.
 *
 * This structure is used in conjunction with various queue manipulation
 * functions to support task scheduling and execution in concurrent systems.
 */
typedef struct coin_queue_t
{
    coin_queue_node_t * p_head;
    coin_queue_node_t * p_tail;
    size_t              size;
} coin_queue_t;

/**
 * Initializes a coin queue by dynamically allocating memory for it and setting
 * up its initial state.
 *
 * @param pp_queue A pointer to a pointer of type coin_queue_t. This pointer
 *                 will be set to the address of the newly allocated and initialized
 *                 coin queue. Must not be NULL.
 * @return A coin_status_t value indicating the result of the initialization:
 *         - COIN_SUCCESS: Queue was successfully initialized.
 *         - COIN_INVALID_INPUT: Input parameter pp_queue is NULL.
 *         - COIN_ALLOCATION_FAILURE: Memory allocation failed.
 *         - COIN_GENERIC_FAILURE: An unexpected error occurred.
 */
coin_status_t coin_queue_init(coin_queue_t ** pp_queue);

/**
 * Destroys a coin queue by deallocating all of its nodes and associated tasks,
 * as well as the queue structure itself.
 *
 * @param p_queue A pointer to a coin queue (coin_queue_t) to be destroyed.
 *                This pointer must not be NULL. If the pointer is NULL,
 *                the function will return an appropriate error status.
 * @return A coin_status_t value indicating the result of the operation:
 *         - COIN_SUCCESS: Queue and its nodes were successfully destroyed.
 *         - COIN_INVALID_INPUT: Input parameter p_queue is NULL.
 *         - COIN_GENERIC_FAILURE: An unexpected error occurred.
 */
coin_status_t coin_queue_destroy(coin_queue_t * p_queue);

/**
 * Adds a new task to the coin queue. Allocates memory for a new node to
 * encapsulate the task and appends it to the end of the queue. Updates the
 * queue's internal pointers and size accordingly.
 *
 * @param p_queue A pointer to the coin queue where the task will be added. Must not be NULL.
 * @param p_task A pointer to the task to be pushed into the queue. Must not be NULL.
 * @return A coin_status_t value indicating the outcome of the operation:
 *         - COIN_SUCCESS: Task was successfully added to the queue.
 *         - COIN_INVALID_INPUT: Either p_queue or p_task is NULL.
 *         - COIN_ALLOCATION_FAILURE: Memory allocation for the new queue node failed.
 *         - COIN_GENERIC_FAILURE: An unexpected error occurred.
 */
coin_status_t coin_queue_push(coin_queue_t * p_queue, coin_task_t * p_task);

/**
 * Removes and retrieves the first task from the coin queue.
 *
 * @param p_queue A pointer to the coin_queue_t from which the task should be
 *                removed. Must not be NULL.
 * @param pp_task A pointer to a pointer of type coin_task_t. This will be set
 *                to the task removed from the queue. If the queue is empty
 *                or an error occurs, it will be set to NULL. Must not be NULL.
 * @return A coin_status_t value indicating the result of the operation:
 *         - COIN_SUCCESS: Task successfully removed from the queue.
 *         - COIN_INVALID_INPUT: One or more input parameters are NULL.
 *         - COIN_EMPTY_QUEUE_ERROR: The queue is empty.
 *         - COIN_GENERIC_FAILURE: An unexpected error occurred.
 */
coin_status_t coin_queue_pop(coin_queue_t * p_queue, coin_task_t ** pp_task);

#endif // COIN_QUEUE_H
