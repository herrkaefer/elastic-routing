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

    vrptw_t *self = (vrptw_t *) malloc (sizeof (vrptw_t));
    assert (self);

    self->vrp = vrp;

    // Set start and end nodes
    assert (vrp_num_vehicles (vrp) == 1);
    size_t vehicle_id = listu_get (vrp_vehicles (vrp), 0);
    assert (vehicle_id != ID_NONE);

    self->start_node_id = vrp_vehicle_start_node_id (vrp, vehicle_id);
    self->end_node_id = vrp_vehicle_end_node_id (vrp, vehicle_id);

    // Make regularized route template: (start_node_id, ..., end_node_id)
    // Note that start or end node of vehicle may not be specified, then the
    // model is one-way and open ended.

    // Do not estimate the size, start with a trivial number.
    self->template = route_new (3);

    if (self->start_node_id != ID_NONE)
        listu_append (self->template, self->start_node_id);

    // Only add nonvisited nodes by scanning the pending requests
    const listu_t *request_ids = vrp_pending_request_ids (vrp);
    size_t num_requests = listu_size (request_ids);
    for (size_t idx = 0; idx < num_requests; idx++) {
        size_t request_id = listu_get (request_ids, idx);
        size_t node_id = vrp_request_pickup_node (vrp, request_id);
        if (node_id != ID_NONE &&
            node_id != self->start_node_id &&
            node_id != self->end_node_id)
            listu_append (self->template, node_id);
        else {
            node_id = vrp_request_delivery_node (vrp, request_id);
            if (node_id != ID_NONE &&
                node_id != self->start_node_id &&
                node_id != self->end_node_id)
                listu_append (self->template, node_id);
        }
    }

    if (self->end_node_id != ID_NONE)
        listu_append (self->template, self->end_node_id);

    print_info ("vrptw derived from generic VRP model.\n");
    print_info ("route template: #nodes: %zu, %s trip, start: %s, end: %s\n",
                vrptw_num_nodes (self),
                vrptw_is_round_trip (self) ? "round" : "one-way",
                (self->start_node_id != ID_NONE) ? "y" : "n",
                (self->end_node_id != ID_NONE) ? "y" : "n");
    route_print (self->template);

    self->rng = rng_new ();
    return self;
}


void vrptw_free (vrptw_t **self_p) {
    assert (self_p);
    if (*self_p) {
        vrptw_t *self = *self_p;
        route_free (&self->template);
        rng_free (&self->rng);
        free (self);
        *self_p = NULL;
    }
    print_info ("vrptw freed.\n");
}


solution_t *vrptw_solve (vrptw_t *self) {
    assert (self);

    // Deal with small numboer of nodes
    if (vrptw_num_nodes (self) <= SMALL_NUM_NODES)
        return vrptw_solve_small_model (self);

    // Create evolution object
    evol_t *evol = evol_new ();

    // Set context
    evol_set_context (evol, self);

    // Set all necessary callbacks
    evol_set_genome_destructor (evol, (destructor_t) route_free);
    evol_set_genome_printer (evol, (printer_t) route_print);
    evol_set_fitness_assessor (evol, (evol_fitness_assessor_t) vrptw_fitness);
    evol_set_distance_assessor (evol, (evol_distance_assessor_t) vrptw_distance);

    if (vrp_coord_sys (self->vrp) != CS_NONE)
        evol_register_heuristic (evol,
                                 (evol_heuristic_t) vrptw_sweep,
                                 false,
                                 1);

    size_t num_free_nodes = route_size (self->template);
    num_free_nodes -= (self->start_node_id != ID_NONE) ? 1 : 0;
    num_free_nodes -= (self->end_node_id != ID_NONE) ? 1 : 0;
    evol_register_heuristic (evol,
                             (evol_heuristic_t) vrptw_random_permutation,
                             true,
                             factorial (num_free_nodes));

    evol_register_crossover (evol, (evol_crossover_t) vrptw_ox);

    // Run evolution
    evol_run (evol);

    // Get best route
    route_t *route = route_dup ((route_t *) evol_best_genome (evol));
    assert (route);

    // Destroy evolution object
    evol_free (&evol);

    double route_cost = route_total_distance (route, self->vrp);
    print_info ("route cost after evol: %.2f\n", route_cost);

    // Post optimization
    double delta_cost = vrptw_2_opt (self, route);
    double improvement = -delta_cost / route_cost;
    route_cost = route_total_distance (route, self->vrp);
    print_info ("route cost after post-optimization: %.2f\n", route_cost);
    print_info ("post-optimization improved: %.2f%%\n", improvement * 100);

    // Return route as generic solution representation.
    // Set model reference as NULL because this model is not a generic one.
    solution_t *sol = solution_new (self->vrp);
    solution_add_route (sol, route);

    return sol;
}


void vrptw_test (bool verbose) {
    print_info ("* vrptw: \n");

    // 1. manually created problem

    coord2d_t node_coords[] = {
        (coord2d_t) {0, 0},
        (coord2d_t) {3, 1},
        (coord2d_t) {2, 2},
        (coord2d_t) {1, 1.5},
        (coord2d_t) {4, 0.5},
        (coord2d_t) {5, 0}
    };
    size_t start_node_id = 0, end_node_id = 0;

    size_t num_nodes = sizeof (node_coords) / sizeof (coord2d_t);

    // Create generic model
    vrp_t *vrp = vrp_new ();

    // Set roadgraph and add visiting request for each node
    vrp_set_coord_sys (vrp, CS_CARTESIAN2D);
    char *ext_id = malloc (16);
    for (size_t idx = 0; idx < num_nodes; idx++) {
        sprintf (ext_id, "node-%4zu", idx);
        size_t nid = vrp_add_node (vrp, ext_id, NR_CUSTOMER);
        vrp_set_node_coord (vrp, nid, node_coords[idx]);

        sprintf (ext_id, "visit-%4zu", idx);
        vrp_add_request (vrp, ext_id, ID_NONE, nid, 0);
    }
    vrp_generate_beeline_distances (vrp);

    // Add a vehicle
    sprintf (ext_id, "vehicle-%4d", 0);
    vrp_add_vehicle (vrp, ext_id, DOUBLE_MAX, start_node_id, end_node_id);

    // Create TSP model from generic model
    vrptw_t *vrptw = vrptw_new_from (vrp);
    assert (vrptw);

    // Solve
    solution_t *sol = vrptw_solve (vrptw);
    assert (sol);
    solution_print (sol);

    free (ext_id);
    vrptw_free (&vrptw);
    assert (vrptw == NULL);
    vrp_free (&vrp);
    assert (vrp == NULL);
    solution_free (&sol);
    assert (sol == NULL);


    print_info ("OK\n");
}
