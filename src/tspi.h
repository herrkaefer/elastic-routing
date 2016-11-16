/*  =========================================================================
    tspi - TSP model solved with evol (independent of generic model)

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __TSPI_H_INCLUDED__
#define __TSPI_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _tspi_t tspi_t;

// typedef double (*tspi_cost_t)(size_t node1, size_t node2);

// Create a new tspi object.
// Generate nodes with IDs: 0 ~ num_nodes-1
tspi_t *tspi_new (size_t num_nodes);

// Destroy tspi object
void tspi_free (tspi_t **self_p);

// Set arc cost from node with ID node_id1 to node with ID node_id2.
// Set arc cost one by one, or generate them using
// tspi_generate_beeline_distances_as_costs() if nodes' coordinates are set.
void tspi_set_cost (tspi_t *self, size_t node_id1, size_t node_id2, double cost);

// Set coordinate system (required if coordinates of nodes are set)
void tspi_set_coord_system (tspi_t *self, coord2d_sys_t coord_sys);

// Set node's 2D coordinates (optional)
void tspi_set_node_coord (tspi_t *self, size_t node_id, coord2d_t coord);

// Generate straight distances for arcs (nodes' coordinates should be set)
void tspi_generate_beeline_distances_as_costs (tspi_t *self);

// Set start node (optional)
void tspi_set_start_node (tspi_t *self, size_t start_node_id);

// Set end node (optional)
void tspi_set_end_node (tspi_t *self, size_t end_node_id);

// Set round trip or one-way trip.
// Note that setting of start and end node are both optional, for either round
// trip or one-way trip. You can specify none, one, or both of them.
// But, if start and end nodes are both set, for round trip, they must be same;
// For one-way trip, they must be different.
void tspi_set_round_trip (tspi_t *self, bool is_round_trip);

// Solve.
// Return the optimized route (sequence of node ID)
solution_t *tspi_solve (tspi_t *self);

// Self test
void tspi_test (bool verbose);


#ifdef __cplusplus
}
#endif

#endif
