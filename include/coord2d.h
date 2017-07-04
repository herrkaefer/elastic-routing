/*  =========================================================================
    coord - 2D coordinate

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __COORD2D_H_INCLUDED__
#define __COORD2D_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// Distance between two points under the same coordinate system
double coord2d_distance (const coord2d_t *p1,
                         const coord2d_t *p2,
                         coord2d_sys_t coord_sys);

// Convert Cartesian coordinates to polar coordinates
// If ref is NULL, origin is used as the reference point
coord2d_t coord2d_to_polar (const coord2d_t *point,
                            const coord2d_t *ref,
                            coord2d_sys_t coord_sys);

// Set point to NONE
void coord2d_set_none (coord2d_t *point);

// Check if a point is NONE
bool coord2d_is_none (const coord2d_t *point);

// compare angles
// Assume that param p1 and p2 are both polar2d
int coord2d_compare_polar_angle (const coord2d_t *p1,
                                 const coord2d_t *p2);

// Generate an array of random 2D cartesian coordinates within a square.
// Caller is responsible for freeing it after use.
coord2d_t *coord2d_random_cartesian_range (double xmin, double xmax,
                                           double ymin, double ymax,
                                           size_t num,
                                           rng_t *rng);

#ifdef __cplusplus
}
#endif

#endif
