/*  =========================================================================
    cvrp - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

/* Todos:

- [ ] remove customer duplicates when creating model
- [x] fitness (cost of splited giant tour)
- [x] split giant tour to solution_t
- [x] intra-route local search for evol
- [ ] 3-opt local search for post optimization
- [ ] inter-route local search for post optimization
- [ ] more heuristics:
    - [ ] sweep
    - [ ] petal
- [ ] small model solver

Details:

giant tour (gtour): a sequence of customers (node IDs in generic model).

For evolution:

- genome: s_genome_t object in which gtour is for crossover, sol is for
          local search. sol is got by split the giant tour
- heuristics:
    - parameterized CW
    - sweep giant tours
    - random giant tours
- crossover: OX of gtour
- cost of genome: cost of splited giant tour
- fitness of genome: inverse of cost averaged over arcs
- distance between genomes:

*/

#include "classes.h"


#define SMALL_NUM_NODES 10


// Private node representation
typedef struct {
    size_t id; // node ID in roadgraph of generic model
    double demand;
    const coord2d_t *coord;
} s_node_t;


struct _cvrp_t {
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

static double cvrp_arc_distance_by_idx (cvrp_t *self,
                                        size_t idx1, size_t idx2) {
    return vrp_arc_distance (self->vrp,
                             self->nodes[idx1].id, self->nodes[idx2].id);
}


// Get demand of node. i.e. associated request's quantity
static double cvrp_node_demand (cvrp_t *self, size_t node_id) {
    if (node_id == self->nodes[0].id) // depot
        return 0;

    const listu_t *requests =
        vrp_node_pending_request_ids (self->vrp, node_id);
    assert (requests);
    assert (listu_size (requests) == 1);
    size_t request_id = listu_get (requests, 0);
    assert (node_id == vrp_request_receiver (self->vrp, request_id));
    return vrp_request_quantity (self->vrp, request_id);
}


// Transform CVRP solution to giant tour representation
static route_t *cvrp_giant_tour_from_solution (cvrp_t *self, solution_t *sol) {
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


// ----------------------------------------------------------------------------

// Split algorithm: giant tour -> CVRP solution
// Notes:
// Shortest path algorithm is performed on directed acyclic graph (auxiliary
// graph H). In H, arc (i, j) exists means route (depot, i+1, ..., j, depot)
// is feasible.
// Index i in H is cooresponed to index (i-1) in giant tour.
// Ref: Prins 2004
static solution_t *cvrp_split (cvrp_t *self, route_t *gtour) {
    size_t N = self->num_customers;
    size_t depot = self->nodes[0].id;

    // cost of the shortest path from node 0 to node (1 ~ N) in H
    double *sp_cost = (double *) malloc ((N + 1) * sizeof (double));
    assert (sp_cost);

    // predecessor of node (1 ~ N) on the shortest path
    size_t *predecessor = (size_t *) malloc ((N + 1) * sizeof (size_t));
    assert (predecessor);

    // initialize
    for (size_t i = 0; i <= N; i++)
        sp_cost[i] = DOUBLE_MAX;
    sp_cost[0] = 0;
    predecessor[0] = SIZE_NONE; // NIL

    // Arc construction and relax
    for (size_t i = 1; i <= N; i++) {
        double route_demand = 0;
        double route_cost = 0;

        for (size_t j = i; j <= N; j++) {
            route_demand += cvrp_node_demand (self, route_at (gtour, j-1));

            // Route (depot, i, ..., j-1, j, depot) is not feasible, or arc (i-1, j)
            // does not exist in H.
            // Note: only capacity constraint is considered here. More
            // constraints could be added.
            if (route_demand > self->capacity)
                break;

            // Arc (i-1, j) exists in H
            if (i == j) // route (depot, i, depot)
                route_cost =
                    vrp_arc_distance (self->vrp, depot, route_at (gtour, j-1)) +
                    vrp_arc_distance (self->vrp, route_at (gtour, j-1), depot);
            else
                route_cost = route_cost -
                    vrp_arc_distance (self->vrp, route_at (gtour, j-2), depot) +
                    vrp_arc_distance (self->vrp, route_at (gtour, j-2),
                                                 route_at (gtour, j-1)) +
                    vrp_arc_distance (self->vrp, route_at (gtour, j-1), depot);

            if (sp_cost[i-1] + route_cost < sp_cost[j]) {
                sp_cost[j] = sp_cost[i-1] + route_cost;
                predecessor[j] = i - 1;
            }
        }
    }

    solution_t *sol = solution_new (self->vrp);
    assert (sol);

    size_t j = N;
    size_t i = predecessor[N];

    while (i != SIZE_NONE) {
        // Add route: (depot, i+1, ..., j, depot)
        route_t *route = route_new (2 + j - i);
        assert (route);
        route_append_node (route, depot); // depot
        for (size_t k = i + 1; k <= j; k++)
            route_append_node (route, route_at (gtour, k-1));
        route_append_node (route, depot); // depot
        solution_prepend_route (sol, route);

        j = i;
        i = predecessor[i];
    }

    assert (sp_cost[N] > 0);
    solution_set_total_distance (sol, sp_cost[N]);
    // assert (sp_cost[N] == solution_cal_set_total_distance (sol, self->vrp));

    free (sp_cost);
    free (predecessor);
    return sol;
}


// ----------------------------------------------------------------------------
// Heuristics: CW

typedef struct {
    size_t c1;
    size_t c2;
    double saving;
} s_cwsaving_t;


// Compare savings used by CW: descending order
static int s_cwsaving_compare (const void *a, const void *b) {
    if (((s_cwsaving_t *) a)->saving > ((s_cwsaving_t *) a)->saving)
        return -1;
    if (((s_cwsaving_t *) a)->saving < ((s_cwsaving_t *) a)->saving)
        return 1;
    return 0;
}


// Return solution in which nodes are with generic IDs
static solution_t *cvrp_clark_wright_parallel (cvrp_t *self, double lambda) {
    size_t N = self->num_customers;

    // Initialize with 0
    size_t *predecessors =
        (size_t *) calloc (N + 1, sizeof (size_t));
    assert (predecessors);

    // Initialize with 0
    size_t *successors =
        (size_t *) calloc (N + 1, sizeof (size_t));
    assert (successors);

    double *route_demands =
        (double *) malloc (sizeof (double) * (N + 1));
    assert (route_demands);
    for (size_t i = 1; i <= N; i++)
        route_demands[i] = self->nodes[i].demand;

    size_t num_savings = N * (N -1);
    s_cwsaving_t *savings =
        (s_cwsaving_t *) malloc (sizeof (s_cwsaving_t) * num_savings);
    assert (savings);

    // Calculate savings
    size_t cnt = 0;
    for (size_t i = 1; i <= N; i++) {
        for (size_t j = 1; j <= N && j != i; j++) {
            savings[cnt].c1 = i;
            savings[cnt].c2 = j;
            savings[cnt].saving =
                cvrp_arc_distance_by_idx (self, i, 0) +
                cvrp_arc_distance_by_idx (self, 0, j) -
                lambda * cvrp_arc_distance_by_idx (self, i, j);
            cnt++;
        }
    }

    // Sort savings in descending order
    qsort (savings, num_savings, sizeof (s_cwsaving_t), s_cwsaving_compare);

    // In saving descending order, try to merge two routes at each iteration
    for (size_t idx_saving = 0; idx_saving < num_savings; idx_saving++) {
        size_t c1 = savings[idx_saving].c1;
        size_t c2 = savings[idx_saving].c2;

        // Link c1 -> c2 only when c1 is the last customer of one route and
        // c2 is the first customer of another route.
        if (successors[c1] != 0 || predecessors[c2] != 0)
            continue;

        // Check new route demand <= capacity
        double new_demand = route_demands[c1] + route_demands[c2];
        if (new_demand > self->capacity)
            continue;

        // Merge two routes by connecting c1 -> c2
        predecessors[c2] = c1;
        successors[c1] = c2;

        // Update route demand at first and last customer of new route
        size_t predecessor = c1;
        while (predecessors[predecessor] != 0)
            predecessor = predecessors[predecessor];
        route_demands[predecessor] = new_demand;

        size_t successor = c2;
        while (successors[successor] != 0)
            successor = successors[successor];
        route_demands[successor] = new_demand;
    }

    // Construct solution from predecessors and successors
    solution_t *sol = solution_new (self->vrp);

    for (size_t idx = 1; idx <= N; idx++) {
        if (predecessors[idx] == 0) { // idx is a first customer of route
            route_t *route = route_new (3); // at least 3 nodes in route
            route_append_node (route, self->nodes[0].id); // depot
            size_t successor = idx;
            while (successor != 0) {
                route_append_node (route, self->nodes[successor].id);
                successor = successors[successor];
            }
            route_append_node (route, self->nodes[0].id); // depot
            solution_append_route (sol, route);
        }
    }

    // route_t *gtour = route_new (N);
    // assert (gtour);

    // for (size_t idx = 1; idx <= N; idx++) {
    //     if (predecessors[idx] == 0) { // first customer on route
    //         size_t successor = idx;
    //         while (successor != 0) {
    //             route_append_node (gtour, successor);
    //             successor = successors[successor];
    //         }
    //     }
    // }

    free (predecessors);
    free (successors);
    free (route_demands);
    free (savings);

    return sol;
}


// Heuristic: Clark-Wright.
// is_random: false
// max_expected: 7 (duplicates removed)
static listx_t *cvrp_clark_wright (cvrp_t *self, size_t num_expected) {
    assert (num_expected <= 7);
    print_info ("CW starting ... (expected: %zu)\n", num_expected);
    listx_t *genomes = listx_new ();
    listu_t *hashes = listu_new (7);

    for (double lambda = 0.4; lambda <= 1.0; lambda += 0.1) {
        solution_t *sol = cvrp_clark_wright_parallel (self, lambda);
        route_t *gtour = cvrp_giant_tour_from_solution (self, sol);
        size_t hash = giant_tour_hash (gtour);
        if (!listu_includes (hashes, hash)) {
            solution_cal_set_total_distance (sol, self->vrp);
            s_genome_t *genome = s_genome_new (gtour, sol);
            listx_append (genomes, genome);
            listu_append (hashes, hash);
            solution_print_internal (sol);
            route_print (gtour);
        }
        else { // drop duplicate solution
            solution_free (&sol);
            route_free (&gtour);
        }
    }

    listu_free (&hashes);
    print_info ("generated: %zu\n", listx_size (genomes));
    return genomes;
}


// Heuristics: giant tours constructed by sorting customers according to angle.
// is_random: true
// max_expected: self->num_customers
static listx_t *cvrp_sweep_giant_tours (cvrp_t *self, size_t num_expected) {
    print_info ("sweep giant tours starting ... (expected: %zu)\n",
                num_expected);
    size_t N = self->num_customers;
    if (num_expected > N)
        num_expected = N;

    listx_t *genomes = listx_new ();

    // Create giant tour template: nodes permutated in angle ascending order
    coord2d_t *polars =
        (coord2d_t *) malloc (sizeof (coord2d_t) * N);
    assert (polars);
    for (size_t idx = 0; idx < N; idx++) {
        polars[idx] =
            coord2d_to_polar (self->nodes[idx+1].coord,
                              self->nodes[0].coord,
                              vrp_coord_sys (self->vrp));
        polars[idx].v1 = (double) (idx + 1); // overwrite v1 with customer index
    }
    qsort (polars,
           N,
           sizeof (coord2d_t),
           (comparator_t) coord2d_compare_polar_angle);

    route_t *gtour_template = route_new (N);
    for (size_t idx = 0; idx < N; idx++)
        route_append_node (gtour_template, (size_t) polars[idx].v1);

    // Random rotation of template tour
    size_t random_num = rng_random_int (self->rng, 0, N);
    route_rotate (gtour_template, random_num);

    listu_t *hashes = listu_new (N);
    assert (hashes);

    for (size_t cnt = 0; cnt < num_expected; cnt++) {
        route_t *gtour = route_dup (gtour_template);
        route_rotate (gtour, cnt);

        size_t hash = giant_tour_hash (gtour);
        if (!listu_includes (hashes, hash)) {
            listx_append (genomes, s_genome_new (gtour, NULL));
            listu_append (hashes, hash);

            // for develop: to show the cost
            // s_genome_t *newest = (s_genome_t *) listx_last (genomes);
            // solution_t *sol = cvrp_split (self, newest->gtour);
            // printf ("cost: %.2f\n", solution_total_distance (sol));
            // solution_free (&sol);
        }
        else
            route_free (&gtour);
    }

    print_info ("generated: %zu\n", listx_size (genomes));
    free (polars);
    listu_free (&hashes);
    route_free (&gtour_template);
    return genomes;
}


// Heuristic: random giant tours.
// is_random: true
// max_expected: factorial (self->num_customers)
static listx_t *cvrp_random_giant_tours (cvrp_t *self, size_t num_expected) {
    print_info ("random giant tours starting ... (expected: %zu)\n",
                num_expected);
    size_t max_expected = factorial (self->num_customers);
    if (num_expected > max_expected)
        num_expected = max_expected;

    route_t *gtour_template = route_new (self->num_customers);
    for (size_t idx = 1; idx <= self->num_customers; idx++)
        route_append_node (gtour_template, self->nodes[idx].id);

    listx_t *genomes = listx_new ();
    listu_t *hashes = listu_new (num_expected / 2 + 1);

    for (size_t cnt = 0; cnt < num_expected; cnt++) {
        route_t *gtour = route_dup (gtour_template);
        route_shuffle (gtour, 0, self->num_customers - 1, self->rng);
        size_t hash = giant_tour_hash (gtour);
        if (!listu_includes (hashes, hash)) {
            listx_append (genomes, s_genome_new (gtour, NULL));
            listu_append (hashes, hash);
        }
        else
            route_free (&gtour);
    }

    print_info ("generated: %zu\n", listx_size (genomes));
    route_free (&gtour_template);
    listu_free (&hashes);
    return genomes;
}


// Heuristics: Sweep algorithm (@todo to implement later)
// is_random: true
// max_expected: self->num_customers
// static listx_t *cvrp_sweep (cvrp_t *self, size_t num_expected) {
//     if (num_expected > self->num_customers)
//         num_expected = self->num_customers;

//     listx_t *genomes = listx_new ();
//     for (size_t cnt = 0; cnt < num_expected; cnt++) {

//         route_t *gtour = cvrp_random_giant_tour (self);

//         listx_append (genomes, s_genome_new (gtour, NULL));
//     }
//     return genomes;
// }


// ----------------------------------------------------------------------------
// Evolution

// Fitness of genome: inverse of cost averaged over arcs of splited giant tour.
static double cvrp_genome_fitness (cvrp_t *self, s_genome_t *genome) {
    // If genome->sol does not exists, split the giant tour and save solution
    // in genome with cost in it.
    if (genome->sol == NULL)
        genome->sol = cvrp_split (self, genome->gtour);
    assert (genome->sol != NULL);
    double cost = solution_total_distance (genome->sol);
    assert (cost >= 0);
    return (cost > 0) ? ((self->num_customers + 1) / cost) : 0;
}


// Distance accessor of genome: Levenshtein distance between giant tours
static double cvrp_genome_distance (cvrp_t *self,
                                    s_genome_t *g1, s_genome_t *g2) {
    return arrayu_levenshtein_distance (
                route_node_array (g1->gtour), route_size (g1->gtour),
                route_node_array (g2->gtour), route_size (g2->gtour));
}


// Crossover: OX of giant tours
static listx_t *cvrp_crossover (cvrp_t *self, s_genome_t *g1, s_genome_t *g2) {
    route_t *r1 = route_dup (g1->gtour);
    route_t *r2 = route_dup (g2->gtour);
    route_ox (r1, r2, 0, self->num_customers-1, self->rng);
    listx_t *children = listx_new ();
    listx_append (children, s_genome_new (r1, NULL));
    listx_append (children, s_genome_new (r2, NULL));
    return children;
}


// Local search for evolution: intra-route 2-opt search (stops after first
// improvement)
static void cvrp_local_search_for_evol (cvrp_t *self, s_genome_t *g) {
    if (g->sol == NULL)
        g->sol = cvrp_split (self, g->gtour);
    assert (g->sol != NULL);

    double saving = 0;
    size_t num_routes = solution_num_routes (g->sol);
    for (size_t idx = 0; idx < num_routes; idx++) {
        route_t *route = solution_route (g->sol, idx);
        assert (route_size (route) >= 2);
        saving -=
            route_2_opt (route, self->vrp, 1, route_size (route) - 2, false);
    }

    // Bug fixed: update solution's total distance and update gtour if improved
    if (saving > 0) {
        solution_set_total_distance (
            g->sol,
            solution_total_distance (g->sol) - saving);
        route_free (&g->gtour);
        g->gtour = cvrp_giant_tour_from_solution (self, g->sol);
    }
}


// Post optimization: exhaustive intra-route 2-opt search
// @todo add inter-route local search
static double cvrp_post_optimize (cvrp_t *self, solution_t *sol) {
    double cost_before = solution_total_distance (sol);
    double saving = 0;
    size_t num_routes = solution_num_routes (sol);
    for (size_t idx = 0; idx < num_routes; idx++) {
        route_t *route = solution_route (sol, idx);
        assert (route_size (route) >= 2);
        saving -=
            route_2_opt (route, self->vrp, 1, route_size (route) - 2, true);
    }

    // Update solution's total distance
    if (saving > 0)
        solution_set_total_distance (
            sol,
            solution_total_distance (sol) - saving);

    print_info ("post-optimization improvement: %.3f%% (%.2f -> %.2f)\n",
        saving / cost_before * 100, cost_before, solution_total_distance (sol));
    return saving;
}


// ----------------------------------------------------------------------------

cvrp_t *cvrp_new_from (vrp_t *vrp) {
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


void cvrp_free (cvrp_t **self_p) {
    assert (self_p);
    if (*self_p) {
        cvrp_t *self = *self_p;

        free (self->nodes);
        rng_free (&self->rng);

        free (self);
        *self_p = NULL;
    }
    print_info ("cvrp freed.\n");
}


solution_t *cvrp_solve (cvrp_t *self) {
    assert (self);

    evol_t *evol = evol_new (self);

    evol_set_genome_destructor (evol, (destructor_t) s_genome_free);
    evol_set_fitness_assessor (evol,
                               (evol_fitness_assessor_t) cvrp_genome_fitness);
    evol_set_distance_assessor (evol,
                                (evol_distance_assessor_t) cvrp_genome_distance);

    evol_register_heuristic (evol,
                             (evol_heuristic_t) cvrp_clark_wright, false, 7);
    evol_register_heuristic (evol,
                             (evol_heuristic_t) cvrp_sweep_giant_tours,
                             true,
                             self->num_customers);
    evol_register_heuristic (evol,
                             (evol_heuristic_t) cvrp_random_giant_tours,
                             true,
                             factorial (self->num_customers));

    evol_register_crossover (evol, (evol_crossover_t) cvrp_crossover);
    evol_register_local_improver (
                        evol,
                        (evol_local_improver_t) cvrp_local_search_for_evol);

    evol_run (evol);

    // Get best solution
    const s_genome_t *genome = (s_genome_t *) evol_best_genome (evol);
    assert (genome);
    solution_t *sol = solution_dup (genome->sol);
    solution_print_internal (sol);

    // Post optimization
    cvrp_post_optimize (self, sol);

    evol_free (&evol);
    return sol;
}


void cvrp_test (bool verbose) {
    print_info ("* cvrp: \n");

    // 1. problem derived from generic model

    char filename[] =
        "benchmark/cvrp/A-n32-k5.vrp";
        // "benchmark/tsplib/tsp/berlin52.tsp";
        // "benchmark/tsplib/tsp/a280.tsp";

    vrp_t *vrp = vrp_new_from_file (filename);
    assert (vrp);

    printf ("#nodes: %zu\n", vrp_num_nodes (vrp));
    printf ("#vehicles: %zu\n", vrp_num_vehicles (vrp));

    solution_t *sol = vrp_solve (vrp);
    if (sol != NULL)
        solution_print_internal (sol);

    vrp_free (&vrp);
    assert (vrp == NULL);
    assert (sol != NULL);
    solution_free (&sol);
    assert (sol == NULL);

    print_info ("OK\n");
}
