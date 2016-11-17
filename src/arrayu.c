/*  =========================================================================
    arrayu - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


size_t *arrayu_new_range (size_t a, size_t b) {
    if (a > b) {
        size_t tmp = a;
        a = b;
        b = tmp;
    }
    size_t len = b - a;
    size_t *array = (size_t *) malloc (sizeof (size_t) * len);
    assert (array);
    for (size_t i = 0; i < len; i++)
        array[i] = a + i;
    return array;
}


size_t *arrayu_new_shuffle_range (size_t a, size_t b, rng_t *rng) {
    if (a > b) {
        size_t tmp = a;
        a = b;
        b = tmp;
    }

    size_t len = b - a;
    size_t *array = (size_t *) malloc (sizeof (size_t) * len);
    assert (array);

    bool own_rng = false;
    if (rng == NULL) {
        rng = rng_new ();
        own_rng = true;
    }

    size_t i, j;
    for (i = 0; i < b - a; i++) {
        j = (size_t) rng_random_int (rng, 0, i+1); // [0, i]
        if (j != i) array[i] = array[j];
        array[j] = a + i;
    }

    if (own_rng)
        rng_free (&rng);
    return array;
}


bool arrayu_includes (const size_t *array, size_t len, size_t value) {
    size_t i = 0;
    while ((i < len) && (array[i] != value)) i++;
    return (i < len);
}


void arrayu_print (const size_t *array, size_t len) {
    printf ("\narrayu: size: %zu\n", len);
    printf ("--------------------------------------------------\n");
    for (size_t idx = 0; idx < len; idx++)
        printf ("%zu ", array[idx]);
    printf ("\n");
}


// Quick sort of array
void arrayu_quick_sort (size_t *array, size_t len, bool ascending) {
    if (len <= 1)
        return;

    size_t t = array[0];
    size_t i = 0, j = len - 1;

    while (1) {
        if (ascending)
            while ((j > i) && (array[j] >= t)) j--;
        else
            while ((j > i) && (array[j] <= t)) j--;

        if (j == i) break;

        array[i] = array[j];
        i++;

        if (ascending)
            while ((i < j) && (array[i] <= t)) i++;
        else
            while ((i < j) && (array[i] >= t)) i++;

        if (i == j) break;

        array[j] = array[i];
        j--;
    }

    array[i] = t;

    arrayu_quick_sort (array, i, ascending);
    arrayu_quick_sort (&array[i+1], len-i-1, ascending);
}


size_t arrayu_binary_search (const size_t *array,
                             size_t length,
                             size_t value,
                             bool ascending) {
    size_t head = 0, tail = length, mid;

    if (ascending) {
        while (head < tail) {
            mid = (head + tail) / 2;
            if (array[mid] == value)
                return mid;
            else if (array[mid] > value)
                tail = mid;
            else
                head = mid + 1;
        }
    }
    else {
        while (head < tail) {
            mid = (head + tail) / 2;
            if (array[mid] == value)
                return mid;
            else if (array[mid] < value)
                tail = mid;
            else
                head = mid + 1;
        }
    }

    return SIZE_NONE;
}


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

