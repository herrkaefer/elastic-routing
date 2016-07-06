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


// typedef struct {
//     route_t *route;
// } tsp_genome_t;


struct _tsp_t {
    vrp_t *vrp; // generic VRP model
    size_t num_nodes; // number of nodes
    size_t *node_ids; // nodes id array
    size_t start_node_id;
    size_t end_node_id;

    route_t *solution;
};


// ---------------------------------------------------------------------------
tsp_t *tsp_new_from (vrp_t *vrp) {
    assert (vrp);
    if (!vrp_is_tsp (vrp)) {
        print_error ("TSP submodel verification failed.");
        return NULL;
    }

    tsp_t *self = (tsp_t *) malloc (sizeof (tsp_t));
    assert (self);

    self->vrp = vrp;
    self->num_nodes = vrp_num_nodes (vrp);
    self->node_ids = vrp_node_ids (vrp);
    assert (self->node_ids);

    size_t *vehicle_ids = vrp_vehicle_ids (vrp);
    assert (vehicle_ids);
    self->start_node_id = vrp_vehicle_start_node_id (vrp, vehicle_ids[0]);
    self->end_node_id = vrp_vehicle_end_node_id (vrp, vehicle_ids[0]);
    free (vehicle_ids);

    self->solution = NULL;

    print_info ("tsp created from generic VRP model.\n");
    return self;
}


void tsp_free (tsp_t **self_p) {
    assert (self_p);
    if (*self_p) {
        tsp_t *self = *self_p;

        free (self->node_ids);
        if (self->solution != NULL)
            route_free (&self->solution);

        free (self);
        *self_p = NULL;
    }
    print_info ("tsp freed.\n");
}


static double tsp_fitness (const tsp_t *self, route_t *route) {
    // return strlen (str);
    return 0;
}


static double tsp_distance (const tsp_t *self,
                            const route_t *r1,
                            const route_t *r2) {
    return 0;
}


static list4x_t *tsp_heuristic (const tsp_t *self, size_t max_expected) {
    list4x_t *list = list4x_new ();
    // for (size_t cnt = 0; cnt < max_expected; cnt++)
    //     list4x_append (list, string_random_alphanum (2, 10, context->rng));
    return list;
}


static list4x_t *tsp_crossover (const tsp_t *self,
                                const route_t *r1,
                                const route_t *r2) {
    list4x_t *list = list4x_new ();
    // for (size_t cnt = 0; cnt < max_expected; cnt++)
    //     list4x_append (list, string_random_alphanum (2, 10, context->rng));
    return list;
}


void tsp_solve (tsp_t *self) {
    assert (self);

    // Create evolution object
    evol_t *evol = evol_new ();

    // Set context
    evol_set_context (evol, self);

    // Set all necessary callbacks
    evol_set_genome_destructor (evol, (destructor_t) route_free);
    evol_set_genome_printer (evol, (printer_t) route_print);
    evol_set_fitness_assessor (evol, (evol_fitness_assessor_t) tsp_fitness);
    evol_set_distance_assessor (evol, (evol_distance_assessor_t) tsp_distance);
    evol_register_heuristic (evol,
                             (evol_heuristic_t) tsp_heuristic,
                             true,
                             SIZE_MAX);
    evol_register_crossover (evol, (evol_crossover_t) tsp_crossover);

    // Run evolution
    evol_run (evol);

    // Get results
    // @todo to add

    // Destroy evolution object
    evol_free (&evol);


    // Post optimization
    // 2-opt, 3-opt, etc.

}


// void tsp_print_route(const TSP_Problem *vrp, const int *route, int start, int end)
// {
//     // printf("route[0] %d\n", route[end]);
//     PRINT("\nroute[%d:%d]: ", start, end);
//     // printf("print route: \n");
//     // printf("%d\n", route[2]);
//     for (int i = start; i <= end; i++) PRINT("%3d ", route[i]);
//     if (vrp != NULL)
//     {
//         PRINT(" (cost: %.2f, demand: %.2f)",
//               compute_route_cost(route, start, end, vrp->c, vrp->N),
//               compute_route_demand(route, start, end, vrp->q, vrp->N));
//     }
//     PRINT("\n");
// }


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
