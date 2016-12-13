/*  =========================================================================
    sol - solution representation for CVRP, VRPTW like model

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __SOL_H_INCLUDED__
#define __SOL_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// Create VRPTW model from generic VRP model.
// Note that the vrp model is already validated as an intrinsic VRPTW model.
sol_t *sol_new (sol_t *vrptw);

// Destructor
void sol_free (sol_t **self_p);


#ifdef __cplusplus
}
#endif

#endif
