/*  =========================================================================
    cvrp - implementation

    Note:

    Inner representation of nodes:
        depot: 0
        customers: 1, 2, ..., num_customers

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


#define SMALL_NUM_NODES 10


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

static cvrp_arc_distance (cvrp_t *self, size_t i, size_t j) {
    return vrp_arc_distance (self->vrp,
                             self->nodes[i].id, self->nodes[j].id);
}


typedef struct {
    size_t c1;
    size_t c2;
    double value;
} s_cwsaving_t;


s_cwsaving_t *s_cwsaving_new (size_t i, size_t j, double value) {
    cw_saving_t *saving = (cw_saving_t *) malloc (sizeof (cw_saving_t));
    assert (saving);
    saving->c1 = i;
    saving->c2 = j;
    saving->value = value;
    return saving;
}


void s_cwsaving_free (s_cwsaving_t **self_p) {
    assert (self_p);
    if (*self_p) {
        free (*self_p);
        *self_p = NULL;
    }
}


static solution_t *cvrp_clark_wright_parallel (cvrp_t *self, double lambda) {
    bool *is_last = (bool *) malloc (sizeof (bool) * self->num_customers);
    assert (is_last);
    listx_t *savings =
        listx_new (self->num_customers * (self->num_customers - 1));
    assert (savings);
    listx_set_comparator (savings, ...)
    listx_sort (savings, false);

    // Create savings list sorted descendingly
    for (size_t i = 1; i < self->num_customers; i++) {
        for (size_t j = 1; j < self->num_customers && j != i; j++) {

            s_cwsaving_t *saving =
                s_cwsaving_new (i, j,
                                cvrp_arc_distance (self, i, 0) +
                                cvrp_arc_distance (self, 0, j) -
                                lambda * cvrp_arc_distance (self, i, j));
            assert (saving);

            listx_insert_sorted (savings, saving);
        }
    }

}


static solution_t *cvrp_clark_wright (cvrp_t *self) {
    listx_t *solutions = listx_new ();
    for (double lambda = 0.4; lambda <= 1.0; lambda += 0.1) {
        solution_t *sol = cvrp_clark_wright_parallel (self, lambda);
        listx_append (solutions, sol);
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
    self->nodes[0].id == ID_NONE;
    self->num_customers = num_requests;

    for (size_t idx = 0; idx < num_requests; idx++) {
        size_t request = listu_get (requests, idx);

        // depot
        if (self->nodes[0].id == ID_NONE) {
            self->nodes[0].id = vrp_request_pickup_node (vrp, request);
            self->nodes[0].demand = 0;
            self->nodes[0].coord = vrp_node_coord (vrp, self->nodes[0].id);
        }
        else {
            assert (self->nodes[0].id == vrp_request_pickup_node (vrp, request));
            ;
        }

        // customer
        self->nodes[idx].id = vrp_request_delivery_node (vrp, request);
        self->nodes[idx].demand = vrp_request_quantity (vrp, request);
        self->nodes[idx].coord =
            vrp_node_coord (vrp, self->nodes[idx].id);
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


    solution_t *sol = solution_new (self->vrp);
    return sol;
}


void cvrp_test (bool verbose) {
    print_info ("* cvrp: \n");

    print_info ("OK\n");
}
