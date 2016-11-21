/*  =========================================================================
    matrixd - a double type matrix

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __MATRIXD_H_INCLUDED__
#define __MATRIXD_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// Create a new matrixd object.
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

