/*  =========================================================================
    liber - project public header

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __ER_H_INCLUDED__
#define __ER_H_INCLUDED__

// External dependencies
#include <czmq.h>

// version macros for compile-time API detection
#define ER_VERSION_MAJOR 0
#define ER_VERSION_MINOR 1
#define ER_VERSION_PATCH 0

#define ER_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define ER_VERSION \
    ER_MAKE_VERSION(ER_VERSION_MAJOR, \
    ER_VERSION_MINOR, \
    ER_VERSION_PATCH)

// Public class structures
typedef struct _arrayset_t arrayset_t;
typedef struct _hash_t hash_t;
typedef struct _listu_t listu_t;
typedef struct _listx_t listx_t;
typedef struct _matrixd_t matrixd_t;
typedef struct _matrixu_t matrixu_t;
typedef struct _queue_t queue_t;
typedef struct _rng_t rng_t;
typedef struct _timer_t timer_t;
typedef struct _evol_t evol_t;

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

typedef listu_t route_t;
typedef struct _solution_t solution_t;

typedef enum {
    NR_NONE,
    NR_SENDER,
    NR_RECEIVER
} node_role_t;

typedef double (*arc_distance_t) (const void *model,
                                  size_t from_node_id,
                                  size_t to_node_id);

typedef size_t (*arc_duration_t) (const void *model,
                                  size_t from_node_id,
                                  size_t to_node_id);

typedef struct _vrp_t vrp_t;


// Public API headers
#include "types.h"
#include "numeric_ext.h"
#include "string_ext.h"
#include "date_ext.h"
#include "arrayi.h"
#include "arrayu.h"
#include "arrayset.h"
#include "hash.h"
#include "listu.h"
#include "listx.h"
#include "matrixd.h"
#include "matrixu.h"
#include "queue.h"
#include "rng.h"
#include "timer.h"
#include "evol.h"
#include "coord2d.h"
#include "route.h"
#include "solution.h"
#include "vrp.h"

#endif
