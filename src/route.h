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

// Create new route object with size
route_t *route_new (size_t alloc_size);

// Create route object from node id array
route_t *route_new_from_array (const size_t *node_ids, size_t num_nodes);

// Create route object from node id list
route_t *route_new_from_list (const listu_t *node_ids);

// Destroy route object
void route_free (route_t **self_p);

// Self test
void route_test (bool verbose);

// Get size of route (number of nodes in it)
size_t route_size (route_t *self);

// Set node at index of route
void route_set_at (route_t *self, size_t idx, size_t node_id);

// Get node id at index of route
size_t route_at (route_t *self, size_t idx);

// Append node to route tail
void route_append_node (route_t *self, size_t node_id);

// Get array of node ids (read only).
const size_t *route_node_array (route_t *self);

// Duplicator
route_t *route_dup (const route_t *self);

// Printer
void route_print (route_t *self);

// Find node_id in route
size_t route_find (route_t *self, size_t node_id);

// Swap two nodes in route
void route_swap_nodes (route_t *self, size_t idx1, size_t idx2);

// Route total distance
double route_total_distance (route_t *self, vrp_t *vrp);

// Shuffle route slice [idx_begin, idx_end] in place.
// Set rng to use your random number generator, or NULL to use an inner one.
void route_shuffle (route_t *self,
                    size_t idx_begin, size_t idx_end, rng_t *rng);

// Distance increment of route flip operation.
// Note the the operation is not performed.
// Flip of route slice [i, j]:
// (0, ..., i-1, i, i+1, ..., j-1, j, j+1, ..., route_length-1) =>
// (0, ..., i-1, j, j-1, ..., i+1, i, j+1, ..., route_length-1)
// double route_flip_delta_distance (route_t *self,
//                                   vrp_t *vrp, int idx_begin, int idx_end);

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

// 2-opt local search.
// Only route cost is concerned. No constraint (such as time window or sequence)
// is taken into account.
// Brute force search: stop when no improvement is possible in neighborhood.
// Return cost increment (negative).
double route_2_opt (route_t *self,
                    vrp_t *vrp, size_t idx_begin, size_t idx_end);


#ifdef __cplusplus
}
#endif

#endif
