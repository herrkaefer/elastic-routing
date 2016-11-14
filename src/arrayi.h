/*  =========================================================================
    arrayi - array of int type values

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __ARRAYI_H_INCLUDED__
#define __ARRAYI_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif


// shuffle the n elements of array in place.
void arrayi_shuffle (int *array, size_t length, rng_t *rng);

// get a shuffled copy of the n elements of array
// Return result in param dest
void arrayi_shuffle_out (int *dest, const int *src, size_t length, rng_t *rng);

// generate random permutation of integers in [a, b)
// Return result in param array
void arrayi_shuffle_range (int *array, int a, int b, rng_t *rng);


#ifdef __cplusplus
}
#endif

#endif
