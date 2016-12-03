/*  =========================================================================
    numeric - extension of numeric types (int, double, size_t, id, ...)

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __NUMERIC_EXT_H_INCLUDED__
#define __NUMERIC_EXT_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif


#define PI 3.1415926

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#define SIZE_NONE SIZE_MAX

#define ID_NONE   SIZE_NONE

#define DOUBLE_MAX       DBL_MAX
#define DOUBLE_MIN       DBL_MIN
#define DOUBLE_NONE      NAN
#define DOUBLE_THRESHOLD (2*DBL_MIN)

#define double_is_none(a) (isnan (a))
#define double_equal(a,b) ((isnan (a) && isnan (b)) || (fabs ((a) - (b)) < DOUBLE_THRESHOLD))

// Compare two double values, for sorting
inline int double_compare (const double *a, const double *b) {
    return (*a < *b) ? -1 : ((*a > *b) ? 1 : 0);
}

// Compare two int values, for sorting
inline int int_compare (const int *a, const int *b) {
    return (*a - *b);
}

#define min3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

// Factorial for small numbers (tested <= 65)
// Will return SIZE_MAX if overflows
size_t factorial (size_t n);


#ifdef __cplusplus
}
#endif

#endif
