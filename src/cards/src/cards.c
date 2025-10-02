/**
 * @file cards.c
 * @author nick
 * @date 9/1/25
 * @brief Implementation of a circular, compact, dynamic array for storage of pointers.
 */

#include "cards/cards.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Initializes a cards_base_s structure with the specified capacity and resize settings.
 *
 * This function sets up the provided cards_base_s structure, allocating the required
 * internal buffer to hold the specified number of elements. It also initializes all
 * internal properties such as capacity and current status. If allocation fails or if
 * the arguments are invalid, the structure remains uninitialized.
 *
 * @param p_cards_struct Pointer to the cards_base_s structure to initialize.
 *                       Must not be NULL.
 * @param initial_capacity The initial number of elements the structure can hold.
 *                         Must be greater than 0 and less than or equal to CARDS_MAX_MEMBERS.
 * @param b_enable_resize Boolean flag to indicate if resizing is enabled when the
 *                        capacity is exceeded.
 * @return CARDS_SUCCESS on successful initialization.
 *         CARDS_NULL_POINTER_ARG if p_cards_struct is NULL.
 *         CARDS_INVALID_SIZE_ARG if initial_capacity is 0 or exceeds CARDS_MAX_MEMBERS.
 *         CARDS_ALLOCATION_FAILURE if memory allocation for the internal buffer fails.
 *         CARDS_GENERIC_ERROR for any other error.
 */
cards_err_e cards_init(cards_base_s * p_cards_struct, const uint16_t initial_capacity, const bool b_enable_resize)
{
    cards_err_e res             = CARDS_GENERIC_ERROR;
    void **     pp_internal_buf = NULL;

    if ((NULL == p_cards_struct) || (0 == initial_capacity) || (CARDS_MAX_MEMBERS < initial_capacity))
    {
        res = (NULL == p_cards_struct) ? CARDS_NULL_POINTER_ARG : CARDS_INVALID_SIZE_ARG;
        goto end;
    }

    pp_internal_buf = calloc(initial_capacity, sizeof(void *));
    if (NULL == pp_internal_buf)
    {
        /* Safely set everything to zero just in case user attempts to use it anyway */
        p_cards_struct->status           = CARDS_UNINITIALIZED;
        p_cards_struct->pp_buf           = NULL;
        p_cards_struct->num_items        = 0;
        p_cards_struct->capacity         = 0;
        p_cards_struct->idx_back         = 0;
        p_cards_struct->idx_front        = 0;
        p_cards_struct->b_resize_enabled = b_enable_resize;
        res                              = CARDS_ALLOCATION_FAILURE;
        goto end;
    }

    p_cards_struct->pp_buf = pp_internal_buf;
    pp_internal_buf        = NULL;

    p_cards_struct->status           = CARDS_INITIALIZED;
    p_cards_struct->num_items        = 0;
    p_cards_struct->capacity         = initial_capacity;
    p_cards_struct->idx_back         = 0;
    p_cards_struct->idx_front        = 0;
    p_cards_struct->b_resize_enabled = b_enable_resize;

    res = CARDS_SUCCESS;

end:
    return res;
}

/**
 * Deinitializes a cards_base_s structure and releases its associated resources.
 *
 * This function deallocates memory used by the internal buffer of the provided structure
 * and resets its properties to their uninitialized state. It ensures that no memory
 * leaks remain and properly cleans up the internal state of the structure. If the input
 * is invalid or the structure is already uninitialized, the function handles the
 * condition and returns appropriate error codes.
 *
 * @param p_cards_struct Pointer to the cards_base_s structure to be deinitialized.
 *                       Must not be NULL and must be in an initialized state.
 * @return CARDS_SUCCESS if deinitialization is successful.
 *         CARDS_NULL_POINTER_ARG if p_cards_struct is NULL.
 *         CARDS_UNINITIALIZED_ERROR if the structure is already uninitialized.
 *         CARDS_GENERIC_ERROR for any other error.
 */
cards_err_e cards_deinit(cards_base_s * p_cards_struct)
{
    cards_err_e res = CARDS_GENERIC_ERROR;

    if (NULL == p_cards_struct)
    {
        res = CARDS_NULL_POINTER_ARG;
        goto end;
    }

    if (CARDS_UNINITIALIZED == p_cards_struct->status)
    {
        res = CARDS_UNINITIALIZED_ERROR;
        goto end;
    }

    free(p_cards_struct->pp_buf);
    p_cards_struct->pp_buf = NULL;

    p_cards_struct->status    = CARDS_UNINITIALIZED;
    p_cards_struct->num_items = 0;
    p_cards_struct->capacity  = 0;
    p_cards_struct->idx_back  = 0;
    p_cards_struct->idx_front = 0;

    res = CARDS_SUCCESS;

end:
    return res;
}

/**
 * Inserts an item at the specified index in a cards_base_s structure.
 *
 * This function inserts a given item at the logical index requested by the
 * user. It properly handles the capacity of the structure and resizes the
 * internal buffer if necessary and allowed. The insertion supports adding
 * an item to the front, middle, or back of the collection. Logical indices
 * are converted to physical indices in the internal circular buffer format.
 * If the insertion fails for any reason, the structure remains unchanged.
 *
 * @param p_cards_struct Pointer to the cards_base_s structure to insert the
 *                       item into. Must not be NULL and must be properly initialized.
 * @param idx The logical index at which to insert the item. Must be within
 *            the range [0, num_items] of the structure.
 * @param p_item Pointer to the item to be inserted. Can be NULL, as the function
 *               supports storing NULL values.
 * @return CARDS_SUCCESS if the item is successfully inserted.
 *         CARDS_NULL_POINTER_ARG if p_cards_struct is NULL.
 *         CARDS_UNINITIALIZED_ERROR if p_cards_struct is not initialized.
 *         CARDS_INVALID_INDEX_ARG if the provided index is out of bounds.
 *         CARDS_MAX_CAPACITY_ERROR if resizing is not allowed or the structure
 *                                  has reached its maximum capacity.
 *         CARDS_ALLOCATION_FAILURE if memory allocation for resizing fails.
 *         CARDS_GENERIC_ERROR for any other failure.
 */
cards_err_e cards_insert_at(cards_base_s * p_cards_struct, const uint16_t idx, void * p_item)
{
    cards_err_e res          = CARDS_GENERIC_ERROR;
    void **     pp_tmp_buf   = NULL;
    uint16_t    new_capacity = 0;
    uint16_t    old_idx      = 0;
    uint16_t    physical_idx = 0;
    uint16_t    cur_idx      = 0;
    uint16_t    orig_idx     = 0;
    uint32_t    size_check   = 0;

    printf("Inserting item. Current size: %d\n", p_cards_struct->num_items);

    /* Explicitly allowing p_item to be NULL - just storing pointer values, users should be able to use NULL */
    if (NULL == p_cards_struct)
    {
        res = CARDS_NULL_POINTER_ARG;
        goto end;
    }

    if (CARDS_UNINITIALIZED == p_cards_struct->status)
    {
        res = CARDS_UNINITIALIZED_ERROR;
        goto end;
    }

    /* Check if the given index is valid based on logical indexing */
    if (p_cards_struct->num_items < idx)
    {
        res = CARDS_INVALID_INDEX_ARG;
        goto end;
    }

    /* Determine if resizing would be needed, then check if it's allowed and resize */
    if (p_cards_struct->num_items == p_cards_struct->capacity)
    {
        if (false == p_cards_struct->b_resize_enabled)
        {
            printf("At the false initialization case.\n");
            res = CARDS_MAX_CAPACITY_ERROR;
            goto end;
        }

        /* If already full, error out of the routine */
        if (CARDS_MAX_MEMBERS == p_cards_struct->capacity)
        {
            printf("At the max capacity case.\n");
            res = CARDS_MAX_CAPACITY_ERROR;
            goto end;
        }

        /* If resizing the buffer would exceed the maximum allowed capacity, just set it to the max capacity. Otherwise,
         * use the compile-time defined resize factor. Using a uint32_t for the check to account for potential overflows
         * on resize.
         */
        size_check = p_cards_struct->capacity * CARDS_RESIZE_FACTOR;
        if (CARDS_MAX_MEMBERS < size_check)
        {
            new_capacity = CARDS_MAX_MEMBERS;
        }
        else
        {
            new_capacity = p_cards_struct->capacity * CARDS_RESIZE_FACTOR;
        }

        pp_tmp_buf = calloc(new_capacity, sizeof(void *));
        if (NULL == pp_tmp_buf)
        {
            res = CARDS_ALLOCATION_FAILURE;
            goto end;
        }

        /* Copy the contents of the old buffer to the new buffer */
        for (uint16_t i = 0; i < p_cards_struct->num_items; i++)
        {
            old_idx       = (p_cards_struct->idx_front + i) % p_cards_struct->capacity;
            pp_tmp_buf[i] = p_cards_struct->pp_buf[old_idx];
        }

        /* Update the structure with the new buffer and discard the old buffer */
        free(p_cards_struct->pp_buf);
        p_cards_struct->pp_buf = pp_tmp_buf;
        pp_tmp_buf             = NULL;

        p_cards_struct->capacity  = new_capacity;
        p_cards_struct->idx_front = 0;
        p_cards_struct->idx_back  = p_cards_struct->num_items - 1;
    }

    /* Immediately deal with an empty array to simplify things later */
    if (0 == p_cards_struct->num_items)
    {
        p_cards_struct->idx_front = 0;
        p_cards_struct->idx_back  = 0;
        p_cards_struct->pp_buf[0] = p_item;
        p_cards_struct->num_items++;
        res = CARDS_SUCCESS;
        goto end;
    }

    /* Dealing with "logical" indices from user input that are converted to the internal index */
    if (0 == idx)
    {
        /* Insert the item into the front of the array */
        p_cards_struct->idx_front =
          (0 == p_cards_struct->idx_front) ? (p_cards_struct->capacity - 1) : (p_cards_struct->idx_front - 1);
        p_cards_struct->pp_buf[p_cards_struct->idx_front] = p_item;
    }

    else if (p_cards_struct->num_items == idx)
    {
        /* Insert the item at the back of the array */
        p_cards_struct->idx_back                         = (p_cards_struct->idx_back + 1) % p_cards_struct->capacity;
        p_cards_struct->pp_buf[p_cards_struct->idx_back] = p_item;
    }

    else
    {
        /* Insert the item somewhere in the middle of the array. First, translate the given logical index to the
         * physical index into the array.
         *
         */
        physical_idx = (p_cards_struct->idx_front + idx) % p_cards_struct->capacity;

        /* Next, expand the array and shift all of the items to make room for the inserted item */
        p_cards_struct->idx_back = (p_cards_struct->idx_back + 1) % p_cards_struct->capacity;
        cur_idx                  = p_cards_struct->idx_back;
        while (cur_idx != physical_idx)
        {
            /* Find the index immediately preceding the current item, accounting for wraparounds */
            orig_idx = (0 == cur_idx) ? (p_cards_struct->capacity - 1) : (cur_idx - 1);

            /* Shift the item to the right */
            p_cards_struct->pp_buf[cur_idx] = p_cards_struct->pp_buf[orig_idx];

            /* Move to the next item */
            cur_idx = orig_idx;
        }

        /* Finally, insert the item at the given logical index and freed up slot */
        p_cards_struct->pp_buf[physical_idx] = p_item;
    }

    p_cards_struct->num_items++;
    res = CARDS_SUCCESS;

end:
    return res;
}

/**
 * Removes an element at the specified logical index from a cards_base_s structure.
 *
 * This function removes an element from the specified index within the circular buffer
 * of a cards_base_s structure. The removed element can optionally be retrieved via the
 * pp_out parameter. It adjusts the internal indices and shifts items as necessary to
 * maintain the logical order. The function handles special cases, such as removing an
 * element when the structure contains only one item, or when the element to be removed
 * is at the front or rear.
 *
 * @param p_cards_struct Pointer to the cards_base_s structure from which an element is to
 *                       be removed. Must not be NULL and must be properly initialized.
 * @param idx The logical index of the element to remove. Must be less than the number of
 *            elements currently stored in the structure.
 * @param pp_out Pointer to store the removed element if not NULL. Can be NULL if the caller
 *               does not need the removed element returned.
 * @return CARDS_SUCCESS if the operation is successful.
 *         CARDS_NULL_POINTER_ARG if p_cards_struct is NULL.
 *         CARDS_UNINITIALIZED_ERROR if p_cards_struct is not initialized.
 *         CARDS_INVALID_INDEX_ARG if the given index is out of bounds.
 *         CARDS_GENERIC_ERROR for any other error.
 */
cards_err_e cards_remove_at(cards_base_s * p_cards_struct, const uint16_t idx, void ** pp_out)
{
    cards_err_e res            = CARDS_GENERIC_ERROR;
    uint16_t    front          = 0;
    uint16_t    rear           = 0;
    uint16_t    physical_index = 0;
    uint16_t    cur_idx        = 0;

    /* Explicitly allowing NULL out values in case the user does not care to get the information back out */
    if (NULL == p_cards_struct)
    {
        res = CARDS_NULL_POINTER_ARG;
        goto end;
    }

    if (CARDS_UNINITIALIZED == p_cards_struct->status)
    {
        res = CARDS_UNINITIALIZED_ERROR;
        goto end;
    }

    if (p_cards_struct->num_items <= idx)
    {
        res = CARDS_INVALID_INDEX_ARG;
        goto end;
    }

    physical_index = (p_cards_struct->idx_front + idx) % p_cards_struct->capacity;
    front          = p_cards_struct->idx_front;
    rear           = p_cards_struct->idx_back;
    if (NULL != pp_out)
    {
        *pp_out = p_cards_struct->pp_buf[physical_index];
    }
    p_cards_struct->pp_buf[physical_index] =
      NULL; /* Shouldn't matter, but just for avoiding keeping a reference I don't own. */

    /* Adjust the internal array so that it just doesn't index the element that was popped. Move everything left,
     * accounting for wrap. Start with case 1, removing an item when there's only one item in the array. Just reset
     * everything to 0.
     */
    if (1 == p_cards_struct->num_items)
    {
        p_cards_struct->idx_front = 0;
        p_cards_struct->idx_back  = 0;
    }

    /* Case 2: Removing an item at the logical front of the array */
    else if (idx == 0)
    {
        p_cards_struct->idx_front = (front + 1) % p_cards_struct->capacity;
    }

    /* Case 3: Removing an item at the logical rear of the array */
    else if (idx == (p_cards_struct->num_items - 1))
    {
        p_cards_struct->idx_back =
          (rear - 1 + p_cards_struct->capacity) % p_cards_struct->capacity; // Keep non-negative to avoid underflow
    }

    /* Case 4: Remove from somewhere in the middle and recompact the array to close the gap */
    else
    {
        cur_idx = physical_index;
        while (cur_idx != rear)
        {
            physical_index                  = (cur_idx + 1) % p_cards_struct->capacity;
            p_cards_struct->pp_buf[cur_idx] = p_cards_struct->pp_buf[physical_index];
            cur_idx                         = physical_index;
        }

        p_cards_struct->idx_back =
          (rear - 1 + p_cards_struct->capacity) % p_cards_struct->capacity; // Keep non-negative to avoid underflow
    }

    p_cards_struct->num_items--;
    res = CARDS_SUCCESS;

end:
    return res;
}

/**
 * Replaces an item at a specific index in a cards_base_s structure.
 *
 * This function replaces the item at the specified index in the circular buffer
 * of the cards_base_s structure with the provided item. The index is relative
 * to the logical order of elements, not the physical storage order. Note, the
 * replaced element is overwritten - if there is memory associated with the pointer,
 * it will be lost. Users should take care to clear the memory as required.
 *
 * @param p_cards_struct Pointer to the cards_base_s structure. Must not be NULL and
 *                       must be initialized.
 * @param idx The logical index of the element to replace. Must be less than the
 *            current number of items in the structure.
 * @param p_item Pointer to the new item to be placed at the specified index.
 * @return CARDS_SUCCESS on successful replacement.
 *         CARDS_NULL_POINTER_ARG if p_cards_struct is NULL.
 *         CARDS_UNINITIALIZED_ERROR if p_cards_struct is uninitialized.
 *         CARDS_INVALID_INDEX_ARG if the specified index is out of range.
 *         CARDS_GENERIC_ERROR for any other error.
 */
cards_err_e cards_replace_at(const cards_base_s * p_cards_struct, const uint16_t idx, void * p_item)
{
    cards_err_e res            = CARDS_GENERIC_ERROR;
    uint16_t    physical_index = 0;

    if (NULL == p_cards_struct)
    {
        res = CARDS_NULL_POINTER_ARG;
        goto end;
    }


    if (CARDS_UNINITIALIZED == p_cards_struct->status)
    {
        res = CARDS_UNINITIALIZED_ERROR;
        goto end;
    }

    if (p_cards_struct->num_items <= idx)
    {
        res = CARDS_INVALID_INDEX_ARG;
        goto end;
    }

    physical_index                         = (p_cards_struct->idx_front + idx) % p_cards_struct->capacity;
    p_cards_struct->pp_buf[physical_index] = p_item;
    res                                    = CARDS_SUCCESS;

end:
    return res;
}

/**
 * Retrieves the item at a specified index from the cards_base_s structure without removing it.
 *
 * This function allows access to elements stored within the cards_base_s structure
 * at the given logical index. The resultant element is returned via a double pointer.
 * If the provided arguments are invalid, the function returns an appropriate error code.
 *
 * @param p_cards_struct Pointer to the cards_base_s structure containing the items.
 *                       Must not be NULL and must be initialized.
 * @param idx Logical index of the desired item within the structure. Must be within the range
 *            [0, num_items - 1], where num_items is the number of elements in the structure.
 * @param pp_out Double pointer to the location where the resulting item will be stored.
 *               Must not be NULL.
 * @return CARDS_SUCCESS on successful retrieval of the item.
 *         CARDS_NULL_POINTER_ARG if p_cards_struct or pp_out is NULL.
 *         CARDS_UNINITIALIZED_ERROR if p_cards_struct is not initialized.
 *         CARDS_INVALID_INDEX_ARG if idx is out of bounds.
 *         CARDS_GENERIC_ERROR for any other error.
 */
cards_err_e cards_peek_at(const cards_base_s * p_cards_struct, const uint16_t idx, void ** pp_out)
{
    cards_err_e err            = CARDS_GENERIC_ERROR;
    uint16_t    physical_index = 0;

    if ((NULL == pp_out) || (NULL == p_cards_struct))
    {
        err = CARDS_NULL_POINTER_ARG;
        goto end;
    }

    if (CARDS_UNINITIALIZED == p_cards_struct->status)
    {
        err = CARDS_UNINITIALIZED_ERROR;
        goto end;
    }

    if (p_cards_struct->num_items <= idx)
    {
        err = CARDS_INVALID_INDEX_ARG;
        goto end;
    }

    physical_index = (p_cards_struct->idx_front + idx) % p_cards_struct->capacity;
    *pp_out        = p_cards_struct->pp_buf[physical_index];
    err            = CARDS_SUCCESS;

end:
    return err;
}

/**
 * Iterates through each element in the cards_base_s structure and applies the provided callback function.
 *
 * This function traverses all the items currently stored in the cards_base_s structure in the
 * order they were added (front to back) and passes each item, along with a user-provided argument,
 * to a specified callback function. It ensures that the cards_base_s structure is initialized
 * before processing and validates the provided arguments.
 *
 * @param p_cards_struct Pointer to the initialized cards_base_s structure to iterate over.
 *                       Must not be NULL and must have a valid initialization state.
 * @param callback Function pointer to the callback to be invoked for each item.
 *                 Takes two parameters: the item pointer and a user-provided argument pointer.
 *                 Must not be NULL.
 * @param p_arg Pointer to user-specific data that will be passed to the callback. Can be NULL.
 * @return CARDS_SUCCESS if the iteration completes successfully and all callbacks are invoked.
 *         CARDS_NULL_POINTER_ARG if p_cards_struct or callback is NULL.
 *         CARDS_UNINITIALIZED_ERROR if the cards_base_s structure is uninitialized.
 *         CARDS_GENERIC_ERROR for any other error.
 */
cards_err_e cards_foreach(const cards_base_s * p_cards_struct, void (*callback)(void * p_item, void * p_arg),
                          void *               p_arg)
{
    cards_err_e res            = CARDS_GENERIC_ERROR;
    uint16_t    physical_index = 0;

    if ((NULL == callback) || (NULL == p_cards_struct))
    {
        /* Explicitly allowing arguments to be NULL - I'm not going to say what the caller can and can't do with it */
        res = CARDS_NULL_POINTER_ARG;
        goto end;
    }

    if (CARDS_UNINITIALIZED == p_cards_struct->status)
    {
        res = CARDS_UNINITIALIZED_ERROR;
        goto end;
    }

    for (uint16_t i = 0; i < p_cards_struct->num_items; i++)
    {
        physical_index = (p_cards_struct->idx_front + i) % p_cards_struct->capacity;
        callback(p_cards_struct->pp_buf[physical_index], p_arg);
    }

    res = CARDS_SUCCESS;

end:
    return res;
}

/**
 * Removes all instances of the sentinel value from the provided cards_base_s structure
 * and compacts the remaining items towards the front of the internal buffer.
 *
 * This function iterates over the structure's elements, discarding items that match
 * the sentinel value and shifting the remaining items to fill gaps created by removed
 * elements. It also resets unused positions in the buffer to NULL and updates internal
 * indices and counts as needed.
 *
 * @param p_cards_struct Pointer to the cards_base_s structure to compact.
 *                       Must not be NULL and must point to an initialized structure.
 * @param p_sentinel_value Pointer to the sentinel value to identify items to remove.
 *                         Items equal to the sentinel value are removed from the structure.
 *                         Can be NULL to remove NULL items.
 * @return CARDS_SUCCESS on successful compaction.
 *         CARDS_NULL_POINTER_ARG if p_cards_struct is NULL.
 *         CARDS_UNINITIALIZED_ERROR if the structure's status is uninitialized.
 *         CARDS_GENERIC_ERROR for any other error.
 */
cards_err_e cards_compact(cards_base_s * p_cards_struct, const void * p_sentinel_value)
{
    cards_err_e res          = CARDS_GENERIC_ERROR;
    uint16_t    write_idx    = 0;
    uint16_t    physical_idx = 0;
    void *      p_tmp        = NULL;

    if (NULL == p_cards_struct)
    {
        res = CARDS_NULL_POINTER_ARG;
        goto end;
    }

    if (CARDS_UNINITIALIZED == p_cards_struct->status)
    {
        res = CARDS_UNINITIALIZED_ERROR;
        goto end;
    }

    /* If already nothing, then there's nothing to do */
    if (0 == p_cards_struct->num_items)
    {
        res = CARDS_SUCCESS;
        goto end;
    }

    for (uint16_t i = 0; i < p_cards_struct->num_items; i++)
    {
        physical_idx = (p_cards_struct->idx_front + i) % p_cards_struct->capacity;
        p_tmp        = p_cards_struct->pp_buf[physical_idx];

        if (p_tmp != p_sentinel_value)
        {
            /* If the item is not the sentinel value, then it's a valid item. Move it to the write index and increment
             * write index. */
            p_cards_struct->pp_buf[write_idx++] = p_tmp;
        }
    }

    /* NULL out the rest of the array just for safety */
    for (uint16_t i = write_idx; i < p_cards_struct->capacity; i++)
    {
        p_cards_struct->pp_buf[i] = NULL;
    }

    /* Update the internal structure indices and values */
    p_cards_struct->num_items = write_idx;
    p_cards_struct->idx_front = 0;
    p_cards_struct->idx_back  = (0 == p_cards_struct->num_items) ? 0 : (p_cards_struct->num_items - 1);
    res                       = CARDS_SUCCESS;

end:
    return res;
}

/**
 * Clears all items from a cards_base_s structure and optionally frees allocated memory using a user-defined function.
 *
 * This function removes all items stored in the provided cards_base_s structure by resetting its indices
 * and counters. If a custom deallocation function (free_func) is provided, it is called on each non-NULL item
 * in the structure before the reset. After execution, the structure is empty and ready for reuse.
 *
 * @param p_cards_struct Pointer to the cards_base_s structure to be cleared. Must not be NULL and must be initialized.
 * @param free_func Pointer to a function for deallocating individual items stored in the structure.
 *                  If NULL, no deallocation will be performed.
 * @return CARDS_SUCCESS on successful cleanup and reset.
 *         CARDS_NULL_POINTER_ARG if p_cards_struct is NULL.
 *         CARDS_UNINITIALIZED_ERROR if p_cards_struct is not in an initialized state.
 *         CARDS_GENERIC_ERROR for any other error.
 */
cards_err_e cards_clear(cards_base_s * p_cards_struct, void (*free_func)(void *))
{
    cards_err_e res            = CARDS_GENERIC_ERROR;
    uint16_t    physical_index = 0;
    if (NULL == p_cards_struct)
    {
        res = CARDS_NULL_POINTER_ARG;
        goto end;
    }

    if (CARDS_UNINITIALIZED == p_cards_struct->status)
    {
        res = CARDS_UNINITIALIZED_ERROR;
        goto end;
    }

    if (NULL != free_func)
    {
        for (uint16_t i = 0; i < p_cards_struct->num_items; i++)
        {
            physical_index = (p_cards_struct->idx_front + i) % p_cards_struct->capacity;

            /* Freeing NULL is okay, but I don't control the free function so just safeguard against it */
            if (NULL != p_cards_struct->pp_buf[physical_index])
            {
                free_func(p_cards_struct->pp_buf[physical_index]);
            }
        }
    }

    /* Just reset the indices. Existing pointers will be overwritten on future inserts */
    p_cards_struct->idx_front = 0;
    p_cards_struct->idx_back  = 0;
    p_cards_struct->num_items = 0;
    res                       = CARDS_SUCCESS;

end:
    return res;
}

/**
 * Reverses the order of elements in a cards_base_s structure.
 *
 * This function inverts the positions of elements in the given cards_base_s structure.
 * It assumes the elements are stored in a circular buffer and adjusts the indices accordingly.
 * If the structure contains no elements or only a single element, no changes are made.
 * The function performs necessary error checks such as null pointer validation and ensures
 * the structure is initialized before proceeding.
 *
 * @param p_cards_struct Pointer to the cards_base_s structure to reverse.
 *                       Must not be NULL and must be initialized.
 * @return CARDS_SUCCESS if the reversal is successful.
 *         CARDS_NULL_POINTER_ARG if p_cards_struct is NULL.
 *         CARDS_UNINITIALIZED_ERROR if the structure is not initialized.
 *         CARDS_GENERIC_ERROR for any other unexpected error.
 */
cards_err_e cards_reverse(const cards_base_s * p_cards_struct)
{
    cards_err_e res = CARDS_GENERIC_ERROR;

    uint16_t left      = 0;
    uint16_t right     = 0;
    uint16_t left_idx  = 0;
    uint16_t right_idx = 0;
    void *   p_tmp     = NULL;

    if (NULL == p_cards_struct)
    {
        res = CARDS_NULL_POINTER_ARG;
        goto end;
    }

    if (CARDS_UNINITIALIZED == p_cards_struct->status)
    {
        res = CARDS_UNINITIALIZED_ERROR;
        goto end;
    }

    if (1 == p_cards_struct->num_items)
    {
        /* Nothing to reverse */
        res = CARDS_SUCCESS;
        goto end;
    }

    left  = 0;
    right = p_cards_struct->num_items - 1;

    /* Additionally covers the case of an empty array */
    while (left < right)
    {
        left_idx  = (p_cards_struct->idx_front + left) % p_cards_struct->capacity;
        right_idx = (p_cards_struct->idx_front + right) % p_cards_struct->capacity;

        p_tmp                             = p_cards_struct->pp_buf[left_idx];
        p_cards_struct->pp_buf[left_idx]  = p_cards_struct->pp_buf[right_idx];
        p_cards_struct->pp_buf[right_idx] = p_tmp;

        left++;
        right--;
    }

    res = CARDS_SUCCESS;

end:
    return res;
}

/* END OF FILE cards.c */
