/*  =========================================================================
    vrp - generic VRP model

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __VRP_H_INCLUDED__
#define __VRP_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    NR_NONE,
    NR_DEPOT,
    NR_CUSTOMER
} node_role_t;

typedef struct _vrp_t vrp_t;

// ---------------------------------------------------------------------------
// Constructor and destructor
// ---------------------------------------------------------------------------

// Create a new vrp object
vrp_t *vrp_new (void);

// Create VRP model from file
vrp_t *vrp_new_from_file (const char *filename);

// Destroy vrp object
void vrp_free (vrp_t **self_p);

// ---------------------------------------------------------------------------
// Roadgraph: nodes and arcs
// ---------------------------------------------------------------------------

// Set coordinate system
void vrp_set_coord_sys (vrp_t *self, coord2d_sys_t coord_sys);

// Get coordinate system
coord2d_sys_t vrp_coord_sys (vrp_t *self);

// Add a new node
size_t vrp_add_node (vrp_t *self, const char *ext_id, node_role_t role);

// Check if node is depot
bool vrp_node_is_depot (vrp_t *self, size_t node_id);

// Check if node is customer
bool vrp_node_is_customer (vrp_t *self, size_t node_id);

// Query node by external id.
// Return node id if node exists, ID_NONE if not.
size_t vrp_query_node (vrp_t *self, const char *node_ext_id);

// Check if node exists by id
bool vrp_node_exists (vrp_t *self, size_t node_id);

// Set node coordinate
void vrp_set_node_coord (vrp_t *self, size_t node_id, coord2d_t coord);

// Get node coordinate
coord2d_t vrp_node_coord (vrp_t *self, size_t node_id);

// Get number of nodes
size_t vrp_num_nodes (vrp_t *self);

// Get number of depots
size_t vrp_num_depots (vrp_t *self);

// Get number of customers
size_t vrp_num_customers (vrp_t *self);

// Get array of node ids
size_t *vrp_node_ids (vrp_t *self);

// Get id array of depots
size_t *vrp_depot_ids (vrp_t *self);

// Get id array of customers
size_t *vrp_customer_ids (vrp_t *self);

// Set distances between nodes
void vrp_set_distance (vrp_t *self,
                       size_t from_node_id,
                       size_t to_node_id,
                       double distance);

// Automatically generate straight arc distances accroding to coordinates
int vrp_generate_beeline_distances (vrp_t *self);

// Get distance between two nodes
double vrp_distance (vrp_t *self, size_t from_node_id, size_t to_node_id);

// Set arc duration (number of time units)
void vrp_set_duration (vrp_t *self,
                       size_t from_node_id,
                       size_t to_node_id,
                       size_t duration);

// Automatically generate arc durations by distance and speed
int vrp_generate_durations (vrp_t *self, double speed);

// Get duration between two nodes
size_t vrp_duration (vrp_t *self, size_t from_node_id, size_t to_node_id);

// ---------------------------------------------------------------------------
// Fleet
// ---------------------------------------------------------------------------

// Add a vehicle to fleet
// Return vehicle ID
size_t vrp_add_vehicle (vrp_t *self, const char *vehicle_ext_id);

// Get number of vehicles in fleet
size_t vrp_num_vehicles (vrp_t *self);

// Get vehicle ID array
size_t *vrp_vehicle_ids (vrp_t *self);

// Get vehicle's external ID
const char *vrp_vehicle_ext_id (vrp_t *self, size_t vehicle_id);

// Get vehicle's maximal capacity
double vrp_vehicle_max_capacity (vrp_t *self, size_t vehicle_id);

// Set vehicle's maximal capacity
void vrp_set_vehicle_max_capacity (vrp_t *self, size_t vehicle_id, double max_capacity);

// Get vehicle's current capacity
double vrp_vehicle_capacity (vrp_t *self, size_t vehicle_id);

// Reset vehicle's capacity to maximum
void vrp_reset_vehicle_capacity (vrp_t *self, size_t vehicle_id);

// Reset all vehicles' capacities
void vrp_reset_all_vehicles_capacities (vrp_t *self);

// Get vehicle's current load
double vrp_vehicle_load (vrp_t *self, size_t vehicle_id);

// Vehicle pickups some goods
void vrp_vehicle_do_pickup (vrp_t *self, size_t vehicle_id, double quantity);

// Vehicle deliverys some goods
void vrp_vehicle_do_delivery (vrp_t *self, size_t vehicle_id, double quantity);

// Get vehicle's start node ID
size_t vrp_vehicle_start_node_id (vrp_t *self, size_t vehicle_id);

// Set vehicle's start node ID
void vrp_set_vehicle_start_node_id (vrp_t *self, size_t vehilce_id, size_t node_id);

// Get vehicle's end node ID
size_t vrp_vehicle_end_node_id (vrp_t *self, size_t vehicle_id);

// Set vehicle's end node ID
void vrp_set_vehicle_end_node_id (vrp_t *self, size_t vehicle_id, size_t node_id);

// Get vehicle's route ID
size_t vrp_vehicle_route_id (vrp_t *self, size_t vehicle_id);

// Attach a route to vehicle
void vrp_attach_route_to_vehicle (vrp_t *self, size_t vehicle_id, size_t route_id);

// Detach route from vehicle
void vrp_detach_route_from_vehicle (vrp_t *self, size_t vehicle_id);

// ----------------------------------------------------------------------------
// Requests
// ----------------------------------------------------------------------------

// Add a new request by setting a task.
// Return request ID.
// For node-visiting task with some quantity (could be 0), set either
// pickup_node_id or delivery_node_id, and the other one to ID_NONE.
size_t vrp_add_request (vrp_t *self,
                        const char *request_ext_id,
                        size_t pickup_node_id,
                        size_t delivery_node_id,
                        double quantity);

// Add time window for request.
// Multiple time windows can be added by calling this one by one.
// time is represented by an non-negative int number which means number of time
// units since a reference point.
void vrp_add_time_window_for_request (vrp_t *self,
                                      size_t requset_id,
                                      bool is_pickup,
                                      size_t earliest,
                                      size_t latest);

// Set service duration for request.
void vrp_set_service_duration_for_request (vrp_t *self,
                                           size_t request_id,
                                           bool is_pickup,
                                           size_t service_duration);

// Get number of pickup TWs
size_t vrp_num_pickup_time_windows_of_request (vrp_t *self, size_t request_id);

// Get earliest from pickup TW
size_t vrp_earliest_pickup_time_of_request (vrp_t *self,
                                            size_t request_id,
                                            size_t tw_idx);
// Get latest from pickup TW
size_t vrp_latest_pickup_time_of_request (vrp_t *self,
                                          size_t request_id,
                                          size_t tw_idx);

// Get number of delivery TWs
size_t vrp_num_delivery_time_windows_of_request (vrp_t *self, size_t request_id);

// Get earliest from delivery TW
size_t vrp_earliest_delivery_time_of_request (vrp_t *self,
                                              size_t request_id,
                                              size_t tw_idx);

// Get latest from delivery TW
size_t vrp_latest_delivery_time_of_request (vrp_t *self,
                                            size_t request_id,
                                            size_t tw_idx);

// Get pickup service duration
size_t vrp_pickup_duration_of_request (vrp_t *self, size_t request_id);

// Get delivery service duration
size_t vrp_delivery_duration_of_request (vrp_t *self, size_t request_id);

// Query a request by external ID.
// Return request ID if request exists, ID_NONE if not.
size_t vrp_query_request (vrp_t *self, const char *request_ext_id);

// Get number of requests
size_t vrp_num_requests (vrp_t* self);

// ---------------------------------------------------------------------------
// Sub-model verifications
// ---------------------------------------------------------------------------

// Current state of fleet is homogeneous
bool vrp_fleet_is_homogeneous (vrp_t *self);

// CVRP verification
bool vrp_is_cvrp (vrp_t *self);

// TSP verification
bool vrp_is_tsp (vrp_t *self);


// Self test
void vrp_test (bool verbose);


#ifdef __cplusplus
}
#endif

#endif
