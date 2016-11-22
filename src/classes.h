/*  =========================================================================
    classes - project private header

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __CLASSES_H_INCLUDED__
#define __CLASSES_H_INCLUDED__

// Standard ANSI include files. Not necessary if czmq.h is included.
// #include <ctype.h>
// #include <limits.h>
// #include <stdarg.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <stddef.h>
// #include <string.h>
// #include <time.h>
// #include <errno.h>
// #include <float.h>
// #include <math.h>
// #include <signal.h>
// #include <setjmp.h>
// #include <assert.h>
// #include <stdbool.h>


// External APIs
#include "../include/er_library.h"


// Private class structures
typedef listu_t route_t;
typedef struct _solution_t solution_t;

typedef enum {
    NT_NONE,
    NT_DEPOT,
    NT_CUSTOMER
} node_type_t;

typedef struct _vrp_t vrp_t;
typedef struct _cvrp_t cvrp_t;
typedef struct _tsp_t tsp_t;
typedef struct _vrptw_t vrptw_t;
typedef struct _tspi_t tspi_t;

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


// Internal API headers
#include "numeric_ext.h"
#include "string_ext.h"
#include "date_ext.h"
#include "arrayi.h"
#include "arrayu.h"
#include "coord2d.h"
#include "util.h"

#include "route.h"
#include "solution.h"
#include "vrp.h"
#include "tspi.h"
#include "tsp.h"

#endif
