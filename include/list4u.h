/*  =========================================================================
    list4u - list container of usigned integers

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/


#ifndef __LIST4U_H_INCLUDED__
#define __LIST4U_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _list4u_t list4u_t;

// Create a new list.
// alloc_size is the initial size, or 0 to use a default value.
list4u_t *list4u_new (size_t alloc_size);

// Create a new list containing an arithmetic progression of integers.
// Return [start, start+step, ..., (end)]
// The last item depends on step
list4u_t *list4u_new_range (size_t start, size_t stop, int step);

// Destroy an list
void list4u_free (list4u_t **self_p);

// Get number of values in list
size_t list4u_size (list4u_t *self);

// Get value at index
size_t list4u_get (list4u_t *self, size_t index);

// Set value at index
void list4u_set (list4u_t *self, size_t index, size_t value);

// Append value to the end
void list4u_append (list4u_t *self, size_t value);

// Remove and return value at the end
size_t list4u_pop_last (list4u_t *self);

// Extend list by appending values from another list
void list4u_extend (list4u_t *self, list4u_t *list);

// Extend list by appending values from an array
void list4u_extend_array (list4u_t *self, size_t *array, size_t length);

// Insert value before index
void list4u_insert_at (list4u_t *self, size_t index, size_t value);

// Insert value to a sorted list.
// If the list is not sorted, it will be sorted in ascending order.
// Return the index.
size_t list4u_insert_sorted (list4u_t *self, size_t value);

// Remove item at index
void list4u_remove_at (list4u_t *self, size_t index);

// Remove slice
void list4u_remove_slice (list4u_t *self, size_t index_begin, size_t index_end);

// Find all items with value and remove them.
// Return 0 if value is found and removed, -1 if not found
int list4u_remove_value (list4u_t *self, size_t value);

// Clear list
void list4u_purge (list4u_t *self);

// Check if list is sorted
bool list4u_is_sorted (list4u_t *self);

// Check if list is sorted ascendingly
bool list4u_is_sorted_ascending (list4u_t *self);

// Check if list is sorted descendingly
bool list4u_is_sorted_descending (list4u_t *self);

// Sort list in specified order
void list4u_sort (list4u_t *self, bool ascending);

// Reverse list in place
void list4u_reverse (list4u_t *self);

// Shuffle list in place
// Set rng to use your random number generator.
// Set rng to NULL to use inner one.
void list4u_shuffle (list4u_t *self, rng_t *rng);

// Find the index of value.
// Return the index if found, SIZE_NONE if not found.
// The index could be arbitrary if multiple items with value exist.
size_t list4u_find (list4u_t *self, size_t value);

// Check if value is in list
bool list4u_includes (list4u_t *self, size_t value);

// Count numbers of value in list
size_t list4u_count (list4u_t *self, size_t value);

// Export list to array
size_t *list4u_dup_array (list4u_t *self);

// Self test
void list4u_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
