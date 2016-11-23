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

// Create solution object
solution_t *solution_new (vrp_t *vrp);

// Destroy solution object
void solution_free (solution_t **self_p);

// Add a route to solution. The route is then taken over ownership by solution.
void solution_add_route (solution_t *self, route_t *route);

// Add a route from node ids array
void solution_add_route_from_array (solution_t *self,
                                    const size_t *node_ids,
                                    size_t num_nodes);

// Get number of routes in solution
size_t solution_num_routes (solution_t *self);

// Get route in solution
route_t *solution_route (solution_t *self, size_t route_idx);

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
