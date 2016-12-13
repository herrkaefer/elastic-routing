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

// ---------------------------------------------------------------------------
// Constructor and destructor
// ---------------------------------------------------------------------------

// Create a new vrp object
vrp_t *vrp_new (void);

// Create VRP model from file
vrp_t *vrp_new_from_file (const char *filename);

// Destroy vrp object
void vrp_free (vrp_t **self_p);

// Self test
void vrp_test (bool verbose);

// ---------------------------------------------------------------------------
// Roadgraph: nodes and arcs
// ---------------------------------------------------------------------------

// Set coordinate system
void vrp_set_coord_sys (vrp_t *self, coord2d_sys_t coord_sys);

// Add a new node.
// Return node ID (ID_NONE if node already exists).
size_t vrp_add_node (vrp_t *self, const char *ext_id);

// Set node coordinate
void vrp_set_node_coord (vrp_t *self, size_t node_id, coord2d_t coord);

// Set arc distance
void vrp_set_arc_distance (vrp_t *self,
                           size_t from_node_id, size_t to_node_id,
                           double distance);

// Set arc duration (number of time units)
void vrp_set_arc_duration (vrp_t *self,
                           size_t from_node_id, size_t to_node_id,
                           size_t duration);

// Generate straight arc distances accroding to coordinates
void vrp_generate_beeline_distances (vrp_t *self);

// Generate arc durations from distance and speed.
// Note that arc distances should already be set or generated.
void vrp_generate_durations (vrp_t *self, double speed);

// Get coordinate system of roadgraph
coord2d_sys_t vrp_coord_sys (vrp_t *self);

// Query node by external id.
// Return node ID if node exists, ID_NONE if not.
size_t vrp_query_node (vrp_t *self, const char *node_ext_id);

// Check if node exists by id
bool vrp_node_exists (vrp_t *self, size_t node_id);

// Get external ID of node
const char *vrp_node_ext_id (const vrp_t *self, size_t node_id);

// Get node coordinate
const coord2d_t *vrp_node_coord (vrp_t *self, size_t node_id);

// Get list of associated request ids
const listu_t *vrp_node_pending_request_ids (vrp_t *self, size_t node_id);

// Get number of nodes in roadgraph
size_t vrp_num_nodes (vrp_t *self);

// Get ID list of nodes in roadgraph
const listu_t *vrp_nodes (vrp_t *self);

// Get distance between two nodes.
// The caller must ensure that arc distances are already properly set.
double vrp_arc_distance (const vrp_t *self, size_t from_node_id, size_t to_node_id);

// Get duration between two nodes.
// The caller must ensure that arc durations are already properly set.
size_t vrp_arc_duration (const vrp_t *self, size_t from_node_id, size_t to_node_id);

// ---------------------------------------------------------------------------
// Fleet
// ---------------------------------------------------------------------------

// Add a vehicle to fleet.
// Return vehicle ID (ID_NONE if vehicle already exists).
// start_node_id or end_node_id could be ID_NONE.
size_t vrp_add_vehicle (vrp_t *self,
                        const char *vehicle_ext_id,
                        double max_capacity,
                        size_t start_node_id,
                        size_t end_node_id);

// Attach a route to vehicle
void vrp_attach_route_to_vehicle (vrp_t *self, size_t vehicle_id, size_t route_id);

// Detach route from vehicle
void vrp_detach_route_from_vehicle (vrp_t *self, size_t vehicle_id);

// Vehicle pickups some goods
void vrp_vehicle_do_pickup (vrp_t *self, size_t vehicle_id, double quantity);

// Vehicle deliverys some goods
void vrp_vehicle_do_delivery (vrp_t *self, size_t vehicle_id, double quantity);

// Reset vehicle's capacity to maximum
void vrp_reset_vehicle_capacity (vrp_t *self, size_t vehicle_id);

// Reset all vehicles' capacities
void vrp_reset_all_vehicles_capacities (vrp_t *self);

// Get number of vehicles in fleet
size_t vrp_num_vehicles (vrp_t *self);

// Get vehicle ID list
const listu_t *vrp_vehicles (vrp_t *self);

// Get vehicle's external ID
const char *vrp_vehicle_ext_id (vrp_t *self, size_t vehicle_id);

// Get vehicle's maximal capacity
double vrp_vehicle_max_capacity (vrp_t *self, size_t vehicle_id);

// Get vehicle's current capacity
double vrp_vehicle_capacity (vrp_t *self, size_t vehicle_id);

// Get vehicle's current load
double vrp_vehicle_load (vrp_t *self, size_t vehicle_id);

// Get vehicle's start node ID
size_t vrp_vehicle_start_node_id (vrp_t *self, size_t vehicle_id);

// Get vehicle's end node ID
size_t vrp_vehicle_end_node_id (vrp_t *self, size_t vehicle_id);

// Get vehicle's route ID
size_t vrp_vehicle_route_id (vrp_t *self, size_t vehicle_id);

// ----------------------------------------------------------------------------
// Requests
// ----------------------------------------------------------------------------

// Add a new request by setting a task.
// Return request ID.
// For node-visiting task with some quantity (could be 0), set either one of
// sender or receiver, and the other one to ID_NONE.
size_t vrp_add_request (vrp_t *self,
                        const char *request_ext_id,
                        size_t sender,
                        size_t receiver,
                        double quantity);

// Add time window for sender or receiver of request.
// Return 0 for success, -1 for failure.
// Multiple time windows can be added by calling this one by one. But they
// should be non-overlapping time intervals.
// Time is represented by an non-negative int number which means number of time
// units since a common reference point.
int vrp_add_time_window (vrp_t *self,
                         size_t requset_id,
                         node_role_t node_role,
                         size_t earliest,
                         size_t latest);

// Set service duration for sender or receiver of request.
// Default: 0.
void vrp_set_service_duration (vrp_t *self,
                               size_t request_id,
                               node_role_t node_role,
                               size_t service_duration);

// Query a request by external ID.
// Return request ID if request exists, ID_NONE if not.
size_t vrp_query_request (vrp_t *self, const char *request_ext_id);

// Get number of requests
size_t vrp_num_requests (vrp_t* self);

// Get pending requests' IDs
const listu_t *vrp_pending_request_ids (vrp_t *self);

// Get number of senders
size_t vrp_num_senders (vrp_t *self);

// Get number of receivers
size_t vrp_num_receivers (vrp_t *self);

// Get ID list of senders
const listu_t *vrp_senders (vrp_t *self);

// Get ID list of receivers
const listu_t *vrp_receivers (vrp_t *self);

// Get pickup node ID of request
size_t vrp_request_sender (vrp_t *self, size_t request_id);

// Get delivery node ID of request
size_t vrp_request_receiver (vrp_t *self, size_t request_id);

// Get quantity of request
double vrp_request_quantity (vrp_t *self, size_t request_id);

// Get number of time windows of sender or receiver
size_t vrp_num_time_windows (vrp_t *self,
                             size_t request_id,
                             node_role_t node_role);

// Get earliest of time window of sender or receiver
size_t vrp_earliest_of_time_window (vrp_t *self,
                                    size_t request_id,
                                    node_role_t node_role,
                                    size_t tw_idx);

// Get latest of time window of sender or receiver
size_t vrp_latest_of_time_window (vrp_t *self,
                                  size_t request_id,
                                  node_role_t node_role,
                                  size_t tw_idx);

// Get earliest service time of node (sender or receiver) of request.
// i.e. earliest time of all time windows.
size_t vrp_earliest_service_time (vrp_t *self,
                                  size_t request_id,
                                  node_role_t node_role);

// Get latest service time of node (sender or receiver) of request
size_t vrp_latest_service_time (vrp_t *self,
                                size_t request_id,
                                node_role_t node_role);

// Get service duration of sender or receiver.
// i.e. latest time of all time windows.
size_t vrp_service_duration (vrp_t *self,
                             size_t request_id,
                             node_role_t node_role);

// Check if pickup or delivery time windows are equal for two requests
bool vrp_time_windows_are_equal (vrp_t *self,
                                 size_t request_id1, node_role_t node_role1,
                                 size_t request_id2, node_role_t node_role2);

// ---------------------------------------------------------------------------
// Solve
// ---------------------------------------------------------------------------

// Solve
solution_t *vrp_solve (vrp_t *self);


#ifdef __cplusplus
}
#endif

#endif
