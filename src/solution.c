/*  =========================================================================
    solution - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


struct _solution_t {
    vrp_t *vrp; // Problem reference. Solution does not own it.
    listx_t *routes; // list of route_t objects
    listu_t *vehicles; // list of vehicles cooresponding to routes

    // auxiliaries
    bool feasible;
    double total_distance;
};


// Create a new solution object
solution_t *solution_new (vrp_t *vrp) {
    solution_t *self = (solution_t *) malloc (sizeof (solution_t));
    assert (self);

    self->vrp = vrp;

    self->routes = listx_new ();
    assert (self->routes);
    listx_set_destructor (self->routes, (destructor_t) route_free);

    self->vehicles = NULL;
    self->total_distance = DOUBLE_NONE;
    return self;
}


// Destroy solution object
void solution_free (solution_t **self_p) {
    assert (self_p);
    if (*self_p) {
        solution_t *self = *self_p;
        listx_free (&self->routes);
        listu_free (&self->vehicles);
        free (self);
        *self_p = NULL;
    }
}


void solution_prepend_route (solution_t *self, route_t *route) {
    assert (self);
    assert (route);
    listx_prepend (self->routes, route);
}


void solution_append_route (solution_t *self, route_t *route) {
    assert (self);
    assert (route);
    listx_append (self->routes, route);
}


void solution_prepend_route_from_array (solution_t *self,
                                        const size_t *node_ids,
                                        size_t num_nodes) {
    assert (self);
    assert (node_ids);
    listx_prepend (self->routes, route_new_from_array (node_ids, num_nodes));
}


void solution_append_route_from_array (solution_t *self,
                                       const size_t *node_ids,
                                       size_t num_nodes) {
    assert (self);
    assert (node_ids);
    listx_append (self->routes, route_new_from_array (node_ids, num_nodes));
}


void solution_remove_route (solution_t *self, size_t idx) {
    assert (self);
    assert (idx < listx_size (self->routes));
    listx_remove_at (self->routes, idx);
}


size_t solution_num_routes (const solution_t *self) {
    assert (self);
    return listx_size (self->routes);
}


route_t *solution_route (const solution_t *self, size_t route_idx) {
    assert (self);
    assert (route_idx < solution_num_routes (self));
    return listx_item_at (self->routes, route_idx);
}


void solution_set_total_distance (solution_t *self, double distance) {
    assert (self);
    self->total_distance = distance;
}


double solution_cal_set_total_distance (solution_t *self, const vrp_t *vrp) {
    assert (self);
    assert (vrp);
    double total_dist = 0;
    for (size_t idx = 0; idx < solution_num_routes (self); idx++)
        total_dist += route_total_distance (solution_route (self, idx), vrp);
    self->total_distance = total_dist;
    return total_dist;
}


double solution_cal_total_distance (const solution_t *self, const vrp_t *vrp) {
    assert (self);
    assert (vrp);
    double total_dist = 0;
    for (size_t idx = 0; idx < solution_num_routes (self); idx++)
        total_dist += route_total_distance (solution_route (self, idx), vrp);
    // self->total_distance = total_dist;
    return total_dist;
}


void solution_increase_total_distance (solution_t *self, double delta_distance) {
    assert (self);
    assert (!double_is_none (self->total_distance));
    self->total_distance += delta_distance;
}


double solution_total_distance (const solution_t *self) {
    assert (self);
    return self->total_distance;
}


solution_t *solution_dup (const solution_t *self) {
    assert (self);
    solution_t *copy = solution_new (self->vrp);
    for (size_t idx = 0; idx < solution_num_routes (self); idx++)
        solution_append_route (copy,
                               route_dup (listx_item_at (self->routes, idx)));

    copy->vehicles = listu_dup (self->vehicles);
    copy->feasible = self->feasible;
    copy->total_distance = self->total_distance;
    return copy;
}


void solution_print (const solution_t *self) {
    if (self == NULL)
        return;

    printf ("\nsolution: #routes: %zu, total distance: %.2f\n",
            listx_size (self->routes), self->total_distance);
    printf ("--------------------------------------------------\n");
    for (size_t idx_r = 0; idx_r < listx_size (self->routes); idx_r++) {
        route_t *route = listx_item_at (self->routes, idx_r);
        size_t route_len = route_size (route);
        printf ("route #%zu (#nodes: %zu):", idx_r, route_len);
        for (size_t idx_n = 0; idx_n < route_len; idx_n++) {
            if (self->vrp != NULL)
                printf (" %s", vrp_node_ext_id (self->vrp, route_at (route, idx_n)));
            else
                printf (" %zu", route_at (route, idx_n));
        }
        printf ("\n");
    }
}


void solution_print_internal (const solution_t *self) {
    assert (self);
    printf ("\nsolution: #routes: %zu, total distance: %.2f\n",
            listx_size (self->routes), self->total_distance);
    printf ("--------------------------------------------------\n");
    for (size_t idx_r = 0; idx_r < listx_size (self->routes); idx_r++) {
        route_t *route = listx_item_at (self->routes, idx_r);
        size_t route_len = route_size (route);
        printf ("route #%zu (#nodes: %zu):", idx_r, route_len);
        for (size_t idx_n = 0; idx_n < route_len; idx_n++)
            printf (" %zu", route_at (route, idx_n));
        printf ("\n");
    }
    printf ("\n");
}


solution_iterator_t solution_iter_init (const solution_t *self) {
    assert (self);
    return (solution_iterator_t) {SIZE_NONE, NULL, SIZE_NONE, ID_NONE};
}


// route_t *solution_iter_route (const solution_t *self, solution_iterator_t *iter) {
//     assert (self);
//     assert (iter);
//     iter->idx_route = (iter->idx_route < listx_size (self) - 1) ?
//                       (iter->idx_route + 1) :
//                       SIZE_NONE;

//     iter->route = (iter->idx_route != SIZE_NONE) ?
//                   listx_item_at (self->routes, iter->idx_route) :
//                   NULL;

//     iter->idx_node = SIZE_NONE;
//     iter->node_id = ID_NONE;
//     return iter->route;
// }


size_t solution_iter_node (const solution_t *self, solution_iterator_t *iter) {
    assert (self);
    assert (iter);

    // Not first iteration
    if (iter->route != NULL) {
        // next node of current route
        if (iter->idx_node < route_size (iter->route) - 1) {
            iter->idx_node++;
            iter->node_id = route_at (iter->route, iter->idx_node);
            return iter->node_id;
        }
        // first node of next route
        else {
            // there is next route
            if (iter->idx_route < listx_size (self->routes) - 1) {
                iter->idx_route++;
                iter->route = listx_item_at (self->routes, iter->idx_route);
                assert (route_size (iter->route) > 0);
                iter->idx_node = 0;
                iter->node_id = route_at (iter->route, 0);
                return iter->node_id;
            }
            // there is no next route
            else
                return ID_NONE;
        }
    }
    // First iteration
    else {
        iter->route = listx_first (self->routes);
        if (iter->route != NULL) {
            iter->idx_route = 0;
            assert (route_size (iter->route) > 0);
            iter->idx_node = 0;
            iter->node_id = route_at (iter->route, 0);
            return iter->node_id;
        }
        else // no route exists in solution
            return ID_NONE;
    }
}



void solution_test (bool verbose) {
    print_info (" * solution: \n");

    rng_t *rng = rng_new ();
    solution_t *sol = solution_new (NULL);
    size_t num_routes = 1000;
    for (size_t idx = 0; idx < num_routes; idx++) {
        route_t *route = route_new_range (rng_random_int (rng, 1, 100),
                                          rng_random_int (rng, 100, 200), 1);
        solution_append_route (sol, route);
    }

    // solution_print_internal (sol);

    solution_iterator_t iter = solution_iter_init (sol);
    while (solution_iter_node (sol, &iter) != ID_NONE) {
        // printf ("route idx: %zu, node idx: %zu, node id: %zu\n",
        //         iter.idx_route, iter.idx_node, iter.node_id);
        assert (iter.route == solution_route (sol, iter.idx_route));
        assert (iter.node_id == route_at (iter.route, iter.idx_node));
    }

    solution_free (&sol);
    rng_free (&rng);
    print_info ("OK\n");
}


