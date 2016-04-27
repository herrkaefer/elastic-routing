/*  =========================================================================
    solver - Elastic Routing: Solver

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __SOLVER_H_INCLUDED__
#define __SOLVER_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _solver_t solver_t;

// Create a new solver object
solver_t *solver_new (void);

// Destroy solver object
void solver_free (solver_t **self_p);

// Self test
void solver_test (bool verbose);

// Run solver
void solver_run (solver_t *self);

#ifdef __cplusplus
}
#endif

#endif
