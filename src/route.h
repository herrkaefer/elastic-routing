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
double route_total_distance (const route_t *self, const vrp_t *vrp);

// Shuffle route slice [idx_begin, idx_end] in place.
// Set rng to use your random number generator, or NULL to use an inner one.
void route_shuffle (route_t *self,
                    size_t idx_begin, size_t idx_end, rng_t *rng);

// Cyclic shift of nodes
void route_rotate (route_t *self, int num);

// Reverse (flip) route slice.
// (..., i, i+1, -->, j, ...) =>
// (..., j, j-1, -->, i, ...)
void route_flip (route_t *self, size_t i, size_t j);

// Swap two nonoverlapping route segments (direction unchanged), i.e.
// (..., i --> j, ..., u --> v, ...) =>
// (..., u --> v, ..., i --> j, ...)
// Require: i <= j < u <=v
void route_swap (route_t *self, size_t i, size_t j, size_t u, size_t v);

// OX: ordered crossover.
// Crossover is performed on common slice [idx_begin, idx_end].
// r1 and r2 are replaced with two children respectively.
void route_ox (route_t *r1, route_t *r2,
               size_t idx_begin, size_t idx_end,
               rng_t *rng);

// 2-opt local search on route slice.
// Only route distance is concerned. No constraint (such as time window or
// sequence) is taken into account.
// If exhaustive is set true, exhaustive searching will stop when no improvement
// is possible in neighborhood. If exhaustive is set false, searching will stop
// whenever the first improvement is achieved.
// Return increment of total distance (non-negative).
double route_2_opt (route_t *self,
                    const vrp_t *vrp,
                    size_t idx_begin, size_t idx_end,
                    bool exhaustive);

// Distance increment of removing node at specific index.
// Note that node removal is not actually performed.
double route_remove_node_delta_distance (route_t *self,
                                         const vrp_t *vrp, size_t idx);

// Remove node at specific index.
// Return increment of total distance.
void route_remove_node (route_t *self, size_t idx);

// Distance increment of removing two consecutive nodes at first_idx and
// (first_idx+1).
// Note that removal is not actually performed.
double route_remove_link_delta_distance (route_t *self,
                                         const vrp_t *vrp, size_t first_idx);

// Remove two consecutive nodes at first_idx and (first_idx+1).
// Return increment of total distance.
void route_remove_link (route_t *self, size_t first_idx);

// Distance increment of insert node at specific index (before the original
// one at index). If idx == size, append the node at tail.
// Note that insertion is not actually performed.
double route_insert_node_delta_distance (const route_t *self,
                                         const vrp_t *vrp,
                                         size_t idx, size_t node_id);

// Insert node before the one at index.
// If idx == size, append the node at tail.
void route_insert_node (route_t *self, size_t idx, size_t node_id);

#ifdef __cplusplus
}
#endif

#endif
