/*  =========================================================================
    arrayi - array of int type values

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
