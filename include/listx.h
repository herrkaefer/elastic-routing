/*  =========================================================================
    listx - a generic list container

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================

listx is a generic list container.

- Item can be accessed

    - by handle: item handle is a pointer which is always bound with the item
      after it is attached to the list, and stays unchanged during sort,
      reorder, reverse, shuffle, etc, unless the item is explicitly altered.
      So actually, the handle could be served as item's ID in the list. Note
      that getting handle from item is not possible.

    - by index: item can be get and set like in ordinary array via index. Note
      that using index is a litte slower than using handle.

    - by iterator (see below)

- List iteration

    - Use listx_iter_init () or listx_iter_init_from () to initialize an
      iterator, and use listx_iter () to get item. e.g.

    ```c
    TYPE *item;
    // Start iteration from the beginning (forward) or end (backward)
    listx_iterator_t iter = listx_iter_init (list, true);
    // or start iteration from specified item handle (start from the NEXT one)
    // listx_iterator_t iter = listx_iter_init_from (list, handle, true);
    while ((item = (TYPE *) listx_iter (list, &iter)) != NULL) {
        // Do stuff with item
        //
        // If you need item handle, it is just iter.handle
        //
        // You can remove current item from list by
        // listx_iter_remove (list);

        // or pop current item from list without destroying it by
        // listx_iter_pop (list);
    }
    ```
*/


#ifndef __LISTX_H_INCLUDED__
#define __LISTX_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *handle; // handle of current item
    bool forward; // iteration direction. true: forward, false: backward
} listx_iterator_t;

// mapping function: mapping (obj1) -> obj2
typedef void *(*mapping_t) (const void *obj);

// reducer function: reducer (accumulator, obj) -> accumulator
// Note: NULL input should be properly handled
typedef void *(*reducer_t) (const void *accumulator, const void *obj);

// filter function: filter (obj) -> true / false
typedef bool (*filter_t) (const void *obj);

// Create a new empty list
listx_t *listx_new (void);

// Destroy a list
void listx_free (listx_t **self_p);

// Set item destructor callback.
// By default, item is not destroyed when it is removed from the list.
void listx_set_destructor (listx_t *self, destructor_t destructor);

// Set item duplicator callback
// If this is set, the list contains a duplication of item.
void listx_set_duplicator (listx_t *self, duplicator_t duplicator);

// Set item comparator callback
void listx_set_comparator (listx_t *self, comparator_t comparator);

// Set item printer callback.
// If this is set, listx_print () will use it to print items.
void listx_set_printer (listx_t *self, printer_t printer);

// Get number of items in list
size_t listx_size (const listx_t *self);

// Check if list is sorted
bool listx_is_sorted (const listx_t *self);

// Check if list is sorted ascendingly
bool listx_is_sorted_ascending (const listx_t *self);

// Check if list is sorted descendingly
bool listx_is_sorted_descending (const listx_t *self);

// Get item by handle
void *listx_item (const listx_t *self, const void *handle);

// Get item at index.
// Note that for list traversing, using list_iterator_t is faster.
void *listx_item_at (const listx_t *self, size_t index);

// Return the first item of the list
// Return NULL if list is empty
void *listx_first (const listx_t *self);

// Return the last item of the list
// Return NULL if list is empty
void *listx_last (const listx_t *self);

// Set item by handle.
// Note that handle-item binding will be updated. Use this carefully if you use
// handle as ID.
void listx_set_item (listx_t *self, void *handle, void *item);

// Set item at index
// Note that handle-item binding will be updated. Use this carefully if you use
// handle as ID.
void listx_set_item_at (listx_t *self, size_t index, void *item);

// Add item to the start.
// Return item handle.
void *listx_prepend (listx_t *self, void *item);

// Add item to the end.
// Return item handle.
void *listx_append (listx_t *self, void *item);

// Remove and return the item by handle
void *listx_pop (listx_t *self, void *handle);

// Remove and return the item at the start.
// Return NULL if the list is empty.
void *listx_pop_first (listx_t *self);

// Remove and return item at the end.
// Return NULL if the list is empty.
void *listx_pop_last (listx_t *self);

// Remove and return the item at index.
// Return NULL if the list is empty.
void *listx_pop_at (listx_t *self, size_t index);

// Extend list by appending items from another list
void listx_extend (listx_t *self, const listx_t *list);

// Extend list by appending items from an array
void listx_extend_array (listx_t *self, void **array, size_t length);

// Insert value at index (before the original item at index).
// Return item handle.
void *listx_insert_at (listx_t *self, size_t index, void *item);

// Insert value to a sorted list.
// Return item handle
// If the list is not sorted, it will be sorted in ascending order.
void *listx_insert_sorted (listx_t *self, void *item);

// Remove item by handle
void listx_remove (listx_t *self, void *handle);

// Remove all list items which equal to item
void listx_remove_item (listx_t *self, void *item);

// Remove first item
void listx_remove_first (listx_t *self);

// Remove last item
void listx_remove_last (listx_t *self);

// Remove item at index
void listx_remove_at (listx_t *self, size_t index);

// Remove items at interval [from_index, to_index]
void listx_remove_slice (listx_t *self, size_t from_index, size_t to_index);

// Remove all items in list.
// After calling this, the list will be empty, however the sorting state will
// be remained.
void listx_purge (listx_t *self);

// Sort list using provided comparator in specified order
void listx_sort (listx_t *self, bool ascending);

// Reorder an item to the right position in list according to the sorting order.
// The item is specified by handle, in list or not.
// It is assumed that the list is sorted except for the only item which is just
// altered and needs to be replaced to the right position.
void listx_reorder (listx_t *self, void *handle);

// Reverse list in place
void listx_reverse (listx_t *self);

// Shuffle list in place
// Set rng to use your random number generator, or NULL to use an default one.
void listx_shuffle (listx_t *self, rng_t *rng);

// Find item in list.
// Return the index if item is found, SIZE_NONE if not found.
// The index could be arbitrary if multiple items exist.
// Note that "found" means the two items are "equal" defined by the comparator
// rather than "identical".
size_t listx_index (const listx_t *self, const void *item);

// Find item in list.
// Return item handle if item is found, NULL if not found.
// If multiple same items exist, return arbitrary one of them.
// Note that "found" means the two items are "equal" defined by the comparator
// rather than "identical".
void *listx_find (const listx_t *self, const void *item);

// Check if item is in list
// Note that "include" means the two items are "equal" defined by the comparator
// rather than "identical".
bool listx_includes (const listx_t *self, const void *item);

// Check if the item is itself (identical item) in the list
bool listx_includes_identical (const listx_t *self, const void *item);

// Count numbers of item in list
size_t listx_count (const listx_t *self, const void *item);

// // Get previous item's handle.
// // Return NULL if the end is reached.
// void *listx_prev_handle (listx_t *self, void *handle);

// // Get next item's handle.
// // Return NULL if the end is reached.
// void *listx_next_handle (listx_t *self, void *handle);

// Create a list iterator before iteration.
// Set forward to true for iterating forward, or false for backward.
// Note that after creation, the iterator does not point to the first item, and
// you should then call listx_iter () to get the item.
listx_iterator_t listx_iter_init (const listx_t *self, bool forward);

// Create a list iterator starting from given item handle.
// Set forward to true for iterating forward, or false for backward.
// Note that calling listx_iter () will get the NEXT item after handle.
listx_iterator_t listx_iter_init_from (const listx_t *self,
                                       void *handle,
                                       bool forward);

// Get the next item using list iterator.
// Return the item, or NULL if iterator reaches the end.
// If item is returned, iterator->handle is just the handle of item.
void *listx_iter (const listx_t *self, listx_iterator_t *iterator);

// Pop current item during iteration.
// The iterator will go back to last position.
void *listx_iter_pop (listx_t *self, listx_iterator_t *iterator);

// Remove current item during iteration.
// The iterator will go back to last position.
void listx_iter_remove (listx_t *self, listx_iterator_t *iterator);

// Apply mapping on items in the list.
// Return a list of the results.
listx_t *listx_map (const listx_t *self, mapping_t mapping);

// Apply a function of two arguments cumulatively to the items of a list,
// from left to right, so as to reduce the sequence to a single item.
// initial item could be NULL.
void *listx_reduce (const listx_t *self, reducer_t reducer, void *initial);

// Return a list contains items for which filter (item) is true
// The returned list has the same callbacks and sorting state with input list
listx_t *listx_filter (const listx_t *self, filter_t filter);

// All items in list satisfy filter (item) == true
bool listx_all (const listx_t *self, filter_t filter);

// Any item in list satisfies filter (item) == true
bool listx_any (const listx_t *self, filter_t filter);

// Duplicator: duplicate a list
listx_t *listx_dup (const listx_t *self);

// Matcher: check if two lists are equal: same items in the same sequence.
bool listx_equal (const listx_t *self, const listx_t *list);

// Printer.
// Items are displayed using provided printer if available.
// If printer is not set, just display some basic infomations.
void listx_print (const listx_t *self);

// ---------------------------------------------------------------------------
// Assert that list is sorted correctly in given order.
// param order could be "ascending", "descending", or "no".
// This method is provided for debug purpose.
void listx_assert_sort (const listx_t *self, const char *order);

// Self test
void listx_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
