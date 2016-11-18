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
    size_t *self = (size_t *) malloc (sizeof (size_t) * len);
    assert (self);
    for (size_t i = 0; i < len; i++)
        self[i] = a + i;
    return self;
}


size_t *arrayu_new_shuffle_range (size_t a, size_t b, rng_t *rng) {
    if (a > b) {
        size_t tmp = a;
        a = b;
        b = tmp;
    }

    size_t len = b - a;
    size_t *self = (size_t *) malloc (sizeof (size_t) * len);
    assert (self);

    bool own_rng = false;
    if (rng == NULL) {
        rng = rng_new ();
        own_rng = true;
    }

    size_t i, j;
    for (i = 0; i < b - a; i++) {
        j = (size_t) rng_random_int (rng, 0, i+1); // [0, i]
        if (j != i) self[i] = self[j];
        self[j] = a + i;
    }

    if (own_rng)
        rng_free (&rng);
    return self;
}


size_t arrayu_find (const size_t *self, size_t len, size_t value) {
    assert (self);
    size_t idx = 0;
    while ((idx < len) && (self[idx] != value))
        idx++;
    return (idx < len) ? idx : SIZE_NONE;
}


size_t arrayu_count (const size_t *self, size_t len, size_t value) {
    assert (self);
    size_t cnt = 0;
    for (size_t idx = 0; idx < len; idx++) {
        if (self[idx] == value)
            cnt++;
    }
    return cnt;
}


bool arrayu_includes (const size_t *self, size_t len, size_t value) {
    assert (self);
    return arrayu_find (self, len, value) != SIZE_NONE;
}


void arrayu_print (const size_t *self, size_t len) {
    assert (self);
    printf ("\narrayu: size: %zu\n", len);
    printf ("--------------------------------------------------\n");
    for (size_t idx = 0; idx < len; idx++)
        printf ("%zu ", self[idx]);
    printf ("\n");
}


void arrayu_quick_sort (size_t *self, size_t len, bool ascending) {
    assert (self);
    if (len <= 1)
        return;

    size_t t = self[0];
    size_t i = 0, j = len - 1;

    while (1) {
        if (ascending)
            while ((j > i) && (self[j] >= t)) j--;
        else
            while ((j > i) && (self[j] <= t)) j--;

        if (j == i) break;

        self[i] = self[j];
        i++;

        if (ascending)
            while ((i < j) && (self[i] <= t)) i++;
        else
            while ((i < j) && (self[i] >= t)) i++;

        if (i == j) break;

        self[j] = self[i];
        j--;
    }

    self[i] = t;

    arrayu_quick_sort (self, i, ascending);
    arrayu_quick_sort (&self[i+1], len-i-1, ascending);
}


size_t arrayu_binary_search (const size_t *self,
                             size_t length,
                             size_t value,
                             bool ascending) {
    assert (self);
    size_t head = 0, tail = length, mid;

    if (ascending) {
        while (head < tail) {
            mid = (head + tail) / 2;
            if (self[mid] == value)
                return mid;
            else if (self[mid] > value)
                tail = mid;
            else
                head = mid + 1;
        }
    }
    else {
        while (head < tail) {
            mid = (head + tail) / 2;
            if (self[mid] == value)
                return mid;
            else if (self[mid] < value)
                tail = mid;
            else
                head = mid + 1;
        }
    }

    return SIZE_NONE;
}


void arrayu_reverse (size_t *self, size_t len) {
    assert (self);
    size_t tmp;
    for (size_t idx = 0; idx < len / 2; idx++) {
        tmp = *(self + idx);
        *(self + idx) = *(self + len - idx - 1);
        *(self + len - idx - 1) = tmp;
    }
}


void arrayu_shuffle (size_t *self, size_t len, rng_t *rng) {
    assert (self);

    bool own_rng = false;
    if (rng == NULL) {
        rng = rng_new ();
        own_rng = true;
    }

    size_t i, j;
    size_t tmp;
    for (i = 0; i < len - 1; i++) {
        j = (size_t) rng_random_int (rng, i, len); // j in [i, len-1]
        // Swap values at i and j
        tmp = *(self + j);
        *(self + j) = *(self + i);
        *(self + i) = tmp;
    }

    if (own_rng)
        rng_free (&rng);
}


void arrayu_swap (size_t *self, size_t idx1, size_t idx2) {
    assert (self);
    size_t tmp = *(self + idx1);
    *(self + idx1) = *(self + idx2);
    *(self + idx2) = tmp;
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

