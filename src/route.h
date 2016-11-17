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

typedef struct _route_t route_t;

// Create new route object with size
route_t *route_new (size_t size);

// Create route object from node id array
route_t *route_new_from_array (const size_t *node_ids, size_t num_nodes);

// Create route object from node id list
route_t *route_new_from_list (const listu_t *node_ids);

// Destroy route object
void route_free (route_t **self_p);

// Get size of route (number of nodes in it)
size_t route_size (route_t *self);

// Set node at index of route
void route_set_at (route_t *self, size_t index, size_t node_id);

// Get node id at index of route
size_t route_at (route_t *self, size_t index);

// Dump route as array of node ids (read only).
const size_t *route_node_array (route_t *self);

// Duplicator
route_t *route_dup (const route_t *self);

// Printer
void route_print (route_t *self);

// Append node to route tail
void route_append_node (route_t *self, size_t node_id);

// Find node_id in route
size_t route_find (route_t *self, size_t node_id);

// Shuffle route slice [index_begin, index_end] in place.
// Set rng to use your random number generator, or NULL to use an inner one.
void route_shuffle_slice (route_t *self,
                          size_t index_begin, size_t index_end, rng_t *rng);


// Swap two nodes in route
void route_swap (route_t *self, size_t index1, size_t index2);

// OX: ordered crossover.
// r1 and r2 are replaced with two children.
void route_ox (route_t *r1, route_t *r2,
               size_t idx_begin, size_t idx_end,
               rng_t *rng);


// double route_2_opt ();


// void route_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
