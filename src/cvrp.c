/*  =========================================================================
    cvrp - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

/* Todos:

- [x] fitness (cost of splited giant tour)
- [ ] split giant tour to solution_t
- [ ] inter-route local search
- [ ] more heuristics

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
    s_node_t *nodes; // depot: 0; customers: 1, 2, ..., num_customers

    rng_t *rng;
};


// ----------------------------------------------------------------------------
// Genome for evolution

typedef struct {
    route_t *gtour; // giant tour: a sequence of customers: 1 ~ N
    solution_t *sol; // splited giant tour
    double cost;
} s_genome_t;


static s_genome_t *s_genome_new_from_giant_tour (route_t *gtour) {
    assert (gtour);
    s_genome_t *self = (s_genome_t *) malloc (sizeof (s_genome_t));
    assert (self);
    self->gtour = gtour;
    self->sol = NULL;
    self->cost = DOUBLE_NONE;
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

static double cvrp_arc_distance (cvrp_t *self, size_t i, size_t j) {
    return vrp_arc_distance (self->vrp,
                             self->nodes[i].id, self->nodes[j].id);
}


// ----------------------------------------------------------------------------
// Heuristics

typedef struct {
    size_t c1;
    size_t c2;
    double saving;
} s_cwsaving_t;


// Simple hash function of giant tour which could discriminate permutations
static size_t giant_tour_hash (route_t *gtour) {
    size_t hash = 0;
    for (size_t idx = 0; idx < route_size (gtour); idx++)
        hash += route_at (gtour, idx) * idx;
    return hash;
}


// Compare savings: descending order
static int compare_cwsavings (const void *a, const void *b) {
    if (((s_cwsaving_t *) a)->saving > ((s_cwsaving_t *) a)->saving)
        return -1;
    if (((s_cwsaving_t *) a)->saving < ((s_cwsaving_t *) a)->saving)
        return 1;
    return 0;
}


// Return giant tour
static route_t *cvrp_clark_wright_parallel (cvrp_t *self, double lambda) {
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

    // @todo problem: solution with inner ID?
    // solution_t *sol = solution_new (self->vrp);

    // for (size_t idx = 1; idx <= N; idx++) {
    //     if (predecessors[idx] == 0) { // idx is a first customer of route
    //         route_t *route = route_new (3); // at least 3 nodes in route
    //         route_append_node (route, 0); // depot
    //         size_t successor = idx;
    //         while (successor != 0) {
    //             route_append_node (route, successor);
    //             successor = successors[successor];
    //         }
    //         route_append_node (route, 0); // depot
    //         solution_add_route (sol, route);
    //     }
    // }

    route_t *gtour = route_new (N);
    assert (gtour);

    for (size_t idx = 1; idx <= N; idx++) {
        if (predecessors[idx] == 0) { // first customer on route
            size_t successor = idx;
            while (successor != 0) {
                route_append_node (gtour, successor);
                successor = successors[successor];
            }
        }
    }

    free (predecessors);
    free (successors);
    free (route_demands);
    free (savings);

    assert (route_size (gtour) == N);
    return gtour;
}


// Clark-Wright heuristic.
// Return list of at most 7 giant tours (duplicates removed)
static listx_t *cvrp_clark_wright (cvrp_t *self) {
    listx_t *genomes = listx_new ();
    listu_t *hashes = listu_new (7);

    for (double lambda = 0.4; lambda <= 1.0; lambda += 0.1) {
        route_t *gtour = cvrp_clark_wright_parallel (self, lambda);
        size_t hash = giant_tour_hash (gtour);
        if (!listu_includes (hashes, hash)) {
            s_genome_t *genome = s_genome_new_from_giant_tour (gtour);
            listx_append (genomes, genome);
            listu_append (hashes, hash);
            route_print (gtour);
        }
    }

    listu_free (&hashes);
    print_info ("%zu genomes generated by CW\n", listx_size (genomes));
    return genomes;
}


// Heuristic: random giant tour
static route_t *cvrp_random_giant_tour (cvrp_t *self) {
    route_t *giant_tour = route_new_range (1, self->num_customers, 1);
    assert (giant_tour);
    route_shuffle (giant_tour, 0, self->num_customers - 1, self->rng);
    return giant_tour;
}


// ----------------------------------------------------------------------------
// Split algorithm related


/* split a TSP route into several CVRP routes, and construct an individual (Prins2004)
   @param vrp: CVRP problem
   @param route: TSP route. format: 0->route[1]->route[2]->...->route[vrp->N]->...
   @param individual (output): CVRP individual
*/
void split(const CVRP_Problem *vrp, const int *route, Individual *individual)
{
    int N = vrp->N;
    double *sp_cost = NULL; // cost of the shortest path from node 0 to node j in auxiliary graph H
    int *predecessor = NULL; // predecessor of node j on the shortest path
    double route_demand, route_cost;
    int i, j, k;

    sp_cost = (double *)malloc((N+1)*sizeof(double));
    predecessor = (int *)malloc((N+1)*sizeof(int));
    if (sp_cost == NULL || predecessor == NULL)
    {
        PRINT("ERROR: split: out of memory.\n");
        goto LABEL_SPLIT;
    }

    // initialize
    for (i = 0; i <= N; i++)
    {
        sp_cost[i] = DBL_MAX;
        predecessor[i] = -1; // NIL
    }
    sp_cost[0] = 0;

    // relax. NOTE: only capacity constraint is considered (i.e. CVRP).
    for (i = 1; i <= N; i++)
    {
        route_demand = 0;
        route_cost = 0;

        for (j = i; j <= N; j++)
        {
            route_demand += (vrp->q)[route[j]];

            if (route_demand <= vrp->Q) // arc (i-1,j) exists in auxiliary graph H
            {
                if (i == j)
                {
                    route_cost = (vrp->c)[route[j]] + (vrp->c)[(N+1)*route[j]]; // c(0,route[j])+c(route[j],0)
                }
                else
                {
                    route_cost  = route_cost
                                - (vrp->c)[(N+1)*route[j-1]]          // c(route[j-1],0)
                                + (vrp->c)[(N+1)*route[j-1]+route[j]] // c(route[j-1],route[j])
                                + (vrp->c)[(N+1)*route[j]];           // c(route[j],0)
                }

                if (sp_cost[i-1] + route_cost < sp_cost[j])
                {
                    sp_cost[j] = sp_cost[i-1] + route_cost;
                    predecessor[j] = i - 1;
                }
            }
            else
            {
                break; // to next i
            }
        }
    }

    // PRINT("\nroute:");
    // for (i = 0; i <= N; i++) PRINT(" %d", route[i]);
    // PRINT("\n");

    // PRINT("predecessor:");
    // for (i = 0; i <= N; i++) PRINT(" %d", predecessor[i]);
    // PRINT("\n");
    // PRINT("sp_cost:");
    // for (i = 0; i <= N; i++) PRINT(" %.2f", sp_cost[i]);
    // PRINT("\n");

    // generate individual from predecessor
    individual->num_route = 0;
    j = N;
    i = predecessor[N];
    while (i >= 0)
    {
        (individual->first_customer)[individual->num_route] = route[i+1];
        for (k = i+1; k < j; k++)
        {
            individual->successor[route[k]-1] = route[k+1];
        }
        individual->successor[route[j]-1] = 0;

        individual->num_route += 1;
        j = i;
        i = predecessor[i];
    }

    if (sp_cost[N] > 0)
    {
        individual->feasible = 1;
        individual->cost = sp_cost[N];
        // individual->cost = cvrp_compute_cost(vrp, individual); // for comparison
    }

    LABEL_SPLIT:

        free(sp_cost);
        free(predecessor);
}


static void cvrp_split (cvrp_t *self, s_genome_t *genome) {
    size_t N = self->num_customers;
    double route_demand, route_cost;

    // cost of the shortest path from node 0 to node j in auxiliary graph H
    double *sp_cost = (double *) malloc ((N + 1) * sizeof (double));
    assert (sp_cost);

    // predecessor of node j on the shortest path
    size_t *predecessor = (size_t *) malloc ((N + 1) * sizeof (size_t));
    assert (predecessor);

    // initialize
    for (size_t i = 0; i <= N; i++) {
        sp_cost[i] = DOUBLE_MAX;
        predecessor[i] = SIZE_NONE; // NIL
    }
    sp_cost[0] = 0;

@todo modified HERE 11.24 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


    // relax. NOTE: only capacity constraint is considered (i.e. CVRP).
    for (size_t i = 1; i <= N; i++) {

        route_demand = 0;
        route_cost = 0;

        for (size_t j = i; j <= N; j++) {

            route_demand += (vrp->q)[route[j]];

            // arc (i-1,j) exists in auxiliary graph H
            if (route_demand <= vrp->Q)  {
                if (i == j)
                    route_cost = (vrp->c)[route[j]] + (vrp->c)[(N+1)*route[j]]; // c(0,route[j])+c(route[j],0)

                else
                    route_cost  = route_cost
                                - (vrp->c)[(N+1)*route[j-1]]          // c(route[j-1],0)
                                + (vrp->c)[(N+1)*route[j-1]+route[j]] // c(route[j-1],route[j])
                                + (vrp->c)[(N+1)*route[j]];           // c(route[j],0)


                if (sp_cost[i-1] + route_cost < sp_cost[j]) {
                    sp_cost[j] = sp_cost[i-1] + route_cost;
                    predecessor[j] = i - 1;
                }
            }

            else
                break; // to next i
        }
    }

    // PRINT("\nroute:");
    // for (i = 0; i <= N; i++) PRINT(" %d", route[i]);
    // PRINT("\n");

    // PRINT("predecessor:");
    // for (i = 0; i <= N; i++) PRINT(" %d", predecessor[i]);
    // PRINT("\n");
    // PRINT("sp_cost:");
    // for (i = 0; i <= N; i++) PRINT(" %.2f", sp_cost[i]);
    // PRINT("\n");

    // generate individual from predecessor
    individual->num_route = 0;
    j = N;
    i = predecessor[N];
    while (i >= 0)
    {
        (individual->first_customer)[individual->num_route] = route[i+1];
        for (k = i+1; k < j; k++)
        {
            individual->successor[route[k]-1] = route[k+1];
        }
        individual->successor[route[j]-1] = 0;

        individual->num_route += 1;
        j = i;
        i = predecessor[i];
    }

    if (sp_cost[N] > 0)
    {
        individual->feasible = 1;
        individual->cost = sp_cost[N];
        // individual->cost = cvrp_compute_cost(vrp, individual); // for comparison
    }

    LABEL_SPLIT:

        free(sp_cost);
        free(predecessor);

}


// Fitness of genome: inverse of average cost of splited giant tour.
// Note that solution is also generated and saved in genome with cost.
static double cvrp_fitness (cvrp, s_genome_t *genome) {
    // If genome->sol does not exists, split giant tour
    if (genome->sol == NULL)
        cvrp_split (self, genome);

    assert (genome->sol != NULL);
    assert (!double_is_none (genome->cost));

    if (genome->cost > 0)
        return (self->num_customers + 1) / genome->cost;
    else
        return 0;
}


// ----------------------------------------------------------------------------
// Crossover

static listx_t *cvrp_crossover (cvrp_t *self, s_genome_t *g1, s_genome_t *g2) {
    route_t *r1 = route_dup (g1->tour);
    route_t *r2 = route_dup (g2->tour);
    route_ox (r1, r2, 0, self->num_customers-1, self->rng);
    listx_t *children = listx_new ();
    listx_append (children, s_genome_new_from_giant_tour (r1));
    listx_append (children, s_genome_new_from_giant_tour (r2));
    return children;
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

    listx_t *genomes = cvrp_clark_wright (self);
    listx_set_destructor (genomes, (destructor_t) s_genome_free);
    listx_free (&genomes);
    return NULL;
}


void cvrp_test (bool verbose) {
    print_info ("* cvrp: \n");

    print_info ("OK\n");
}
