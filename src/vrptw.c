/*  =========================================================================
    vrptw - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


#define SMALL_NUM_NODES 10


// Private node representation
typedef struct {
    size_t id; // node ID in roadgraph of generic model
    double demand;
    const coord2d_t *coord; // reference of node coords in roadgraph
    const listu_t *time_windows; // reference of time windows in request
} s_node_t;


struct _vrptw_t {
    vrp_t *vrp; // reference of generic model
    double capacity;
    size_t num_vehicles;
    size_t num_customers;
    s_node_t *nodes; // indices: depot: 0; customers: 1, 2, ..., num_customers
    rng_t *rng;
};


// ----------------------------------------------------------------------------

vrptw_t *vrptw_new_from (vrp_t *vrp) {
    assert (vrp);

    cvrp_t *self = (cvrp_t *) malloc (sizeof (cvrp_t));
    assert (self);

    self->vrp = vrp;

    self->num_vehicles = vrp_num_vehicles (vrp);

    size_t one_vehicle = listu_get (vrp_vehicles (vrp), 0);
    self->capacity = vrp_vehicle_max_capacity (vrp, one_vehicle);

    const listu_t *requests = vrp_pending_request_ids (vrp);
    size_t num_requests = listu_size (requests);
    assert (num_requests > 0);

    // Note that number of all customers on roadgraph is used to allocate
    // self->nodes, which may be larger than need.
    self->nodes =
        (s_node_t *) malloc (sizeof (s_node_t) * (vrp_num_customers (vrp) + 1));
    assert (self->nodes);
    self->nodes[0].id = ID_NONE;
    self->num_customers = num_requests;

    for (size_t idx = 0; idx < num_requests; idx++) {
        size_t request = listu_get (requests, idx);

        // printf ("request p: %zu, d: %zu\n",
        //         vrp_request_pickup_node (vrp, request),
        //         vrp_request_delivery_node (vrp, request));

        // depot
        if (self->nodes[0].id == ID_NONE) {
            self->nodes[0].id = vrp_request_sender (vrp, request);
            // printf ("depot set to: %zu\n", self->nodes[0].id);
            self->nodes[0].demand = 0;
            self->nodes[0].coord = vrp_node_coord (vrp, self->nodes[0].id);
        }
        assert (self->nodes[0].id == vrp_request_sender (vrp, request));

        // customer
        self->nodes[idx+1].id = vrp_request_receiver (vrp, request);
        self->nodes[idx+1].demand = vrp_request_quantity (vrp, request);
        self->nodes[idx+1].coord =
            vrp_node_coord (vrp, self->nodes[idx+1].id);
        // printf ("customer added: %zu\n", self->nodes[idx+1].id);
    }

    self->rng = rng_new ();
    return self;
}


void vrptw_free (vrptw_t **self_p) {
    assert (self_p);
    if (*self_p) {
        vrptw_t *self = *self_p;

        free (self);
        *self_p = NULL;
    }
    print_info ("vrptw freed.\n");
}


solution_t *vrptw_solve (vrptw_t *self) {

}


void vrptw_test (bool verbose) {
    print_info ("* vrptw: \n");

    print_info ("OK\n");
}
