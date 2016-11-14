/*  =========================================================================
    arrayi - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


void arrayi_shuffle (int *array, size_t length, rng_t *rng) {
    assert (array);
    assert (rng);

    if (length <= 1)
        return;

    size_t i, j;
    int tmp;
    for (i = 0; i < length - 1; i++) {
        j = (size_t) rng_random_int (rng, i, length); // [0, length-1]
        tmp = array[j];
        array[j] = array[i];
        array[i] = tmp;
    }
}


void arrayi_shuffle_out (int *dest, const int *src, size_t length, rng_t *rng) {
    assert (src);
    assert (dest);
    assert (rng);
    size_t i, j;
    for (i = 0; i < length - 1; i++) {
        j = (size_t) rng_random_int (rng, 0, i+1); // [0, i]
        if (j != i) dest[i] = dest[j];
        dest[j] = src[i];
    }
}


void arrayi_shuffle_range (int *array, int a, int b, rng_t *rng) {
    assert (array);
    assert (a <= b);
    assert (rng);
    size_t i, j;
    for (i = 0; i < b - a; i++) {
        j = (size_t) rng_random_int (rng, 0, i+1); // [0, i]
        if (j != i) array[i] = array[j];
        array[j] = a + i;
    }
}

