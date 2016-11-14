/*  =========================================================================
    cvrp - Elastic Routing: CVRP

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __CVRP_H_INCLUDED__
#define __CVRP_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _cvrp_t cvrp_t;

// Create a new cvrp object
cvrp_t *cvrp_new (void);

// Set cvrp from a generic model
void cvrp_set_from_generic (cvrp_t *self, const vrp_t *vrp);

// Create a new cvrp object from file
cvrp_t *cvrp_new_from_file (const char *filename);

// Copy assignment
// void cvrp_copy (cvrp_t *self, cvrp_t *dest);

cvrp_t *cvrp_dup (cvrp_t *self);

// Destroy cvrp object
void cvrp_free (cvrp_t **self_p);

// Self test
void cvrp_test (bool verbose);

// Get customer number
size_t cvrp_num_customers (cvrp_t *self);

// Get customer number
size_t cvrp_num_vehicles (cvrp_t *self);

// Get vehicle capacity
double cvrp_capacity (cvrp_t *self);

// Get customer demands
const double *cvrp_demands (cvrp_t *self);

// Get customer demand
double cvrp_demand (cvrp_t *self, size_t node_index);

// Get arc cost
double cvrp_cost (cvrp_t *self, size_t from_node_index, size_t to_node_index);

// Get coordinates
// const coord2d_t *cvrp_coords (cvrp_t *self);

// Get node coordinate
const coord2d_t *cvrp_coord (cvrp_t *self, size_t node);

#ifdef __cplusplus
}
#endif

#endif
