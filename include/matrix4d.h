/*  =========================================================================
    matrix4d - a double type matrix

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __MATRIX4D_H_INCLUDED__
#define __MATRIX4D_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _matrix4d_t matrix4d_t;

// Create a new matrix4d object
// initial_rows and initial_cols could be 0 if they are not known.
matrix4d_t *matrix4d_new (size_t initial_rows, size_t initial_cols);

// Destroy matrix4d object
void matrix4d_free (matrix4d_t **self_p);

// Get element at position (row, col)
// You can use double_is_none () to verify the return value
double matrix4d_get (matrix4d_t *self, size_t row, size_t col);

// Set element at position (row, col)
// value could be NAN
void matrix4d_set (matrix4d_t *self, size_t row, size_t col, double value);

// Printer
void matrix4d_print (matrix4d_t *self);

// Self test
void matrix4d_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif

