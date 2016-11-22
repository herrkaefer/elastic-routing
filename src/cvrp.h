/*  =========================================================================
    cvrp - classic CVRP model derived from generic model

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __CVRP_H_INCLUDED__
#define __CVRP_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// Create CVRP model from generic VRP model
void cvrp_new_from (vrp_t *vrp);

// Destroy cvrp object
void cvrp_free (cvrp_t **self_p);

// Solve
solution_t *cvrp_solve (cvrp_t *self);

// Self test
void cvrp_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
