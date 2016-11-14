/*  =========================================================================
    arrayu - array of unsigned int type values

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __ARRAYU_H_INCLUDED__
#define __ARRAYU_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif


// Levenshtein distance between two unsigned int arrays
size_t arrayu_levenshtein_distance (const size_t *array1, size_t len1,
                                    const size_t *array2, size_t len2);

// Check if unsigned int array includes value
bool arrayu_includes (size_t *array, size_t len, size_t value);

// Printer of unsigned int array
void arrayu_print (size_t *array, size_t len);

// generate random permutation of unsigned integers in [a, b)
void arrayu_shuffle_range (size_t *array, size_t a, size_t b, rng_t *rng);


#ifdef __cplusplus
}
#endif

#endif
