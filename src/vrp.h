/*  =========================================================================
    vrp - generic VRP model

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __VRP_H_INCLUDED__
#define __VRP_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _vrp_t vrp_t;

// Create a new vrp object
vrp_t *vrp_new (void);

// Create VRP model from file
vrp_t *vrp_new_from_file (const char *filename);

// Destroy vrp object
void vrp_free (vrp_t **self_p);

// ---------------------------------------------------------------------------
// Roadgraph
// ---------------------------------------------------------------------------

// Set coordinate system
void vrp_set_coord_sys (vrp_t *self, coord2d_sys_t coord_sys);

// Add a new node
size_t vrp_add_node (vrp_t *self, const char *ext_id);

// Set node as depot (set once for one node)
void vrp_set_node_depot (vrp_t *self, size_t node_id);

// Set node as customer (set once for one node)
void vrp_set_node_customer (vrp_t *self, size_t node_id);

// Check if node is depot
bool vrp_node_is_depot (vrp_t *self, size_t node_id);

// Check if node is customer
bool vrp_node_is_customer (vrp_t *self, size_t node_id);

// Query node by external id.
// Return node id if node exists, and ID_NONE if not.
size_t vrp_query_node (vrp_t *self, const char *node_ext_id);

// Check if node exists by id
bool vrp_node_exists (vrp_t *self, size_t node_id);

// Set node coordinate
void vrp_set_node_coord (vrp_t *self, size_t node_id, coord2d_t coord);

// Get node coordinate
const coord2d_t *vrp_node_coord (vrp_t *self, size_t node_id);

// Add nodes
// void vrp_add_nodes (vrp_t *self, node_t **nodes, size_t num_nodes);

// Get number of nodes
size_t vrp_num_nodes (vrp_t *self);

// Get number of depots
size_t vrp_num_depots (vrp_t *self);

// Get number of customers
size_t vrp_num_customers (vrp_t *self);

// Get id array of nodes
size_t *vrp_node_ids (vrp_t *self);

// Get id array of depots
size_t *vrp_depot_ids (vrp_t *self);

// Get id array of customers
size_t *vrp_customer_ids (vrp_t *self);

// Set arc distances
void vrp_set_distance (vrp_t *self,
                       size_t from_node_id,
                       size_t to_node_id,
                       double distance);

// Automatically generate straight arc distances
int vrp_auto_set_distances (vrp_t *self);

// Get distance between two nodes
double vrp_distance (vrp_t *self, size_t from_node_id, size_t to_node_id);

// Set arc duration
void vrp_set_duration (vrp_t *self,
                       size_t from_node_id,
                       size_t to_node_id,
                       double duration);

// Automatically generate arc durations by distance and speed
int vrp_auto_set_durations (vrp_t *self, double speed);

// Get duration between two nodes
double vrp_duration (vrp_t *self, size_t from_node_id, size_t to_node_id);

// ---------------------------------------------------------------------------
// Fleet
// ---------------------------------------------------------------------------

size_t vrp_add_vehicle (vrp_t *self, const char *vehicle_ext_id);

size_t vrp_num_vehicles (vrp_t *self);

const char *vrp_vehicle_ext_id (vrp_t *self, size_t vehicle_id);

double vrp_vehicle_max_capacity (vrp_t *self, size_t vehicle_id);

void vrp_set_vehicle_max_capacity (vrp_t *self, size_t vehicle_id, double max_capacity);

double vrp_vehicle_capacity (vrp_t *self, size_t vehicle_id);

void vrp_reset_vehicle_capacity (vrp_t *self, size_t vehicle_id);

void vrp_reset_all_vehicles_capacities (vrp_t *self);

double vrp_vehicle_load (vrp_t *self, size_t vehicle_id);

void vrp_vehicle_do_pickup (vrp_t *self, size_t vehicle_id, double quantity);

void vrp_vehicle_do_delivery (vrp_t *self, size_t vehicle_id, double quantity);

size_t vrp_vehicle_start_node_id (vrp_t *self, size_t vehicle_id);

void vrp_set_vehicle_start_node_id (vrp_t *self, size_t vehilce_id, size_t node_id);

size_t vrp_vehicle_end_node_id (vrp_t *self, size_t vehicle_id);

void vrp_set_vehicle_end_node_id (vrp_t *self, size_t vehicle_id, size_t node_id);

size_t vrp_vehicle_route_id (vrp_t *self, size_t vehicle_id);

void vrp_attach_route_to_vehicle (vrp_t *self, size_t vehicle_id, size_t route_id);

void vrp_detach_route_from_vehicle (vrp_t *self, size_t vehicle_id);

// ---------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------

// Add a new request.
// Return request id.
size_t vrp_add_request (vrp_t *self, const char *request_ext_id);

size_t vrp_query_request (vrp_t *self, const char *request_ext_id);

// Set request task.
// For visiting with zero quantity, set any one of pickup_node_id or
// delivery_node_id (the other one to ID_NONE), and quantity to 0.
void vrp_set_request_task (vrp_t *self,
                           size_t request_id,
                           size_t pickup_node_id,
                           size_t delivery_node_id,
                           double quantity);

void vrp_set_request_pickup_time (vrp_t *self,
                                  size_t request_id,
                                  time_t pickup_earliest,
                                  time_t pickup_latest,
                                  size_t pickup_duration);

void vrp_set_request_delivery_time (vrp_t *self,
                                    size_t request_id,
                                    time_t delivery_earliest,
                                    time_t delivery_latest,
                                    size_t delivery_duration);




// ---------------------------------------------------------------------------
// Sub-model deciding
// ---------------------------------------------------------------------------



// Self test
void vrp_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
