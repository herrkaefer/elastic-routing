/*  =========================================================================
    tspi - TSP (independent static model, not related with generic model)

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

void tspi_set_cost (tspi_t *self, size_t node_id1, size_t node_id2, double cost);

void tspi_set_coord_system (tspi_t *self, coord2d_sys_t coord_sys);

void tspi_set_node_coord (tspi_t *self, size_t node_id, coord2d_t coord);

void tspi_generate_straight_distances_as_costs (tspi_t *self);

void tspi_set_start_node (tspi_t *self, size_t start_node_id);

void tspi_set_end_node (tspi_t *self, size_t end_node_id);

void tspi_set_round_trip (tspi_t *self, bool is_round_trip);

// Solve
void tspi_solve (tspi_t *self);

// Self test
void tspi_test (bool verbose);


#ifdef __cplusplus
}
#endif

#endif
