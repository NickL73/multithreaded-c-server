/**
 * @file cards.h
 * @author nick
 * @date 9/1/25
 * @brief
 */
#ifndef CARDS_CARDS_H
#define CARDS_CARDS_H

#include <stdbool.h>
#include <stdint.h>

#ifndef CARDS_MAX_MEMBERS
#define CARDS_MAX_MEMBERS 64
#endif

#if CARDS_MAX_MEMBERS > UINT16_MAX
#error "CARD_MAX_MEMBERS cannot exceed UINT16_MAX"
#endif

#ifndef CARDS_RESIZE_FACTOR
#define CARDS_RESIZE_FACTOR 2
#endif

#if CARDS_RESIZE_FACTOR <= 1
#error "CARDS_RESIZE_FACTOR must be greater than 1"
#endif


typedef enum
{
    CARDS_SUCCESS             = 0,
    CARDS_GENERIC_ERROR       = 1,
    CARDS_NOT_IMPLEMENTED     = 2,
    CARDS_UNINITIALIZED_ERROR = 3,
    CARDS_NULL_POINTER_ARG    = 4,
    CARDS_INVALID_SIZE_ARG    = 5,
    CARDS_INVALID_INDEX_ARG   = 6,
    CARDS_ALLOCATION_FAILURE  = 7,
    CARDS_MAX_CAPACITY_ERROR  = 8,
} cards_err_e;

typedef enum
{
    CARDS_UNINITIALIZED = 0,
    CARDS_INITIALIZED   = 1,
} cards_initialization_status_e;

typedef struct
{
    cards_initialization_status_e status;
    void **                       pp_buf;
    uint16_t                      num_items;
    uint16_t                      capacity;
    uint16_t                      idx_front;
    uint16_t                      idx_back;
    bool                          b_resize_enabled;
} cards_base_s;

/* STANDARD API */

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
cards_err_e cards_init(cards_base_s * p_cards_struct, const uint16_t initial_capacity, const bool b_enable_resize);

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
cards_err_e cards_deinit(cards_base_s * p_cards_struct);

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
cards_err_e cards_insert_at(cards_base_s * p_cards_struct, uint16_t idx, void * p_item);

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
cards_err_e cards_remove_at(cards_base_s * p_cards_struct, const uint16_t idx, void ** pp_out);

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
cards_err_e cards_replace_at(const cards_base_s * p_cards_struct, const uint16_t idx, void * p_item);

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
cards_err_e cards_peek_at(const cards_base_s * p_cards_struct, const uint16_t idx, void ** pp_out);

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
                          void *               p_arg);

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
cards_err_e cards_compact(cards_base_s * p_cards_struct, const void * p_sentinel_value);

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
cards_err_e cards_clear(cards_base_s * p_cards_struct, void (*free_func)(void *));

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
cards_err_e cards_reverse(const cards_base_s * p_cards_struct);

/* CONVENIENCE ALIASING */

static inline cards_err_e cards_push_front(cards_base_s * p_cards_struct, void * p_item)
{
    return cards_insert_at(p_cards_struct, 0, p_item);
}

static inline cards_err_e cards_push_rear(cards_base_s * p_cards_struct, void * p_item)
{
    return cards_insert_at(p_cards_struct, p_cards_struct->num_items, p_item);
}

static inline cards_err_e cards_pop_front(cards_base_s * p_cards_struct, void ** pp_out)
{
    return cards_remove_at(p_cards_struct, 0, pp_out);
}

static inline cards_err_e cards_pop_rear(cards_base_s * p_cards_struct, void ** pp_out)
{
    return cards_remove_at(p_cards_struct, (p_cards_struct->num_items - 1), pp_out);
}

static inline cards_err_e cards_peek_front(const cards_base_s * p_cards_struct, void ** pp_out)
{
    return cards_peek_at(p_cards_struct, 0, pp_out);
}

static inline cards_err_e cards_peek_rear(const cards_base_s * p_cards_struct, void ** pp_out)
{
    return cards_peek_at(p_cards_struct, (p_cards_struct->num_items - 1), pp_out);
}

#endif // CARDS_CARDS_H
