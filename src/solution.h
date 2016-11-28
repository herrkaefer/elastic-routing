/*  =========================================================================
    solution - generic VRP solution representation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __SOLUTION_H_INCLUDED__
#define __SOLUTION_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// Constructor
solution_t *solution_new (vrp_t *vrp);

// Destructor
void solution_free (solution_t **self_p);

// Append a route to solution. The route is then taken over ownership by solution.
void solution_prepend_route (solution_t *self, route_t *route);

// Prepend a route to solution. The route is then taken over ownership by solution.
void solution_append_route (solution_t *self, route_t *route);

// Prepend a route from node ids array
void solution_prepend_route_from_array (solution_t *self,
                                        const size_t *node_ids,
                                        size_t num_nodes);

// Append a route from node ids array
void solution_append_route_from_array (solution_t *self,
                                       const size_t *node_ids,
                                       size_t num_nodes);

// Get number of routes in solution
size_t solution_num_routes (const solution_t *self);

// Get route in solution
route_t *solution_route (const solution_t *self, size_t route_idx);

// Set total distance
void solution_set_total_distance (solution_t *self, double distance);

// Calculate total distance explicitly
double solution_cal_set_total_distance (solution_t *self, vrp_t *vrp);

// Get total distance
double solution_total_distance (const solution_t *self);

// Duplicator
solution_t *solution_dup (const solution_t *self);

// Printer
void solution_print (const solution_t *self);

// Printer (display internal node ID)
void solution_print_internal (const solution_t *self);

// Self test
void solution_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
