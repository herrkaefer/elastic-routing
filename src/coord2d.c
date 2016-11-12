/*  =========================================================================
    coord2d - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"

#define EARTH_RADIUS 6373.0


static double angle_degree_to_radian (double angle) {
    return angle * PI / 180.0;
}


double coord2d_distance (const coord2d_t *p1,
                         const coord2d_t *p2,
                         coord2d_sys_t coord_sys) {
    assert (p1);
    assert (p2);

    double dx, dy, lat1, lng1, lat2, lng2, dlat, dlng, a, c;

    switch (coord_sys) {
        case CS_CARTESIAN2D:
            dx = p1->v1 - p2->v1;
            dy = p1->v2 - p2->v2;
            return sqrt (dx * dx + dy * dy);

        case CS_POLAR2D:
            a = pow (p1->v1, 2) + pow (p2->v1, 2) -
                2 * p1->v1 * p2->v1 * cos (p1->v2 - p2->v2);
            return sqrt (a);

        case CS_GCJ02:
            print_warning ("for CS_GCJ02 the result may not be correct.\n");

        case CS_WGS84:
            // Haversine formula: (assumes that the Earth is a sphere)
            // See: http://www.movable-type.co.uk/scripts/latlong.html
            // a = sin²(Δφ/2) + cos φ1 ⋅ cos φ2 ⋅ sin²(Δλ/2)
            // c = 2 ⋅ atan2 ( √a, √(1−a) )
            // d = R ⋅ c
            lat1 = angle_degree_to_radian (p1->v1);
            lng1 = angle_degree_to_radian (p1->v2);
            lat2 = angle_degree_to_radian (p2->v1);
            lng2 = angle_degree_to_radian (p2->v2);
            dlat = lat2 - lat1;
            dlng = lng2 - lng1;
            a = pow (sin (dlat/2), 2) +
                cos (lat1) * cos (lat2) * pow (sin (dlng/2), 2);
            c = 2 * atan2 (sqrt (a), sqrt (1-a));
            return EARTH_RADIUS * c;

        default:
            print_error ("coordinate system not supported.\n");
            assert (false);
            return DOUBLE_NONE;
    }
}


coord2d_t coord2d_to_polar (const coord2d_t *point,
                            const coord2d_t *ref,
                            coord2d_sys_t coord_sys) {
    assert (point);
    double dx, dy, x, y, lat1, lng1, lat2, lng2;
    coord2d_t polar;

    switch (coord_sys) {
        case CS_CARTESIAN2D:
            if (ref) {
                dx = point->v1 - ref->v1;
                dy = point->v2 - ref->v2;
            }
            else {
                dx = point->v1;
                dy = point->v2;
            }

            polar.v1 = sqrt (dx * dx + dy * dy);
            if (polar.v1 > 0) {
                polar.v2 = atan2 (dy, dx);
                if (polar.v2 < 0 )
                    polar.v2 += 2*PI;
            }
            else
                polar.v2 = 0;

            return polar;

        case CS_POLAR2D:
            print_warning ("input point is already a polar.\n");
            return *point;

        case CS_GCJ02:
            print_warning ("for CS_GCJ02 the result may not be correct.\n");

        case CS_WGS84:
            if (ref == NULL)
                ref = &((coord2d_t) {0.0, 0.0});

            polar.v1 = coord2d_distance (point, ref, coord_sys);

            // Formula for computing (initial) bearing:
            // The bearing to a point is the angle measured in a clockwise
            // direction from the north line. range: [0, 2*PI)
            // See: http://www.movable-type.co.uk/scripts/latlong.html
            // φ: latitude, λ: longitude
            // θ = atan2 (sin Δλ ⋅ cos φ2,
            //            cos φ1 ⋅ sin φ2 − sin φ1 ⋅ cos φ2 ⋅ cos Δλ )
            lat1 = angle_degree_to_radian (ref->v1);
            lng1 = angle_degree_to_radian (ref->v2);
            lat2 = angle_degree_to_radian (point->v1);
            lng2 = angle_degree_to_radian (point->v2);
            y = sin (lng2 - lng1) * cos (lat2);
            x = cos (lat1) * sin (lat2) -
                sin (lat1) * cos (lat2) * cos (lng2 - lng1);
            polar.v2 = atan2 (y, x);
            if (polar.v2 < 0)
                polar.v2 += 2*PI;

            return polar;

        default:
            print_error ("coordinate system not supported.\n");
            assert (false);
            return *point;
    }
}


void coord2d_set_none (coord2d_t *point) {
    assert (point);
    point->v1 = DOUBLE_NONE;
    point->v2 = DOUBLE_NONE;
}


bool coord2d_is_none (const coord2d_t *point) {
    assert (point);
    return double_is_none (point->v1) || double_is_none (point->v2);
}


int coord2d_compare_polar_angle (const coord2d_t *p1,
                                 const coord2d_t *p2) {
    return (p1->v2 < p2->v2) ?
           -1 :
           ((p1->v2 > p2->v2) ? 1 : 0);
}
