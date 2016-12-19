/*  =========================================================================
    route - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


route_t *route_new (size_t alloc_size) {
    return listu_new (alloc_size);
}


route_t *route_new_from_array (const size_t *node_ids, size_t num_nodes) {
    return listu_new_from_array (node_ids, num_nodes);
}


route_t *route_new_from_list (const listu_t *node_ids) {
    return listu_dup (node_ids);
}


route_t *route_new_range (size_t start, size_t stop, int step) {
    return listu_new_range (start, stop, step);
}


void route_free (route_t **self_p) {
    assert (self_p);
    listu_free (self_p);
}


route_t *route_dup (const route_t *self) {
    assert (self);
    return listu_dup (self);
}


bool route_equal (const route_t *self, const route_t *route) {
    assert (self);
    assert (route);
    return listu_equal (self, route);
}


void route_print (const route_t *self) {
    size_t size = route_size (self);
    printf ("route (#node: %zu):", size);
    for (size_t idx = 0; idx < size; idx++)
        printf (" %zu", route_at (self, idx));
    printf ("\n");
}


size_t route_size (const route_t *self) {
    assert (self);
    return listu_size (self);
}


void route_set_at (route_t *self, size_t idx, size_t node_id) {
    assert (self);
    assert (idx < route_size (self));
    listu_set (self, idx, node_id);
}


size_t route_at (const route_t *self, size_t idx) {
    assert (self);
    assert (idx < route_size (self));
    return listu_get (self, idx);
}


const size_t *route_node_array (const route_t *self) {
    assert (self);
    return listu_array (self);
}


void route_append_node (route_t *self, size_t node_id) {
    assert (self);
    listu_append (self, node_id);
}


size_t route_find (const route_t *self, size_t node_id) {
    assert (self);
    return listu_find (self, node_id);
}


void route_swap_nodes (route_t *self, size_t idx1, size_t idx2) {
    assert (self);
    listu_swap (self, idx1, idx2);
}


double route_total_distance (const route_t *self,
                             const void *context,
                             vrp_arc_distance_t dist_fn) {
    assert (self);
    size_t len = route_size (self);
    double total_dist = 0;
    for (size_t idx = 0; idx < len - 1; idx++)
        total_dist += dist_fn (context,
                               route_at (self, idx),
                               route_at (self, idx + 1));
    return total_dist;
}


void route_shuffle (route_t *self,
                    size_t idx_begin, size_t idx_end, rng_t *rng) {
    assert (self);
    listu_shuffle_slice (self, idx_begin, idx_end, rng);
}


void route_rotate (route_t *self, int num) {
    assert (self);
    listu_rotate (self, num);
}


double route_reverse_delta_distance (const route_t *self,
                                     size_t i, size_t j,
                                     const void *context,
                                     vrp_arc_distance_t dist_fn) {
    assert (self);
    assert (i <= j);
    size_t len = route_size (self);
    assert (j < len);

    if (i == j)
        return 0;

    double ddist = 0;

    // Bug fixed: i or j may be the end.
    if (i > 0)
        ddist = ddist +
            dist_fn (context, route_at (self, i - 1), route_at (self, j)) -
            dist_fn (context, route_at (self, i - 1), route_at (self, i));

    if (j < len - 1)
        ddist = ddist +
            dist_fn (context, route_at (self, i), route_at (self, j + 1)) -
            dist_fn (context, route_at (self, j), route_at (self, j + 1));

    for (size_t k = i; k < j; k++) {
        ddist = ddist +
          dist_fn (context, route_at (self, k + 1), route_at (self, k)) -
          dist_fn (context, route_at (self, k), route_at (self, k + 1));
    }

    return ddist;
}


void route_reverse (route_t *self, size_t i, size_t j) {
    assert (self);
    listu_reverse_slice (self, i, j);
}


double route_swap_slices_delata_distance (const route_t *self,
                                        size_t i, size_t j,
                                        size_t u, size_t v,
                                        const void *context,
                                        vrp_arc_distance_t dist_fn) {
    assert (self);
    assert (i <= j);
    assert (j < u);
    assert (u <= v);
    size_t size = route_size (self);
    assert (v < size);

    size_t node_i = route_at (self, i);
    size_t node_j = route_at (self, j);
    size_t node_u = route_at (self, u);
    size_t node_v = route_at (self, v);
    double ddist = 0;

    if (i > 0) {
        size_t node_i_pred = route_at (self, i - 1);
        ddist = ddist -
                dist_fn (context, node_i_pred, node_i) +
                dist_fn (context, node_i_pred, node_u);
    }

    if (v + 1 < size) {
        size_t node_v_succ = route_at (self, v + 1);
        ddist = ddist -
                dist_fn (context, v, node_v_succ) +
                dist_fn (context, j, node_v_succ);
    }

    if (j + 1 < u) {
        size_t node_j_succ = route_at (self, j + 1);
        size_t node_u_pred = route_at (self, u - 1);
        ddist = ddist -
                dist_fn (context, node_j, node_j_succ) -
                dist_fn (context, node_u_pred, node_u) +
                dist_fn (context, node_v, node_j_succ) +
                dist_fn (context, node_u_pred, node_i);
    }
    else // j directly links with u
        ddist = ddist -
                dist_fn (context, node_j, node_u) +
                dist_fn (context, node_v, node_i);

    return ddist;
}


void route_swap_slices (route_t *self, size_t i, size_t j, size_t u, size_t v) {
    assert (self);
    listu_swap_slices (self, i, j, u, v);
}


double route_remove_node_delta_distance (route_t *self,
                                         size_t idx,
                                         const void *context,
                                         vrp_arc_distance_t dist_fn) {
    assert (self);
    size_t size = route_size (self);
    assert (idx < size);

    double ddist = 0;
    if (idx > 0) // there is a predecessor to unlink with
        ddist -= dist_fn (context,
                          route_at (self, idx - 1),
                          route_at (self, idx));
    if (idx + 1 < size) // there is a successor to unlink with
        ddist -= dist_fn (context,
                          route_at (self, idx),
                          route_at (self, idx + 1));
    if (idx > 0 && idx + 1 < size) // link predesessor with successor
        ddist += dist_fn (context,
                          route_at (self, idx - 1),
                          route_at (self, idx + 1));
    return ddist;
}


void route_remove_node (route_t *self, size_t idx) {
    assert (self);
    assert (idx < route_size (self));
    listu_remove_at (self, idx);
}


double route_remove_link_delta_distance (route_t *self,
                                         size_t idx,
                                         const void *context,
                                         vrp_arc_distance_t dist_fn) {
    assert (self);
    size_t size = route_size (self);
    assert (idx + 1 < size);

    double ddist = 0;
    if (idx > 0) // there is a predecessor to unlink with
        ddist -= dist_fn (context,
                          route_at (self, idx - 1),
                          route_at (self, idx));
    if (idx + 2 < size) // there is a successor to unlink with
        ddist -= dist_fn (context,
                          route_at (self, idx + 1),
                          route_at (self, idx + 2));
    if (idx > 0 && idx + 2 < size) // link predesessor with successor
        ddist += dist_fn (context,
                          route_at (self, idx - 1),
                          route_at (self, idx + 2));
    return ddist;
}


void route_remove_link (route_t *self, size_t idx) {
    assert (self);
    assert (idx + 1 < route_size (self));
    listu_remove_slice (self, idx, idx + 1);
}


double route_insert_node_delta_distance (const route_t *self,
                                         size_t idx,
                                         size_t node_id,
                                         const void *context,
                                         vrp_arc_distance_t dist_fn) {
    assert (self);
    size_t size = route_size (self);
    assert (idx <= size);

    double ddist = 0;
    if (idx > 0) // there is a node before to link with
        ddist += dist_fn (context, route_at (self, idx - 1), node_id);
    if (idx < size) // there is a node after to link with
        ddist += dist_fn (context, node_id, route_at (self, idx));
    if (idx > 0 && idx < size) // there is a link before to remove
        ddist -=
            dist_fn (context, route_at (self, idx - 1), route_at (self, idx));
    return ddist;
}


void route_insert_node (route_t *self, size_t idx, size_t node_id) {
    assert (self);
    assert (idx <= route_size (self));
    listu_insert_at (self, idx, node_id);
}


// Distance increment of replacing node at index with new node
static double route_replace_node_delta_distance (const route_t *self,
                                                 size_t idx, size_t new_node,
                                                 const void *context,
                                                 vrp_arc_distance_t dist_fn) {
    size_t size = route_size (self);
    size_t old_node = route_at (self, idx);
    if (new_node == old_node)
        return 0;

    double ddist = 0;
    if (idx > 0) { // there is a predecessor
        size_t predecessor = route_at (self, idx - 1);
        ddist -= dist_fn (context, predecessor, old_node);
        ddist += dist_fn (context, predecessor, new_node);
    }
    if (idx + 1 < size) { // there is successor
        size_t successor = route_at (self, idx + 1);
        ddist -= dist_fn (context, old_node, successor);
        ddist += dist_fn (context, new_node, successor);
    }
    return ddist;
}


double route_exchange_nodes_delta_distance (const route_t *self,
                                            const route_t *route,
                                            size_t idx1, size_t idx2,
                                            const void *context,
                                            vrp_arc_distance_t dist_fn) {
    assert (self);
    assert (route);
    assert (self != route);
    assert (idx1 < route_size (self));
    assert (idx2 < route_size (route));

    return
        route_replace_node_delta_distance (self,
                                           idx1,
                                           route_at (route, idx2),
                                           context,
                                           dist_fn) +
        route_replace_node_delta_distance (route,
                                           idx2,
                                           route_at (self, idx1),
                                           context,
                                           dist_fn);
}


void route_exchange_nodes (route_t *self, route_t *route,
                           size_t idx1, size_t idx2) {
    assert (self);
    assert (route);
    assert (self != route);
    assert (idx1 < route_size (self));
    assert (idx2 < route_size (route));

    route_print (self);
    route_print (route);
    printf ("idx1: %zu, idx2: %zu\n", idx1, idx2);
    size_t node1 = route_at (self, idx1);
    route_set_at (self, idx1, route_at (route, idx2));
    route_set_at (route, idx2, node1);
    route_print (self);
    route_print (route);
}


double route_exchange_tails_delta_distance (const route_t *self,
                                            const route_t *route,
                                            size_t idx1, size_t idx2,
                                            const void *context,
                                            vrp_arc_distance_t dist_fn) {
    assert (self);
    assert (route);
    assert (self != route);
    size_t size1 = route_size (self);
    size_t size2 = route_size (route);
    assert (idx1 < size1);
    assert (idx2 < size2);

    double ddist = 0;

    // There is node after idx1 of self
    if (idx1 + 1 < size1)
        ddist = ddist -
                dist_fn (context,
                         route_at (self, idx1),
                         route_at (self, idx1 + 1)) +
                dist_fn (context,
                         route_at (route, idx2),
                         route_at (self, idx1 + 1));

    // There is node after idx2 of route
    if (idx2 + 1 < size2)
        ddist = ddist -
                dist_fn (context,
                         route_at (route, idx2),
                         route_at (route, idx2 + 1)) +
                dist_fn (context,
                         route_at (self, idx1),
                         route_at (route, idx2 + 1));

    return ddist;
}


void route_exchange_tails (route_t *self, route_t *route,
                           size_t idx1, size_t idx2) {
    assert (self);
    assert (route);
    assert (self != route);
    size_t size1 = route_size (self);
    size_t size2 = route_size (route);
    assert (idx1 < size1);
    assert (idx2 < size2);

    if (idx1 == size1 - 1 && idx2 == size2 - 1)
        return;

    // Swap common length of tail
    size_t tmp;
    while (idx1 < size1 - 1 && idx2 < size2 - 1) {
        idx1++;
        idx2++;
        tmp = route_at (self, idx1);
        route_set_at (self, idx1, route_at (route, idx2));
        route_set_at (route, idx2, tmp);
    }

    // Deal with remaining tail
    if (idx1 < size1 - 1) {
        const size_t *array = route_node_array (self);
        listu_extend_array (route, array + idx1 + 1, size1 - idx1 - 1);
        listu_remove_slice (self, idx1 + 1, size1 - 1);
    }
    else if (idx2 < size2 - 1) {
        const size_t *array = route_node_array (route);
        listu_extend_array (self, array + idx2 + 1, size2 - idx2 - 1);
        listu_remove_slice (route, idx2 + 1, size2 - 1);
    }
}


void route_ox (route_t *route1, route_t *route2,
               size_t start, size_t end, rng_t *rng) {
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


// double route_2_opt (route_t *self,
//                     const vrp_t *vrp,
//                     size_t idx_begin, size_t idx_end,
//                     bool exhaustive) {
//     assert (self);
//     assert (vrp);
//     assert (idx_begin <= idx_end);
//     assert (idx_end < route_size (self));

//     if (idx_begin == idx_end)
//         return 0;

//     double total_delta_cost = 0, delta_cost;
//     bool improved = true;

//     while (improved) {
//         improved = false;
//         for (size_t i = idx_begin; i < idx_end && !improved; i++) {
//             for (size_t j = i + 1; j <= idx_end && !improved; j++) {
//                 delta_cost = route_reverse_delta_distance (self, i, j, vrp);
//                 if (delta_cost < 0) {
//                     route_reverse (self, i, j);
//                     total_delta_cost += delta_cost;
//                     // For non-exhaustive mode, stop searching
//                     if (!exhaustive)
//                         return total_delta_cost;
//                     // For exhaustive mode, set improved to start searching over
//                     improved = true;
//                 }
//             }
//         }
//     }

//     return total_delta_cost;
// }


void route_test (bool verbose) {
    print_info (" * route: \n");
    // roadnet_t *roadnet = roadnet_new ();
    // // ...
    // roadnet_free (&roadnet);
    print_info ("OK\n");
}
