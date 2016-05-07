/*  =========================================================================
    rng - random number generator.

    A wrapper of [PCG](http://www.pcg-random.org/) RNGs.

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __RNG_H_INCLUDED__
#define __RNG_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _rng_t rng_t;

// Create a rng
rng_t *rng_new (void);

// Destroy rng
void rng_free (rng_t **self_p);

// Generate random double value in [0, 1)
double rng_random (rng_t *self);

// Generate random double value in [0, 1]
// double rng_random_c (rng_t *self);

// Generate random integer distributed uniformly in [a, b) or [b, a).
// If a == b, return a.
int rng_random_int (rng_t *self, int a, int b);

// Self test
void rng_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
