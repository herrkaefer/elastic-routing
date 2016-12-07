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


Equivalent TW of route (single TW):

etw_k' = etw_k
ltw_k' = min {ltw_k, ltw_(k+1) - t_(k, k+1) - sd_k}

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
// Heuristics: CW adapted for VRPTW

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
static solution_t *vrptw_clark_wright_parallel (vrptw_t *self,
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
                vrptw_arc_distance_by_idx (self, i, 0) +
                vrptw_arc_distance_by_idx (self, 0, j) -
                vrptw_arc_distance_by_idx (self, i, j) * lambda;
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

        // Check compatibility of capacity
        double new_demand = route_demands[c1] + route_demands[c2];
        if (new_demand > self->capacity)
            continue;

        // Check compatibility of time windows
         @todo


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
        (s_cwsaving_t *) malloc (sizeof (s_cwsaving_t) * N * (N -1));
    assert (savings);

    for (double lambda = 0.4; lambda <= 1.0; lambda += 0.1) {
        solution_t *sol = vrptw_clark_wright_parallel (self,
                                                       predecessors,
                                                       successors,
                                                       route_demands,
                                                       savings,
                                                       lambda);

        route_t *gtour = vrptw_giant_tour_from_solution (self, sol);
        size_t hash = giant_tour_hash (gtour);
        if (!listu_includes (hashes, hash)) {
            solution_cal_set_total_distance (sol, self->vrp);
            s_genome_t *genome = s_genome_new (gtour, sol);
            listx_append (genomes, genome);
            listu_append (hashes, hash);
            vrptw_print_solution (self, sol);
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
