/*  =========================================================================
    matrixd - a double type matrix

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __MATRIXD_H_INCLUDED__
#define __MATRIXD_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _matrixd_t matrixd_t;

// Create a new matrixd object
// initial_rows and initial_cols could be 0 if they are not known.
matrixd_t *matrixd_new (size_t initial_rows, size_t initial_cols);

// Destroy matrixd object
void matrixd_free (matrixd_t **self_p);

// Get element at position (row, col)
// You can use double_is_none () to verify the return value
double matrixd_get (matrixd_t *self, size_t row, size_t col);

// Set element at position (row, col)
// value could be NAN
void matrixd_set (matrixd_t *self, size_t row, size_t col, double value);

// Printer
void matrixd_print (matrixd_t *self);

// Self test
void matrixd_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif

