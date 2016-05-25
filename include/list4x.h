/*  =========================================================================
    list4x - a generic list container

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/
/*
list4x is a generic list container.

- Item can be accessed

    - by handle: item handle is a pointer which is always bound with the item
      after it is attached to the list, and stays unchanged during sort,
      reorder, reverse, shuffle, etc, unless the item is explicitly altered.
      So actually, the handle could be served as item's ID in the list.

    - by index: item can be get and set like in ordinary array. Using index is a
      litter slower than using item handle.

    - through iteration (see below)

- List iteration

    - Use list4x_iter_init () or list4x_iter_init_from () to initialize an
      iterator, and use list4x_iter () to get item. e.g.

    TYPE *item;
    // Start iteration from the beginning (forward) or end (backward)
    list4x_iterator_t iter = list4x_iter_init (list, true);
    // or start iteration from specified item (start from the NEXT one)
    // list4x_iterator_t iter = list4x_iter_init_from (list, handle, true);
    while ((item = (TYPE *) list4x_iter (list, &iter)) != NULL) {
        // Do stuff with item
        //
        // If you need item handle, it is just iter.handle
        //
        // You can remove current item from list by
        // list4x_iter_remove (list);

        // or pop current item from list without destroying it by
        // list4x_iter_pop (list);
    }
*/


#ifndef __LIST4X_H_INCLUDED__
#define __LIST4X_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _list4x_t list4x_t;

typedef struct {
    void *handle; // handle of current item
    bool forward; // iteration direction. true: forward, false: backward
} list4x_iterator_t;

// Create a new list
list4x_t *list4x_new (void);

// Destroy a list
void list4x_free (list4x_t **self_p);

// Set item destructor callback.
// By default, item is not destroyed when it is removed from the list.
void list4x_set_destructor (list4x_t *self, destructor_t destructor);

// Set item duplicator callback
// If this is set, the list contains a duplication of item.
void list4x_set_duplicator (list4x_t *self, duplicator_t duplicator);

// Set item comparator callback
void list4x_set_comparator (list4x_t *self, comparator_t comparator);

// Set item printer callback.
// If this is set, list4x_print () will use it to print items.
void list4x_set_printer (list4x_t *self, printer_t printer);

// Get number of items in list
size_t list4x_size (list4x_t *self);

// Check if list is sorted
bool list4x_is_sorted (list4x_t *self);

// Check if list is sorted ascendingly
bool list4x_is_sorted_ascending (list4x_t *self);

// Check if list is sorted descendingly
bool list4x_is_sorted_descending (list4x_t *self);

// Get item by handle
void *list4x_item (list4x_t *self, void *handle);

// Get item at index.
// For list traversing, using list_iterato_t is much faster.
void *list4x_item_at (list4x_t *self, size_t index);

// Return the first item of the list
// Return NULL if list is empty
void *list4x_first (list4x_t *self);

// Return the last item of the list
// Return NULL if list is empty
void *list4x_last (list4x_t *self);

// Set item by handle
void list4x_set_item (list4x_t *self, void *handle, void *item);

// Set item at index
void list4x_set_item_at (list4x_t *self, size_t index, void *item);

// Add item to the start.
// Return item handle.
void *list4x_prepend (list4x_t *self, void *item);

// Add item to the end.
// Return item handle.
void *list4x_append (list4x_t *self, void *item);

// Remove and return the item by handle
void *list4x_pop (list4x_t *self, void *handle);

// Remove and return the item at the start.
// Return NULL if the list is empty.
void *list4x_pop_first (list4x_t *self);

// Remove and return item at the end.
// Return NULL if the list is empty.
void *list4x_pop_last (list4x_t *self);

// Remove and return the item at index.
// Return NULL if the list is empty.
void *list4x_pop_at (list4x_t *self, size_t index);

// Extend list by appending items from another list
void list4x_extend (list4x_t *self, list4x_t *list);

// Extend list by appending items from an array
void list4x_extend_array (list4x_t *self, void **array, size_t length);

// Insert value at index (before the original item at index).
// Return item handle.
void *list4x_insert_at (list4x_t *self, size_t index, void *item);

// Insert value to a sorted list.
// Return item handle
// If the list is not sorted, it will be sorted in ascending order.
void *list4x_insert_sorted (list4x_t *self, void *item);

// Remove item by handle
void list4x_remove (list4x_t *self, void *handle);

// Remove all list items which equal to item
void list4x_remove_item (list4x_t *self, void *item);

// Remove first item
void list4x_remove_first (list4x_t *self);

// Remove last item
void list4x_remove_last (list4x_t *self);

// Remove item at index
void list4x_remove_at (list4x_t *self, size_t index);

// Remove items at interval [from_index, to_index]
void list4x_remove_slice (list4x_t *self, size_t from_index, size_t to_index);

// Remove all items in list.
// After calling this, the list will be empty, however the sorting state will
// be remained.
void list4x_purge (list4x_t *self);

// Sort list using provided comparator in specified order
void list4x_sort (list4x_t *self, bool ascending);

// Reorder an item to the right position in list according to the sorting order.
// The item is given by handle, in list or not.
// It is assumed that the list is sorted except for the only item which is just
// altered and needs to be replaced to the right position.
void list4x_reorder (list4x_t *self, void *handle);

// Reverse list in place
void list4x_reverse (list4x_t *self);

// Shuffle list in place
// Set rng to use your random number generator, or NULL to use an default one.
void list4x_shuffle (list4x_t *self, rng_t *rng);

// Find item in list.
// Return the index if item is found, SIZE_NONE if not found.
// The index could be arbitrary if multiple items exist.
// Note that "found" means the two items are "equal" defined by the comparator
// rather than "identical".
size_t list4x_index (list4x_t *self, void *item);

// Find item in list.
// Return item handle if item is found, NULL if not found.
// If multiple same items exist, return arbitrary one of them.
// Note that "found" means the two items are "equal" defined by the comparator
// rather than "identical".
void *list4x_find (list4x_t *self, void *item);

// Check if item is in list
// Note that "include" means the two items are "equal" defined by the comparator
// rather than "identical".
bool list4x_includes (list4x_t *self, void *item);

// Check if the item is itself (identical item) in the list
bool list4x_includes_identical (list4x_t *self, void *item);

// Count numbers of item in list
size_t list4x_count (list4x_t *self, void *item);

// Create a list iterator before iteration.
// Set forward to true for iterating forward, or false for backward.
// Note that after creation, the iterator does not point to the first item, and
// you should then call list4x_iter () to get the item.
list4x_iterator_t list4x_iter_init (list4x_t *self, bool forward);

// Create a list iterator starting from given item handle.
// Set forward to true for iterating forward, or false for backward.
// Note that calling list4x_iter () will get the NEXT item after handle.
list4x_iterator_t list4x_iter_init_from (list4x_t *self,
                                         void *handle,
                                         bool forward);

// Get the next item using list iterator.
// Return the item, or NULL if iterator reaches the end.
// If item is returned, iterator->handle is just the handle of item.
void *list4x_iter (list4x_t *self, list4x_iterator_t *iterator);

// Pop current item during iteration.
// The iterator will go back to last position.
void *list4x_iter_pop (list4x_t *self, list4x_iterator_t *iterator);

// Remove current item during iteration.
// The iterator will go back to last position.
void list4x_iter_remove (list4x_t *self, list4x_iterator_t *iterator);

// Apply operation on items in the list
// Return a list of the results
list4x_t *list4x_map (list4x_t *self, mapping_t mapping);

// Apply a function of two arguments cumulatively to the items of a list,
// from left to right, so as to reduce the sequence to a single item.
// initial item could be NULL.
void *list4x_reduce (list4x_t *self, reducer_t reducer, void *initial);

// Return a list contains items for which fn(item) is true
// The returned list has the same callbacks and sorting state with input list
list4x_t *list4x_filter (list4x_t *self, filter_t filter);

// All items in list satisfy fn (item) == true
bool list4x_all (list4x_t *self, filter_t filter);

// Any item in list satisfies fn (item) == true
bool list4x_any (list4x_t *self, filter_t filter);

// Duplicate a list
list4x_t *list4x_duplicate (const list4x_t *self);

// Check if two lists are equal: same items in the same sequence.
bool list4x_equal (const list4x_t *self, const list4x_t *list);

// Print a list.
// Items are displayed using provided printer if available.
void list4x_print (const list4x_t *self);

// ---------------------------------------------------------------------------
// Assert that list is sorted correctly in given order.
// param order could be "ascending", "descending", or "no".
// This method is provided for debug purpose.
void list4x_assert_sort (list4x_t *self, const char *order);

// Self test
void list4x_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
