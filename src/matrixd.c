/*  =========================================================================
    matrixd - implementation

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


#define VTYPE double // type of data value
#define VTYPE_NONE DOUBLE_NONE // none value
#define MATRIX4D_DEFAULT_ORDER 16


struct _matrixd_t {
    VTYPE *data;
    size_t order; // order of the internal square matrix
};


// Enlarge matrixd to order larger than new_order_at_least
static void matrixd_enlarge (matrixd_t *self, size_t new_order_at_least) {
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

    // initialize new alloced part as DOUBLE_NONE
    for (size_t i = old_order * old_order; i < new_order * new_order; i++)
        self->data[i] = DOUBLE_NONE;

    // print_info ("matrixd enlarged to order %zu.\n", self->order);
}


matrixd_t *matrixd_new (size_t initial_rows, size_t initial_cols) {
    matrixd_t *self = (matrixd_t *) malloc (sizeof (matrixd_t));
    assert (self);

    size_t initial_order =
        (initial_rows > initial_cols) ? initial_rows : initial_cols;
    if (initial_order == 0)
        initial_order = MATRIX4D_DEFAULT_ORDER;
    self->data =
        (VTYPE *) malloc (sizeof (VTYPE) * initial_order * initial_order);
    assert (self->data);
    self->order = initial_order;

    // initialize as DOUBLE_NONE
    for (size_t i = 0; i < initial_order * initial_order; i++)
        self->data[i] = DOUBLE_NONE;

    print_info ("matrixd created with order %zu.\n", self->order);
    return self;
}


void matrixd_free (matrixd_t **self_p) {
    assert (self_p);
    if (*self_p) {
        matrixd_t *self = *self_p;
        free (self->data);
        free (self);
        *self_p = NULL;
    }
    // print_info ("matrixd freed.\n");
}


VTYPE matrixd_get (matrixd_t *self, size_t row, size_t col) {
    assert (self);
    assert (row < self->order);
    assert (col < self->order);

    if (row > col)
        return self->data[row*row+col];
    else
        return self->data[col*col+2*col-row];
}


void matrixd_set (matrixd_t *self, size_t row, size_t col, VTYPE value) {
    assert (self);

    if (row > col) {
        if (row > self->order - 1)
            matrixd_enlarge (self, row + 1);
        self->data[row*row+col] = value;
    }
    else {
        if (col > self->order - 1)
            matrixd_enlarge (self, col + 1);
        self->data[col*col+2*col-row] = value;
    }
}


void matrixd_print (matrixd_t *self) {
    assert (self);

    printf ("\nmatrixd order: %zu\n", self->order);
    printf ("-------------------------------------------\n");
    for (size_t r = 0; r < self->order; r++) {
        for (size_t c = 0; c < self->order; c++)
            printf ("%6.1f ", matrixd_get (self, r, c));
        printf ("\n");
    }
}


void matrixd_test (bool verbose) {
    print_info (" * matrixd: \n");

    size_t row, col, order = 400, rows = 200, cols = 300;
    VTYPE value;
    matrixd_t *mat = NULL;

    // 1
    mat = matrixd_new (rows, cols);
    assert (mat);
    for (size_t row = 0; row < order; row++) {
        for (size_t col = 0; col < order; col++) {
            matrixd_set (mat, row, col, row * col * col);
        }
    }

    for (row = 0; row < order; row++) {
        for (col = 0; col < order; col++) {
            value = matrixd_get (mat, row, col);
            // print_info ("row: %zu, col: %zu, value: %f.\n", row, col, value);
            assert (double_equal (value, row * col * col));
        }
    }

    matrixd_free (&mat);

    // 2
    mat = matrixd_new (0, 0);
    assert (mat);
    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            // print_info ("set: row: %zu, col: %zu.\n", row, col);
            matrixd_set (mat, row, col, row*col + 3*col - sqrt (row));
        }
    }

    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            value = matrixd_get (mat, row, col);
            // print_info ("get: row: %zu, col: %zu, value: %f.\n", row, col, value);
            assert (double_equal (value, row*col + 3*col - sqrt (row)));
        }
    }

    matrixd_free (&mat);

    // 3
    mat = matrixd_new (0, 0);
    assert (mat);
    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            // print_info ("set: row: %zu, col: %zu.\n", row, col);
            if (col == 1) {
                matrixd_set (mat, row, col, DOUBLE_NONE);
            }
            else {
                matrixd_set (mat, row, col, row*col + 3*col - sqrt (row));
            }
        }
    }

    for (row = 0; row < 3*order; row++) {
        for (col = 0; col < 3*order; col++) {
            value = matrixd_get (mat, row, col);
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

    matrixd_free (&mat);
    assert (mat == NULL);
    print_info ("OK\n");
}
