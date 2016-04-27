/*  =========================================================================
    matrix4d - a double type matrix

    Internally, datas of a square matrix with large enough size are stored in
    left-upper-block-major order.

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"


struct _matrix4d_t {
    double *data;
    size_t order; // order of the internal square matrix
};


// Enlarge matrix4d to order larger than new_order_at_least
static void matrix4d_enlarge (matrix4d_t *self, size_t new_order_at_least) {
    assert (self);
    assert (new_order_at_least > self->order);

    size_t old_order = self->order;

    // double order until it is larger than new_order_at_least
    size_t new_order = self->order * 2;
    while (new_order < new_order_at_least)
        new_order = new_order * 2;

    double *new_data =
      (double *) realloc (self->data, sizeof (double) * new_order * new_order);
    assert (new_data);

    self->data = new_data;
    self->order = new_order;

    // initialize new alloced part as DOUBLE_NONE
    for (size_t i = old_order * old_order; i < new_order * new_order; i++)
        self->data[i] = DOUBLE_NONE;

    print_info ("matrix4d enlarged to order %zu.\n", self->order);
}


matrix4d_t *matrix4d_new (size_t initial_rows, size_t initial_cols) {
    matrix4d_t *self = (matrix4d_t *) malloc (sizeof (matrix4d_t));
    assert (self);

    size_t initial_order =
        (initial_rows > initial_cols) ? initial_rows : initial_cols;
    if (initial_order == 0)
        initial_order = 128;
    self->data =
        (double *) malloc (sizeof (double) * initial_order * initial_order);
    assert (self->data);
    self->order = initial_order;

    // initialize as DOUBLE_NONE
    for (size_t i = 0; i < initial_order * initial_order; i++)
        self->data[i] = DOUBLE_NONE;

    print_info ("matrix4d created with order %zu.\n", self->order);
    return self;
}


void matrix4d_free (matrix4d_t **self_p) {
    assert (self_p);
    if (*self_p) {
        matrix4d_t *self = *self_p;
        free (self->data);
        free (self);
        *self_p = NULL;
    }
    print_info ("matrix4d freed.\n");
}


double matrix4d_get (matrix4d_t *self, size_t row, size_t col) {
    assert (self);
    assert (row < self->order);
    assert (col < self->order);

    if (row > col)
        return self->data[row*row+col];
    else
        return self->data[col*col+2*col-row];
}


void matrix4d_set (matrix4d_t *self, size_t row, size_t col, double value) {
    assert (self);

    if (row > col) {
        if (row > self->order - 1)
            matrix4d_enlarge (self, row + 1);
        self->data[row*row+col] = value;
    }
    else {
        if (col > self->order - 1)
            matrix4d_enlarge (self, col + 1);
        self->data[col*col+2*col-row] = value;
    }
}


void matrix4d_test (bool verbose) {
    print_info (" * matrix4d: \n");

    size_t row, col, order = 400, rows = 200, cols = 300;
    double value;
    matrix4d_t *mat = NULL;

    // 1
    mat = matrix4d_new (rows, cols);
    assert (mat);
    for (size_t row = 0; row < order; row++) {
        for (size_t col = 0; col < order; col++) {
            matrix4d_set (mat, row, col, row * col * col);
        }
    }

    for (row = 0; row < order; row++) {
        for (col = 0; col < order; col++) {
            value = matrix4d_get (mat, row, col);
            // print_info ("row: %zu, col: %zu, value: %f.\n", row, col, value);
            assert (double_equal (value, row * col * col));
        }
    }

    matrix4d_free (&mat);

    // 2
    mat = matrix4d_new (0, 0);
    assert (mat);
    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            // print_info ("set: row: %zu, col: %zu.\n", row, col);
            matrix4d_set (mat, row, col, row*col + 3*col - sqrt (row));
        }
    }

    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            value = matrix4d_get (mat, row, col);
            // print_info ("get: row: %zu, col: %zu, value: %f.\n", row, col, value);
            assert (double_equal (value, row*col + 3*col - sqrt (row)));
        }
    }

    // 3
    mat = matrix4d_new (0, 0);
    assert (mat);
    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            // print_info ("set: row: %zu, col: %zu.\n", row, col);
            if (col == 1) {
                matrix4d_set (mat, row, col, DOUBLE_NONE);
            }
            else {
                matrix4d_set (mat, row, col, row*col + 3*col - sqrt (row));
            }
        }
    }

    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            value = matrix4d_get (mat, row, col);
            // print_info ("get: row: %zu, col: %zu, value: %f.\n", row, col, value);
            if (col == 1) {
                // printf ("value: %f\n", value);
                assert (double_is_none (value));
                // assert (double_equal (value, 10));
            }
            else {
                assert (double_equal (value, row*col + 3*col - sqrt (row)));
            }
        }
    }

    matrix4d_free (&mat);
    assert (mat == NULL);
    print_info ("OK\n");
}
