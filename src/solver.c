/*  =========================================================================
    solver - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"


struct _solver_t {
    // properties here
};


solver_t * solver_new () {
    solver_t *self = (solver_t *) malloc (sizeof (solver_t));
    assert (self);

    // Aim to initialize all properties to null/zero/false/empty by default.


    print_info ("solver created.\n");
    return self;
}


void solver_free (solver_t **self_p) {
    assert (self_p);
    if (*self_p) {
        solver_t *self = *self_p;
        // free properties here

        free (self);
        *self_p = NULL;
    }
    print_info ("solver freed.\n");
}


void solver_run (solver_t *self) {
    assert (self);
    print_info ("Solver running ...\n");
    // sleep(1);
    print_info ("Solver stopped.\n");
}


void solver_test (bool verbose) {
    print_info (" * solver: \n");
    solver_t *solver = solver_new ();
    solver_run (solver);
    solver_free (&solver);
    print_info ("OK\n");
}
