/*  =========================================================================
    tsp.c - TSP

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"



struct _tsp_t {
    vrp_t *vrp; // generic model

    size_t num_nodes; // number of nodes
    list4u_t *template;
    size_t start_node_id;
    size_t end_node_id;
    bool is_round_trip;

    rng_t *rng;
};


// Heuristic: random generation
static list4x_t *tsp_random_routes (tsp_t *self, size_t max_expected) {
    list4x_t *list = list4x_new ();

    size_t route_size = list4u_size (self->template);
    size_t shuffle_start = (self->start_node != SIZE_NONE) ? 1 : 0;
    size_t shuffle_end =
        (self->end_node != SIZE_NONE) ? (route_size-2) : (route_size-1);

    for (size_t cnt = 0; cnt < max_expected; cnt++) {
        list4u_t *route = list4u_duplicate (self->template);
        assert (route);
        list4u_shuffle_slice (route, shuffle_start, shuffle_end, self->rng);
        list4x_append (list, route);
        // print_info ("random route:\n");
        // list4u_print (route);
    }

    return list;
}


// ---------------------------------------------------------------------------
tsp_t *tsp_new_from (vrp_t *vrp) {
    assert (vrp);

    // Sub-model verification
    if (!vrp_is_tsp (vrp)) {
        print_error ("TSP submodel verification failed.\n");
        return NULL;
    }

    tsp_t *self = (tsp_t *) malloc (sizeof (tsp_t));
    assert (self);

    self->vrp = vrp;

    size_t *vehicle_ids = vrp_vehicle_ids (vrp);
    assert (vehicle_ids);
    self->start_node_id = vrp_vehicle_start_node_id (vrp, vehicle_ids[0]);
    self->end_node_id = vrp_vehicle_end_node_id (vrp, vehicle_ids[0]);
    free (vehicle_ids);

    // Make regularized route template
    self->template = list4u_new (vrp_num_nodes (vrp) + 1);
    size_t *node_ids = vrp_node_ids (vrp);
    assert (node_ids);

    if (self->start_node_id != ID_NONE)
        list4u_append (self->template, self->start_node_id);

    for (size_t cnt = 0; cnt < self->num_nodes; cnt++) {
        if (node_ids[cnt] != self->start_node_id &&
            node_ids[cnt] != self->end_node_id) {
            list4u_append (self->template, node_ids[cnt]);
        }
    }

    if (self->end_node_id != ID_NONE)
        list4u_append (self->template, self->end_node_id);

    free (node_ids);

    self->is_round_trip = self->start_node_id != ID_NONE &&
                          self->end_node_id != ID_NONE &&
                          self->start_node_id == self->end_node_id;

    self->num_nodes = self->is_round_trip ?
                      list4u_size (self->template) - 1 :
                      list4u_size (self->template);

    print_info ("tsp created from generic VRP model.\n");
    print_info ("route template: #nodes: %zu, %s trip\n",
                self->num_nodes, self->is_round_trip ? "round" : "one-way");
    list4u_print (self->template);

    return self;
}


void tsp_free (tsp_t **self_p) {
    assert (self_p);
    if (*self_p) {
        tsp_t *self = *self_p;

        list4u_free (&self->template);

        free (self);
        *self_p = NULL;
    }
    print_info ("tsp freed.\n");
}


list4u_t *tsp_solve (tsp_t *self) {
    assert (self);

    // Create evolution object
    evol_t *evol = evol_new ();

    // Set context
    evol_set_context (evol, self);

    // Set all necessary callbacks
    evol_set_genome_destructor (evol, (destructor_t) list4u_free);
    evol_set_genome_printer (evol, (printer_t) list4u_print);
    evol_set_fitness_assessor (evol, (evol_fitness_assessor_t) tspi_fitness);
    evol_set_distance_assessor (evol, (evol_distance_assessor_t) tspi_distance);
    evol_register_heuristic (evol,
                             (evol_heuristic_t) tspi_sweep,
                             false,
                             1);
    // size_t max_random_routes = factorial (list4u_size (self->template)-2);
    // print_debug("");
    // print_info ("num_nodes: %zu, max random routes: %zu\n",
    //     list4u_size (self->template)-2, max_random_routes);
    evol_register_heuristic (evol,
                             (evol_heuristic_t) tspi_generate_random_route,
                             true,
                             factorial (list4u_size (self->template)-2));
    evol_register_crossover (evol, (evol_crossover_t) tspi_ox);

    // Run evolution
    evol_run (evol);

    // Get results
    list4u_t *result = list4u_duplicate ((list4u_t *) evol_best_genome (evol));
    assert (result);

    // Destroy evolution object
    evol_free (&evol);

    // Post optimization
    // 2-opt, 3-opt, etc.


    return result;

}





void tsp_test (bool verbose) {
    print_info (" * tsp: \n");
    // tsp_t *tsp = tsp_new_from_file ("testdata/A-n32-k5.vrp");
    // assert (tsp);
    // print_info ("N: %d, K: %d, Q: %f\n",
    //     tsp_num_customers (tsp),
    //     tsp_num_vehicles (tsp),
    //     tsp_capacity (tsp));

    // // ...
    // tsp_free (&tsp);
    print_info ("OK\n");
}
