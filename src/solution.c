/*  =========================================================================
    solution - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


struct _solution_t {
    vrp_t *vrp; // Problem reference. Solution does not own it.
    listx_t *routes; // list of route_t objects
    listu_t *vehicle_ids;
};


// Create a new solution object
solution_t *solution_new (vrp_t *vrp) {
    // assert (vrp);

    solution_t *self = (solution_t *) malloc (sizeof (solution_t));
    assert (self);

    self->vrp = vrp;

    self->routes = listx_new ();
    assert (self->routes);
    listx_set_destructor (self->routes, (destructor_t) route_free);

    print_info ("solution created.\n");
    return self;
}


// Destroy solution object
void solution_free (solution_t **self_p) {
    assert (self_p);
    if (*self_p) {
        solution_t *self = *self_p;

        if (self->routes != NULL)
            listx_free (&self->routes);

        free (self);
        *self_p = NULL;
    }
    print_info ("solution freed.\n");
}


void solution_add_route (solution_t *self, route_t *route) {
    assert (self);
    assert (route);
    listx_append (self->routes, route);
}


void solution_add_route_from_array (solution_t *self,
                                    const size_t *node_ids,
                                    size_t num_nodes) {
    assert (self);
    assert (node_ids);
    listx_append (self->routes, route_new_from_array (node_ids, num_nodes));
}


size_t solution_num_routes (solution_t *self) {
    assert (self);
    return listx_size (self->routes);
}


route_t *solution_route (solution_t *self, size_t route_idx) {
    assert (self);
    assert (route_idx < solution_num_routes (self));
    return listx_item_at (self->routes, route_idx);
}


size_t solution_route_length (solution_t *self, size_t route_idx) {
    assert (self);
    assert (route_idx < solution_num_routes (self));
    return route_size (listx_item_at (self->routes, route_idx));
}


void solution_print (solution_t *self) {
    assert (self);
    printf ("solution: #routes: %zu\n", listx_size (self->routes));
    printf ("--------------------------------------------------\n");
    for (size_t idx_r = 0; idx_r < listx_size (self->routes); idx_r++) {
        route_t *route = solution_route (self, idx_r);
        size_t route_len = route_size (route);
        printf ("route #%zu (#nodes: %zu):", idx_r, route_len);
        for (size_t idx_n = 0; idx_n < route_len; idx_n++)
            printf (" %zu", route_at (route, idx_n));
        printf ("\n");
    }
}


void solution_test (bool verbose) {
    print_info (" * solution: \n");
    // roadnet_t *roadnet = roadnet_new ();
    // // ...
    // roadnet_free (&roadnet);
    print_info ("OK\n");
}
