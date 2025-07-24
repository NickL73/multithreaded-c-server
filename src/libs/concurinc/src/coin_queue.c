/**
 * @file coin_queue.c
 * @author nick
 * @date 5/6/25
 * @brief
 */
#include <assert.h>
#include <stdlib.h>

#include "coin_queue.h"

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
coin_status_t coin_queue_init(coin_queue_t ** pp_queue)
{
    coin_status_t  status  = COIN_GENERIC_FAILURE;
    coin_queue_t * p_queue = NULL;

    if (NULL == pp_queue)
    {
        status = COIN_INVALID_INPUT;
        goto end;
    }

    p_queue = calloc(1, sizeof(coin_queue_t));
    if (NULL == p_queue)
    {
        status = COIN_ALLOCATION_FAILURE;
        goto end;
    }

    p_queue->p_head = NULL;
    p_queue->p_tail = NULL;
    p_queue->size   = 0;

    *pp_queue = p_queue;
    status    = COIN_SUCCESS;
end:
    return status;
}

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
coin_status_t coin_queue_destroy(coin_queue_t * p_queue)
{
    coin_status_t status = COIN_GENERIC_FAILURE;

    coin_queue_node_t * p_tmp = NULL;
    coin_queue_node_t * p_cur = NULL;
    if (NULL != p_queue)
    {
        p_cur = p_queue->p_head;
        while (NULL != p_cur)
        {
            p_tmp = p_cur->p_next;

            free(p_cur->p_task);
            free(p_cur);
            p_cur = p_tmp;
        }

        free(p_queue);
        status = COIN_SUCCESS;
    }

    else
    {
        status = COIN_INVALID_INPUT;
    }

    return status;
}

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
coin_status_t coin_queue_push(coin_queue_t * p_queue, coin_task_t * p_task)
{
    coin_status_t       status = COIN_GENERIC_FAILURE;
    coin_queue_node_t * p_node = NULL;

    if ((NULL == p_queue) || (NULL == p_task))
    {
        status = COIN_INVALID_INPUT;
        goto end;
    }

    p_node = calloc(1, sizeof(coin_queue_node_t));
    if (NULL == p_node)
    {
        status = COIN_ALLOCATION_FAILURE;
        goto end;
    }

    p_node->p_task = p_task;

    /* Nothing in the queue */
    if (NULL == p_queue->p_head)
    {
        p_queue->p_head = p_node;
        p_queue->p_tail = p_node;
        p_queue->size++;
    }

    else
    {
        /* One item in the queue */
        if (p_queue->p_head == p_queue->p_tail)
        {
            p_queue->p_head->p_next = p_node;
            p_queue->p_tail         = p_node;
        }

        else
        {
            p_queue->p_tail->p_next = p_node;
            p_queue->p_tail         = p_node;
        }

        p_queue->size++;
    }

    status = COIN_SUCCESS;

end:
    return status;
}

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
coin_status_t coin_queue_pop(coin_queue_t * p_queue, coin_task_t ** pp_task)
{
    coin_status_t       status = COIN_GENERIC_FAILURE;
    coin_queue_node_t * p_node = NULL;

    if ((NULL == p_queue) || (NULL == pp_task))
    {
        if (NULL != pp_task)
        {
            *pp_task = NULL;
        }
        status = COIN_INVALID_INPUT;
        goto end;
    }

    if (NULL == p_queue->p_head)
    {
        status   = COIN_EMPTY_QUEUE_ERROR;
        *pp_task = NULL;
        goto end;
    }

    p_node = p_queue->p_head;

    /* One item was in the queue */
    if (p_queue->p_head == p_queue->p_tail)
    {
        p_queue->p_head = NULL;
        p_queue->p_tail = NULL;
    }

    else
    {
        p_queue->p_head = p_node->p_next;
    }

    *pp_task = p_node->p_task;
    free(p_node);
    p_node = NULL;
    p_queue->size--;

    status = COIN_SUCCESS;

end:
    return status;
}

/* END OF FILE coin_queue.c */
