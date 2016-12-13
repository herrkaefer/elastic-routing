/*  =========================================================================
    VRPTW sol - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


typedef struct {
    // size_t id;
    size_t pred; // predesessor index
    size_t succ; // successor index
    size_t edt; // earliest departure time of route silce till node
    listu_t *rtws; // time windows of route slice from node
} s_node_t;


struct _sol_t {
    vrptw_t *vrptw; // Model reference. Solution does not own it.
    s_node_t *nodes; // array of s_node_t
    listu_t *first_visitors;
};


// Create a new sol object
sol_t *sol_new (vrptw_t *vrptw) {
    sol_t *self = (sol_t *) malloc (sizeof (sol_t));
    assert (self);

    self->vrptw = vrptw;
    size_t num_customers = vrptw_num_customers (vrptw);
    assert (num_customers > 0);
    self->nodes = (s_node_t *) malloc (sizeof (s_node_t) * num_customers);
    assert (self->nodes);

    for (size_t idx = 0; idx < self->num_customers; idx++) {
        s_node_t *node = self->nodes + idx;
        // node->id = vrptw_node_id (vrptw, idx + 1);
        node->pred = ID_NONE;
        node->succ = ID_NONE;
        node->edt = SIZE_NONE;
        node->rtws = listu_new (2);
        listu_sort (node->tws, true);
    }

    self->first_visitors = listu_new (1);

    return self;
}


// Destroy sol object
void sol_free (sol_t **self_p) {
    assert (self_p);
    if (*self_p) {
        sol_t *self = *self_p;

        for (size_t idx = 0; idx < vrptw_num_customers (self->vrptw); idx++) {
            s_node_t *node = self->nodes + idx;
            listu_free (&node->rtws);
        }
        free (self->nodes);
        listu_free (&self->first_visitors);

        free (self);
        *self_p = NULL;
    }
}


static void sol_add_route_nodes (sol_t *self, const route_t *route) {
    size_t size = route_size (route);

    for (size_t idx = 1; idx < size - 1; idx++) {
        size_t node_idx = route_at (route, idx);
        s_node_t *node = self->nodes + node_idx;
        node->pred = route_at (route, idx - 1);
        node->succ = route_at (route, idx + 1);

        size_t edt_pred;
        if (idx == 1) { // first customer
            edt_pred = vrptw_earliest_service_time (self->vrptw, 0) +
                       vrptw_service_duration (self->vrptw, 0);
        }
        else {
            size_t node_idx_pred = route_at (route, idx - 1);
            edt_pred = self->nodes[node_idx_pred-1].edt;
        }
        node->edt = sol_cal_edt (self, edt_pred, node_idx_pred, node_idx);
    }

    // Calculate route tws backwards
    for (size_t idx = size - 2; idx > 0; idx--) {
        size_t node_idx = route_at (route, idx);
        s_node_t *node = self->nodes + node_idx;

        listu_t *rtws_succ;
        if (idx == size - 2) { // last customer
            rtws_succ = vrptw_node_time_windows (self->vrptw, 0);
        }
        else {
            size_t node_idx_succ = route_at (route, idx + 1);
            rtws_succ = self->nodes[node_idx_succ-1].rtws;
        }
        node->rtws = sol_cal_rtws (self, rtws_succ, node_idx_succ, node_idx);
    }
}


void sol_prepend_route (sol_t *self, const route_t *route) {
    assert (self);
    assert (route);
    assert (route_size (route) > 2);

    sol_add_route_nodes (self, route);
    listu_prepend (self->first_visitors, route_at (route, 1));
}


void sol_append_route (sol_t *self, route_t *route) {
    assert (self);
    assert (route);
    assert (route_size (route) > 2);

    sol_add_route_nodes (self, route);
    listu_append (self->first_visitors, route_at (route, 1));
}


size_t sol_num_routes (const sol_t *self) {
    assert (self);
    return listu_size (self->first_visitors);
}


route_t *sol_route (const sol_t *self, size_t route_idx) {
    assert (self);
    assert (route_idx < sol_num_routes (self));

    route_t *route = route_new (2);
    assert (route);

    route_append_node (vrptw_node_id (self->vrptw, 0));
    size_t successor = listu_get (self->first_visitors, route_idx);
    while (successor != 0) {
        route_append_node (route, successor);
        successor = self->nodes[successor-1].succ;
    }

    route_append_node (route, vrptw_node_id (self->vrptw, 0));
    return route;
}


// void sol_set_total_distance (sol_t *self, double distance) {
//     assert (self);
//     self->total_distance = distance;
// }


// double sol_cal_set_total_distance (sol_t *self, const vrp_t *vrp) {
//     assert (self);
//     assert (vrp);
//     double total_dist = 0;
//     for (size_t idx = 0; idx < sol_num_routes (self); idx++)
//         total_dist += route_total_distance (sol_route (self, idx), vrp);
//     self->total_distance = total_dist;
//     return total_dist;
// }


// double sol_cal_total_distance (const sol_t *self, const vrp_t *vrp) {
//     assert (self);
//     assert (vrp);
//     double total_dist = 0;
//     for (size_t idx = 0; idx < sol_num_routes (self); idx++)
//         total_dist += route_total_distance (sol_route (self, idx), vrp);
//     // self->total_distance = total_dist;
//     return total_dist;
// }


// void sol_increase_total_distance (sol_t *self, double delta_distance) {
//     assert (self);
//     assert (!double_is_none (self->total_distance));
//     self->total_distance += delta_distance;
// }


// double vrptwsol_total_distance (const sol_t *self) {
//     assert (self);
//     return self->total_distance;
// }


// sol_t *sol_dup (const sol_t *self) {
//     assert (self);
//     sol_t *copy = sol_new (self->vrp);
//     for (size_t idx = 0; idx < sol_num_routes (self); idx++)
//         sol_append_route (copy,
//                                route_dup (listx_item_at (self->routes, idx)));

//     copy->vehicles = listu_dup (self->vehicles);
//     copy->feasible = self->feasible;
//     copy->total_distance = self->total_distance;
//     return copy;
// }


// void sol_print (const sol_t *self) {
//     if (self == NULL)
//         return;

//     printf ("\nsol: #routes: %zu, total distance: %.2f\n",
//             listx_size (self->routes), self->total_distance);
//     printf ("--------------------------------------------------\n");
//     for (size_t idx_r = 0; idx_r < listx_size (self->routes); idx_r++) {
//         route_t *route = listx_item_at (self->routes, idx_r);
//         size_t route_len = route_size (route);
//         printf ("route #%zu (#nodes: %zu):", idx_r, route_len);
//         for (size_t idx_n = 0; idx_n < route_len; idx_n++) {
//             if (self->vrp != NULL)
//                 printf (" %s", vrp_node_ext_id (self->vrp, route_at (route, idx_n)));
//             else
//                 printf (" %zu", route_at (route, idx_n));
//         }
//         printf ("\n");
//     }
// }


// void sol_print_internal (const sol_t *self) {
//     assert (self);
//     printf ("\nsol: #routes: %zu, total distance: %.2f\n",
//             listx_size (self->routes), self->total_distance);
//     printf ("--------------------------------------------------\n");
//     for (size_t idx_r = 0; idx_r < listx_size (self->routes); idx_r++) {
//         route_t *route = listx_item_at (self->routes, idx_r);
//         size_t route_len = route_size (route);
//         printf ("route #%zu (#nodes: %zu):", idx_r, route_len);
//         for (size_t idx_n = 0; idx_n < route_len; idx_n++)
//             printf (" %zu", route_at (route, idx_n));
//         printf ("\n");
//     }
//     printf ("\n");
// }


// sol_iterator_t sol_iter_init (const sol_t *self) {
//     assert (self);
//     return (sol_iterator_t) {SIZE_NONE, NULL, SIZE_NONE, ID_NONE};
// }


// size_t sol_iter_node (const sol_t *self, sol_iterator_t *iter) {
//     assert (self);
//     assert (iter);

//     // Not first iteration
//     if (iter->route != NULL) {
//         // next node of current route
//         if (iter->idx_node < route_size (iter->route) - 1) {
//             iter->idx_node++;
//             iter->node_id = route_at (iter->route, iter->idx_node);
//             return iter->node_id;
//         }
//         // first node of next route
//         else {
//             // there is next route
//             if (iter->idx_route < listx_size (self->routes) - 1) {
//                 iter->idx_route++;
//                 iter->route = listx_item_at (self->routes, iter->idx_route);
//                 assert (route_size (iter->route) > 0);
//                 iter->idx_node = 0;
//                 iter->node_id = route_at (iter->route, 0);
//                 return iter->node_id;
//             }
//             // there is no next route
//             else
//                 return ID_NONE;
//         }
//     }
//     // First iteration
//     else {
//         iter->route = listx_first (self->routes);
//         if (iter->route != NULL) {
//             iter->idx_route = 0;
//             assert (route_size (iter->route) > 0);
//             iter->idx_node = 0;
//             iter->node_id = route_at (iter->route, 0);
//             return iter->node_id;
//         }
//         else // no route exists in sol
//             return ID_NONE;
//     }
// }



