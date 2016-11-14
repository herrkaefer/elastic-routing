/*  =========================================================================
    solver - Elastic Routing: Solver

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
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
