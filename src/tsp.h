/*  =========================================================================
    tsp - classic TSP model derived from generic model

    TSP model:
        Mimimizing total distance of one route (a number of visiting reqeuests
        accomplished by one vehicle).

        No sequential, time window, or other constraints.

        The start or end node may be specified or not (open), and they may be
        same (round trip) or not (one-way trip).

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __TSP_H_INCLUDED__
#define __TSP_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// Create TSP model from generic VRP model.
// Note that the VRP model is already validated as an intrinsic TSP model, so
// here we do not validate the attributes again.
tsp_t *tsp_new_from_generic (vrp_t *vrp);

// Destroy tsp object
void tsp_free (tsp_t **self_p);

// Solve
solution_t *tsp_solve (tsp_t *self);

// Self test
void tsp_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
