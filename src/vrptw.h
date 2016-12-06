/*  =========================================================================
    vrptw - VRPTW model derived from generic model

    VRPTW model:
        static classic CVRP with time window constraints.

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __VRPTW_H_INCLUDED__
#define __VRPTW_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// Create VRPTW model from generic VRP model.
// Note that the vrp model is already validated as an intrinsic VRPTW model.
vrptw_t *vrptw_new_from_generic (vrp_t *vrp);

// Destructor
void vrptw_free (vrptw_t **self_p);

// Solve
solution_t *vrptw_solve (vrptw_t *self);

// Self test
void vrptw_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
