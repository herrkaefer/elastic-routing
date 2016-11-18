/*  =========================================================================
    route - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"

// For now just extension of listu_t
// typedef listu_t _route_t;


route_t *route_new (size_t size) {
    return listu_new (size);
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


void route_shuffle_slice (route_t *self,
                          size_t idx_begin, size_t idx_end, rng_t *rng) {
    assert (self);
    listu_shuffle_slice (self, idx_begin, idx_end, rng);
}


void route_swap (route_t *self, size_t index1, size_t index2) {
    assert (self);
    listu_swap (self, index1, index2);
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
