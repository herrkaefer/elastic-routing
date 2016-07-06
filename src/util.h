/*  =========================================================================
    util - Elastic Routing: Utilities

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __UTIL_H_INCLUDED__
#define __UTIL_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
    #define print_info(...)    {fprintf (stdout, "INFO: %s: ", __func__); \
                                fprintf (stdout, ##__VA_ARGS__);}
    #define print_warning(...) {fprintf (stdout, "WARNING: %s: ", __func__); \
                                fprintf (stdout, ##__VA_ARGS__);}
    #define print_error(...)   {fprintf (stderr, "ERROR: %s: ", __func__); \
                                fprintf (stderr, ##__VA_ARGS__);}
    #define print_debug(...)   {fprintf (stderr, \
                                "\n>>>>>>>> Debug @ %s (line %d) >>>>>>>>>>\n",\
                                __func__, __LINE__); \
                                fprintf (stderr, ##__VA_ARGS__);}
#else
    #define print_info(...)
    #define print_warning(...)
    #define print_error(...)
    #define print_debug(...)
#endif

#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

// Compare two int values, for sorting
inline int int_compare (const int *a, const int *b) {
    return (*a - *b);
}

// Compare two double values, for sorting
inline int double_compare (const double *a, const double *b) {
    return (*a < *b) ? -1 : ((*a > *b) ? 1 : 0);
}

// If two double values are equal
// inline bool double_equal (double a, double b) {
//     return (fabs (a - b) < DOUBLE_THRESHOLD);
// }

// If double value is none
// inline bool double_is_none (double a) {
//     return isnan (a);
// }


// Levenshtein distance between two unsigned int arrays
size_t arrayu_levenshtein_distance (const size_t *array1, size_t len1,
                                    const size_t *array2, size_t len2);


// Check if unsigned int array includes value
bool arrayu_includes (size_t *array, size_t len, size_t value);

// Printer of unsigned int array
void arrayu_print (size_t *array, size_t len);

// ---------------------------------------------------------------------------

// String hash function
size_t string_hash (const char *string);

// Compare two strings
int string_compare (const char *str1, const char *str2);

// If two strings are equal
bool string_equal (const char *str1, const char *str2);

// Free a string
void string_free (char **string_p);

// Duplicate a string
char *string_duplicate (const char *string);

// Print a string
void string_print (const char *string);

// Generate a random string consisting of alpha-numeric characters with
// specific length range.
// Provide your rng or set it to NULL to use a inner one.
char *string_random_alphanum (size_t min_length, size_t max_length, rng_t *rng);

// String Levenshtein distance.
// Modified from:
// https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C
size_t string_levenshtein_distance (const char *str1, const char *str2);

// String cut and splice crossover
// str1:   +++++++|++++++++++++
// str2:   **********|*************
// ======>
// child1: +++++++|*************
// child2: **********|++++++++++++
list4x_t *string_cut_and_splice (const char *str1, const char *str2, rng_t *rng);

// ---------------------------------------------------------------------------

// void safe_free (void **self_p) {
//     assert (self_p);
//     if (*self_p) {
//         free (*self_p);
//         *self_p = NULL;
//     }
// }

// ---------------------------------------------------------------------------

// Check whether the software passed the expiry date
bool out_of_date (time_t expiry_t);

// Check whether the software passed the expiry date
bool out_of_date_by_date (int expiry_year, int expiry_month, int expiry_day);

// Convert date to time_t
time_t date_to_time (int year, int mon, int day, int hour, int min, int sec);

// ---------------------------------------------------------------------------

// shuffle the n elements of array in place.
void array4i_shuffle (int *array, size_t length, rng_t *rng);

// get a shuffled copy of the n elements of array
// Return result in param dest
void array4i_shuffle_out (int *dest, const int *src, size_t length, rng_t *rng);

// generate random permutation of integers in [a, b)
// Return result in param array
void array4i_shuffle_range (int *array, int a, int b, rng_t *rng);

// generate random permutation of unsigned integers in [a, b)
void array4u_shuffle_range (size_t *array, size_t a, size_t b, rng_t *rng);

// ---------------------------------------------------------------------------

size_t factorial (size_t n);


#ifdef __cplusplus
}
#endif

#endif
