/*  =========================================================================
    vrptw - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/
/*
Glossary:

- arrival time (at)
- waiting time (wt)
- service time (st)
- service duration (sd)
- departure time (dt)
- earlisest of time window (etw_i)
- latest of time window (ltw_i)
- earliest service time (est): min {etw_i}
- latest service time (lst): max {ltw_i}

Relations (single TW):

feasiblity condition: at <= lst

at_k = dt_(k-1) + arc_duration (k-1, k)

wt = max (etw_i - at, 0), where i = argmin {at <= ltw_i}

st = at + wt = max (at, etw_i)

dt = st + sd = at + wt + sd

Illustration of relations (single TW):

case 1: at < etw

 at   st(=etw)   dt
--|-----|---------|----
     wt      sd

case 2: etw <= at <= ltw

  etw  at(=st)     dt
---|-----|---------|----
       wt=0   sd


Time window of route (single TW case):

etw_k' = etw_k
ltw_k' = min {ltw_k, ltw_(k+1)' - t_(k, k+1) - sd_k}


Implementation details:

nodes in route and solution are represented with inner IDs, and transformed to
generic ID for output.

For local search (in evolution or not), an auxiliary data structure is created
along with the solution which records the time windows informations for
acceleration purpose.

*/

#include "classes.h"


#define SMALL_NUM_NODES 10


// Private node representation
typedef struct {
    size_t id; // node ID in roadgraph of generic model
    double demand;
    const coord2d_t *coord; // reference of node coords in roadgraph
    const listu_t *time_windows; // reference of time windows in request
    size_t service_duration;
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
// Helpers

static double vrptw_arc_distance (vrptw_t *self,
                                  size_t node1_idx, size_t node2_idx) {
    return vrp_arc_distance (self->vrp,
                             self->nodes[node1_idx].id,
                             self->nodes[node2_idx].id);
}


static size_t vrptw_arc_duration (vrptw_t *self,
                                  size_t node1_idx, size_t node2_idx) {
    return vrp_arc_duration (self->vrp,
                             self->nodes[node1_idx].id,
                             self->nodes[node2_idx].id);
}


// Get demand of node. i.e. associated request's quantity
static double vrptw_node_demand (vrptw_t *self, size_t node_idx) {
    return self->nodes[node_idx].demand;
}


// Get number of time windows of node
static size_t vrptw_num_time_windows (vrptw_t *self, size_t node_idx) {
    return listu_size (self->nodes[node_idx].time_windows) / 2;
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


static size_t vrptw_earliest_service_time (vrptw_t *self, size_t node_idx) {
    const listu_t *tws = self->nodes[node_idx].time_windows;
    return (listu_size (tws) > 0) ? listu_get (tws, 0) : 0;
}


static size_t vrptw_latest_service_time (vrptw_t *self, size_t node_idx) {
    const listu_t *tws = self->nodes[node_idx].time_windows;
    size_t size = listu_size (tws);
    return (size > 0) ? listu_get (tws, size - 1) : SIZE_MAX;
}


static size_t vrptw_cal_service_time_by_arrival_time (vrptw_t *self,
                                                      size_t node_idx,
                                                      size_t arrival_time) {
    const listu_t *tws = self->nodes[node_idx].time_windows;
    size_t size = listu_size (tws);
    if (size == 0)
        return arrival_time + self->nodes[node_idx].service_duration;

    size_t idx_tw;
    for (idx_tw = 0; idx_tw < size; idx_tw += 2) {
        size_t ltw = listu_get (tws, idx_tw + 1);
        if (arrival_time <= ltw)
            break; // time window found
    }

    // No time window satisfied, or infeasible
    if (idx_tw > size)
        return SIZE_NONE;

    size_t etw = listu_get (tws, idx_tw);
    return max2 (arrival_time, etw);
}


// Transform VRPTW solution to giant tour representation
static route_t *vrptw_giant_tour_from_solution (vrptw_t *self, solution_t *sol) {
    route_t *gtour = route_new (self->num_customers);

    solution_iterator_t iter = solution_iter_init (sol);
    size_t node;
    while ((node = solution_iter_node (sol, &iter)) != ID_NONE) {
        if (node != self->nodes[0].id)
            route_append_node (gtour, node);
    }

    // for (size_t idx_r = 0; idx_r < solution_num_routes (sol); idx_r++) {
    //     route_t *route = solution_route (sol, idx_r);
    //     for (size_t idx = 0; idx < route_size (route); idx++) {
    //         size_t node = route_at (route, idx);
    //         if (node != self->nodes[0].id)
    //             route_append_node (gtour, route_at (route, idx));
    //     }
    // }
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


// Check if solution is feasible VRPTW solution
static bool vrptw_solution_is_feasible (vrptw_t *self, solution_t *sol) {
    for (size_t idx_r = 0; idx_r < solution_num_routes (sol); idx_r++) {
        route_t *route = solution_route (sol, idx_r);

        // Capacity
        if (vrptw_route_demand (self, route) > self->capacity)
            return false;

        // Time windows
        size_t node = route_at (route, 0);
        size_t departure_time = vrptw_earliest_service_time (self, node);
        for (size_t idx_n = 0; idx_n < route_size (route); idx_n++) {
            size_t last_node = node;
            node = route_at (route, idx_n);
            size_t arrival_time =
                departure_time + vrptw_arc_duration (self, last_node, node);
            if (arrival_time > vrptw_latest_service_time (self, node))
                return false;
            size_t service_time =
                vrptw_cal_service_time_by_arrival_time (self,
                                                        node, arrival_time);
            departure_time = service_time + self->nodes[node].service_duration;
        }
    }
    return true;
}


static void vrptw_print_solution (vrptw_t *self, solution_t *sol) {
    size_t num_routes = solution_num_routes (sol);
    printf ("\nVRPTW solution: #routes: %zu, total distance: %.2f (capacity: %.2f)\n",
            num_routes, solution_total_distance (sol), self->capacity);
    printf ("--------------------------------------------------------------------\n");

    for (size_t idx_r = 0; idx_r < num_routes; idx_r++) {
        route_t *route = solution_route (sol, idx_r);
        size_t route_len = route_size (route);


        printf ("route #%3zu (#nodes: %zu, distance: %.2f, demand: %.2f):\n",
                idx_r, route_len,
                route_total_distance (route,
                                      self->vrp,
                                      (vrp_arc_distance_t) vrp_arc_distance),
                vrptw_route_demand (self, route));

        size_t node = route_at (route, 0);
        size_t departure_time = vrptw_earliest_service_time (self, node);
        size_t arrival_time, service_time;

        for (size_t idx_n = 0; idx_n < route_len; idx_n++) {
            size_t last_node = node;
            node = route_at (route, idx_n);
            arrival_time =
                departure_time + vrptw_arc_duration (self, last_node, node);
            service_time =
                vrptw_cal_service_time_by_arrival_time (self,
                                                        node, arrival_time);
            departure_time =
                (service_time == SIZE_NONE) ?
                SIZE_NONE :
                (service_time + self->nodes[node].service_duration);
            printf ("    %3zu (at: %zu st: %zu dt: %zu) TWs:",
                    node, arrival_time, service_time, departure_time);
            size_t num_tws = listu_size (self->nodes[node].time_windows) / 2;
            for (size_t idx_tw = 0; idx_tw < num_tws; idx_tw++) {
                size_t etw = listu_get (self->nodes[node].time_windows, idx_tw);
                size_t ltw =
                    listu_get (self->nodes[node].time_windows, idx_tw + 1);
                printf (" [%zu, %zu]", etw, ltw);
            }
            printf (" SD: %zu\n", self->nodes[node].service_duration);
        }
        printf ("\n");
    }
    printf ("\n");
}


// @todo
static solution_t *vrptw_split (vrptw_t *self, route_t *gtour) {
    return NULL;
}


// ----------------------------------------------------------------------------
// Genome for evolution

// Time related recording associated with node in route
typedef struct {
    size_t departure_time; // earliest departure time of node
    listu_t *subroute_tws; // time windows of sub-route starting from node
} s_meta_t;


typedef struct {
    route_t *gtour; // giant tour: a sequence of customers: 1 ~ N
    solution_t *sol; // splited giant tour

    // auxiliaries
    s_meta_t *meta;
} s_genome_t;


// Calculate departure time given departure time of last visit
static size_t vrptw_cal_departure_time_by_predecessor (
                                            vrptw_t *self,
                                            size_t node,
                                            size_t predecessor,
                                            size_t departure_time_predecessor) {
    size_t arrival_time = departure_time_predecessor +
                          vrptw_arc_duration (self, predecessor, node);
    size_t service_time =
        vrptw_cal_service_time_by_arrival_time (self,
                                                node, arrival_time);
    return service_time + self->nodes[node].service_duration;
}


// Calculate subroute time windows given time windows of subroute starting
// from successor
static listu_t *vrptw_cal_subroute_tws_by_successor (
                                        vrptw_t *self,
                                        size_t node,
                                        size_t successor,
                                        const listu_t *subroute_tws_successor) {
    size_t latest_arrival_time_successor = listu_last (subroute_tws_successor);
    if (latest_arrival_time_successor == SIZE_NONE)
        return listu_dup (self->nodes[node].time_windows);

    size_t latest_service_time = latest_arrival_time_successor -
                                 vrptw_arc_duration (self, node, successor) -
                                 self->nodes[node].service_duration;
    const listu_t *tws = self->nodes[node].time_windows;

    listu_t *subroute_tws = listu_new (2);
    for (size_t idx_tw = 0; idx_tw < listu_size (tws) / 2; idx_tw += 2) {
        size_t earliest_tw = listu_get (tws, idx_tw);
        size_t latest_tw = listu_get (tws, idx_tw + 1);
        if (latest_tw <= latest_service_time) {
            listu_append (subroute_tws, earliest_tw);
            listu_append (subroute_tws, latest_tw);
        }
        else {
            assert (earliest_tw <= latest_service_time);
            listu_append (subroute_tws, earliest_tw);
            listu_append (subroute_tws, latest_service_time);
            break;
        }
    }
    return subroute_tws;
}


// Calculate meta structure of solution
static s_meta_t *vrptw_cal_solution_meta (vrptw_t *self, solution_t *sol) {
    s_meta_t *meta =
        (s_meta_t *) malloc (sizeof (s_meta_t) * (self->num_customers + 1));
    assert (meta);

    // meta data of depot
    meta[0].departure_time = vrptw_earliest_service_time (self, 0) +
                             self->nodes[0].service_duration;
    meta[0].subroute_tws = listu_dup (self->nodes[0].time_windows);

    // For each route
    for (size_t idx_r = 0; idx_r < solution_num_routes (sol); idx_r++) {
        route_t *route = solution_route (sol, idx_r);
        size_t size = route_size (route);
        assert (size > 2);

        // Calculate earliest departure time in forward direction
        for (size_t idx_n = 1; idx_n < size - 1; idx_n++) {
            size_t predecessor = route_at (route, idx_n - 1);
            size_t node = route_at (route, idx_n);
            meta[node].departure_time =
                vrptw_cal_departure_time_by_predecessor (
                                            self,
                                            node,
                                            route_at (route, idx_n-1),
                                            meta[predecessor].departure_time);
        }

        // Calculate equivalent route TWs in backward direction
        for (size_t idx_n = size - 2; idx_n >= 1; idx_n--) {
            size_t successor = route_at (route, idx_n + 1);
            size_t node = route_at (route, idx_n);
            meta[node].subroute_tws =
                vrptw_cal_subroute_tws_by_successor (
                                                self,
                                                node,
                                                successor,
                                                meta[successor].subroute_tws);
        }
    }

    return meta;
}


// Create genome by setting at least one of gtour or sol.
static s_genome_t *vrptw_new_genome (vrptw_t *self,
                                     route_t *gtour, solution_t *sol) {
    assert (gtour != NULL || sol != NULL);
    s_genome_t *genome = (s_genome_t *) malloc (sizeof (s_genome_t));
    assert (genome);
    genome->gtour = gtour;
    genome->sol = sol;

    if (gtour == NULL)
        genome->gtour = vrptw_giant_tour_from_solution (self, sol);
    if (sol == NULL)
        genome->sol = vrptw_split (self, gtour);
    assert (!double_is_none (solution_total_distance (sol)));
    assert (genome->gtour);
    assert (genome->sol);

    genome->meta = vrptw_cal_solution_meta (self, sol);
    return genome;
}


static void s_genome_free (s_genome_t **self_p) {
    assert (self_p);
    if (*self_p) {
        s_genome_t *self = *self_p;

        for (size_t idx = 0; idx <= route_size (self->gtour); idx++)
            listu_free (&self->meta[idx].subroute_tws);
        free (self->meta);

        route_free (&self->gtour);
        solution_free (&self->sol);

        free (self);
        *self_p = NULL;
    }
}


static int s_genome_compare_cost (s_genome_t *g1, s_genome_t *g2) {
    assert (g1->sol != NULL && g2->sol != NULL);
    if (solution_total_distance (g1->sol) <
        solution_total_distance (g2->sol))
        return -1;
    else if (solution_total_distance (g1->sol) >
             solution_total_distance (g2->sol))
        return 1;
    else
        return 0;
}


// ----------------------------------------------------------------------------
// Heuristics: CW adapted for VRPTW

typedef struct {
    size_t last_visit;
    size_t first_visit;
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


// Sub routine of CW algorithm with specified lambda parameter
static solution_t *vrptw_clark_wright_parallel (vrptw_t *self,
                                                size_t *predecessors,
                                                size_t *successors,
                                                double *route_demands,
                                                s_cwsaving_t *savings,
                                                s_meta_t *meta,
                                                double lambda) {
    size_t N = self->num_customers;

    // Initialize auxilary variables
    size_t cnt = 0;

    meta[0].departure_time = vrptw_earliest_service_time (self, 0) +
                             self->nodes[0].service_duration;
    meta[0].subroute_tws = listu_dup (self->nodes[0].time_windows);

    for (size_t i = 1; i <= N; i++) {
        predecessors[i] = 0;
        successors[i] = 0;
        route_demands[i] = self->nodes[i].demand;
        for (size_t j = 1; j <= N; j++) {
            if (j == i)
                continue;
            savings[cnt].last_visit = i;
            savings[cnt].first_visit = j;
            savings[cnt].saving = vrptw_arc_distance (self, i, 0) +
                                  vrptw_arc_distance (self, 0, j) -
                                  vrptw_arc_distance (self, i, j) * lambda;
            cnt++;
        }

        meta[i].departure_time =
            vrptw_cal_departure_time_by_predecessor (self, i, 0,
                                                     meta[0].departure_time);
        meta[i].subroute_tws =
            vrptw_cal_subroute_tws_by_successor (self, i, 0,
                                                 meta[0].subroute_tws);
    }

    // Sort savings in descending order
    size_t num_savings = N * (N -1);
    assert (cnt == num_savings);
    qsort (savings, num_savings, sizeof (s_cwsaving_t), s_cwsaving_compare);

    // In saving descending order, try to merge two routes at each iteration
    for (size_t idx_saving = 0; idx_saving < num_savings; idx_saving++) {
        size_t last_visit = savings[idx_saving].last_visit;
        size_t first_visit = savings[idx_saving].first_visit;

        // Check if last_visit is the last customer of one route and
        // first_visit is the first customer of another route.
        if (successors[last_visit] != 0 || predecessors[first_visit] != 0)
            continue;

        // Check if last_visit and first_visit are not in the same route
        size_t node = last_visit;
        while (predecessors[node] != 0)
            node = predecessors[node];
        if (node == first_visit)
            continue;

        // Check compatibility of capacity
        double new_demand =
            route_demands[last_visit] + route_demands[first_visit];
        if (new_demand > self->capacity)
            continue;

        // Check compatibility of time windows
        // @todo
        size_t arrival_time =
            meta[last_visit].departure_time +
            vrptw_arc_duration (self, last_visit, first_visit);
        size_t latest_service_time =
            listu_last (meta[first_visit].subroute_tws);
        if (latest_service_time != SIZE_NONE &&
            arrival_time > latest_service_time)
            continue;

        // Merging two routes is feasible. Link last_visit with first_visit.
        predecessors[first_visit] = last_visit;
        successors[last_visit] = first_visit;

        // Update record of route demand, and meta of TWs:
        // departure time for tail, and subroute TWs for head
        node = first_visit;
        while (node != 0) {
            size_t predecessor = predecessors[node];
            meta[node].departure_time =
                vrptw_cal_departure_time_by_predecessor (
                                            self,
                                            node,
                                            predecessor,
                                            meta[predecessor].departure_time);
            route_demands[node] = new_demand;
            node = successors[node];
        }

        node = last_visit;
        while (node != 0) {
            size_t successor = successors[node];
            meta[node].subroute_tws =
                vrptw_cal_subroute_tws_by_successor (
                                                self,
                                                node,
                                                successor,
                                                meta[successor].subroute_tws);
            route_demands[node] = new_demand;
            node = predecessors[node];
        }
    }

    // Construct solution from predecessors and successors
    solution_t *sol = solution_new (self->vrp);
    for (size_t idx = 1; idx <= N; idx++) {
        if (predecessors[idx] == 0) { // idx is first customer of a route
            route_t *route = route_new (3); // a route has at least 3 nodes
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
static listx_t *vrptw_clark_wright (vrptw_t *self, size_t num_expected) {
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
        (s_cwsaving_t *) malloc (sizeof (s_cwsaving_t) * N * (N - 1));
    assert (savings);
    s_meta_t *meta = (s_meta_t *) malloc (sizeof (s_meta_t) * (N + 1));
    assert (meta);

    for (double lambda = 0.4; lambda <= 1.0; lambda += 0.1) {
        solution_t *sol = vrptw_clark_wright_parallel (self,
                                                       predecessors,
                                                       successors,
                                                       route_demands,
                                                       savings,
                                                       meta,
                                                       lambda);
        assert (sol);
        assert (vrptw_solution_is_feasible (self, sol));

        route_t *gtour = vrptw_giant_tour_from_solution (self, sol);
        assert (gtour);

        size_t hash = giant_tour_hash (gtour);
        if (!listu_includes (hashes, hash)) {
            solution_cal_set_total_distance (
                sol, self, (vrp_arc_distance_t) vrptw_arc_distance);
            s_genome_t *genome = vrptw_new_genome (self, gtour, sol);
            assert (genome);
            listx_append (genomes, genome);
            listu_append (hashes, hash);
            // vrptw_print_solution (self, sol);
            // route_print (gtour);
        }
        else { // drop duplicate solution
            solution_free (&sol);
            route_free (&gtour);
        }
    }

    listu_free (&hashes);
    free (predecessors);
    free (successors);
    free (route_demands);
    free (savings);
    for (size_t idx = 0; idx <= N; idx++)
        listu_free (&meta[idx].subroute_tws);
    free (meta);

    print_info ("generated: %zu\n", listx_size (genomes));
    return genomes;
}


// ----------------------------------------------------------------------------

// Small model solver.
// Select best solution of constructive heuristics, and local search
static solution_t *vrptw_solve_small_model (vrptw_t *self) {
    print_info ("solve a small model...\n");

    double min_dist = DOUBLE_MAX;
    listx_t *genomes = NULL;
    s_genome_t *g = NULL;
    solution_t *sol = NULL;

    genomes = vrptw_clark_wright (self, 7);
    listx_set_destructor (genomes, (destructor_t) s_genome_free);
    listx_set_comparator (genomes, (comparator_t) s_genome_compare_cost);
    listx_sort (genomes, true);

    g = (s_genome_t *) listx_first (genomes);
    if (g != NULL && solution_total_distance (g->sol) < min_dist) {
        solution_free (&sol);
        sol = solution_dup (g->sol);
    }
    listx_free (&genomes);

    // genomes = cvrp_sweep_giant_tours (self, self->num_customers);
    // listx_iterator_t iter = listx_iter_init (genomes, true);
    // while ((g = (s_genome_t *) listx_iter (genomes, &iter)) != NULL)
    //     g->sol = cvrp_split (self, g->gtour);
    // listx_set_destructor (genomes, (destructor_t) s_genome_free);
    // listx_set_comparator (genomes, (comparator_t) cvrp_compare_genome_cost);
    // listx_sort (genomes, true);
    // g = listx_first (genomes);
    // if (g != NULL && solution_total_distance (g->sol) < min_dist) {
    //     solution_free (&sol);
    //     sol = solution_dup (g->sol);
    // }
    // listx_free (&genomes);

    // if (sol == NULL) {
    //     genomes = cvrp_random_giant_tours (self, self->num_customers);
    //     while ((g = (s_genome_t *) listx_iter (genomes, &iter)) != NULL)
    //         g->sol = cvrp_split (self, g->gtour);
    //     listx_set_destructor (genomes, (destructor_t) s_genome_free);
    //     listx_set_comparator (genomes, (comparator_t) cvrp_compare_genome_cost);
    //     listx_sort (genomes, true);
    //     g = listx_first (genomes);
    //     if (g != NULL && solution_total_distance (g->sol) < min_dist) {
    //         solution_free (&sol);
    //         sol = solution_dup (g->sol);
    //     }
    //     listx_free (&genomes);
    // }

    assert (sol != NULL);
    // vrptw_print_solution (self, sol);

    // vrptw_post_optimize (self, sol);
    // vrptw_print_solution (self, sol);

    vrptw_print_solution (self, sol);
    return sol;
}


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

        // common sender, or depot
        if (self->nodes[0].id == ID_NONE) {
            self->nodes[0].id = vrp_request_sender (vrp, request);
            self->nodes[0].demand = 0;
            self->nodes[0].coord = vrp_node_coord (vrp, self->nodes[0].id);
            self->nodes[0].time_windows =
                vrp_time_windows (vrp, request, NR_SENDER);
            self->nodes[0].service_duration =
                vrp_service_duration (vrp, request, NR_SENDER);
        }
        assert (self->nodes[0].id == vrp_request_sender (vrp, request));

        // receivers, or customers
        self->nodes[idx+1].id = vrp_request_receiver (vrp, request);
        self->nodes[idx+1].demand = vrp_request_quantity (vrp, request);
        self->nodes[idx+1].coord =
            vrp_node_coord (vrp, self->nodes[idx+1].id);
        self->nodes[idx+1].time_windows =
            vrp_time_windows (vrp, request, NR_RECEIVER);
        self->nodes[idx+1].service_duration =
                vrp_service_duration (vrp, request, NR_RECEIVER);
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
    return vrptw_solve_small_model (self);
}


void vrptw_test (bool verbose) {
    print_info ("* vrptw: \n");

    // char filename[] =
    //     "benchmark/vrptw/solomon_100/R101.txt";
    //     // "benchmark/tsplib/tsp/berlin52.tsp";
    //     // "benchmark/tsplib/tsp/a280.tsp";

    // vrp_t *vrp = vrp_new_from_file (filename);
    // assert (vrp);

    // printf ("#nodes: %zu\n", vrp_num_nodes (vrp));
    // printf ("#vehicles: %zu\n", vrp_num_vehicles (vrp));

    // solution_t *sol = vrp_solve (vrp);
    // if (sol != NULL)
    //     solution_print (sol);

    // vrp_free (&vrp);
    // assert (vrp == NULL);
    // assert (sol != NULL);
    // solution_free (&sol);
    // assert (sol == NULL);

    print_info ("OK\n");
}
