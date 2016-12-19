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

// Node iterator
typedef struct {
    size_t idx_route; // index of current route
    route_t *route; // current route
    size_t idx_node; // index of current node on current route
    size_t node_id; // ID of current node
} solution_iterator_t;

// Constructor
solution_t *solution_new ();

// Destructor
void solution_free (solution_t **self_p);

// Append a route to solution.
// The route is then taken over ownership by solution.
void solution_prepend_route (solution_t *self, route_t *route);

// Prepend a route to solution.
// The route is then taken over ownership by solution.
void solution_append_route (solution_t *self, route_t *route);

// Prepend a route from node ids array
void solution_prepend_route_from_array (solution_t *self,
                                        const size_t *node_ids,
                                        size_t num_nodes);

// Append a route from node ids array
void solution_append_route_from_array (solution_t *self,
                                       const size_t *node_ids,
                                       size_t num_nodes);

// Remove route at index
void solution_remove_route (solution_t *self, size_t idx);

// Get number of routes in solution
size_t solution_num_routes (const solution_t *self);

// Get route in solution
route_t *solution_route (const solution_t *self, size_t route_idx);

// Set total distance
void solution_set_total_distance (solution_t *self, double distance);

// Calculate and set total distance
double solution_cal_set_total_distance (solution_t *self,
                                        const void *context,
                                        vrp_arc_distance_t dist_fn);

// Calculate total distance (do not set)
double solution_cal_total_distance (const solution_t *self,
                                    const void *context,
                                    vrp_arc_distance_t dist_fn);

// Increase total distance by given value (for development)
void solution_increase_total_distance (solution_t *self, double delta_distance);

// Get total distance
double solution_total_distance (const solution_t *self);

// Duplicator
solution_t *solution_dup (const solution_t *self);

// Printer
void solution_print (const solution_t *self);

// Printer (display external node ID)
void solution_print_external (const solution_t *self,
                              const void *context,
                              vrp_arc_distance_t dist_fn);

// Create an iterator
solution_iterator_t solution_iter_init (const solution_t *self);

// Get next route by iterator
// void *solution_iter_route (const solution_t *self, solution_iterator_t *iter);

// Iterate to next node
size_t solution_iter_node (const solution_t *self, solution_iterator_t *iter);

// Self test
void solution_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
