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
- [x] inter-route local search for post optimization
- [ ] more heuristics:
    - [ ] sweep
    - [ ] petal
- [x] small model solver

Details:

giant tour (gtour): a sequence of customers (node IDs in generic model).

For evolution:

- genome: s_genome_t object which includes a giant tour (gtour) and a solution
          (sol). gtour is for crossover, sol is for local search. sol is got
          by split the giant tour.
- heuristics:
    - parameterized CW
    - sweep giant tours
    - random giant tours
- crossover: OX of gtour
- fitness accessor of genome: inverse of cost averaged over arcs
- distance accessor of genomes: levenshtein distance

*/

#include "classes.h"


#define SMALL_NUM_NODES 40


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


// Sum of demand of nodes on a route
static double cvrp_route_demand (cvrp_t *self, route_t *route) {
    double demand = 0;
    for (size_t idx = 0; idx < route_size (route); idx++)
        demand += cvrp_node_demand (self, route_at (route, idx));
    return demand;
}


// Sum of node demands on route slice [idx_from, idx_to]
static double cvrp_route_slice_demand (cvrp_t *self, route_t *route,
                                       size_t idx_from, size_t idx_to) {
    assert (idx_from <= idx_to);
    assert (idx_to < route_size (route));
    double demand = 0;
    for (size_t idx = idx_from; idx <= idx_to; idx++)
        demand += cvrp_node_demand (self, route_at (route, idx));
    return demand;
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


static bool cvrp_solution_is_feasible (cvrp_t *self, solution_t *sol) {
    for (size_t idx = 0; idx < solution_num_routes (sol); idx++) {
        route_t *route = solution_route (sol, idx);
        if (cvrp_route_demand (self, route) > self->capacity)
            return false;
    }
    return true;
}


static void cvrp_print_solution (cvrp_t *self, solution_t *sol) {
    printf ("\nCVRP solution: #routes: %zu, total distance: %.2f (capacity: %.2f)\n",
            solution_num_routes (sol), solution_total_distance (sol), self->capacity);
    printf ("--------------------------------------------------------------------\n");

    for (size_t idx_r = 0; idx_r < solution_num_routes (sol); idx_r++) {
        route_t *route = solution_route (sol, idx_r);
        size_t route_len = route_size (route);
        printf ("route #%3zu (#nodes: %3zu, distance: %6.2f, demand: %6.2f):",
                idx_r, route_len,
                route_total_distance (route,
                                      self->vrp,
                                      (arc_distance_t) vrp_arc_distance),
                cvrp_route_demand (self, route));
        for (size_t idx_n = 0; idx_n < route_len; idx_n++)
            printf (" %zu", route_at (route, idx_n));
        printf ("\n");
    }
    printf ("\n");
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
static solution_t *cvrp_clark_wright_parallel (cvrp_t *self,
                                               size_t *predecessors,
                                               size_t *successors,
                                               double *route_demands,
                                               s_cwsaving_t *savings,
                                               double lambda) {
    size_t N = self->num_customers;

    // Initialize auxilary variables
    size_t cnt = 0;
    for (size_t i = 1; i <= N; i++) {
        predecessors[i] = 0;
        successors[i] = 0;
        route_demands[i] = self->nodes[i].demand;
        for (size_t j = 1; j <= N; j++) {
            if (j == i)
                continue;
            savings[cnt].c1 = i;
            savings[cnt].c2 = j;
            savings[cnt].saving =
                cvrp_arc_distance_by_idx (self, i, 0) +
                cvrp_arc_distance_by_idx (self, 0, j) -
                cvrp_arc_distance_by_idx (self, i, j) * lambda;
            cnt++;
        }
    }

    // Sort savings in descending order
    size_t num_savings = N * (N -1);
    assert (cnt == num_savings);
    qsort (savings, num_savings, sizeof (s_cwsaving_t), s_cwsaving_compare);

    // In saving descending order, try to merge two routes at each iteration
    for (size_t idx_saving = 0; idx_saving < num_savings; idx_saving++) {
        size_t c1 = savings[idx_saving].c1;
        size_t c2 = savings[idx_saving].c2;

        // Check if c1 is the last customer of one route and
        // c2 is the first customer of another route.
        if (successors[c1] != 0 || predecessors[c2] != 0)
            continue;

        // Check if c1 and c2 are already on the same route
        size_t first_visit = c1;
        while (predecessors[first_visit] != 0)
            first_visit = predecessors[first_visit];
        if (first_visit == c2)
            continue;

        // Check if new route demand <= capacity
        double new_demand = route_demands[c1] + route_demands[c2];
        if (new_demand > self->capacity)
            continue;

        // Merge two routes by connecting c1 -> c2
        predecessors[c2] = c1;
        successors[c1] = c2;

        // Update record of route demand
        size_t last_visit = c2;
        while (successors[last_visit] != 0)
            last_visit = successors[last_visit];
        route_demands[first_visit] = new_demand;
        route_demands[last_visit] = new_demand;
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
    return sol;
}


// Heuristic: Clark-Wright.
// is_random: false
// max_expected: 7 (duplicates removed)
static listx_t *cvrp_clark_wright (cvrp_t *self, size_t num_expected) {
    print_info ("CW starting ... (expected: %zu)\n", num_expected);
    if (num_expected > 7)
        num_expected = 7;
    size_t N = self->num_customers;
    listx_t *genomes = listx_new ();
    listu_t *hashes = listu_new (7);

    size_t *predecessors = (size_t *) malloc (sizeof (size_t) * (N + 1));
    assert (predecessors);
    size_t *successors = (size_t *) malloc (sizeof (size_t) * (N + 1));
    assert (successors);
    double *route_demands = (double *) malloc (sizeof (double) * (N + 1));
    assert (route_demands);
    s_cwsaving_t *savings =
        (s_cwsaving_t *) malloc (sizeof (s_cwsaving_t) * N * (N -1));
    assert (savings);

    for (double lambda = 0.4; lambda <= 1.0; lambda += 0.1) {
        solution_t *sol = cvrp_clark_wright_parallel (self,
                                                      predecessors,
                                                      successors,
                                                      route_demands,
                                                      savings,
                                                      lambda);

        route_t *gtour = cvrp_giant_tour_from_solution (self, sol);
        size_t hash = giant_tour_hash (gtour);
        if (!listu_includes (hashes, hash)) {
            solution_cal_set_total_distance (sol,
                                             self->vrp,
                                             (arc_distance_t) vrp_arc_distance);
            s_genome_t *genome = s_genome_new (gtour, sol);
            listx_append (genomes, genome);
            listu_append (hashes, hash);
            cvrp_print_solution (self, sol);
            // route_print (gtour);
        }
        else { // drop duplicate solution
            solution_free (&sol);
            route_free (&gtour);
        }
    }

    print_info ("generated: %zu\n", listx_size (genomes));
    listu_free (&hashes);
    free (predecessors);
    free (successors);
    free (route_demands);
    free (savings);
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


// ----------------------------------------------------------------------------
// Local search

// Local search: inter-route or-opt of node: relocate one node to another route
static double cvrp_or_opt_node (cvrp_t *self, solution_t *sol, bool exhaustive) {
    double saving = 0;
    bool improved = true;

    while (improved) {
        improved = false;

        solution_iterator_t iter1 = solution_iter_init (sol);
        while (solution_iter_node (sol, &iter1) != ID_NONE && !improved) {
            if (iter1.node_id == self->nodes[0].id) // ignore depot
                continue;

            // Removal of customer node
            double dcost_remove =
                route_remove_node_delta_distance (iter1.route,
                                                  iter1.idx_node,
                                                  self->vrp,
                                                  (arc_distance_t) vrp_arc_distance);
            double node_demand = cvrp_node_demand (self, iter1.node_id);

            // Try to insert node to another route
            solution_iterator_t iter2 = solution_iter_init (sol);
            while (solution_iter_node (sol, &iter2) != ID_NONE && !improved) {
                // Ignore same route and first node (depot)
                if (iter1.idx_route == iter2.idx_route || iter2.idx_node == 0)
                    continue;

                // Feasibility check: capacity
                double route_demand = cvrp_route_demand (self, iter2.route);
                if (route_demand + node_demand > self->capacity)
                    continue;

                double dcost_insert =
                    route_insert_node_delta_distance (iter2.route,
                                                      iter2.idx_node,
                                                      iter1.node_id,
                                                      self->vrp,
                                                      (arc_distance_t) vrp_arc_distance);
                double dcost = dcost_remove + dcost_insert;
                if (dcost < 0) {
                    // printf ("improved: %.2f\n", -dcost);
                    route_remove_node (iter1.route, iter1.idx_node);
                    route_insert_node (iter2.route, iter2.idx_node, iter1.node_id);
                    // Remove route if it is empty (only depot nodes left)
                    if (route_size (iter1.route) == 2)
                        solution_remove_route (sol, iter1.idx_route);
                    saving -= dcost;
                    solution_set_total_distance (
                        sol,
                        solution_total_distance (sol) + dcost);
                    if (!exhaustive) {
                        // print_info ("or-opt-node saving: %.2f\n", saving);
                        return saving;
                    }
                    improved = true;
                }
            }
        }
    }
    // print_info ("or-opt-node saving (end): %.2f\n", saving);
    return saving;
}


// Local search: inter-route or-opt of two consecutive nodes
// @todo not implemented
static double cvrp_or_opt_link (cvrp_t *self, solution_t *sol, bool exhaustive) {
    return 0;
}


// Local search: inter-route exchange: swap two nodes of two routes
static double cvrp_exchange_nodes (cvrp_t *self,
                                   solution_t *sol, bool exhaustive) {
    double saving = 0;
    bool improved = true;

    while (improved) {
        improved = false;

        solution_iterator_t iter1 = solution_iter_init (sol);
        while (solution_iter_node (sol, &iter1) != ID_NONE && !improved) {
            if (iter1.node_id == self->nodes[0].id) // ignore depot
                continue;

            double route1_demand = cvrp_route_demand (self, iter1.route);
            double node1_demand = cvrp_node_demand (self, iter1.node_id);

            solution_iterator_t iter2 = solution_iter_init (sol);
            while (solution_iter_node (sol, &iter2) != ID_NONE && !improved) {
                // start from next route and ignore depot
                if (iter2.idx_route <= iter1.idx_route ||
                    iter2.node_id == self->nodes[0].id)
                    continue;

                // Feasibility check: capacity of two routes
                double node2_demand = cvrp_node_demand (self, iter2.node_id);
                if (route1_demand - node1_demand + node2_demand >
                    self->capacity)
                    continue;
                if (cvrp_route_demand (self, iter2.route) -
                    node2_demand + node1_demand >
                    self->capacity)
                    continue;

                // assert (route_at (iter1.route, iter1.idx_node) == iter1.node_id);
                // assert (route_at (iter2.route, iter2.idx_node) == iter2.node_id);

                // dcost of exchanging iter1.node_id and iter2.node_id
                double dcost =
                    route_exchange_nodes_delta_distance (iter1.route,
                                                         iter2.route,
                                                         iter1.idx_node,
                                                         iter2.idx_node,
                                                         self->vrp,
                                                         (arc_distance_t) vrp_arc_distance);
                // double dcost_old =
                //     route_replace_node_delta_distance (iter1.route,
                //                                        iter1.idx_node,
                //                                        iter2.node_id,
                //                                        self->vrp) +
                //     route_replace_node_delta_distance (iter2.route,
                //                                        iter2.idx_node,
                //                                        iter1.node_id,
                //                                        self->vrp);

                // printf ("dcost: %.3f, dcost_old: %.3f\n", dcost,dcost_old);
                // assert (double_equal (dcost, dcost_old));


                if (dcost < 0) {
                    // printf ("---------------------------\n");
                    // route_print (iter1.route);
                    // printf ("idx_node: %zu, node_id: %zu\n", iter1.idx_node, iter1.node_id);
                    // route_print (iter2.route);
                    // printf ("idx_node: %zu, node_id: %zu\n", iter2.idx_node, iter2.node_id);

                    route_exchange_nodes (iter1.route, iter2.route,
                                          iter1.idx_node, iter2.idx_node);

                    // route_set_at (iter1.route, iter1.idx_node, iter2.node_id);
                    // route_set_at (iter2.route, iter2.idx_node, iter1.node_id);

                    // route_print (iter1.route);
                    // route_print (iter2.route);

                    saving -= dcost;
                    solution_increase_total_distance (sol, dcost);
                    if (!exhaustive) {
                        // print_info ("exchange saving: %.3f\n", saving);
                        return saving;
                    }
                    improved = true;
                }
            }
        }
    }
    // print_info ("exchange saving (end): %.3f\n", saving);
    return saving;
}


// Local search: inter-route 2-opt* (exchange tails of two routes)
static double cvrp_2_opt_star (cvrp_t *self, solution_t *sol, bool exhaustive) {
    double saving = 0;
    bool improved = true;

    while (improved) {
        improved = false;

        solution_iterator_t iter1 = solution_iter_init (sol);
        while (solution_iter_node (sol, &iter1) != ID_NONE && !improved) {

            // Ignore last depot node
            if (iter1.idx_node == route_size (iter1.route) - 1)
                continue;

            solution_iterator_t iter2 = solution_iter_init (sol);
            while (solution_iter_node (sol, &iter2) != ID_NONE && !improved) {

                // start from next route
                if (iter2.idx_route <= iter1.idx_route)
                    continue;

                if (iter1.node_id == self->nodes[0].id &&
                    iter2.node_id == self->nodes[0].id)
                    continue;

                if (iter2.idx_node == route_size (iter2.route) - 1)
                    continue;

                // Feasibility check: capacity of two routes
                if (cvrp_route_slice_demand (self, iter1.route,
                                             0,
                                             iter1.idx_node) +
                    cvrp_route_slice_demand (self, iter2.route,
                                             iter2.idx_node + 1,
                                             route_size (iter2.route) - 1) >
                    self->capacity)
                    continue;

                if (cvrp_route_slice_demand (self, iter2.route,
                                             0,
                                             iter2.idx_node) +
                    cvrp_route_slice_demand (self, iter1.route,
                                             iter1.idx_node + 1,
                                             route_size (iter1.route) - 1) >
                    self->capacity)
                    continue;

                double dcost =
                    route_exchange_tails_delta_distance (iter1.route,
                                                         iter2.route,
                                                         iter1.idx_node,
                                                         iter2.idx_node,
                                                         self->vrp,
                                                         (arc_distance_t) vrp_arc_distance);

                if (dcost < 0) {
                    // printf ("2-opt* ---------------------------\n");
                    // route_print (iter1.route);
                    // printf ("idx_node: %zu, node_id: %zu\n", iter1.idx_node, iter1.node_id);
                    // route_print (iter2.route);
                    // printf ("idx_node: %zu, node_id: %zu\n", iter2.idx_node, iter2.node_id);

                    route_exchange_tails (iter1.route, iter2.route,
                                          iter1.idx_node, iter2.idx_node);

                    // route_print (iter1.route);
                    // route_print (iter2.route);

                    // Remove route if it is empty (only two depot nodes left)
                    if (route_size (iter1.route) == 2)
                        solution_remove_route (sol, iter1.idx_route);
                    if (route_size (iter2.route) == 2)
                        solution_remove_route (sol, iter2.idx_route);

                    saving -= dcost;
                    solution_set_total_distance (
                        sol,
                        solution_total_distance (sol) + dcost);
                    if (!exhaustive) {
                        // print_info ("2-opt* saving: %.3f\n", saving);
                        return saving;
                    }
                    improved = true;
                }
            }
        }
    }
    // print_info ("2-opt* saving (end): %.3f\n", saving);
    return saving;
}


// Local search: intra-route 2-opt
static double cvrp_2_opt (cvrp_t *self, solution_t *sol, bool exhaustive) {
    double saving = 0, ddist;
    bool improved = true;
    size_t num_routes = solution_num_routes (sol);

    while (improved) {
        improved = false;

        // For each route in solution
        for (size_t idx = 0; idx < num_routes && !improved; idx++) {
            route_t *route = solution_route (sol, idx);
            size_t size = route_size (route);
            assert (size >= 2);

            // For each possible route sile
            for (size_t i = 1; i < size - 2 && !improved; i++) {
                for (size_t j = i + 1; j <= size - 2 && !improved; j++) {
                    ddist =
                        route_reverse_delta_distance (route, i, j, self->vrp,
                                                      (arc_distance_t) vrp_arc_distance);
                    if (ddist < 0) {
                        route_reverse (route, i, j);
                        saving -= ddist;
                        solution_increase_total_distance (sol, ddist);
                        if (!exhaustive)
                            return saving;
                        improved = true;
                    }
                }
            }
        }
    }
    return saving;
}


// ----------------------------------------------------------------------------

// Local search used in evolution
static void cvrp_local_search_for_evol (cvrp_t *self, s_genome_t *g) {
    if (g->sol == NULL)
        g->sol = cvrp_split (self, g->gtour);
    assert (g->sol != NULL);

    double saving;
    while (true) {
        saving = cvrp_or_opt_node (self, g->sol, false);
        if (saving > 0)
            break;

        saving = cvrp_2_opt (self, g->sol, false);
        break;
    }

    // Bug fixed: update gtour if improved
    if (saving > 0) {
        route_free (&g->gtour);
        g->gtour = cvrp_giant_tour_from_solution (self, g->sol);
    }
}


// Post optimization by local search.
// Return saving.
static double cvrp_post_optimize (cvrp_t *self, solution_t *sol) {
    double cost_before = solution_total_distance (sol);
    double total_saving = 0;
    bool improved = true;
    double saving;

    print_info ("cal cost before post optimization: %.2f\n",
                solution_cal_total_distance (sol,
                                             self->vrp,
                                             (arc_distance_t) vrp_arc_distance));

    while (improved) {
        improved = false;

        assert (cvrp_solution_is_feasible (self, sol));

        // Or-opt of node
        saving = cvrp_or_opt_node (self, sol, false);
        if (saving > 0) {
            print_info ("or-opt saving: %.2f\n", saving);
            total_saving += saving;
            improved = true;
            continue;
        }

        // Exchange of nodes
        saving = cvrp_exchange_nodes (self, sol, false);
        if (saving > 0) {
            print_info ("exchange saving: %.2f\n", saving);
            total_saving += saving;
            improved = true;
            continue;
        }

        // intra-route 2-opt
        saving = cvrp_2_opt (self, sol, false);
        if (saving > 0) {
            print_info ("2-opt saving: %.2f\n", saving);
            total_saving += saving;
            improved = true;
            continue;
        }

        // 2-opt*
        saving = cvrp_2_opt_star (self, sol, false);
        if (saving > 0) {
            print_info ("2-opt* saving: %.2f\n", saving);
            total_saving += saving;
            improved = true;
            continue;
        }
    }

    print_info ("cal cost after post optimization: %.2f\n",
                solution_cal_total_distance (sol,
                                             self->vrp,
                                             (arc_distance_t) vrp_arc_distance));
    print_info ("post-optimization improvement: %.3f%% (%.2f -> %.2f)\n",
                total_saving / cost_before * 100,
                cost_before, solution_total_distance (sol));
    return saving;
}


static int cvrp_compare_genome_cost (s_genome_t *g1, s_genome_t *g2) {
    assert (g1->sol != NULL && g2->sol != NULL);
    // assert (solution_total_distance (g1->sol) > 0);
    // assert (solution_total_distance (g2->sol) > 0);

    if (solution_total_distance (g1->sol) <
        solution_total_distance (g2->sol))
        return -1;
    else if (solution_total_distance (g1->sol) >
             solution_total_distance (g2->sol))
        return 1;
    else
        return 0;
}


// Small model solver.
// Select best solution of constructive heuristics, and local search
static solution_t *cvrp_solve_small_model (cvrp_t *self) {
    print_info ("solve a small model...\n");

    double min_dist = DOUBLE_MAX;
    listx_t *genomes = NULL;
    s_genome_t *g = NULL;
    solution_t *sol = NULL;

    genomes = cvrp_clark_wright (self, 7);
    listx_set_destructor (genomes, (destructor_t) s_genome_free);
    listx_set_comparator (genomes, (comparator_t) cvrp_compare_genome_cost);
    listx_sort (genomes, true);
    g = listx_first (genomes);
    if (g != NULL && solution_total_distance (g->sol) < min_dist) {
        solution_free (&sol);
        sol = solution_dup (g->sol);
    }
    listx_free (&genomes);

    genomes = cvrp_sweep_giant_tours (self, self->num_customers);
    listx_iterator_t iter = listx_iter_init (genomes, true);
    while ((g = (s_genome_t *) listx_iter (genomes, &iter)) != NULL)
        g->sol = cvrp_split (self, g->gtour);
    listx_set_destructor (genomes, (destructor_t) s_genome_free);
    listx_set_comparator (genomes, (comparator_t) cvrp_compare_genome_cost);
    listx_sort (genomes, true);
    g = listx_first (genomes);
    if (g != NULL && solution_total_distance (g->sol) < min_dist) {
        solution_free (&sol);
        sol = solution_dup (g->sol);
    }
    listx_free (&genomes);

    if (sol == NULL) {
        genomes = cvrp_random_giant_tours (self, self->num_customers);
        while ((g = (s_genome_t *) listx_iter (genomes, &iter)) != NULL)
            g->sol = cvrp_split (self, g->gtour);
        listx_set_destructor (genomes, (destructor_t) s_genome_free);
        listx_set_comparator (genomes, (comparator_t) cvrp_compare_genome_cost);
        listx_sort (genomes, true);
        g = listx_first (genomes);
        if (g != NULL && solution_total_distance (g->sol) < min_dist) {
            solution_free (&sol);
            sol = solution_dup (g->sol);
        }
        listx_free (&genomes);
    }

    assert (sol != NULL);
    cvrp_print_solution (self, sol);

    cvrp_post_optimize (self, sol);
    cvrp_print_solution (self, sol);
    return sol;
}


// ----------------------------------------------------------------------------

cvrp_t *cvrp_new_from_generic (vrp_t *vrp) {
    assert (vrp);

    cvrp_t *self = (cvrp_t *) malloc (sizeof (cvrp_t));
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

    // Deal with small numboer of nodes
    if (self->num_customers <= SMALL_NUM_NODES)
        return cvrp_solve_small_model (self);

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
    evol_register_educator (evol, (evol_educator_t) cvrp_local_search_for_evol);

    evol_run (evol);

    // Get best solution
    const s_genome_t *genome = (s_genome_t *) evol_best_genome (evol);
    assert (genome);
    solution_t *sol = solution_dup (genome->sol);
    cvrp_print_solution (self, sol);

    // Post optimization
    cvrp_post_optimize (self, sol);
    cvrp_print_solution (self, sol);

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
        solution_print (sol);

    vrp_free (&vrp);
    assert (vrp == NULL);
    assert (sol != NULL);
    solution_free (&sol);
    assert (sol == NULL);

    print_info ("OK\n");
}
