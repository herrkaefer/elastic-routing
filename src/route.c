/*  =========================================================================
    route - route sequence

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"


struct _route_t {
    list4u_t *seq; // sequence of node ids
    size_t vehicle_id; // vehicle assigned to
};


// Create a new route object
route_t *route_new () {
    route_t *self = (route_t *) malloc (sizeof (route_t));
    assert (self);

    // ...

    print_info ("route created.\n");
    return self;
}


// Destroy route object
void route_free (route_t **self_p) {
    assert (self_p);
    if (*self_p) {
        route_t *self = *self_p;

        // free properties here

        free (self);
        *self_p = NULL;
    }
    print_info ("route freed.\n");
}


void route_print (route_t *self) {
    printf ("route: length: %zu\n", list4u_size (self->seq));

}


void route_test (bool verbose) {
    print_info (" * route: \n");
    // roadnet_t *roadnet = roadnet_new ();
    // // ...
    // roadnet_free (&roadnet);
    print_info ("OK\n");
}
