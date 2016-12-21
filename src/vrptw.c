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


#define SMALL_NUM_NODES 200


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

static double vrptw_arc_distance (const vrptw_t *self,
                                  size_t node1_idx, size_t node2_idx) {
    return vrp_arc_distance (self->vrp,
                             self->nodes[node1_idx].id,
                             self->nodes[node2_idx].id);
}


static size_t vrptw_arc_duration (const vrptw_t *self,
                                  size_t node1_idx, size_t node2_idx) {
    return vrp_arc_duration (self->vrp,
                             self->nodes[node1_idx].id,
                             self->nodes[node2_idx].id);
}


// Get demand of node. i.e. associated request's quantity
static double vrptw_node_demand (const vrptw_t *self, size_t node_idx) {
    return self->nodes[node_idx].demand;
}


// Get number of time windows of node
static size_t vrptw_num_time_windows (const vrptw_t *self, size_t node_idx) {
    return listu_size (self->nodes[node_idx].time_windows) / 2;
}


// Sum of demand of nodes on a route
static double vrptw_route_demand (const vrptw_t *self, const route_t *route) {
    double demand = 0;
    for (size_t idx = 0; idx < route_size (route); idx++)
        demand += vrptw_node_demand (self, route_at (route, idx));
    return demand;
}


// Sum of node demands on route slice [idx_from, idx_to]
static double vrptw_route_slice_demand (const vrptw_t *self,
                                        const route_t *route,
                                       size_t idx_from, size_t idx_to) {
    assert (idx_from <= idx_to);
    assert (idx_to < route_size (route));
    double demand = 0;
    for (size_t idx = idx_from; idx <= idx_to; idx++)
        demand += vrptw_node_demand (self, route_at (route, idx));
    return demand;
}


// Earliest possible service time of node
static size_t vrptw_earliest_service_time (const vrptw_t *self,
                                           size_t node_idx) {
    const listu_t *tws = self->nodes[node_idx].time_windows;
    return (listu_size (tws) > 0) ? listu_get (tws, 0) : 0;
}


// Latest possible service time of node
static size_t vrptw_latest_service_time (const vrptw_t *self, size_t node_idx) {
    const listu_t *tws = self->nodes[node_idx].time_windows;
    size_t size = listu_size (tws);
    return (size > 0) ? listu_get (tws, size - 1) : SIZE_MAX;
}


// Determine service time given arrival time and time windows.
// Return SIZE_NONE if no time window fits.
static size_t service_time_by_arrival_time (const listu_t *time_windows,
                                            size_t arrival_time) {
    // No time windows
    if (time_windows == NULL)
        return arrival_time;
    size_t size = listu_size (time_windows);
    if (size == 0)
        return arrival_time;

    // No time window satisfied, or infeasible
    if (arrival_time > listu_last (time_windows))
        return SIZE_NONE;

    size_t idx_tw;
    for (idx_tw = 0; idx_tw < size; idx_tw += 2) {
        size_t ltw = listu_get (time_windows, idx_tw + 1);
        if (arrival_time <= ltw)
            break; // time window found
    }
    assert (idx_tw < size - 1);

    size_t etw = listu_get (time_windows, idx_tw);
    return max2 (arrival_time, etw);
}


// Determine service time given arrival time.
// Return SIZE_NONE if no time window fits.
static size_t vrptw_cal_service_time_by_arrival_time (const vrptw_t *self,
                                                      size_t node_idx,
                                                      size_t arrival_time) {
    return service_time_by_arrival_time (self->nodes[node_idx].time_windows,
                                         arrival_time);
}


// Transform VRPTW solution to giant tour representation
static route_t *vrptw_giant_tour_from_solution (const vrptw_t *self,
                                                const solution_t *sol) {
    route_t *gtour = route_new (self->num_customers);

    solution_iterator_t iter = solution_iter_init (sol);
    size_t node;
    while ((node = solution_iter_node (sol, &iter)) != ID_NONE) {
        if (node != 0)
            route_append_node (gtour, node);
    }

    assert (route_size (gtour) == self->num_customers);
    return gtour;
}


// Simple hash function of giant tour which could discriminate permutations
static size_t giant_tour_hash (const route_t *gtour) {
    size_t hash = 0;
    for (size_t idx = 0; idx < route_size (gtour); idx++)
        hash += route_at (gtour, idx) * idx;
    return hash;
}


// Check if solution is feasible VRPTW solution
static bool vrptw_solution_is_feasible (const vrptw_t *self,
                                        const solution_t *sol) {
    for (size_t idx_r = 0; idx_r < solution_num_routes (sol); idx_r++) {
        route_t *route = solution_route (sol, idx_r);

        // Capacity
        if (vrptw_route_demand (self, route) > self->capacity)
            return false;

        // Time windows
        size_t node = route_at (route, 0);
        size_t departure_time = vrptw_earliest_service_time (self, 0);
        size_t arrival_time, service_time;
        for (size_t idx_n = 0; idx_n < route_size (route); idx_n++) {
            size_t last_node = node;
            node = route_at (route, idx_n);
            arrival_time = departure_time +
                           vrptw_arc_duration (self, last_node, node);
            if (arrival_time > vrptw_latest_service_time (self, node))
                return false;
            service_time =
                vrptw_cal_service_time_by_arrival_time (self,
                                                        node, arrival_time);
            departure_time = service_time + self->nodes[node].service_duration;
        }
    }
    return true;
}


static void vrptw_print_solution (const vrptw_t *self, const solution_t *sol) {
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


// Split algorithm: giant tour -> VRPTW solution
// Notes:
// Shortest path algorithm is performed on directed acyclic graph (auxiliary
// graph H). In H, arc (i, j) exists means route (depot, i+1, ..., j, depot)
// is feasible.
// Index i in H is cooresponed to index (i-1) in giant tour.
// Ref: Prins 2004
static solution_t *vrptw_split (const vrptw_t *self, const route_t *gtour) {
    size_t N = self->num_customers;
    size_t depot = 0;

    // Cost of the shortest path from node 0 to node (1 ~ N) in H
    double *sp_costs = (double *) malloc ((N + 1) * sizeof (double));
    assert (sp_costs);

    // Predecessors of nodes on the shortest path
    size_t *predecessors = (size_t *) malloc ((N + 1) * sizeof (size_t));
    assert (predecessors);

    // Initialize
    for (size_t i = 0; i <= N; i++)
        sp_costs[i] = DOUBLE_MAX;
    sp_costs[0] = 0;
    predecessors[0] = SIZE_NONE; // NIL

    // Arc construction and relax. i and j are node indice in H.
    for (size_t i = 1; i <= N; i++) {
        double route_demand = 0;
        double route_distance = 0;
        size_t departure_time = vrptw_earliest_service_time (self, depot) +
                                self->nodes[depot].service_duration;
        size_t last_node = depot;

        for (size_t j = i; j <= N; j++) {
            size_t node = route_at (gtour, j-1);

            // Check feasibility of route (depot, i, ..., j, depot),
            // or whether arc (i-1, j) exists in H.

            // Compatibility of capacity
            route_demand += vrptw_node_demand (self, node);
            if (route_demand > self->capacity)
                break;

            // Compatibility of time windows
            size_t arrival_time = departure_time +
                                  vrptw_arc_duration (self, last_node, node);
            if (arrival_time > vrptw_latest_service_time (self, node))
                break;

            // Feasibility check passed
            size_t service_time =
                vrptw_cal_service_time_by_arrival_time (self,
                                                        node,
                                                        arrival_time);
            departure_time = service_time + self->nodes[node].service_duration;
            last_node = node;

            if (i == j) // route (depot, node, depot)
                route_distance =
                    vrptw_arc_distance (self, depot, node) +
                    vrptw_arc_distance (self, node, depot);
            else
                route_distance = route_distance -
                    vrptw_arc_distance (self, last_node, depot) +
                    vrptw_arc_distance (self, last_node, node) +
                    vrptw_arc_distance (self, node, depot);

            if (sp_costs[i-1] + route_distance < sp_costs[j]) {
                sp_costs[j] = sp_costs[i-1] + route_distance;
                predecessors[j] = i - 1;
            }
        }
    }

    // Construct solution
    solution_t *sol = solution_new (self->vrp);
    assert (sol);

    size_t j = N;
    size_t i = predecessors[N];

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
        i = predecessors[i];
    }

    assert (sp_costs[N] > 0);
    solution_set_total_distance (sol, sp_costs[N]);
    // assert (sp_costs[N] == solution_cal_set_total_distance (sol, self->vrp));

    free (sp_costs);
    free (predecessors);

    assert (vrptw_solution_is_feasible (self, sol));
    return sol;
}


// ----------------------------------------------------------------------------
// Meta of solution

// Time related recording associated with node in route
typedef struct {
    size_t departure_time; // earliest departure time of node
    listu_t *subroute_tws; // time windows of sub-route starting from node
} s_meta_item_t;


typedef struct {
    size_t size;
    s_meta_item_t *data;
} s_meta_t;


// Calculate departure time given departure time of last visit.
// Return SIZE_NONE if visiting node within time windows is infeasible.
static size_t vrptw_cal_departure_time_by_predecessor (
                                            const vrptw_t *self,
                                            size_t node,
                                            size_t predecessor,
                                            size_t departure_time_predecessor) {
    size_t arrival_time = departure_time_predecessor +
                          vrptw_arc_duration (self, predecessor, node);
    size_t service_time =
        vrptw_cal_service_time_by_arrival_time (self,
                                                node, arrival_time);
    return (service_time == SIZE_NONE) ?
           SIZE_NONE :
           service_time + self->nodes[node].service_duration;
}


// Calculate subroute time windows given time windows of subroute starting
// from successor
static listu_t *vrptw_cal_subroute_tws_by_successor (
                                        const vrptw_t *self,
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


// Create meta structure from solution
static s_meta_t *s_meta_new (const vrptw_t *vrptw, const solution_t *sol) {
    assert (sol);

    s_meta_t *self = (s_meta_t *) malloc (sizeof (s_meta_t));
    assert (self);
    self->size = vrptw->num_customers + 1;
    self->data = (s_meta_item_t *) malloc (sizeof (s_meta_t) * self->size);
    assert (self->data);

    s_meta_item_t *data = self->data;

    // meta data of depot
    data[0].departure_time = vrptw_earliest_service_time (vrptw, 0) +
                             vrptw->nodes[0].service_duration;
    data[0].subroute_tws = listu_dup (vrptw->nodes[0].time_windows);

    // For each route
    for (size_t idx_r = 0; idx_r < solution_num_routes (sol); idx_r++) {
        route_t *route = solution_route (sol, idx_r);
        size_t size = route_size (route);
        assert (size > 2);

        // Calculate earliest departure time in forward direction
        for (size_t idx_n = 1; idx_n < size - 1; idx_n++) {
            size_t predecessor = route_at (route, idx_n - 1);
            size_t node = route_at (route, idx_n);
            data[node].departure_time =
                vrptw_cal_departure_time_by_predecessor (
                                            vrptw,
                                            node,
                                            route_at (route, idx_n-1),
                                            data[predecessor].departure_time);
        }

        // Calculate equivalent route TWs in backward direction
        for (size_t idx_n = size - 2; idx_n >= 1; idx_n--) {
            size_t successor = route_at (route, idx_n + 1);
            size_t node = route_at (route, idx_n);
            data[node].subroute_tws =
                vrptw_cal_subroute_tws_by_successor (
                                                vrptw,
                                                node,
                                                successor,
                                                data[successor].subroute_tws);
        }
    }

    return self;
}


// Destroy meta structure
static void s_meta_free (s_meta_t **meta_p) {
    assert (meta_p);
    if (*meta_p) {
        s_meta_t *self = *meta_p;
        for (size_t idx = 0; idx < self->size; idx++)
            listu_free (&self->data[idx].subroute_tws);
        free (self);
        *meta_p = NULL;
    }
}


// Get departure time of subroute ending at node
static size_t s_meta_departure_time (const s_meta_t *self, size_t idx) {
    assert (idx < self->size);
    return self->data[idx].departure_time;
}


// Get subroute TWs starting at node
static listu_t *s_meta_subroute_tws (const s_meta_t *self, size_t idx) {
    assert (idx < self->size);
    return self->data[idx].subroute_tws;
}


// Check TW compatability of inserting node before successor in route
static bool s_meta_insert_node_is_feasible (const s_meta_t *self,
                                            const vrptw_t *vrptw,
                                            const route_t *route,
                                            size_t node,
                                            size_t idx_successor) {
    assert (idx_successor > 0);
    size_t predecessor = route_at (route, idx_successor - 1);
    size_t successor = route_at (route, idx_successor);
    size_t departure_time_at_node =
        vrptw_cal_departure_time_by_predecessor (
                                        vrptw,
                                        node,
                                        predecessor,
                                        self->data[predecessor].departure_time);
    if (departure_time_at_node == SIZE_NONE)
        return false;

    size_t arrival_time_at_successor =
        departure_time_at_node + vrptw_arc_duration (vrptw, node, successor);
    size_t service_time_at_successor =
        service_time_by_arrival_time (self->data[successor].subroute_tws,
                                      arrival_time_at_successor);
    if (service_time_at_successor == SIZE_NONE)
        return false;

    return true;
}


// Modify meta when inserting node before successor in route.
// Insertion may happened or not.
static void s_meta_insert_node (s_meta_t *self,
                                const vrptw_t *vrptw,
                                const route_t *route,
                                size_t node,
                                size_t idx_successor) {
    assert (idx_successor > 0);

    size_t predecessor = route_at (route, idx_successor - 1);
    self->data[node].departure_time =
        vrptw_cal_departure_time_by_predecessor (
                                        vrptw,
                                        node,
                                        predecessor,
                                        self->data[predecessor].departure_time);

    predecessor = node;
    for (size_t idx = idx_successor; idx < route_size (route) - 1; idx++) {
        size_t current_node = route_at (route, idx);
        size_t departure_time =
            vrptw_cal_departure_time_by_predecessor (
                                        vrptw,
                                        current_node,
                                        predecessor,
                                        self->data[predecessor].departure_time);
        if (self->data[current_node].departure_time != departure_time)
            self->data[current_node].departure_time = departure_time;
        else
            break;
        predecessor = current_node;
    }

    size_t successor = route_at (route, idx_successor);
    self->data[node].subroute_tws =
        vrptw_cal_subroute_tws_by_successor (
                                            vrptw,
                                            node,
                                            successor,
                                            self->data[successor].subroute_tws);
    successor = node;
    for (size_t idx = idx_successor - 1; idx > 0; idx--) {
        size_t current_node = route_at (route, idx);
        listu_t *rtws = vrptw_cal_subroute_tws_by_successor (
                                            vrptw,
                                            current_node,
                                            successor,
                                            self->data[successor].subroute_tws);
        if (!listu_equal (self->data[current_node].subroute_tws, rtws)) {
            listu_free (&self->data[current_node].subroute_tws);
            self->data[current_node].subroute_tws = rtws;
        }
        else
            break;
        successor = current_node;
    }
}


// Modify meta when node at idx_node is removed from route.
// Removal may happened or not.
static void s_meta_remove_node (s_meta_t *self, const vrptw_t *vrptw,
                                const route_t *route, size_t idx_node) {
    assert (idx_node > 0 && idx_node < route_size (route) - 1);

    size_t predecessor = route_at (route, idx_node - 1);
    for (size_t idx = idx_node + 1; idx < route_size (route) - 1; idx++) {
        size_t current_node = route_at (route, idx);
        size_t departure_time =
            vrptw_cal_departure_time_by_predecessor (
                                        vrptw,
                                        current_node,
                                        predecessor,
                                        self->data[predecessor].departure_time);
        if (self->data[current_node].departure_time != departure_time)
            self->data[current_node].departure_time = departure_time;
        else
            break;
        predecessor = current_node;
    }

    size_t successor = route_at (route, idx_node + 1);
    for (size_t idx = idx_node - 1; idx > 0; idx--) {
        size_t current_node = route_at (route, idx);
        listu_t *rtws = vrptw_cal_subroute_tws_by_successor (
                                            vrptw,
                                            current_node,
                                            successor,
                                            self->data[successor].subroute_tws);
        if (!listu_equal (self->data[current_node].subroute_tws, rtws)) {
            listu_free (&self->data[current_node].subroute_tws);
            self->data[current_node].subroute_tws = rtws;
        }
        else
            break;
        successor = current_node;
    }
}


// ----------------------------------------------------------------------------
// Genome for evolution

typedef struct {
    route_t *gtour; // giant tour: a sequence of customers: 1 ~ N
    solution_t *sol; // splited giant tour

    // auxiliaries
    s_meta_t *meta;
} s_genome_t;


// Create genome by setting at least one of gtour or sol.
// Return genome with both gtour and sol set, and total distance of sol set.
static s_genome_t *s_genome_new (const vrptw_t *vrptw,
                                 route_t *gtour,
                                 solution_t *sol) {
    assert (gtour != NULL || sol != NULL);
    s_genome_t *self = (s_genome_t *) malloc (sizeof (s_genome_t));
    assert (self);
    self->gtour = gtour;
    self->sol = sol;

    if (self->gtour == NULL)
        self->gtour = vrptw_giant_tour_from_solution (vrptw, sol);
    if (self->sol == NULL)
        self->sol = vrptw_split (vrptw, gtour);
    assert (self->gtour);
    assert (self->sol);
    if (double_is_none (solution_total_distance (self->sol)))
        solution_cal_set_total_distance (
                                    self->sol,
                                    vrptw,
                                    (vrp_arc_distance_t) vrptw_arc_distance);
    self->meta = s_meta_new (vrptw, self->sol);
    return self;
}


// Destroy genome object
static void s_genome_free (s_genome_t **self_p) {
    assert (self_p);
    if (*self_p) {
        s_genome_t *self = *self_p;
        route_free (&self->gtour);
        solution_free (&self->sol);
        s_meta_free (&self->meta);
        free (self);
        *self_p = NULL;
    }
}


// Comparator: compare cost of solution in genome
static int s_genome_compare_cost (const s_genome_t *g1, const s_genome_t *g2) {
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


// Comparator: compare savings used by CW in descending order
static int s_cwsaving_compare (const s_cwsaving_t *s1, const s_cwsaving_t *s2) {
    if (s1->saving > s2->saving)
        return -1;
    if (s1->saving < s2->saving)
        return 1;
    return 0;
}


// Sub routine of CW algorithm with specified lambda parameter
static solution_t *vrptw_clark_wright_parallel (const vrptw_t *self,
                                                size_t *predecessors,
                                                size_t *successors,
                                                double *route_demands,
                                                s_cwsaving_t *savings,
                                                s_meta_item_t *meta,
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
    qsort (savings, num_savings, sizeof (s_cwsaving_t),
           (comparator_t) s_cwsaving_compare);

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
static listx_t *vrptw_clark_wright (const vrptw_t *self, size_t num_expected) {
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
    s_meta_item_t *meta =
        (s_meta_item_t *) malloc (sizeof (s_meta_item_t) * (N + 1));
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
            // solution_cal_set_total_distance (
            //     sol, self, (vrp_arc_distance_t) vrptw_arc_distance);
            s_genome_t *genome = s_genome_new (self, gtour, sol);
            listx_append (genomes, genome);
            listu_append (hashes, hash);
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


// Heuristics: giant tours constructed by sorting customers according to angle.
// is_random: true
// max_expected: self->num_customers
static listx_t *vrptw_sweep_giant_tours (const vrptw_t *self,
                                         size_t num_expected) {
    print_info ("sweep giant tours starting ... (expected: %zu)\n",
                num_expected);
    size_t N = self->num_customers;
    if (num_expected > N)
        num_expected = N;

    listx_t *genomes = listx_new ();
    if (coord2d_is_none (self->nodes[0].coord)) {
        print_info ("Coordinates of nodes are not available.\n");
        return genomes;
    }

    // Create giant tour template: nodes permutated in ascending angle order
    coord2d_t *polars = (coord2d_t *) malloc (sizeof (coord2d_t) * N);
    assert (polars);
    for (size_t idx = 0; idx < N; idx++) {
        if (coord2d_is_none (self->nodes[idx + 1].coord)) {
            print_info ("Coordinates of nodes are not available.\n");
            free (polars);
            return genomes;
        }
        polars[idx] = coord2d_to_polar (self->nodes[idx + 1].coord,
                                        self->nodes[0].coord,
                                        vrp_coord_sys (self->vrp));
        // Overwrite v1 with customer index
        polars[idx].v1 = (double) (idx + 1);
    }
    qsort (polars, N, sizeof (coord2d_t),
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
            listx_append (genomes, s_genome_new (self, gtour, NULL));
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
static listx_t *vrptw_random_giant_tours (const vrptw_t *self,
                                          size_t num_expected) {
    print_info ("random giant tours starting ... (expected: %zu)\n",
                num_expected);
    size_t max_expected = factorial (self->num_customers);
    if (num_expected > max_expected)
        num_expected = max_expected;

    route_t *gtour_template = route_new_range (1, self->num_customers, 1);
    listx_t *genomes = listx_new ();
    listu_t *hashes = listu_new (num_expected / 2 + 1);

    for (size_t cnt = 0; cnt < num_expected; cnt++) {
        route_t *gtour = route_dup (gtour_template);
        route_shuffle (gtour, 0, self->num_customers - 1, self->rng);
        size_t hash = giant_tour_hash (gtour);
        if (!listu_includes (hashes, hash)) {
            listx_append (genomes, s_genome_new (self, gtour, NULL));
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


// ----------------------------------------------------------------------------
// Local search

// Local search: inter-route or-opt of node: relocate one node to another route
static double vrptw_or_opt_node (const vrptw_t *self,
                                 solution_t *sol, bool exhaustive) {
    double saving = 0;
    bool improved = true;
    size_t depot = 0;
    s_meta_t *meta = s_meta_new (self, sol);

    while (improved) {
        improved = false;

        solution_iterator_t iter1 = solution_iter_init (sol);
        while (solution_iter_node (sol, &iter1) != ID_NONE && !improved) {
            if (iter1.node_id == depot) // ignore depot
                continue;

            // Removal of customer node
            double dcost_remove =
                route_remove_node_delta_distance (
                                    iter1.route,
                                    iter1.idx_node,
                                    self,
                                    (vrp_arc_distance_t) vrptw_arc_distance);
            double node_demand = vrptw_node_demand (self, iter1.node_id);

            // Try to insert node before another node in another route
            solution_iterator_t iter2 = solution_iter_init (sol);
            while (solution_iter_node (sol, &iter2) != ID_NONE && !improved) {
                // Ignore same route and first node (depot)
                if (iter1.idx_route == iter2.idx_route || iter2.idx_node == 0)
                    continue;

                // Feasibility check: capacity
                double route2_demand = vrptw_route_demand (self, iter2.route);
                if (route2_demand + node_demand > self->capacity)
                    continue;

                // Feasibility check: time windows
                if (!s_meta_insert_node_is_feasible (meta,
                                                     self,
                                                     iter2.route,
                                                     iter1.node_id,
                                                     iter2.idx_node))
                    continue;

                double dcost_insert =
                    route_insert_node_delta_distance (iter2.route,
                                                      iter2.idx_node,
                                                      iter1.node_id,
                                                      self->vrp,
                                                      (vrp_arc_distance_t) vrp_arc_distance);
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

                    // Modify TW meta accordingly
                    s_meta_remove_node (meta,
                                        self,
                                        iter1.route,
                                        iter1.idx_node);

                    s_meta_insert_node (meta,
                                        self,
                                        iter2.route,
                                        iter1.node_id,
                                        iter2.idx_node);

                    improved = true;
                }
            }
        }
    }

    // print_info ("or-opt-node saving (end): %.2f\n", saving);
    s_meta_free (&meta);
    return saving;
}


// ----------------------------------------------------------------------------
static double vrptw_post_optimize (const vrptw_t *self, solution_t *sol) {
    return 0;
}

// ----------------------------------------------------------------------------

static bool vrptw_is_basically_solvable (const vrptw_t *self) {
    double total_demands = 0;
    for (size_t idx = 1; idx < self->num_customers; idx++) {
        if (self->nodes[idx].demand > self->capacity)
            return false;
        total_demands += self->nodes[idx].demand;

        size_t arrival_time = vrptw_earliest_service_time (self, 0) +
                              self->nodes[0].service_duration +
                              vrptw_arc_duration (self, 0, idx);
        if (arrival_time > vrptw_latest_service_time (self, idx))
            return false;

        size_t service_time =
            vrptw_cal_service_time_by_arrival_time (self, idx, arrival_time);
        size_t back_time = service_time +
                           self->nodes[idx].service_duration +
                           vrptw_arc_duration (self, idx, 0);
        if (back_time > vrptw_latest_service_time (self, 0))
            return false;

    }
    if (total_demands > self->capacity * self->num_vehicles)
        return false;

    return true;
}


// Small model solver.
// Select best solution of constructive heuristics, and local search
static solution_t *vrptw_solve_small_model (const vrptw_t *self) {
    print_info ("solve a small model...\n");

    double min_dist = DOUBLE_MAX;
    listx_t *genomes = NULL;
    s_genome_t *g = NULL;
    solution_t *sol = NULL;

    // CW
    genomes = vrptw_clark_wright (self, 7);
    listx_set_destructor (genomes, (destructor_t) s_genome_free);
    listx_set_comparator (genomes, (comparator_t) s_genome_compare_cost);
    listx_sort (genomes, true);
    g = (s_genome_t *) listx_first (genomes);
    if (g != NULL && solution_total_distance (g->sol) < min_dist) {
        solution_free (&sol);
        sol = solution_dup (g->sol);
        min_dist = solution_total_distance (g->sol);
    }
    listx_free (&genomes);
    print_info ("min total distance: %.2f\n", min_dist);

    // Sweep giant tours
    genomes = vrptw_sweep_giant_tours (self, self->num_customers);
    listx_set_destructor (genomes, (destructor_t) s_genome_free);
    listx_set_comparator (genomes, (comparator_t) s_genome_compare_cost);
    listx_sort (genomes, true);
    g = listx_first (genomes);
    if (g != NULL && solution_total_distance (g->sol) < min_dist) {
        solution_free (&sol);
        sol = solution_dup (g->sol);
        min_dist = solution_total_distance (g->sol);
    }
    listx_free (&genomes);
    print_info ("min total distance: %.2f\n", min_dist);

    // Random giant tours
    if (sol == NULL) {
        genomes = vrptw_random_giant_tours (self, self->num_customers);
        listx_set_destructor (genomes, (destructor_t) s_genome_free);
        listx_set_comparator (genomes, (comparator_t) s_genome_compare_cost);
        listx_sort (genomes, true);
        g = listx_first (genomes);
        if (g != NULL && solution_total_distance (g->sol) < min_dist) {
            solution_free (&sol);
            sol = solution_dup (g->sol);
            min_dist = solution_total_distance (g->sol);
        }
        listx_free (&genomes);
    }
    print_info ("min total distance: %.2f\n", min_dist);

    assert (sol != NULL);
    vrptw_print_solution (self, sol);

    vrptw_post_optimize (self, sol);
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
    assert (self);

    if (!vrptw_is_basically_solvable (self))
        return NULL;

    // Deal with small numboer of nodes
    if (self->num_customers <= SMALL_NUM_NODES)
        return vrptw_solve_small_model (self);

    return NULL;
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
