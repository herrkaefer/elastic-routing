/*  =========================================================================
    tspi.c - TSPI

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"


struct _tspi_t {
    size_t num_nodes;
    list4u_t *template; // route template
    matrix4d_t *costs; // cost matrix
    coord2d_t *coords; // nodes coordinates
    coord2d_sys_t coord_sys; // coordinate system
    size_t start_node; // optional
    size_t end_node; // optional
    bool is_round_trip; // true: start_node == end_node != SIZE_NONE;
                        // false: other cases
    rng_t *rng;
};


// #define tspi_cost(i,j) (matrix4d_get (self->costs, (i), (j)))


// Fitness callback
static double tspi_fitness (tspi_t *self, list4u_t *route) {
    double cost = 0.0;
    size_t len = list4u_size (route);
    for (size_t i = 0; i < len-1; i++)
        cost += matrix4d_get (self->costs,
                              list4u_get (route, i),
                              list4u_get (route, i+1));
    assert (!double_equal (cost, 0));
    return 1/cost;
}


// Distance callback (Levenshtein distance)
static double tspi_distance (tspi_t *self,
                             list4u_t *r1,
                             list4u_t *r2) {
    return arrayu_levenshtein_distance (list4u_array (r1), list4u_size (r1),
                                        list4u_array (r2), list4u_size (r2));
}


// Heuristic: Sweep algorithm
// @todo could be modified to generate more (realistic) routes
static list4x_t *tspi_sweep (tspi_t *self, size_t max_expected) {
    list4x_t *list = list4x_new ();
    if (!self->coords)
        return list;

    // Polars of nodes
    coord2d_t *polars = (coord2d_t *) malloc (sizeof (coord2d_t) * self->num_nodes);
    assert (polars);

    coord2d_t *ref = (self->start_node != SIZE_NONE) ?
                     &self->coords[self->start_node] :
                     NULL;

    for (size_t id = 0; id < self->num_nodes; id++) {
        polars[id] = coord2d_to_polar (&self->coords[id], ref, self->coord_sys);
        polars[id].v1 = (double) id; // save ID in radius
    }

    // sort nodes in ascending order of angle
    qsort (polars, self->num_nodes, sizeof (coord2d_t),
           (comparator_t) coord2d_compare_polar_angle);

    list4u_t *route = list4u_new (self->num_nodes + 1);
    assert (route);
    if (self->start_node != SIZE_NONE)
        list4u_append (route, self->start_node);

    for (size_t i = 0; i < self->num_nodes; i++) {
        size_t id = (size_t) polars[i].v1;
        if (id == self->start_node || id == self->end_node)
            continue;
        list4u_append (route, id);
    }

    if (self->end_node != SIZE_NONE)
        list4u_append (route, self->end_node);

    print_info ("route by sweep:\n");
    list4u_print (route);

    list4x_append (list, route);
    return list;
}


// Heuristic: random generation
static list4x_t *tspi_generate_random_route (tspi_t *self,
                                             size_t max_expected) {
    list4x_t *list = list4x_new ();

    for (size_t cnt = 0; cnt < max_expected; cnt++) {
        list4u_t *route = list4u_duplicate (self->template);
        assert (route);

        // Remove start and end nodes if defined
        if (self->start_node != SIZE_NONE)
            list4u_remove_first (route);
        if (self->end_node != SIZE_NONE)
            list4u_remove_last (route);

        list4u_shuffle (route, self->rng);

        // Add back start and end nodes if defined
        if (self->start_node != SIZE_NONE)
            list4u_prepend (route, self->start_node);
        if (self->end_node != SIZE_NONE)
            list4u_append (route, self->end_node);

        list4x_append (list, route);

        print_info ("random route:\n");
        list4u_print (route);
    }

    return list;
}


static void tspi_route_ox (tspi_t *self,
                           size_t *route1, size_t *route2,
                           size_t start, size_t end) {
    assert(start <= end);

    size_t i, j, k, tmp;
    size_t n = end - start + 1;
    size_t len_fix;
    size_t *P1 = route1 + start,
        *P2 = route2 + start;
    size_t pos_c1, pos_c2;

    i = rng_random_int (self->rng, 0, n);
    j = rng_random_int (self->rng, 0, n);

    if (i > j) {
        k = i;
        i = j;
        j = k;
    }
    len_fix = j - i + 1;

    // PRINT("\nP1: ");
    // print_route(P1, 0, n-1, NULL);
    // PRINT("P2: ");
    // print_route(P2, 0, n-1, NULL);
    // PRINT("\nfix segment: %d -> %d. route1: %d->%d, route2: %d->%d\n", i, j, P1[i], P1[j], P2[i], P2[j]);

    // circular
    size_t cnt = 0;
    for (cnt = 0, k = (j + 1) % n, pos_c1 = k, pos_c2 = k;
         cnt < n && (pos_c1 != i || pos_c2 != i);
         cnt++, k = (k + 1) % n) {
        // PRINT("n: %d, cnt: %d, k: %d, pos_c1: %d, pos_c2: %d\n", n, cnt, k, pos_c1, pos_c2);
        tmp = P1[k];
        if (pos_c1 != i && !arrayu_includes (P1+i, len_fix, P2[k])) {
            P1[pos_c1++] = P2[k];
            if (pos_c1 == n) pos_c1 = 0;
        }
        if (pos_c2 != i && !arrayu_includes (P2+i, len_fix, tmp)) {
            P2[pos_c2++] = tmp;
            if (pos_c2 == n) pos_c2 = 0;
        }
    }

    // PRINT("\nC1: ");
    // print_route(C1, 0, n-1, NULL);
    // PRINT("C2: ");
    // print_route(C2, 0, n-1, NULL);
}


// Crossover callback: OX
static list4x_t *tspi_ox (tspi_t *self, list4u_t *route1, list4u_t *route2) {
    list4x_t *list = list4x_new ();

    size_t *r1 = list4u_dump_array (route1);
    assert (r1);
    size_t *r2 = list4u_dump_array (route2);
    assert (r2);

    size_t len1 = list4u_size (route1);
    assert (len1 == list4u_size (route2));

    size_t start_idx = self->start_node != SIZE_NONE ? 1 : 0;
    size_t end_idx = self->end_node != SIZE_NONE ? (len1-2) : (len1-1);
    // print_info ("start: %zu, end: %zu\n", start_idx, end_idx);

    tspi_route_ox (self, r1, r2, start_idx, end_idx);

    // arrayu_print (r1, len1);
    // arrayu_print (r2, len1);

    list4u_t *child1 = list4u_new_from_array (r1, len1);
    list4u_t *child2 = list4u_new_from_array (r2, len1);
    // list4u_print (child1);
    // list4u_print (child2);

    list4x_append (list, child1);
    list4x_append (list, child2);

    free (r1);
    free (r2);
    return list;
}


static bool tspi_validate_costs (tspi_t *self) {
    for (size_t i = 0; i < self->num_nodes; i++)
        for (size_t j = 0; j < self->num_nodes; j++) {
            // if double_is_none (tspi_cost(i, j)) {
            if double_is_none (matrix4d_get (self->costs, i, j)) {
                print_error ("cost from node %zu to %zu is not defined.\n", i, j);
                return false;
            }
        }
    return true;
}


static bool tspi_validate_coords (tspi_t *self) {
    if (self->coords == NULL)
        return true;

    if (self->coord_sys == CS_NONE) {
        print_error ("type of coordinate system is not defined.\n");
        return false;
    }

    for (size_t i = 0; i < self->num_nodes; i++)
        if (coord2d_is_none (&self->coords[i])) {
            print_error ("coordinate of node %zu is not defined.\n", i);
            return false;
        }

    return true;
}


static bool tspi_validate_and_adjust_nodes (tspi_t *self) {
    // For round trip, check and set start and end nodes properly
    if (self->is_round_trip) {
        if (self->start_node != SIZE_NONE &&
            self->end_node != SIZE_NONE &&
            self->start_node != self->end_node) {
            print_error ("start node and end node should be the same for round trip.\n");
            return false;
        }

        if (self->start_node != SIZE_NONE && self->end_node == SIZE_NONE)
            self->end_node = self->start_node;

        if (self->start_node == SIZE_NONE && self->end_node != SIZE_NONE)
            self->start_node = self->end_node;

        if (self->start_node == SIZE_NONE && self->end_node == SIZE_NONE) {
            self->start_node = 0;
            self->end_node = 0;
        }
    }

    // Adjust route template with start node at head and end node at tail.
    // Move start node to the first
    if (self->start_node != SIZE_NONE && self->start_node != 0)
        list4u_swap (self->template, 0, self->start_node);

    // Move end node to the last
    if (self->end_node != SIZE_NONE) {
        if (self->start_node != self->end_node) {
            size_t idx = list4u_find (self->template, self->end_node);
            assert (idx != SIZE_NONE);
            list4u_swap (self->template, self->num_nodes-1, idx);
        }
        else
            list4u_append (self->template, self->end_node);
    }

    print_info ("route template:\n");
    list4u_print (self->template);

    return true;
}


// ---------------------------------------------------------------------------
tspi_t *tspi_new (size_t num_nodes) {
    tspi_t *self = (tspi_t *) malloc (sizeof (tspi_t));
    assert (self);

    self->num_nodes = num_nodes;

    self->template = list4u_new_range (0, num_nodes-1, 1);
    assert (self->template);

    self->costs = matrix4d_new (num_nodes, num_nodes);
    assert (self->costs);

    self->coords = NULL;
    self->coord_sys = CS_NONE;

    self->start_node = SIZE_NONE;
    self->end_node = SIZE_NONE;
    self->is_round_trip = false;
print_info ("here0\n");
    self->rng = rng_new ();

    print_info ("tspi created.\n");

    return self;
}


void tspi_free (tspi_t **self_p) {
    assert (self_p);
    if (*self_p) {
        tspi_t *self = *self_p;

        list4u_free (&self->template);
        matrix4d_free (&self->costs);
        if (self->coords)
            free (self->coords);
        rng_free (&self->rng);

        free (self);
        *self_p = NULL;
    }
    print_info ("tspi freed.\n");
}


void tspi_set_cost (tspi_t *self, size_t node_id1, size_t node_id2, double cost) {
    assert (self);
    assert (node_id1 < self->num_nodes);
    assert (node_id2 < self->num_nodes);
    matrix4d_set (self->costs, node_id1, node_id2, cost);
}


void tspi_set_coord_system (tspi_t *self, coord2d_sys_t coord_sys) {
    assert (self);
    self->coord_sys = coord_sys;
}


void tspi_set_node_coord (tspi_t *self, size_t node_id, coord2d_t coord) {
    assert (self);
    assert (node_id < self->num_nodes);
    if (self->coords == NULL) {
        self->coords =
            (coord2d_t *) malloc (sizeof (coord2d_t) * self->num_nodes);
        assert (self->coords);
    }
    self->coords[node_id] = coord;
}


void tspi_generate_straight_distances_as_costs (tspi_t *self) {
    assert (self);
    assert (self->coords);
    if (!tspi_validate_coords (self)) {
        print_error ("coordinates validation failed.\n");
        return;
    }

    for (size_t i = 0; i < self->num_nodes; i++) {
        for (size_t j = 0; j < self->num_nodes; j++) {
            if (i == j) {
                matrix4d_set (self->costs, i, j, 0);
                continue;
            }
            double dist = coord2d_distance (&self->coords[i],
                                            &self->coords[j],
                                            self->coord_sys);
            matrix4d_set (self->costs, i, j, dist);
        }
    }
}


void tspi_set_start_node (tspi_t *self, size_t start_node_id) {
    assert (self);
    self->start_node = start_node_id;
}


void tspi_set_end_node (tspi_t *self, size_t end_node_id) {
    assert (self);
    self->end_node = end_node_id;
}


void tspi_set_round_trip (tspi_t *self, bool is_round_trip) {
    assert (self);
    self->is_round_trip = is_round_trip;
}


void tspi_solve (tspi_t *self) {
    assert (self);

    // Input validation
    if (!tspi_validate_costs (self) ||
        !tspi_validate_coords (self) ||
        !tspi_validate_and_adjust_nodes (self)) {
        print_error ("model validation failed.\n");
        return;
    }

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
    evol_register_heuristic (evol,
                             (evol_heuristic_t) tspi_generate_random_route,
                             true,
                             factorial (list4u_size (self->template)-2));
    evol_register_crossover (evol, (evol_crossover_t) tspi_ox);

    // Run evolution
    evol_run (evol);

    // Get results
    // @todo to add

    // Destroy evolution object
    evol_free (&evol);


    // Post optimization
    // 2-opt, 3-opt, etc.

}


void tspi_test (bool verbose) {
    print_info (" * tspi: \n");

    coord2d_t node_coords[] = {
        (coord2d_t) {0, 0},
        (coord2d_t) {2, 2},
        (coord2d_t) {1, 1.5},
        (coord2d_t) {3, 1},
        (coord2d_t) {4, 0.5},
        (coord2d_t) {5, 0}
    };
    size_t start_node_id = 0, end_node_id = 5;

    size_t num_nodes = sizeof (node_coords) / sizeof (coord2d_t);
    assert (num_nodes == 6);

    // Create model by number of nodes.
    // Nodes in the model are assigned with IDs from 0 to (num_nodes-1).
    tspi_t *tsp = tspi_new (num_nodes);
    assert (tsp);

    // Set coordinates of nodes
    tspi_set_coord_system (tsp, CS_CARTESIAN2D);
    for (size_t id = 0; id < num_nodes; id++)
        tspi_set_node_coord (tsp, id, node_coords[id]);

    // Set start and end ndoes if necessary
    tspi_set_start_node (tsp, start_node_id);
    tspi_set_end_node (tsp, end_node_id);

    // Use straight distances as costs
    tspi_generate_straight_distances_as_costs (tsp);

    // Solve problem by evolution
    tspi_solve (tsp);

    tspi_free (&tsp);
    print_info ("OK\n");
}
