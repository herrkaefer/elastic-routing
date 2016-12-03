/*  =========================================================================
    arrayu - extension of array of size_t type

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __ARRAYU_H_INCLUDED__
#define __ARRAYU_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// Create array with values [a, a+1, ..., b)
size_t *arrayu_new_range (size_t a, size_t b);

// Create array with random permutation of unsigned integers in [a, b).
// Return an array with size (b-a).
size_t *arrayu_new_shuffle_range (size_t a, size_t b, rng_t *rng);

// Duplicator
size_t *arrayu_dup (const size_t *self, size_t len);

// Matcher
bool arrayu_equal (const size_t *self, const size_t *arrayu, size_t len);

// Printer
void arrayu_print (const size_t *self, size_t len);

// Find value in array.
// Return the index if found, SIZE_NONE if not found.
size_t arrayu_find (const size_t *self, size_t len, size_t value);

// Count values in array
size_t arrayu_count (const size_t *self, size_t len, size_t value);

// Check if value is in array
bool arrayu_includes (const size_t *self, size_t len, size_t value);

// Reverse array in place
void arrayu_reverse (size_t *self, size_t len);

// Rorate array by num elements.
// num > 0: right rotation; num < 0: left rotation.
void arrayu_rotate (size_t *self, size_t len, int num);

// Swap two items
void arrayu_swap (size_t *self, size_t idx1, size_t idx2);

// Quick sort
void arrayu_quick_sort (size_t *self, size_t len, bool ascending);

// Binary search of value in sorted array.
// Note that the array should be sorted. Specify the order with param ascending.
// Return the index of value, or SIZE_NONE if not found.
size_t arrayu_binary_search (const size_t *self,
                             size_t len,
                             size_t value,
                             bool ascending);

// Shuffle array in place
void arrayu_shuffle (size_t *self, size_t len, rng_t *rng);

// Swap two nonoverlapping slices, i.e.
// (..., i --> j, ..., u --> v, ...) =>
// (..., u --> v, ..., i --> j, ...)
// Require: i <= j < u <=v
void arrayu_swap_slices (size_t *self,
                         size_t i, size_t j, size_t u, size_t v);

// Levenshtein distance
size_t arrayu_levenshtein_distance (const size_t *array1, size_t len1,
                                    const size_t *array2, size_t len2);


#ifdef __cplusplus
}
#endif

#endif
