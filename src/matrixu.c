/*  =========================================================================
    matrixu - implementation

    An illustration:

    Internally, values of a square matrix with large enough size are stored in
    left-upper-block-major order. i.e.

    0  3  8 15  |
    1  2  7 14  |
    4  5  6 13  ^
    9 10 11 12  |
    ------>------

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


#define VTYPE size_t // type of data value
#define VTYPE_NONE SIZE_NONE // none value
#define MATRIX4D_DEFAULT_ORDER 16


struct _matrixu_t {
    VTYPE *data;
    size_t order; // order of the internal square matrix
};


// Enlarge matrixu to order larger than new_order_at_least
static void matrixu_enlarge (matrixu_t *self, size_t new_order_at_least) {
    assert (self);
    assert (new_order_at_least > self->order);

    size_t old_order = self->order;

    // Double matrix order until it is larger than new_order_at_least
    size_t new_order = old_order;
    while (new_order < new_order_at_least)
        new_order = new_order * 2;

    VTYPE *new_data =
      (VTYPE *) realloc (self->data, sizeof (VTYPE) * new_order * new_order);
    assert (new_data);

    self->data = new_data;
    self->order = new_order;

    // initialize new alloced part as VTYPE_NONE
    for (size_t i = old_order * old_order; i < new_order * new_order; i++)
        self->data[i] = VTYPE_NONE;

    // print_info ("matrixu enlarged to order %zu.\n", self->order);
}


matrixu_t *matrixu_new (size_t initial_rows, size_t initial_cols) {
    matrixu_t *self = (matrixu_t *) malloc (sizeof (matrixu_t));
    assert (self);

    size_t initial_order =
        (initial_rows > initial_cols) ? initial_rows : initial_cols;
    if (initial_order == 0)
        initial_order = MATRIX4D_DEFAULT_ORDER;
    self->data =
        (VTYPE *) malloc (sizeof (VTYPE) * initial_order * initial_order);
    assert (self->data);
    self->order = initial_order;

    // initialize as VTYPE_NONE
    for (size_t i = 0; i < initial_order * initial_order; i++)
        self->data[i] = VTYPE_NONE;

    print_info ("matrixu created with order %zu.\n", self->order);
    return self;
}


void matrixu_free (matrixu_t **self_p) {
    assert (self_p);
    if (*self_p) {
        matrixu_t *self = *self_p;
        free (self->data);
        free (self);
        *self_p = NULL;
    }
    // print_info ("matrixu freed.\n");
}


VTYPE matrixu_get (matrixu_t *self, size_t row, size_t col) {
    assert (self);
    assert (row < self->order);
    assert (col < self->order);

    if (row > col)
        return self->data[row*row+col];
    else
        return self->data[col*col+2*col-row];
}


void matrixu_set (matrixu_t *self, size_t row, size_t col, VTYPE value) {
    assert (self);

    if (row > col) {
        if (row > self->order - 1)
            matrixu_enlarge (self, row + 1);
        self->data[row*row+col] = value;
    }
    else {
        if (col > self->order - 1)
            matrixu_enlarge (self, col + 1);
        self->data[col*col+2*col-row] = value;
    }
}


void matrixu_print (matrixu_t *self) {
    assert (self);

    printf ("\nmatrixu order: %zu\n", self->order);
    printf ("-------------------------------------------\n");
    for (size_t r = 0; r < self->order; r++) {
        for (size_t c = 0; c < self->order; c++)
            printf ("%zu ", matrixu_get (self, r, c));
        printf ("\n");
    }
}


void matrixu_test (bool verbose) {
    print_info (" * matrixu: \n");

    size_t row, col, order = 400, rows = 200, cols = 300;
    VTYPE value;
    matrixu_t *mat = NULL;

    // 1
    mat = matrixu_new (rows, cols);
    assert (mat);
    for (size_t row = 0; row < order; row++) {
        for (size_t col = 0; col < order; col++) {
            matrixu_set (mat, row, col, row * col * col);
        }
    }

    for (row = 0; row < order; row++) {
        for (col = 0; col < order; col++) {
            value = matrixu_get (mat, row, col);
            // print_info ("row: %zu, col: %zu, value: %f.\n", row, col, value);
            assert (value == row * col * col);
        }
    }

    matrixu_free (&mat);

    // 2
    mat = matrixu_new (0, 0);
    assert (mat);
    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            // print_info ("set: row: %zu, col: %zu.\n", row, col);
            matrixu_set (mat, row, col, row*col + 3*col - (int) sqrt (row));
        }
    }

    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            value = matrixu_get (mat, row, col);
            // print_info ("get: row: %zu, col: %zu, value: %f.\n", row, col, value);
            assert (value == row*col + 3*col - (int) sqrt (row));
        }
    }

    matrixu_free (&mat);

    // 3
    mat = matrixu_new (0, 0);
    assert (mat);
    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            // print_info ("set: row: %zu, col: %zu.\n", row, col);
            if (col == 1) {
                matrixu_set (mat, row, col, VTYPE_NONE);
            }
            else {
                matrixu_set (mat, row, col, row*col + 3*col - (int) sqrt (row));
            }
        }
    }

    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            value = matrixu_get (mat, row, col);
            // print_info ("get: row: %zu, col: %zu, value: %f.\n", row, col, value);
            if (col == 1) {
                // printf ("value: %f\n", value);
                assert (value == VTYPE_NONE);
                // assert (double_equal (value, 10));
            }
            else {
                assert (value == row*col + 3*col - (int) sqrt (row));
            }
        }
    }

    matrixu_free (&mat);
    assert (mat == NULL);
    print_info ("OK\n");
}
