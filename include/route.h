/*  =========================================================================
    route - node sequence

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __ROUTE_H_INCLUDED__
#define __ROUTE_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif


// Constructor
route_t *route_new (size_t alloc_size);

// Constructor: create route object from node id array
route_t *route_new_from_array (const size_t *node_ids, size_t num_nodes);

// Constructor: create route object from node id list
route_t *route_new_from_list (const listu_t *node_ids);

// Constructor: create route with values [start, start+step, ..., (end)]
route_t *route_new_range (size_t start, size_t stop, int step);

// Destructor
void route_free (route_t **self_p);

// Duplicator
route_t *route_dup (const route_t *self);

// Matcher
bool route_equal (const route_t *self, const route_t *route);

// Printer
void route_print (const route_t *self);

// Self test
void route_test (bool verbose);

// Get size of route (number of nodes in it)
size_t route_size (const route_t *self);

// Set node at index of route
void route_set_at (route_t *self, size_t idx, size_t node_id);

// Get node id at index of route
size_t route_at (const route_t *self, size_t idx);

// Append node to route tail
void route_append_node (route_t *self, size_t node_id);

// Get array of node ids (read only).
const size_t *route_node_array (const route_t *self);

// Find node_id in route
size_t route_find (const route_t *self, size_t node_id);

// Swap two nodes in route
void route_swap_nodes (route_t *self, size_t idx1, size_t idx2);

// Route total distance
double route_total_distance (const route_t *self,
                             const void *context,
                             vrp_arc_distance_t dist_fn);

// Shuffle route slice [idx_begin, idx_end] in place.
// Set rng to use your random number generator, or NULL to use an inner one.
void route_shuffle (route_t *self,
                    size_t idx_begin, size_t idx_end, rng_t *rng);

// Cyclic shift of nodes.
// num > 0: right rotation; num < 0: left rotation.
void route_rotate (route_t *self, int num);

// Distance increment of route reverse operation.
// Note that flip operation is not actually performed.
// Require: i <= j
// context is used by dist_fn for computing distance between nodes (same for
// other functions) in this module.
double route_reverse_delta_distance (const route_t *self,
                                     size_t i, size_t j,
                                     const void *context,
                                     vrp_arc_distance_t dist_fn);

// Reverse route slice [i, j].
// (..., i, i+1, -->, j, ...) =>
// (..., j, j-1, -->, i, ...)
// Require: i <= j
void route_reverse (route_t *self, size_t i, size_t j);

// Distance increment of swapping slices operation.
// Note that operation is not actually performed.
double route_swap_slices_delata_distance (const route_t *self,
                                          size_t i, size_t j,
                                          size_t u, size_t v,
                                          const void *context,
                                          vrp_arc_distance_t dist_fn);

// Swap two nonoverlapping route segments (direction unchanged), i.e.
// (..., i --> j, ..., u --> v, ...) =>
// (..., u --> v, ..., i --> j, ...)
// Require: i <= j < u <=v
void route_swap_slices (route_t *self, size_t i, size_t j, size_t u, size_t v);

// Distance increment of removing node at specific index.
// Note that node removal is not actually performed.
double route_remove_node_delta_distance (route_t *self,
                                         size_t idx,
                                         const void *context,
                                         vrp_arc_distance_t dist_fn);

// Remove node at specific index.
// Return increment of total distance.
void route_remove_node (route_t *self, size_t idx);

// Distance increment of removing two consecutive nodes at first_idx and
// (first_idx+1).
// Note that removal is not actually performed.
double route_remove_link_delta_distance (route_t *self,
                                         size_t first_idx,
                                         const void *context,
                                         vrp_arc_distance_t dist_fn);

// Remove two consecutive nodes at first_idx and (first_idx+1).
// Return increment of total distance.
void route_remove_link (route_t *self, size_t first_idx);

// Distance increment of inserting node at specific index (before the original
// one at index). If idx == size, append the node at tail.
// Note that insertion is not actually performed.
double route_insert_node_delta_distance (const route_t *self,
                                         size_t idx, size_t node_id,
                                         const void *context,
                                         vrp_arc_distance_t dist_fn);

// Insert node before the one at index.
// If idx == size, append the node at tail.
void route_insert_node (route_t *self, size_t idx, size_t node_id);

// Distance increment of exchanging two nodes of two different routes.
// Note that exchange operation is not actually performed.
double route_exchange_nodes_delta_distance (const route_t *self,
                                            const route_t *route,
                                            size_t idx1, size_t idx2,
                                            const void *context,
                                            vrp_arc_distance_t dist_fn);

// Exchange two nodes of two different routes.
void route_exchange_nodes (route_t *self, route_t *route,
                           size_t idx1, size_t idx2);

// Distance increment of 2-opt* operation between two routes.
// Note that operation is not actually performed.
double route_exchange_tails_delta_distance (const route_t *self,
                                            const route_t *route,
                                            size_t idx1, size_t idx2,
                                            const void *context,
                                            vrp_arc_distance_t dist_fn);

// 2-opt* operation of two routes. i.e. swap tails of routes
// (+++, idx1, ***) ==> (+++, idx1, ...)
// (---, idx2, ...) ==> (---, idx2, ***)
void route_exchange_tails (route_t *self, route_t *route,
                           size_t idx1, size_t idx2);

// OX: ordered crossover of two routes.
// Crossover is performed on common slice [idx_begin, idx_end].
// r1 and r2 are replaced with two children respectively.
void route_ox (route_t *r1, route_t *r2,
               size_t idx_begin, size_t idx_end, rng_t *rng);

#ifdef __cplusplus
}
#endif

#endif
