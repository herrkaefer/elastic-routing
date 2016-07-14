/*  =========================================================================
    listu - array list container of usigned integers

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/


#ifndef __LISTU_H_INCLUDED__
#define __LISTU_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _listu_t listu_t;

// Create a new list.
// alloc_size is the initial size, or 0 to use a default value.
listu_t *listu_new (size_t alloc_size);

// Create a new list containing an arithmetic progression of integers.
// Return [start, start+step, ..., (end)]
// The last item depends on step
listu_t *listu_new_range (size_t start, size_t stop, int step);

// Create a new list from array
listu_t *listu_new_from_array (const size_t *array, size_t len);

// Destroy an list
void listu_free (listu_t **self_p);

// Get number of values in list
size_t listu_size (listu_t *self);

// Get value at index
size_t listu_get (listu_t *self, size_t index);

// Set value at index
void listu_set (listu_t *self, size_t index, size_t value);

// Add value to the first
void listu_prepend (listu_t *self, size_t value);

// Append value to the end
void listu_append (listu_t *self, size_t value);

// Remove and return value at the end
size_t listu_pop_last (listu_t *self);

// Extend list by appending values from another list
void listu_extend (listu_t *self, listu_t *list);

// Extend list by appending values from an array
void listu_extend_array (listu_t *self, size_t *array, size_t length);

// Insert value before index
void listu_insert_at (listu_t *self, size_t index, size_t value);

// Insert value to a sorted list.
// If the list is not sorted, it will be sorted in ascending order.
// Return the index.
size_t listu_insert_sorted (listu_t *self, size_t value);

// Remove item at index
void listu_remove_at (listu_t *self, size_t index);

// Remove first item
void listu_remove_first (listu_t *self);

// Remove last item
void listu_remove_last (listu_t *self);

// Remove list slice [index_begin, index_end]
void listu_remove_slice (listu_t *self, size_t index_begin, size_t index_end);

// Find all items with value and remove them.
// Return 0 if value is found and removed, -1 if not found
int listu_remove (listu_t *self, size_t value);

// Swap items at two indices
void listu_swap (listu_t *self, size_t index1, size_t index2);

// Clear list
void listu_purge (listu_t *self);

// Dump data array (read only)
const size_t *listu_array (listu_t *self);

// Check if list is sorted
bool listu_is_sorted (listu_t *self);

// Check if list is sorted ascendingly
bool listu_is_sorted_ascending (listu_t *self);

// Check if list is sorted descendingly
bool listu_is_sorted_descending (listu_t *self);

// Sort list in specified order
void listu_sort (listu_t *self, bool ascending);

// Reverse list in place
void listu_reverse (listu_t *self);

// Shuffle list in place
// Set rng to use your random number generator, or NULL to use the inner one.
void listu_shuffle (listu_t *self, rng_t *rng);

// Shuffle list slice [index_begin, index_end] in place.
// Set rng to use your random number generator, or NULL to use the inner one.
void listu_shuffle_slice (listu_t *self,
                           size_t index_begin,
                           size_t index_end,
                           rng_t *rng);

// Find the index of value.
// Return the index if found, SIZE_NONE if not found.
// The index could be arbitrary if multiple items with value exist.
size_t listu_find (listu_t *self, size_t value);

// Check if value is in list
bool listu_includes (listu_t *self, size_t value);

// Count numbers of value in list
size_t listu_count (listu_t *self, size_t value);

// Export list to array
size_t *listu_dump_array (listu_t *self);

// Duplicator
listu_t *listu_duplicate (listu_t *self);

// Printer
void listu_print (const listu_t *self);

// Self test
void listu_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif