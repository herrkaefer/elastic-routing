/*  =========================================================================
    coord - Elastic Routing: Coordinate

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

        case CS_WGS84:
            lat1 = angle_degree_to_radian (p1->v1);
            lng1 = angle_degree_to_radian (p1->v2);
            lat2 = angle_degree_to_radian (p2->v1);
            lng2 = angle_degree_to_radian (p2->v2);
            dlat = lat2 - lat1;
            dlng = lng2 - lng1;
            a = pow (
                  sin (dlat/2), 2) + cos(lat1) * cos (lat2) * pow (sin (dlng/2),
                  2);
            c = 2 * atan2 (sqrt (a), sqrt (1-a));
            return EARTH_RADIUS * c;

        case CS_GCJ02:
            // @toadd: convert to WGS84?
            print_error ("CS_GCJ02 not supported.\n");
            assert (false);
            return -1;

        default:
            print_error ("coordinate system not supported.\n");
            return -1;
    }
}


coord2d_t coord2d_to_polar (const coord2d_t *point,
                            const coord2d_t *ref,
                            coord2d_sys_t coord_sys) {
    assert (point);

    double dx, dy;

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

            coord2d_t polar;
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
            assert (false);
            return *point;

        case CS_WGS84:
        case CS_GCJ02:
            print_error ("coordinate system not supported.\n");
            assert (false);
            return *point;

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


// bool polar2d_is_none (const polar2d_t *polar) {
//     assert (polar);
//     return double_is_none (polar->r) || double_is_none (polar->theta);
// }

