/*  =========================================================================
    tsp - TSP model derived from generic model

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __TSP_H_INCLUDED__
#define __TSP_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _tsp_t tsp_t;

// Create a new tsp object
tsp_t *tsp_new_from (vrp_t *vrp);

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
