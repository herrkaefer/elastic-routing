/*  =========================================================================
    route - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"

// For now just extension of listu_t
// typedef listu_t _route_t;


route_t *route_new (size_t alloc_size) {
    return listu_new (alloc_size);
}


route_t *route_new_from_array (const size_t *node_ids, size_t num_nodes) {
    return listu_new_from_array (node_ids, num_nodes);
}


route_t *route_new_from_list (const listu_t *node_ids) {
    return listu_dup (node_ids);
}


void route_free (route_t **self_p) {
    assert (self_p);
    listu_free (self_p);
}


size_t route_size (route_t *self) {
    assert (self);
    return listu_size (self);
}


void route_set_at (route_t *self, size_t idx, size_t node_id) {
    assert (self);
    assert (idx < route_size (self));
    listu_set (self, idx, node_id);
}


size_t route_at (route_t *self, size_t idx) {
    assert (self);
    assert (idx < route_size (self));
    return listu_get (self, idx);
}


const size_t *route_node_array (route_t *self) {
    assert (self);
    return listu_array (self);
}


route_t *route_dup (const route_t *self) {
    assert (self);
    return listu_dup (self);
}


void route_append_node (route_t *self, size_t node_id) {
    assert (self);
    listu_append (self, node_id);
}


size_t route_find (route_t *self, size_t node_id) {
    assert (self);
    return listu_find (self, node_id);
}


void route_swap_nodes (route_t *self, size_t idx1, size_t idx2) {
    assert (self);
    listu_swap (self, idx1, idx2);
}


double route_total_distance (route_t *self, vrp_t *vrp) {
    assert (self);
    assert (vrp);
    size_t len = route_size (self);
    double total_dist = 0;
    for (size_t idx = 0; idx < len - 1; idx++)
        total_dist += vrp_arc_distance (vrp,
                                        route_at (self, idx),
                                        route_at (self, idx + 1));
    return total_dist;
}


void route_shuffle (route_t *self,
                    size_t idx_begin, size_t idx_end, rng_t *rng) {
    assert (self);
    listu_shuffle_slice (self, idx_begin, idx_end, rng);
}


// Cost increment for route_flip operation.
// Flip of route slice [i, j]
// (0, ..., i-1, i, i+1, ..., j-1, j, j+1, ..., route_length-1) =>
// (0, ..., i-1, j, j-1, ..., i+1, i, j+1, ..., route_length-1)
double route_flip_delta_distance (route_t *self,
                                  vrp_t *vrp, int i, int j) {
    assert (self);
    assert (vrp);
    assert (i <= j);
    assert (j < route_size (self));

    if (i == j)
        return 0;

    double dcost =
        vrp_arc_distance (vrp, route_at (self, i - 1), route_at (self, j)) +
        vrp_arc_distance (vrp, route_at (self, i), route_at (self, j + 1)) -
        vrp_arc_distance (vrp, route_at (self, i - 1), route_at (self, i)) -
        vrp_arc_distance (vrp, route_at (self, j), route_at (self, j + 1));

    for (size_t k = i; k < j; k++) {
        dcost = dcost +
          vrp_arc_distance (vrp, route_at (self, k + 1), route_at (self, k)) -
          vrp_arc_distance (vrp, route_at (self, k), route_at (self, k + 1));
    }
    return dcost;
}


void route_flip (route_t *self, size_t i, size_t j) {
    assert (self);
    listu_reverse_slice (self, i, j);
}


void route_swap (route_t *self, size_t i, size_t j, size_t u, size_t v) {
    assert (self);
    listu_swap_slices (self, i, j, u, v);
}


void route_ox (route_t *route1, route_t *route2,
               size_t start, size_t end,
               rng_t *rng) {
    assert (route1);
    assert (route2);
    assert(start <= end);

    bool own_rng = false;
    if (rng == NULL) {
        rng = rng_new ();
        own_rng = true;
    }

    size_t i, j, k, tmp;
    size_t n = end - start + 1;
    size_t len_fix;
    // size_t *P1 = route1 + start,
    //        *P2 = route2 + start;
    const size_t *P1 = route_node_array (route1) + start;
    const size_t *P2 = route_node_array (route2) + start;
    size_t pos_c1, pos_c2;

    i = rng_random_int (rng, 0, n);
    j = rng_random_int (rng, 0, n);

    if (i > j) {
        k = i;
        i = j;
        j = k;
    }
    len_fix = j - i + 1;

    // PRINT("\nP1: ");
    // print_route(P1, 0, n-1, NULL);
    // PRINT("P2: ");
    // print_route(P2, 0, n-1, NULL);
    // PRINT("\nfix segment: %d -> %d. route1: %d->%d, route2: %d->%d\n", i, j, P1[i], P1[j], P2[i], P2[j]);

    // circular
    size_t cnt = 0;
    for (cnt = 0, k = (j + 1) % n, pos_c1 = k, pos_c2 = k;
         cnt < n && (pos_c1 != i || pos_c2 != i);
         cnt++, k = (k + 1) % n) {
        // PRINT("n: %d, cnt: %d, k: %d, pos_c1: %d, pos_c2: %d\n", n, cnt, k, pos_c1, pos_c2);
        tmp = P1[k];
        if (pos_c1 != i && !arrayu_includes (P1+i, len_fix, P2[k])) {
            // P1[pos_c1++] = P2[k];
            listu_set (route1, start + pos_c1, P2[k]);
            pos_c1++;
            if (pos_c1 == n)
                pos_c1 = 0;
        }
        if (pos_c2 != i && !arrayu_includes (P2+i, len_fix, tmp)) {
            // P2[pos_c2++] = tmp;
            listu_set (route2, start + pos_c2, tmp);
            pos_c2++;
            if (pos_c2 == n)
                pos_c2 = 0;
        }
    }

    // PRINT("\nC1: ");
    // print_route(C1, 0, n-1, NULL);
    // PRINT("C2: ");
    // print_route(C2, 0, n-1, NULL);
    if (own_rng)
        rng_free (&rng);
}


double route_2_opt (route_t *self,
                    vrp_t *vrp, size_t idx_begin, size_t idx_end) {
    assert (self);
    assert (vrp);
    assert (idx_begin <= idx_end);
    assert (idx_end < route_size (self));

    print_info ("2-opt local search start.\n");
    if (idx_begin == idx_end)
        return 0;

    // size_t route_len = route_size (route);
    // size_t idx_begin = (self->start_node != SIZE_NONE) ? 1 : 0;
    // size_t idx_end =
    //     (self->end_node != SIZE_NONE) ? (route_len-2) : (route_len-1);
    // print_info ("idx_begin: %zu, idx_end: %zu\n", idx_begin, idx_end);

    double total_delta_cost = 0, delta_cost;
    bool improved = true;

    while (improved) {
        improved = false;
        for (size_t i = idx_begin; i < idx_end; i++) {
            for (size_t j = i + 1; j <= idx_end; j++) {
                delta_cost = route_flip_delta_distance (self, vrp, i, j);
                if (delta_cost < -DOUBLE_THRESHOLD) {
                    route_flip (self, i, j);
                    total_delta_cost += delta_cost;
                    improved = true;
                }
            }
        }
    }

    return total_delta_cost;
}


void route_print (route_t *self) {
    size_t size = route_size (self);
    printf ("route (#node: %zu):", size);
    for (size_t idx = 0; idx < size; idx++)
        printf (" %zu", route_at (self, idx));
    printf ("\n");
}


void route_test (bool verbose) {
    print_info (" * route: \n");
    // roadnet_t *roadnet = roadnet_new ();
    // // ...
    // roadnet_free (&roadnet);
    print_info ("OK\n");
}
