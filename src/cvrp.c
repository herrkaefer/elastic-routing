/*  =========================================================================
    cvrp - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


#define SMALL_NUM_NODES 10


// Private node representation
typedef struct {
    size_t id; // node ID in roadgraph of generic model
    double demand;
    const coord2d_t *coord;
} s_node_t;


// typedef struct {
//     size_t
// } s_solution_t;

struct _cvrp_t {
    vrp_t *vrp; // reference of generic model

    double capacity;
    size_t num_vehicles;
    size_t num_customers;
    s_node_t *nodes; // depot: 0; customers: 1, 2, ..., num_customers

    rng_t *rng;
};


// ----------------------------------------------------------------------------

static double cvrp_arc_distance (cvrp_t *self, size_t i, size_t j) {
    return vrp_arc_distance (self->vrp,
                             self->nodes[i].id, self->nodes[j].id);
}


typedef struct {
    size_t c1;
    size_t c2;
    double saving;
} s_cwsaving_t;


// Compare savings: descending order
static int compare_cwsavings (const void *a, const void *b) {
    if (((s_cwsaving_t *) a)->saving > ((s_cwsaving_t *) a)->saving)
        return -1;
    if (((s_cwsaving_t *) a)->saving < ((s_cwsaving_t *) a)->saving)
        return 1;
    return 0;
}


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
                cvrp_arc_distance (self, i, 0) +
                cvrp_arc_distance (self, 0, j) -
                lambda * cvrp_arc_distance (self, i, j);
            cnt++;
        }
    }

    // Sort savings in descending order
    qsort (savings, num_savings, sizeof (s_cwsaving_t), compare_cwsavings);

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
            route_append_node (route, 0); // depot
            size_t successor = idx;
            while (successor != 0) {
                route_append_node (route, successor);
                successor = successors[successor];
            }
            route_append_node (route, 0); // depot
            solution_add_route (sol, route);
        }
    }

    free (predecessors);
    free (successors);
    free (route_demands);
    free (savings);

    return sol;
}


// Clark-Wright heuristic.
// Return at most 7 solutions.
static listx_t *cvrp_clark_wright (cvrp_t *self) {
    listx_t *solutions = listx_new ();
    for (double lambda = 0.4; lambda <= 1.0; lambda += 0.1) {
        solution_t *sol = cvrp_clark_wright_parallel (self, lambda);
        if (sol != NULL)
            listx_append (solutions, sol);
        solution_print_internal (sol);
    }
    return solutions;
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
        (s_node_t *) malloc (sizeof (s_node_t) *
                                 (vrp_num_customers (vrp) + 1));
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
            self->nodes[0].id = vrp_request_pickup_node (vrp, request);
            // printf ("depot set to: %zu\n", self->nodes[0].id);
            self->nodes[0].demand = 0;
            self->nodes[0].coord = vrp_node_coord (vrp, self->nodes[0].id);
        }
        assert (self->nodes[0].id == vrp_request_pickup_node (vrp, request));

        // customer
        self->nodes[idx+1].id = vrp_request_delivery_node (vrp, request);
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

    listx_t *sols = cvrp_clark_wright (self);
    listx_set_destructor (sols, solution_free);
    listx_free (&sols);
    return NULL;
}


void cvrp_test (bool verbose) {
    print_info ("* cvrp: \n");

    print_info ("OK\n");
}
