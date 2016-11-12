/*  =========================================================================
    arrayu - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"


size_t arrayu_levenshtein_distance (const size_t *array1, size_t len1,
                                    const size_t *array2, size_t len2) {
    assert (array1 && array2);
    size_t x, y;
    size_t *matrix = (size_t *) malloc (sizeof (size_t) * (len1+1) * (len2+1));
    assert (matrix);

    matrix[0] = 0;
    for (x = 1; x <= len2; x++)
        matrix[x*(len1+1)] = matrix[(x-1)*(len1+1)] + 1;
    for (y = 1; y <= len1; y++)
        matrix[y] = matrix[y-1] + 1;
    for (x = 1; x <= len2; x++)
        for (y = 1; y <= len1; y++)
            matrix[x*(len1+1)+y] =
                min3 (matrix[(x-1)*(len1+1)+y] + 1,
                      matrix[x*(len1+1)+(y-1)] + 1,
                      matrix[(x-1)*(len1+1)+(y-1)] +
                        (array1[y-1] == array2[x-1] ? 0 : 1));

    size_t distance = matrix[len2*(len1+1) + len1];
    free (matrix);
    return distance;
}


bool arrayu_includes (size_t *array, size_t len, size_t value) {
    size_t i = 0;
    while ((i < len) && (array[i] != value)) i++;
    return (i < len);
}


void arrayu_print (size_t *array, size_t len) {
    printf ("\narrayu: size: %zu\n", len);
    printf ("--------------------------------------------------\n");
    for (size_t idx = 0; idx < len; idx++)
        printf ("%zu ", array[idx]);
    printf ("\n");
}


void arrayu_shuffle_range (size_t *array, size_t a, size_t b, rng_t *rng) {
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
