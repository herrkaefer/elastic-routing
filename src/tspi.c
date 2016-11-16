/*  =========================================================================
    tspi - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


struct _tspi_t {
    size_t num_nodes;
    listu_t *template; // a basic route template other operations are refered to
    matrixd_t *costs; // cost matrix
    coord2d_t *coords; // nodes coordinates.
    coord2d_sys_t coord_sys; // coordinate system.
    size_t start_node;
    size_t end_node;
    bool is_round_trip; // true: start_node == end_node != SIZE_NONE;
                        // false: other cases
    rng_t *rng;
};


// #define tspi_cost(i,j) (matrixd_get (self->costs, (i), (j)))


// Fitness callback: average cost over arcs
static double tspi_fitness (tspi_t *self, listu_t *route) {
    double cost = 0.0;
    size_t len = listu_size (route);
    if (len == 1)
        return cost;

    for (size_t i = 0; i < len-1; i++)
        cost += matrixd_get (self->costs,
                              listu_get (route, i),
                              listu_get (route, i+1));
    assert (cost > 0);
    return (len-1)/cost;
}


// Distance callback (Levenshtein distance)
static double tspi_distance (tspi_t *self,
                             listu_t *r1,
                             listu_t *r2) {
    return arrayu_levenshtein_distance (listu_array (r1), listu_size (r1),
                                        listu_array (r2), listu_size (r2));
}


// Heuristic: Sweep algorithm
// @todo could be modified to generate more (realistic) routes
static listx_t *tspi_sweep (tspi_t *self, size_t max_expected) {
    listx_t *list = listx_new ();
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

    listu_t *route = listu_new (self->num_nodes + 1);
    assert (route);
    if (self->start_node != SIZE_NONE)
        listu_append (route, self->start_node);

    for (size_t i = 0; i < self->num_nodes; i++) {
        size_t id = (size_t) polars[i].v1;
        if (id == self->start_node || id == self->end_node)
            continue;
        listu_append (route, id);
    }

    if (self->end_node != SIZE_NONE)
        listu_append (route, self->end_node);

    print_info ("route generated by sweep:\n");
    listu_print (route);

    listx_append (list, route);
    return list;
}


// Heuristic: random generation
static listx_t *tspi_random_routes (tspi_t *self, size_t max_expected) {
    listx_t *list = listx_new ();

    size_t route_size = listu_size (self->template);
    size_t shuffle_start = (self->start_node != SIZE_NONE) ? 1 : 0;
    size_t shuffle_end =
        (self->end_node != SIZE_NONE) ? (route_size-2) : (route_size-1);

    for (size_t cnt = 0; cnt < max_expected; cnt++) {
        listu_t *route = listu_dup (self->template);
        assert (route);
        listu_shuffle_slice (route, shuffle_start, shuffle_end, self->rng);
        listx_append (list, route);
        // print_info ("random route:\n");
        // listu_print (route);
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
            if (pos_c1 == n)
                pos_c1 = 0;
        }
        if (pos_c2 != i && !arrayu_includes (P2+i, len_fix, tmp)) {
            P2[pos_c2++] = tmp;
            if (pos_c2 == n)
                pos_c2 = 0;
        }
    }

    // PRINT("\nC1: ");
    // print_route(C1, 0, n-1, NULL);
    // PRINT("C2: ");
    // print_route(C2, 0, n-1, NULL);
}


// Crossover: OX
static listx_t *tspi_ox (tspi_t *self, listu_t *route1, listu_t *route2) {
    listx_t *list = listx_new ();

    size_t *r1 = listu_dump_array (route1);
    assert (r1);
    size_t *r2 = listu_dump_array (route2);
    assert (r2);

    size_t len1 = listu_size (route1);
    assert (len1 == listu_size (route2));

    size_t start_idx = self->start_node != SIZE_NONE ? 1 : 0;
    size_t end_idx = self->end_node != SIZE_NONE ? (len1-2) : (len1-1);
    // print_info ("start: %zu, end: %zu\n", start_idx, end_idx);

    tspi_route_ox (self, r1, r2, start_idx, end_idx);

    // arrayu_print (r1, len1);
    // arrayu_print (r2, len1);

    listu_t *child1 = listu_new_from_array (r1, len1);
    listu_t *child2 = listu_new_from_array (r2, len1);
    // listu_print (child1);
    // listu_print (child2);

    listx_append (list, child1);
    listx_append (list, child2);

    free (r1);
    free (r2);
    return list;
}


static bool tspi_validate_costs (tspi_t *self) {
    for (size_t i = 0; i < self->num_nodes; i++)
        for (size_t j = 0; j < self->num_nodes; j++) {
            // if double_is_none (tspi_cost(i, j)) {
            if double_is_none (matrixd_get (self->costs, i, j)) {
                print_error ("cost from node %zu to %zu is not defined.\n", i, j);
                return false;
            }
        }

    // matrixd_print (self->costs);
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


static bool tspi_regularize_template (tspi_t *self) {
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
    // For one-way trip, start node and end node should be different if defined
    else {
        if (self->start_node != SIZE_NONE &&
            self->end_node != SIZE_NONE &&
            self->start_node == self->end_node) {
            print_warning ("start node and end node are same, change to round trip.\n");
            self->is_round_trip = true;
        }
    }

    // Adjust route template to shift start node to head and end node to tail.
    // Move the start node to head if defined
    if (self->start_node != SIZE_NONE && self->start_node != 0)
        listu_swap (self->template, 0, self->start_node);

    // Move the end node to tail if defined
    if (self->end_node != SIZE_NONE) {
        if (self->start_node != self->end_node) {
            size_t idx = listu_find (self->template, self->end_node);
            assert (idx != SIZE_NONE);
            listu_swap (self->template, self->num_nodes-1, idx);
        }
        else
            listu_append (self->template, self->end_node);
    }

    print_info ("route template: #nodes: %zu, %s trip\n",
                self->num_nodes, self->is_round_trip ? "round" : "one-way");
    listu_print (self->template);

    return true;
}


// ---------------------------------------------------------------------------
tspi_t *tspi_new (size_t num_nodes) {
    tspi_t *self = (tspi_t *) malloc (sizeof (tspi_t));
    assert (self);

    self->num_nodes = num_nodes;

    self->template = listu_new_range (0, num_nodes-1, 1);
    assert (self->template);

    self->costs = matrixd_new (num_nodes, num_nodes);
    assert (self->costs);

    self->coords = NULL;
    self->coord_sys = CS_NONE;

    self->start_node = SIZE_NONE;
    self->end_node = SIZE_NONE;
    self->is_round_trip = false; // default: one-way
    self->rng = rng_new ();

    print_info ("tspi created.\n");
    return self;
}


void tspi_free (tspi_t **self_p) {
    assert (self_p);
    if (*self_p) {
        tspi_t *self = *self_p;

        listu_free (&self->template);
        matrixd_free (&self->costs);
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
    matrixd_set (self->costs, node_id1, node_id2, cost);
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


void tspi_generate_beeline_distances_as_costs (tspi_t *self) {
    assert (self);
    assert (self->coords);
    if (!tspi_validate_coords (self)) {
        print_error ("coordinates validation failed.\n");
        return;
    }

    for (size_t i = 0; i < self->num_nodes; i++) {
        for (size_t j = 0; j < self->num_nodes; j++) {
            if (i == j) {
                matrixd_set (self->costs, i, j, 0);
                continue;
            }
            double dist = coord2d_distance (&self->coords[i],
                                            &self->coords[j],
                                            self->coord_sys);
            matrixd_set (self->costs, i, j, dist);
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


solution_t *tspi_solve (tspi_t *self) {
    assert (self);

    // Input validation
    if (!tspi_validate_costs (self) ||
        !tspi_validate_coords (self) ||
        !tspi_regularize_template (self)) {
        print_error ("model validation failed.\n");
        return NULL;
    }

    // Create evolution object
    evol_t *evol = evol_new ();

    // Set context
    evol_set_context (evol, self);

    // Set all necessary callbacks
    evol_set_genome_destructor (evol, (destructor_t) listu_free);
    evol_set_genome_printer (evol, (printer_t) listu_print);
    evol_set_fitness_assessor (evol, (evol_fitness_assessor_t) tspi_fitness);
    evol_set_distance_assessor (evol, (evol_distance_assessor_t) tspi_distance);
    evol_register_heuristic (evol,
                             (evol_heuristic_t) tspi_sweep,
                             false,
                             1);
    // size_t max_random_routes = factorial (listu_size (self->template)-2);
    // print_debug("");
    // print_info ("num_nodes: %zu, max random routes: %zu\n",
    //     listu_size (self->template)-2, max_random_routes);
    evol_register_heuristic (evol,
                             (evol_heuristic_t) tspi_random_routes,
                             true,
                             factorial (listu_size (self->template)-2));
    evol_register_crossover (evol, (evol_crossover_t) tspi_ox);

    // Run evolution
    evol_run (evol);

    // Get results
    listu_t *route = listu_dup ((listu_t *) evol_best_genome (evol));
    assert (route);

    // Destroy evolution object
    evol_free (&evol);

    // Post optimization
    // 2-opt, 3-opt, etc.

    solution_t *sol = solution_new (NULL);
    solution_add_route_from_list (sol, route);

    return sol;
}


static coord2d_t *generate_random_cartesian_coords (double xmin, double xmax,
                                                    double ymin, double ymax,
                                                    size_t num) {
    coord2d_t *coords = (coord2d_t *) malloc (sizeof (coord2d_t) * num);
    assert (coords);

    rng_t *rng = rng_new ();
    assert (rng);

    for (size_t idx = 0; idx < num; idx++) {
        double x = rng_random_double (rng, xmin, xmax);
        double y = rng_random_double (rng, ymin, ymax);
        printf ("(%.2f, %.2f)\n", x, y);
        coords[idx] = (coord2d_t) {x, y};
    }

    rng_free (&rng);
    return coords;
}


void tspi_test (bool verbose) {
    print_info (" * tspi: \n");

    // 1. manual coordinates

    coord2d_t node_coords[] = {
        (coord2d_t) {0, 0},
        (coord2d_t) {2, 2},
        (coord2d_t) {1, 1.5},
        (coord2d_t) {3, 1},
        (coord2d_t) {4, 0.5},
        (coord2d_t) {5, 0}
    };
    size_t start_node_id = 0, end_node_id = 0;

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

    // Set round trip or one-way trip
    tspi_set_round_trip (tsp, false);

    // Use straight distances as costs
    tspi_generate_beeline_distances_as_costs (tsp);

    // Solve problem by evolution
    solution_t *sol = tspi_solve (tsp);
    if (sol != NULL)
        solution_print (sol);

    tspi_free (&tsp);
    assert (tsp == NULL);

    solution_free (&sol);
    assert (sol == NULL);

    // 2. random coordinates

    rng_t *rng = rng_new ();
    assert (rng);

    num_nodes = 100;
    coord2d_t *coords =
        generate_random_cartesian_coords (-100, 100, -100, 100, num_nodes);
    start_node_id = rng_random_int (rng, 0, num_nodes);
    end_node_id = rng_random_int (rng, 0, num_nodes);

    // Create model by number of nodes.
    // Nodes in the model are assigned with IDs from 0 to (num_nodes-1).
    tsp = tspi_new (num_nodes);
    assert (tsp);

    // Set coordinates of nodes
    tspi_set_coord_system (tsp, CS_CARTESIAN2D);
    for (size_t id = 0; id < num_nodes; id++)
        tspi_set_node_coord (tsp, id, coords[id]);

    // Set start and end ndoes if necessary
    tspi_set_start_node (tsp, start_node_id);
    // tspi_set_end_node (tsp, end_node_id);

    // Set round trip or one-way trip
    tspi_set_round_trip (tsp, true);

    // Use straight distances as costs
    tspi_generate_beeline_distances_as_costs (tsp);

    // Solve problem by evolution
    sol = tspi_solve (tsp);
    print_info ("solution:\n");
    if (sol != NULL)
        solution_print (sol);

    rng_free (&rng);
    tspi_free (&tsp);
    solution_free (&sol);

    print_info ("OK\n");
}
