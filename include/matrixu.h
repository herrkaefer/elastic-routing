/*  =========================================================================
    matrixu - a unsigned int type matrix

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __MATRIXU_H_INCLUDED__
#define __MATRIXU_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// Create a new matrixu object.
// initial_rows and initial_cols could be 0 if they are not known.
matrixu_t *matrixu_new (size_t initial_rows, size_t initial_cols);

// Destroy matrixu object
void matrixu_free (matrixu_t **self_p);

// Get element at position (row, col)
// You can use double_is_none () to verify the return value
size_t matrixu_get (matrixu_t *self, size_t row, size_t col);

// Set element at position (row, col)
// value could be NAN
void matrixu_set (matrixu_t *self, size_t row, size_t col, size_t value);

// Printer
void matrixu_print (matrixu_t *self);

// Self test
void matrixu_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif

