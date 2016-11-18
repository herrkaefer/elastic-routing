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

// Coordinate systems
typedef enum {
    CS_NONE,
    CS_CARTESIAN2D,
    CS_POLAR2D,
    CS_WGS84, // World Geodetic System 1984. See:
              // https://en.wikipedia.org/wiki/World_Geodetic_System
    CS_GCJ02 // Mars Coordinates. See:
       // https://en.wikipedia.org/wiki/Restrictions_on_geographic_data_in_China
} coord2d_sys_t;

// Generic 2D coordinates representation.
// Use this struct to represent any 2D point with the above coordinate system.
// Need to combine with an outside variable to specify its coordinate system.
// For CS_CARTESIAN2D: v1->x, v2->y
// For CS_POLAR2D: v1->radius, v2->theta
// For geodetic system: v1->latitude, v2->longitude
typedef struct {
    double v1; // x, or r,     lat
    double v2; // y, or theta, lng
} coord2d_t;


// Distance between two points
// Two points should be under the same coordinate system
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
