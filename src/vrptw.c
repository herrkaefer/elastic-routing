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
    // const listu_t *time_windows; // reference of time windows in request
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
// Genome for evolution

typedef struct {
    route_t *gtour; // giant tour: a sequence of customers: 1 ~ N
    solution_t *sol; // splited giant tour
} s_genome_t;


// Create genome by setting at least one of gtour or sol
static s_genome_t *s_genome_new (route_t *gtour, solution_t *sol) {
    assert (gtour != NULL || sol != NULL);
    s_genome_t *self = (s_genome_t *) malloc (sizeof (s_genome_t));
    assert (self);
    self->gtour = gtour;
    self->sol = sol;
    return self;
}


static void s_genome_free (s_genome_t **self_p) {
    assert (self_p);
    if (*self_p) {
        s_genome_t *self = *self_p;
        route_free (&self->gtour);
        solution_free (&self->sol);
        free (self);
        *self_p = NULL;
    }
}


// ----------------------------------------------------------------------------
// Helpers

static double vrptw_arc_distance_by_idx (vrptw_t *self,
                                        size_t idx1, size_t idx2) {
    return vrp_arc_distance (self->vrp,
                             self->nodes[idx1].id, self->nodes[idx2].id);
}


// Get demand of node. i.e. associated request's quantity
static double vrptw_node_demand (vrptw_t *self, size_t node_id) {
    if (node_id == self->nodes[0].id) // depot
        return 0;

    const listu_t *requests = vrp_node_pending_request_ids (self->vrp, node_id);
    assert (requests);
    assert (listu_size (requests) == 1);
    size_t request_id = listu_get (requests, 0);
    assert (node_id == vrp_request_receiver (self->vrp, request_id));
    return vrp_request_quantity (self->vrp, request_id);
}


// Get number of time windows of node
static size_t vrptw_num_time_windows (vrptw_t *self, size_t node_id) {
    const listu_t *requests = vrp_node_pending_request_ids (self->vrp, node_id);
    assert (requests);
    assert (listu_size (requests) == 1);
    size_t request_id = listu_get (requests, 0);
    node_role_t role = (node_id == vrp_request_sender (self->vrp, request_id)) ?
                       NR_SENDER : NR_RECEIVER;
    return vrp_num_time_windows (self->vrp, request_id, role);
}


// Sum of demand of nodes on a route
static double vrptw_route_demand (vrptw_t *self, route_t *route) {
    double demand = 0;
    for (size_t idx = 0; idx < route_size (route); idx++)
        demand += vrptw_node_demand (self, route_at (route, idx));
    return demand;
}


// Sum of node demands on route slice [idx_from, idx_to]
static double vrptw_route_slice_demand (vrptw_t *self, route_t *route,
                                       size_t idx_from, size_t idx_to) {
    assert (idx_from <= idx_to);
    assert (idx_to < route_size (route));
    double demand = 0;
    for (size_t idx = idx_from; idx <= idx_to; idx++)
        demand += vrptw_node_demand (self, route_at (route, idx));
    return demand;
}


// Transform VRPTW solution to giant tour representation
static route_t *vrptw_giant_tour_from_solution (vrptw_t *self, solution_t *sol) {
    route_t *gtour = route_new (self->num_customers);
    for (size_t idx_r = 0; idx_r < solution_num_routes (sol); idx_r++) {
        route_t *route = solution_route (sol, idx_r);
        for (size_t idx = 0; idx < route_size (route); idx++) {
            size_t node = route_at (route, idx);
            if (node != self->nodes[0].id)
                route_append_node (gtour, route_at (route, idx));
        }
    }
    assert (route_size (gtour) == self->num_customers);
    return gtour;
}


// Simple hash function of giant tour which could discriminate permutations
static size_t giant_tour_hash (route_t *gtour) {
    size_t hash = 0;
    for (size_t idx = 0; idx < route_size (gtour); idx++)
        hash += route_at (gtour, idx) * idx;
    return hash;
}


static void vrptw_print_solution (vrptw_t *self, solution_t *sol) {
    printf ("\nVRPTW solution: #routes: %zu, total distance: %.2f (capacity: %.2f)\n",
            solution_num_routes (sol), solution_total_distance (sol), self->capacity);
    printf ("--------------------------------------------------------------------\n");

    for (size_t idx_r = 0; idx_r < solution_num_routes (sol); idx_r++) {
        route_t *route = solution_route (sol, idx_r);
        size_t route_len = route_size (route);
        printf ("route #%3zu (#nodes: %3zu, distance: %6.2f, demand: %6.2f):",
                idx_r, route_len,
                route_total_distance (route, self->vrp),
                vrptw_route_demand (self, route));
        for (size_t idx_n = 0; idx_n < route_len; idx_n++)
            printf (" %zu", route_at (route, idx_n));
        printf ("\n");
    }
    printf ("\n");
}


// ----------------------------------------------------------------------------

vrptw_t *vrptw_new_from_generic (vrp_t *vrp) {
    assert (vrp);

    vrptw_t *self = (vrptw_t *) malloc (sizeof (vrptw_t));
    assert (self);

    self->vrp = vrp;

    self->num_vehicles = vrp_num_vehicles (vrp);

    size_t one_vehicle = listu_get (vrp_vehicles (vrp), 0);
    self->capacity = vrp_vehicle_capacity (vrp, one_vehicle);

    const listu_t *requests = vrp_pending_request_ids (vrp);
    size_t num_requests = listu_size (requests);
    assert (num_requests > 0);

    self->nodes =
        (s_node_t *) malloc (sizeof (s_node_t) * (vrp_num_receivers (vrp) + 1));
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
        free (self->nodes);
        rng_free (&self->rng);
        free (self);
        *self_p = NULL;
    }
    print_info ("vrptw freed.\n");
}


solution_t *vrptw_solve (vrptw_t *self) {
    return NULL;
}


void vrptw_test (bool verbose) {
    print_info ("* vrptw: \n");

    print_info ("OK\n");
}
