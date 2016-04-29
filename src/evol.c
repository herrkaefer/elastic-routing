/*  =========================================================================
    evol - evolution algorithm

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/
/*
@todo

- test renew
- add diversification
    - 当slow down条件满足后report结果，可以选择停止，或者进行diversify后继续。
- 消息机制（放在evol还是solver?）

- [x]rewrite evol_renew_population ()
- [x]diversity的计算及更新
    - 精确算法
    - 近似算法
- [x]把individual放在arrayset里似乎没有必要。

*/

#include "classes.h"

#define EVOL_DEFAULT_MAX_LIVINGS   200
#define EVOL_DEFAULT_MAX_ANCESTORS 40
#define EVOL_DEFAULT_MAX_CHILDREN  10
#define EVOL_DEFAULT_STEP_MAX_ITERS 10
#define EVOL_DEFAULT_STEP_MAX_TIME 0.1 // seconds
#define EVOL_DEFAULT_NUM_DICINGS_FOR_PARENT1 1
#define EVOL_DEFAULT_NUM_DICINGS_FOR_PARENT2 3
#define EVOL_DEFAULT_NUM_DICINGS_FOR_MUTATION 1
#define EVOL_DEFAULT_MAX_NEIGHBORS 5
#define EVOL_DEFAULT_WEIGHT_FITNESS 0.8

#define EVOL_DEFAULT_UNIMPROVED_ITERS 2000
#define EVOL_DEFAULT_UNIMPROVED_PERIOD 3.0 // seconds
#define EVOL_DEFAULT_MIN_IMPROVED_FITNESS 0.01 // percent


typedef struct {
    genome_t genome; // core representation of individual
    free_func_t genome_destructor;
    print_func_t genome_printer;

    // Attributes
    bool   feasible;
    double fitness;
    double diversity;
    double score; // func (alpha * norm_fitness + beta * norm_diversity)

    // handles in livings group
    void *handle_livings_fit;
    void *handle_livings_div;
    void *handle_livings_score;

    // handle in ancestors group
    void *handle_ancestors;

    // handle in children group
    void *handle_children;

    // Auxiliaries
    list4x_t *neighbors; // list of s_neighbor_t. This individual's neighbors.
                         // max size: evol_t.max_neighbors
    list4x_t *as_neighbors; // list of handles of items in other individuals'
                            // neighbors.
} s_indiv_t;


typedef struct {
    s_indiv_t *indiv; // individual who is self's neighbor
    double distance;
} s_neighbor_t;


typedef struct {
    s_indiv_t *indiv; // individual who see self as neighbor
    void *handle_in_neighbors; // handle of item in indiv->neighbors
} s_asneighbor_t;


typedef struct {
    evol_heuristic_t fn;
    bool is_random;
    size_t max_expected;
} s_heuristic_t;


struct _evol_t {

    // ------------------ Population ------------------ //

    list4x_t *livings_rank_fit; // livings sorted by fitness
    list4x_t *livings_rank_div; // livings sorted by diversity
    list4x_t *livings_rank_score; // livings sorted by score
    list4x_t *ancestors; // sorted by adding sequence (a FIFO queue)
    list4x_t *children; // sorted by fitness

    // ------------------ Parameters ------------------ //

    // Individual context
    void *context;

    // Group size
    size_t max_livings;
    size_t max_ancestors;
    size_t max_children;

    // Computational limitaitons
    size_t max_iters;
    double max_time; // second

    // Performance limitations
    size_t unimproved_iters;
    double unimproved_period;
    double min_improved_fit; // percentage

    // Step computational limitaitions
    // (one step is a number of non-disrupted iterations)
    size_t step_max_iters;
    double step_max_time; // second

    // Callbacks
    free_func_t                genome_destructor;
    // hash_func_t                hash_func;
    duplicate_func_t           genome_duplicator;
    print_func_t               genome_printer;
    evol_feasiblity_assessor_t feasiblity_assessor;
    evol_fitness_assessor_t    fitness_assessor;
    evol_distance_assessor_t   distance_assessor;
    evol_should_renew_t        should_renew;
    evol_renewer_t             renewer;
    evol_stopper_t             stopper; // outside stopper

    list4x_t *heuristics; // heuristic funcs
    list4x_t *crossovers; // crossover funcs
    list4x_t *mutators; // mutation funcs
    list4x_t *local_improvers; // local improvement funcs

    // Parent picking dicing
    size_t num_dicings_parent1;
    size_t num_dicings_parent2;
    size_t num_dicings_mutation;

    // Maximal number of closest individuals defined as neighbors for
    // individual's diversity assessment.
    size_t max_neighbors;

    // Weight of fitness for score computation.
    // Weight of diversity then is (1 - weight_fit)
    double weight_fit;

    // ------------------ Recorders ------------------ //

    // For run
    size_t  iters_cnt;
    timer_t *timer;

    // For step
    size_t  step_iters_cnt;
    timer_t *step_timer;

    // For improvement
    double last_best_fit;
    double improved_fit_percent;
    size_t improved_fit_iters_begin;
    double improved_fit_time_begin;

    // ------------------ Statistics ------------------ //

    double initial_best_fit;
    double overall_improved_fit;

    // ------------------ Auxiliaries ------------------ //

    rng_t *rng;
    s_indiv_t *parent1, *parent2;
};


// Create a new neighbor
static s_neighbor_t *s_neighbor_new (s_indiv_t *indiv, double distance) {
    s_neighbor_t *self = (s_neighbor_t *) malloc (sizeof (s_neighbor_t));
    assert (self);
    self->indiv = indiv;
    self->distance = distance;
    return self;
}


// Destroy a neighbor
static void s_neighbor_free (s_neighbor_t **self_p) {
    assert (self_p);
    if (*self_p) {
        free (*self_p);
        *self_p = NULL;
    }
}


// Compare two neighbors
static int s_neighbor_compare (const s_neighbor_t *neighbor1,
                               const s_neighbor_t *neighbor2) {
    return (neighbor1->distance < neighbor2->distance) ?
           -1 :
           ((neighbor1->distance > neighbor2->distance) ? 1 : 0);
}


// Create a new neighbor
static s_asneighbor_t *s_asneighbor_new (s_indiv_t *indiv, void *handle) {
    s_asneighbor_t *self = (s_asneighbor_t *) malloc (sizeof (s_asneighbor_t));
    assert (self);
    self->indiv = indiv;
    self->handle_in_neighbors = handle;
    return self;
}


// // Destroy a neighbor
static void s_asneighbor_free (s_asneighbor_t **self_p) {
    assert (self_p);
    if (*self_p) {
        free (*self_p);
        *self_p = NULL;
    }
}


// Create a new individual
static s_indiv_t *s_indiv_new (genome_t genome,
                               free_func_t genome_destructor) {
    assert (genome);

    s_indiv_t *self = (s_indiv_t *) malloc (sizeof (s_indiv_t));
    assert (self);

    self->genome = genome;
    if (genome_destructor)
        self->genome_destructor = genome_destructor;
    else
        self->genome_destructor = NULL;

    self->feasible    = false;
    self->fitness     = DOUBLE_NONE;
    self->diversity   = DOUBLE_NONE;
    self->score       = DOUBLE_NONE;

    self->handle_livings_fit   = NULL;
    self->handle_livings_div   = NULL;
    self->handle_livings_score = NULL;
    self->handle_ancestors     = NULL;
    self->handle_children      = NULL;

    self->neighbors = list4x_new ();
    list4x_set_destructor (self->neighbors, (free_func_t) s_neighbor_free);
    list4x_set_comparator (self->neighbors, (compare_func_t) s_neighbor_compare);
    list4x_sort (self->neighbors, true);

    self->as_neighbors = list4x_new ();
    list4x_set_destructor (self->as_neighbors, (free_func_t) s_asneighbor_free);

    return self;
}


// Destroy an individual
static void s_indiv_free (s_indiv_t **self_p) {
    assert (self_p);
    if (*self_p) {
        s_indiv_t *self = *self_p;
        if (self->genome_destructor)
            self->genome_destructor (&self->genome);
        list4x_free (&self->neighbors);
        list4x_free (&self->as_neighbors);
        free (self);
        *self_p = NULL;
    }
}


// Reset individual's all properties to the state after creation
static void s_indiv_reset (s_indiv_t *self) {
    self->feasible = false;
    self->fitness     = DOUBLE_NONE;
    self->diversity   = DOUBLE_NONE;
    self->score       = DOUBLE_NONE;

    self->handle_livings_fit   = NULL;
    self->handle_livings_div   = NULL;
    self->handle_livings_score = NULL;
    self->handle_ancestors     = NULL;
    self->handle_children      = NULL;

    list4x_purge (self->neighbors);
    list4x_purge (self->as_neighbors);
}


// Individual compare fitness function
static int s_indiv_compare_fitness (const s_indiv_t *indiv1,
                                    const s_indiv_t *indiv2) {
    return (indiv1->fitness < indiv2->fitness) ?
           -1 :
           ((indiv1->fitness > indiv2->fitness) ? 1 : 0);
}


// Individual compare diversity function
static int s_indiv_compare_diversity (const s_indiv_t *indiv1,
                                      const s_indiv_t *indiv2) {
    return (indiv1->diversity < indiv2->diversity) ?
           -1 :
           ((indiv1->diversity > indiv2->diversity) ? 1 : 0);
}


// Individual compare score function
static int s_indiv_compare_score (const s_indiv_t *indiv1,
                                  const s_indiv_t *indiv2) {
    return (indiv1->score < indiv2->score) ?
           -1 :
           ((indiv1->score > indiv2->score) ? 1 : 0);
}


// Check if indiv is in self's neighbors list
static bool s_indiv_has_neighbor (s_indiv_t *self, s_indiv_t *indiv) {
    if (list4x_size (self->neighbors) == 0)
        return false;
    s_neighbor_t *neighbor = NULL;
    list4x_iterator_t iter = list4x_iter_init (self->neighbors, true);
    while ((neighbor = (s_neighbor_t *) list4x_iter (self->neighbors, &iter))
           != NULL) {
        if (neighbor->indiv == indiv)
            return true;
    }
    return false;
}


// Check if indiv is in self's as_neighbors list
static bool s_indiv_has_as_neighbor (s_indiv_t *self, s_indiv_t *indiv) {
    if (list4x_size (self->as_neighbors) == 0)
        return false;
    s_asneighbor_t *as_nb = NULL;
    list4x_iterator_t iter = list4x_iter_init (self->as_neighbors, true);
    while ((as_nb = (s_asneighbor_t *) list4x_iter (self->as_neighbors, &iter))
           != NULL) {
        if (as_nb->indiv == indiv)
            return true;
    }
    return false;
}


// Add as neighbor record to indiv if it becomes someone else's neighbor
static void s_indiv_add_as_neighbor (s_indiv_t *self,
                                     s_indiv_t *as_neighbor,
                                     void *handle_in_neighbors) {
    assert (self ==
            ((s_neighbor_t *)
                list4x_item (as_neighbor->neighbors, handle_in_neighbors))
            ->indiv);
    assert (!s_indiv_has_as_neighbor (self, as_neighbor));

    list4x_append (self->as_neighbors,
                   s_asneighbor_new (as_neighbor, handle_in_neighbors));
}


static bool s_indiv_is_out_of_population (s_indiv_t *self) {
    return (self->handle_livings_fit == NULL &&
            self->handle_ancestors   == NULL &&
            self->handle_children    == NULL);
}


static bool s_indiv_is_in_livings_group (s_indiv_t *self) {
    return (self->handle_livings_fit != NULL);
}


static bool s_indiv_is_in_ancestors_group (s_indiv_t *self) {
    return (self->handle_ancestors != NULL);
}


static bool s_indiv_is_in_children_group (s_indiv_t *self) {
    return (self->handle_children != NULL);
}


// Create new heuristic structure
static s_heuristic_t *s_heuristic_new (evol_heuristic_t fn,
                                       bool is_random,
                                       size_t max_expected) {
    s_heuristic_t *self = (s_heuristic_t *) malloc (sizeof (s_heuristic_t));
    assert (self);
    self->fn = fn;
    self->is_random = is_random;
    self->max_expected = max_expected;
    return self;
}


// Destroy a heuristic structure
static void s_heuristic_free (s_heuristic_t **self_p) {
    assert (self_p);
    if (*self_p) {
        s_heuristic_t *self = *self_p;
        free (self);
        *self_p = NULL;
    }
}


// ---------------------------------------------------------------------------

// Create new livings group
static void evol_new_livings_group (evol_t *self) {
    // List sorted by fitness descending order
    self->livings_rank_fit = list4x_new ();
    list4x_set_comparator (self->livings_rank_fit,
                           (compare_func_t) s_indiv_compare_fitness);
    list4x_sort (self->livings_rank_fit, false);

    // List sorted by diversity descending order
    self->livings_rank_div = list4x_new ();
    list4x_set_comparator (self->livings_rank_div,
                           (compare_func_t) s_indiv_compare_diversity);
    list4x_sort (self->livings_rank_div, false);

    // List sorted by score descending order.
    // Let this list be responsible for destroying items (individuals).
    self->livings_rank_score = list4x_new ();
    list4x_set_destructor (self->livings_rank_score,
                           (free_func_t) s_indiv_free);
    list4x_set_comparator (self->livings_rank_score,
                           (compare_func_t) s_indiv_compare_score);
    list4x_sort (self->livings_rank_score, false);
}


// Create new ancestors group
static void evol_new_ancestors_group (evol_t *self) {
    self->ancestors = list4x_new ();
    // Let this list destroy ancestors individuals
    list4x_set_destructor (self->ancestors, (free_func_t) s_indiv_free);
}


// Create new children group
static void evol_new_children_group (evol_t *self) {
    self->children = list4x_new ();
    // Let this list destroy children individuals
    list4x_set_destructor (self->children, (free_func_t) s_indiv_free);
    list4x_set_comparator (self->children,
                           (compare_func_t) s_indiv_compare_fitness);
}


// Get number of livings in livings group
static size_t evol_num_livings (evol_t *self) {
    return list4x_size (self->livings_rank_fit);
}


// Get number of ancestors in ancestors group
static size_t evol_num_ancestors (evol_t *self) {
    return list4x_size (self->ancestors);
}


// Get number of children in children group
static size_t evol_num_children (evol_t *self) {
    return list4x_size (self->children);
}


static void evol_print_indiv (evol_t *self, s_indiv_t *indiv) {
    printf ("\nIndividual:\n");
    if (self->genome_printer)
        self->genome_printer (indiv->genome);
    printf ("feasible: %s, fitness: %8.3f, diversity: %8.3f, score: %8.3f\n",
            indiv->feasible ? "true" : "false",
            indiv->fitness, indiv->diversity, indiv->score);
    printf ("is living: %s, #neighbors: %zu, #as_neighbors: %zu\n\n",
            indiv->handle_livings_fit ? "true" : "false",
            list4x_size (indiv->neighbors),
            list4x_size (indiv->as_neighbors));
}


static void evol_print_group (evol_t *self, list4x_t *group) {
    s_indiv_t *indiv;
    printf ("\ngroup size: %zu, sorted: %s\n",
        list4x_size (group),
        list4x_is_sorted (group) ?
        (list4x_is_sorted_ascending (group) ? "ascending" : "descending") :
        "no");
    printf ("-------------------------------------------------------\n");

    list4x_iterator_t iter = list4x_iter_init (group, true);
    while ((indiv = (s_indiv_t *) list4x_iter (group, &iter)) != NULL) {
        if (self->genome_printer)
            self->genome_printer (indiv->genome);
        printf ("fitness: %8.3f, diversity: %8.3f, score: %8.3f\n",
                indiv->fitness, indiv->diversity, indiv->score);
        printf ("is living: %s, #neighbors: %zu, #as_neighbors: %zu\n",
            indiv->handle_livings_fit ? "true" : "false",
            list4x_size (indiv->neighbors),
            list4x_size (indiv->as_neighbors));
    }
    printf ("-------------------------------------------------------\n");
}


// Assert individual
static void evol_assert_indiv (evol_t *self, s_indiv_t *indiv) {
    assert (indiv);
    assert (indiv->genome);

    // Indiv is in livings group
    if (indiv->handle_livings_fit) {
        assert (indiv->handle_livings_div);
        assert (indiv->handle_livings_score);
        assert (indiv->handle_ancestors == NULL);
        assert (indiv->handle_children == NULL);
        assert (indiv == list4x_item (self->livings_rank_fit,
                                      indiv->handle_livings_fit));
        assert (indiv == list4x_item (self->livings_rank_div,
                                      indiv->handle_livings_div));
        assert (indiv == list4x_item (self->livings_rank_score,
                                      indiv->handle_livings_score));

        assert (!double_is_none (indiv->fitness));
        assert (!double_is_none (indiv->diversity));
        assert (!double_is_none (indiv->score));
    }
    // Indiv is in ancestors group
    else if (indiv->handle_ancestors) {
        assert (indiv->handle_livings_fit == NULL);
        assert (indiv->handle_livings_div == NULL);
        assert (indiv->handle_livings_score == NULL);
        assert (indiv->handle_children == NULL);
        assert (indiv == list4x_item (self->ancestors,
                                      indiv->handle_ancestors));
        assert (!double_is_none (indiv->fitness));
        assert (!double_is_none (indiv->diversity));
        assert (!double_is_none (indiv->score));
    }
    // Indiv is in children group
    else if (indiv->handle_children) {
        assert (indiv->handle_livings_fit == NULL);
        assert (indiv->handle_livings_div == NULL);
        assert (indiv->handle_livings_score == NULL);
        assert (indiv->handle_ancestors == NULL);
        assert (indiv == list4x_item (self->children,
                                      indiv->handle_children));
        assert (!double_is_none (indiv->fitness));
        assert (double_is_none (indiv->diversity));
        assert (double_is_none (indiv->score));
    }

    assert (indiv->neighbors);
    assert (indiv->as_neighbors);
    list4x_assert_sort (indiv->neighbors, "ascending");

    s_neighbor_t *nb = NULL;
    list4x_iterator_t iter = list4x_iter_init (indiv->neighbors, true);
    while ((nb = (s_neighbor_t *) list4x_iter (indiv->neighbors, &iter))
           != NULL) {
        assert (nb->indiv->handle_livings_fit || nb->indiv->handle_ancestors);
        assert (nb->indiv->handle_children == NULL);
    }

    if (indiv->handle_livings_fit || indiv->handle_ancestors) {
        s_asneighbor_t *as_nb = NULL;
        list4x_iterator_t iter = list4x_iter_init (indiv->as_neighbors, true);
        while ((as_nb =
                    (s_asneighbor_t *) list4x_iter (indiv->as_neighbors, &iter))
               != NULL) {
            assert (as_nb->indiv->handle_livings_fit ||
                    as_nb->indiv->handle_ancestors);
            assert (as_nb->indiv->handle_children == NULL);
            assert (s_indiv_has_neighbor (as_nb->indiv, indiv));
            s_neighbor_t *nb =
                (s_neighbor_t *) list4x_item (as_nb->indiv->neighbors,
                                              as_nb->handle_in_neighbors);
            assert (nb->indiv == indiv);
        }
    }
}


static void evol_assert_population (evol_t *self) {
    assert (list4x_size (self->livings_rank_fit) ==
            list4x_size (self->livings_rank_div));
    assert (list4x_size (self->livings_rank_fit) ==
            list4x_size (self->livings_rank_score));
    assert (list4x_size (self->livings_rank_fit) <= self->max_livings + 1);
    assert (list4x_size (self->ancestors) <= self->max_ancestors + 1);
    assert (list4x_size (self->children) <= self->max_children + 1);

    list4x_assert_sort (self->livings_rank_fit, "descending");
    list4x_assert_sort (self->livings_rank_div, "descending");
    list4x_assert_sort (self->livings_rank_score, "descending");
    list4x_assert_sort (self->ancestors, "no");

    s_indiv_t *indiv = NULL;
    list4x_iterator_t iter = list4x_iter_init (self->livings_rank_fit, true);
    while ((indiv = (s_indiv_t *) list4x_iter (self->livings_rank_fit, &iter))
           != NULL)
        evol_assert_indiv (self, indiv);

    iter = list4x_iter_init (self->ancestors, true);
    while ((indiv = (s_indiv_t *) list4x_iter (self->ancestors, &iter))
           != NULL)
        evol_assert_indiv (self, indiv);

    iter = list4x_iter_init (self->children, true);
    while ((indiv = (s_indiv_t *) list4x_iter (self->children, &iter))
           != NULL)
        evol_assert_indiv (self, indiv);
}


// Create new individual for evolution
static s_indiv_t *evol_new_indiv (evol_t *self,
                                  genome_t genome,
                                  free_func_t genome_destructor) {
    if (self->genome_duplicator) {
        // if duplicator is set, destructor should also be set
        assert (genome_destructor);
        genome = self->genome_duplicator (genome);
        assert (genome);
    }
    return s_indiv_new (genome, genome_destructor);
}


// Add neighbor_to_be to indiv's neighbors list
// Return handle in neighors list if added, NULL if failed.
static void *evol_add_neighbor_to_indiv (evol_t *self,
                                         s_indiv_t *indiv,
                                         s_indiv_t *neighbor_to_be,
                                         double distance) {
    assert (indiv != neighbor_to_be);
    assert (!s_indiv_has_neighbor (indiv, neighbor_to_be));
    assert (indiv->genome);
    assert (neighbor_to_be->genome);

    if (double_is_none (distance))
        distance = self->distance_assessor (self->context,
                                            indiv->genome,
                                            neighbor_to_be->genome);

    // void *handle =
    //     list4x_insert_sorted (indiv->neighbors,
    //                           s_neighbor_new (neighbor_to_be, distance));

    // bool added = true;
    // if (list4x_size (indiv->neighbors) > self->max_neighbors) {
    //     s_neighbor_t *farmost = list4x_last (indiv->neighbors);
    //     if (farmost && farmost->indiv == neighbor_to_be)
    //         added = false;
    //     list4x_remove_last (indiv->neighbors);
    // }

    // return added ? s_asneighbor_new (indiv, handle) : NULL;


    bool overflow = false;
    if (list4x_size (indiv->neighbors) == self->max_neighbors) {
        s_neighbor_t *farmost = list4x_last (indiv->neighbors);
        if (farmost && farmost->distance <= distance)
            return NULL;
        overflow = true;
    }

    void *handle =
        list4x_insert_sorted (indiv->neighbors,
                              s_neighbor_new (neighbor_to_be, distance));
    if (overflow)
        list4x_remove_last (indiv->neighbors);

    assert (((s_neighbor_t *) list4x_item (indiv->neighbors, handle))->indiv
            == neighbor_to_be);

    return handle;
}


// Get best fitness value obtained during evolution
static double evol_best_fitness (evol_t *self) {
    if (evol_num_livings (self) == 0)
        return DOUBLE_NONE;
    return ((s_indiv_t *) list4x_first (self->livings_rank_fit))->fitness;
}


// Compute individual's feasibility and fitness
static void evol_assess_feasibility_and_fitness (evol_t *self,
                                                 s_indiv_t *indiv) {
    // print_info ("assess feasibility and fitness.\n");
    indiv->feasible = self->feasiblity_assessor ?
        self->feasiblity_assessor (self->context, indiv->genome) :
        true;

    indiv->fitness = self->fitness_assessor (self->context, indiv->genome);
}


// Compute individual's diversity
// @todo  Here is a testing whole-population implementation
static void evol_assess_diversity (evol_t *self, s_indiv_t *indiv) {
    // print_info ("assess diversity.\n");
    // print_debug ("");
    double sum_distance = 0.0;

    s_indiv_t *indiv_lived;
    size_t cnt = 0;

    // print_debug ("");
    // evol_print_group (self, self->livings_rank_score);

    // livings group
    list4x_iterator_t iter = list4x_iter_init (self->livings_rank_fit, true);
    while ((indiv_lived =
               (s_indiv_t *) list4x_iter (self->livings_rank_fit, &iter))
           != NULL) {
        if (indiv == indiv_lived)
            continue;
        assert (indiv->genome);
        assert (indiv_lived->genome);
        sum_distance += self->distance_assessor (self->context,
                                                 indiv->genome,
                                                 indiv_lived->genome);
        cnt++;
    }

    // ancestors group
    iter = list4x_iter_init (self->ancestors, true);
    while ((indiv_lived = (s_indiv_t *) list4x_iter (self->ancestors, &iter))
           != NULL) {
        if (indiv == indiv_lived)
            continue;
        assert (indiv->genome);
        assert (indiv_lived->genome);
        sum_distance += self->distance_assessor (self->context,
                                                 indiv->genome,
                                                 indiv_lived->genome);
        cnt++;
    }

    // Average
    indiv->diversity = sum_distance;
    if (cnt > 0)
        indiv->diversity = indiv->diversity / cnt;
}


// Assess individual's diversity by its neighbors
static void evol_assess_diversity_locally (evol_t *self, s_indiv_t *indiv) {
    size_t num_neighbors = list4x_size (indiv->neighbors);
    if (num_neighbors == 0) {
        indiv->diversity = 0.0;
        return;
    }

    double total_distance = 0.0;
    s_neighbor_t *neighbor = NULL;
    list4x_iterator_t iter = list4x_iter_init (indiv->neighbors, true);
    while ((neighbor = (s_neighbor_t *) list4x_iter (indiv->neighbors, &iter))
            != NULL)
        total_distance += neighbor->distance;

    indiv->diversity = total_distance / num_neighbors;
}


// Compute individual's score
static void evol_assess_score (evol_t *self, s_indiv_t *indiv) {
    // Ensure that feasibility, fitness, and diversity are assessed
    // if (double_is_none (indiv->fitness))
    //     evol_assess_feasibility_and_fitness (self, indiv);

    // if (double_is_none (indiv->diversity))
    //     evol_assess_diversity (self, indiv);

    assert (!double_is_none (indiv->fitness));
    assert (!double_is_none (indiv->diversity));

    // print_info ("assess score.\n");

    double norm_fit = indiv->fitness;
    double norm_div = indiv->diversity;

    // Normalize fitness
    if (evol_num_livings (self) > 0) {
        double best_fit =
            ((s_indiv_t *) list4x_first (self->livings_rank_fit))->fitness;
        if (!double_equal (best_fit, 0))
            norm_fit = norm_fit / best_fit;
    }

    // Normalize diversity
    if (evol_num_livings (self) > 0) {
        double best_div =
            ((s_indiv_t *) list4x_first (self->livings_rank_div))->diversity;
        if (!double_equal (best_div, 0))
            norm_div = norm_div / best_div;
    }

    // Compute score
    indiv->score =
        self->weight_fit * norm_fit + (1 - self->weight_fit) * norm_div;
}


// Assess a newcomer who has no conection with the population (livings and
// ancestors group) yet.
// After this, the newcomer will have its attributes assessed, and neighbors
// defined. Its as_neighbors will remain empty.
// This function will not change the population at all.
static void evol_assess_newcomer (evol_t *self, s_indiv_t *newcomer) {
    if (double_is_none (newcomer->fitness))
        evol_assess_feasibility_and_fitness (self, newcomer);

    assert (list4x_size (newcomer->neighbors) == 0);
    assert (list4x_size (newcomer->as_neighbors) == 0);

    // Find and add neighbors from livings group
    s_indiv_t *indiv = NULL;
    list4x_iterator_t iter = list4x_iter_init (self->livings_rank_fit, true);
    while ((indiv = (s_indiv_t *) list4x_iter (self->livings_rank_fit, &iter))
           != NULL &&
           indiv != newcomer)
        evol_add_neighbor_to_indiv (self, newcomer, indiv, DOUBLE_NONE);

    // Find and add neighbors from ancestors group
    iter = list4x_iter_init (self->ancestors, true);
    while ((indiv = (s_indiv_t *) list4x_iter (self->ancestors, &iter))
           != NULL &&
           indiv != newcomer)
        evol_add_neighbor_to_indiv (self, newcomer, indiv, DOUBLE_NONE);

    evol_assess_diversity_locally (self, newcomer);
    evol_assess_score (self, newcomer);
}


// Update attributes of individuals in livings group after acceptation of
// newcomer. For efficiency, only update newcomer's neighbors.
static void evol_update_assessment_by_newcomer (evol_t *self,
                                                s_indiv_t *newcomer) {
    assert (list4x_size (newcomer->as_neighbors) == 0);

    // For each neighbor
    s_neighbor_t *neighbor = NULL;
    list4x_iterator_t iter = list4x_iter_init (newcomer->neighbors, true);
    while ((neighbor = (s_neighbor_t *)list4x_iter (newcomer->neighbors, &iter))
            != NULL) {
        s_indiv_t *indiv = neighbor->indiv;
        assert (indiv != newcomer);

        // Add newcomer to indiv's as_neighbors list
        s_indiv_add_as_neighbor (indiv, newcomer, iter.handle);

        // To see if newcomer could become indiv's neighbor
        void *handle = evol_add_neighbor_to_indiv (self, indiv, newcomer,
                                                   neighbor->distance);

        // If newcomer becomes indiv's neighbor
        if (handle) {
            // Add indiv to newcomer's as_neighbors list
            s_indiv_add_as_neighbor (newcomer, indiv, handle);
            // If indiv is in livings group, re-assess its diversity and score.
            // If indiv is ancestor, these are not necessary.
            if (indiv->handle_livings_fit) {
                evol_assess_diversity_locally (self, indiv);
                list4x_reorder (self->livings_rank_div,
                                indiv->handle_livings_div);
                evol_assess_score (self, indiv);
                list4x_reorder (self->livings_rank_score,
                                indiv->handle_livings_score);
            }
        }
    }
}


// Update attributes of livings taking acount of a forgotten individual.
// Update forgotten's as_neighbors' neighbors, diversity, and score.
// @todo the implementation may be improved later
static void evol_update_assessment_by_forgotten (evol_t *self,
                                                 s_indiv_t *forgotten) {
    print_debug ("");
    evol_print_indiv (self, forgotten);

    // Temporary list for collecting new neighbors
    list4x_t *new_nbs = list4x_new ();
    list4x_set_destructor (new_nbs, (free_func_t) s_neighbor_free);
    list4x_set_comparator (new_nbs, (compare_func_t) s_neighbor_compare);
    list4x_sort (new_nbs, true);

    s_asneighbor_t *as_nb = NULL;
    list4x_iterator_t iter_as_nb =
        list4x_iter_init (forgotten->as_neighbors, true);
    while ((as_nb = (s_asneighbor_t *)
                    list4x_iter (forgotten->as_neighbors, &iter_as_nb))
           != NULL) {

        // assert (list4x_size (new_nbs) == 0);
        // assert (list4x_is_sorted_ascending (new_nbs));

        // Remove forgotten from as_neighbor's neighbors
        s_indiv_t *indiv = as_nb->indiv;

        // assert (indiv != forgotten);
        print_debug ("");
        assert (indiv);
        print_info ("alive: %s\n", indiv->handle_livings_fit ? "true" : "false");
        // evol_print_indiv (self, indiv);
        assert (indiv->neighbors);
        assert (s_indiv_has_neighbor (indiv, forgotten));
        // void *item = list4x_item (indiv->neighbors, as_nb->handle_in_neighbors);
        // assert (item);

        list4x_remove (indiv->neighbors, as_nb->handle_in_neighbors);

        print_debug ("");

        size_t max_new_nbs =
            self->max_neighbors - list4x_size (indiv->neighbors);

        // Try to add new neighbor for indiv from its neighbors' neighbors
        s_neighbor_t *nb = NULL;
        list4x_iterator_t iter_nb = list4x_iter_init (indiv->neighbors, true);
        while ((nb = (s_neighbor_t *) list4x_iter (indiv->neighbors, &iter_nb))
               != NULL) {

            s_neighbor_t *nb_nb = NULL; // neighbor's neighbor
            list4x_iterator_t iter_nb_nb =
                list4x_iter_init (nb->indiv->neighbors, true);
            while ((nb_nb = (s_neighbor_t *)
                            list4x_iter (nb->indiv->neighbors, &iter_nb_nb))
                   != NULL) {

                if (nb_nb->indiv == forgotten ||
                    nb_nb->indiv == indiv ||
                    s_indiv_has_neighbor (indiv, nb_nb->indiv))
                    continue;

                // Check if nb_nb is already in new_nbs list
                bool in_new_nbs = false;
                s_neighbor_t *new_nb = NULL;
                list4x_iterator_t iter_new_nb = list4x_iter_init (new_nbs, true);
                while ((new_nb =
                        (s_neighbor_t *) list4x_iter (new_nbs, &iter_new_nb))
                       != NULL) {
                    if (new_nb == nb_nb) {
                        in_new_nbs = true;
                        break;
                    }
                }
                if (in_new_nbs)
                    continue;

                double dist = self->distance_assessor (self->context,
                                                       indiv->genome,
                                                       nb_nb->indiv->genome);
                list4x_insert_sorted (new_nbs,
                                      s_neighbor_new (nb_nb->indiv, dist));
                if (list4x_size (new_nbs) > max_new_nbs)
                    list4x_remove_last (new_nbs);
            }
        }

        // Add new neighbor to indiv's neighbors list, and add indiv to new
        // neighbor's as_neighbors list
        if (list4x_size (new_nbs) > 0) {
            s_neighbor_t *new_nb = NULL;
            list4x_iterator_t iter_new_nb = list4x_iter_init (new_nbs, true);
            while ((new_nb =
                    (s_neighbor_t *) list4x_iter (new_nbs, &iter_new_nb))
                   != NULL) {
                list4x_iter_pop (new_nbs, &iter_new_nb);
                void *handle = list4x_insert_sorted (indiv->neighbors, new_nb);
                list4x_append (new_nb->indiv->as_neighbors,
                               s_asneighbor_new (indiv, handle));
            }
        }
        // assert (list4x_size (new_nbs) == 0);
        // assert (list4x_is_sorted_ascending (new_nbs));
    }

    list4x_free (&new_nbs);
}


// Re-assess diversity and score for all livings
// Exact method: slow
// static void evol_update_diversity_and_score (evol_t *self) {
//     // For every living in livings group, re-assess diversity and score
//     s_indiv_t *living;
//     list4x_iterator_t iter = list4x_iter_init (self->livings_rank_fit, true);
//     while ((living = (s_indiv_t *) list4x_iter (self->livings_rank_fit, &iter))
//            != NULL) {
//         evol_assess_diversity (self, living);
//         list4x_reorder (self->livings_rank_div, living->handle_livings_div);

//         // Note: score computed here is not exactly right. Do not care now.
//         evol_assess_score (self, living);
//         list4x_reorder (self->livings_rank_score, living->handle_livings_score);
//     }
//     // print_info ("scores updated.\n");
// }


// Add an individual to children group
static void evol_add_child (evol_t *self, s_indiv_t *child) {
    // Add indiv to children group (append, not sorted)
    child->handle_children = list4x_append (self->children, child);

    // If group is full, randomly remove and destroy one
    if (list4x_size (self->children) > self->max_children) {
        print_warning ("Children group is full. Kill an unlucky one.\n");
        size_t index =
            rng_random_int (self->rng, 0, list4x_size (self->children));
        list4x_remove_at (self->children, index);
    }
}


// Add a new dead individual to ancestors group.
// Return the individual who is removed from ancestors group (forgotten);
// NULL if no one is removed.
static void evol_add_ancestor (evol_t *self, s_indiv_t *dead) {
    // Append the new dead individual (ancestors group is a FIFO queue)
    dead->handle_ancestors = list4x_append (self->ancestors, dead);

    // If ancestors group overflows, forget the oldest
    if (list4x_size (self->ancestors) > self->max_ancestors) {
        s_indiv_t *forgotten = (s_indiv_t *) list4x_first (self->ancestors);
        if (forgotten) {
            // Update assessment of population (approximate algorithm)
            evol_update_assessment_by_forgotten (self, forgotten);
            // Remove the forgotten
            list4x_remove_first (self->ancestors);
        }
    }
}


// Add an individual to livings group.
// Return the individual who is removed from livings group (dead);
// NULL if no one is removed.
// - If livings group is full, and the child's score is worse than the worst,
//   it is rejected and destroyed (no chance to go into ancestors group).
// - Otherwise it is accepted. If livings group overflows, the worst one is
//   killed and moved to ancestors group.
static void evol_add_living (evol_t *self, s_indiv_t *newcomer) {
    print_info ("add a living.\n");
    evol_assert_population (self);

    evol_assess_newcomer (self, newcomer);

    // If livings group is full, and new indiv's score is worse than all
    // livings, drop and destroy it. (Do not move it to ancestors group)
    if (evol_num_livings (self) == self->max_livings) {
        s_indiv_t *worst = (s_indiv_t *) list4x_last (self->livings_rank_score);
        if (worst && newcomer->score <= worst->score ) {
            s_indiv_free (&newcomer);
            // print_info ("individual is dropped because of its low score.\n");
            return;
        }
    }

    // Newcomer accepted. Add it to livings group
    newcomer->handle_livings_fit =
        list4x_insert_sorted (self->livings_rank_fit, newcomer);
    newcomer->handle_livings_div =
        list4x_insert_sorted (self->livings_rank_div, newcomer);
    newcomer->handle_livings_score =
        list4x_insert_sorted (self->livings_rank_score, newcomer);

    // Update assessment of livings (approximate algorithm)
    evol_update_assessment_by_newcomer (self, newcomer);

    print_debug ("");
    evol_assert_indiv (self, newcomer);
    evol_assert_population (self);

    // If livings group overflows, kill the one with the lowest score (while
    // keep the best alive), and move it to ancestors group.
    if (evol_num_livings (self) > self->max_livings) {
        s_indiv_t *dead = NULL; // indiv removed from livings group
        s_indiv_t *best = (s_indiv_t *) list4x_first (self->livings_rank_fit);
        list4x_iterator_t iter =
            list4x_iter_init (self->livings_rank_score, false);
        while ((dead =
                    (s_indiv_t *) list4x_iter (self->livings_rank_score, &iter))
               != NULL) {
            if (dead != best)
                break;
        }
        assert (dead);
        assert (dead != best);

        // Remove dead from livings group
        list4x_pop (self->livings_rank_score, dead->handle_livings_score);
        dead->handle_livings_score = NULL;
        list4x_pop (self->livings_rank_fit, dead->handle_livings_fit);
        dead->handle_livings_fit = NULL;
        list4x_pop (self->livings_rank_div, dead->handle_livings_div);
        dead->handle_livings_div = NULL;

        // Add the dead to ancestors group
        evol_add_ancestor (self, dead);

        print_debug ("");
        evol_assert_population (self);
    }

    print_debug ("");
    evol_assert_population (self);
}


// Deterministic tournament selection.
// See: https://en.wikipedia.org/wiki/Tournament_selection
// Dicing num_dicing times in range [min_value, max_value) and select the
// minimal or maximal one.
static size_t tournament_selection (size_t min_value,
                                    size_t max_value,
                                    size_t num_dicings,
                                    bool select_min,
                                    rng_t *rng) {
    assert (min_value < max_value);
    if (num_dicings == 0)
        return select_min ? min_value : max_value;

    int result = select_min ? max_value : min_value;
    int throw;

    for (size_t i = 0; i < num_dicings; i++) {
        throw = (size_t) rng_random_int (rng, min_value, max_value);
        if (select_min && throw < result)
            result = throw;
        else if (!select_min && throw > result)
            result = throw;
    }
    return result;
}

// Pick two parents from livings group for crossover.
// Return in params p1 and p2 (guaranteed different).
static void evol_pick_parents_for_crossover (evol_t *self) {
    size_t num_livings = evol_num_livings (self);
    if (num_livings < 2) {
        print_warning ("Number of livings is less than 2. "
                       "Crossover can not be executed.\n");
        self->parent1 = NULL;
        self->parent2 = NULL;
        return;
    }

    size_t index_p1 = tournament_selection (0, num_livings,
                                            self->num_dicings_parent1,
                                            true, self->rng);
    // print_debug ("index_p1: %zu\n", index_p1);
    size_t index_p2 = index_p1;
    while (index_p2 == index_p1)
        index_p2 = tournament_selection (0, num_livings,
                                         self->num_dicings_parent2,
                                         true, self->rng);
    // print_debug ("index_p2: %zu\n", index_p2);

    self->parent1 =
        (s_indiv_t *) list4x_item_at (self->livings_rank_score, index_p1);
    self->parent2 =
        (s_indiv_t *) list4x_item_at (self->livings_rank_score, index_p2);
}


// Crossover
// Use registered crossover functions to generate children
static void evol_crossover (evol_t *self) {
    if (list4x_size (self->crossovers) == 0) {
        print_warning ("No crossover registered.\n");
        return;
    }

    // s_indiv_t *parent1 = NULL, *parent2 = NULL;
    evol_crossover_t crossover;
    list4x_t *children_genomes = NULL;
    genome_t child_genome;
    s_indiv_t *child = NULL;

    // For every registered crossover operators
    list4x_iterator_t iter_cx = list4x_iter_init (self->crossovers, true);
    while ((crossover = list4x_iter (self->crossovers, &iter_cx)) != NULL) {
        // Pick two livings as parents
        evol_pick_parents_for_crossover (self);
        assert (self->parent1);
        assert (self->parent2);

        // Crossover to generate genomes of children
        children_genomes =
            crossover (self->context,
                       self->parent1->genome, self->parent2->genome);

        // print_info ("cx generated %zu children.\n",
        //             list4x_size (children_genomes));

        // For each generated genome, create child from genome and add to
        // children group
        list4x_iterator_t iter_g = list4x_iter_init (children_genomes, true);
        while ((child_genome = list4x_iter (children_genomes, &iter_g))
                != NULL) {
            child = evol_new_indiv (self, child_genome,
                                    self->genome_destructor);
            evol_add_child (self, child);
        }

        // print_info ("children added.\n");

        // If the genomes are duplicated during creating individual, let the
        // list destroy them, otherwise make sure that the genomes are not
        // destroyed.
        if (self->genome_duplicator) {
            assert (self->genome_destructor);
            list4x_set_destructor (children_genomes, self->genome_destructor);
        }
        else
            list4x_set_destructor (children_genomes, NULL);

        list4x_free (&children_genomes);
    }
}


// Pick an individual from livings group for mutation
static s_indiv_t *evol_pick_parent_for_mutation (evol_t *self,
                                                 size_t num_dicings) {
    size_t num_livings = evol_num_livings (self);
    if (num_livings == 0) {
        print_warning ("No livings. Mutation can not be executed.\n");
        return NULL;
    }

    size_t index_p =
        tournament_selection (0, num_livings, num_dicings, true, self->rng);
    return list4x_item_at (self->livings_rank_score, index_p);
}


// Mutation
// User registered mutation functions to generate children
static void evol_mutate (evol_t *self) {
    if (list4x_size (self->mutators) == 0) {
        // print_warning ("No mutator registered.\n");
        return;
    }

    // For every registered mutator
    evol_mutator_t mutator;
    list4x_iterator_t iter = list4x_iter_init (self->mutators, true);
    while ((mutator = list4x_iter (self->mutators, &iter)) != NULL) {

        // Pick a living as parent
        s_indiv_t *parent =
            evol_pick_parent_for_mutation (self, self->num_dicings_mutation);

        // Mutate
        genome_t genome = mutator (self->context, parent->genome);

        // Create child and add it to children group
        s_indiv_t *child =
            evol_new_indiv (self, genome, self->genome_destructor);

        // Mutation here is not in place (the parent is untouched).
        // However it is easy to implement a in place version by replacing the
        // parent with child, then reorder it in the livings group.
        evol_add_child (self, child);
    }
}


// Merge children group into livings group.
// The whole process is:
// for each child in children group in fitness descending order:
// - Remove it from children group (do not destroy it)
// - Throw the child into livings group. It may be accepted or rejected, which
// is processed in evol_add_living ()
static void evol_join (evol_t *self) {
    // print_info ("join\n");
    if (evol_num_children (self) == 0)
        return;

    // Make sure children group is sorted in fitness descending order
    if (!list4x_is_sorted_descending (self->children))
        list4x_sort (self->children, false);

    // For every child, remove it from children group,
    // and add it to livings group. After the loop, the children group should
    // be cleared.
    s_indiv_t *child;
    list4x_iterator_t iter = list4x_iter_init (self->children, true);
    while ((child = list4x_iter (self->children, &iter)) != NULL) {
        // Pop child from children group
        list4x_iter_pop (self->children, &iter);
        // Add child to livings group
        evol_add_living (self, child);
    }
    assert (evol_num_children (self) == 0);
}


// Does the evolution reach iteration or time limitations
static bool evol_should_stop_itself (evol_t *self) {
    return (self->iters_cnt >= self->max_iters) ||
           (timer_total (self->timer, 0) >= self->max_time);
}


// Does fitness improve too slow?
static bool evol_slowdown_happens (evol_t *self) {
    return self->improved_fit_percent < self->min_improved_fit;
}


// Should the evolution stop?
static bool evol_should_stop (evol_t *self) {
    if (self->stopper && self->stopper (self->context))
        return true;
    else
        return evol_should_stop_itself (self);
}


// Perform local improvers on every child
static void evol_children_growup (evol_t *self) {
    // print_info ("children growup.\n");
    // if (list4x_size (self->local_improvers) == 0)
    //     return;

    s_indiv_t *child;
    evol_local_improver_t local_improver;

    // For every child
    list4x_iterator_t iter_c = list4x_iter_init (self->children, true);
    while ((child = list4x_iter (self->children, &iter_c)) != NULL) {
        // implement every local improvers on child (in place)
        list4x_iterator_t iter_li = list4x_iter_init (self->local_improvers, true);
        while ((local_improver = list4x_iter (self->local_improvers, &iter_li))
                != NULL)
            local_improver (self->context, child->genome);
        // Assess child's feasibility and fitness
        evol_assess_feasibility_and_fitness (self, child);
    }

    // Sort children group in fitness descending order
    list4x_sort (self->children, false);
    // print_info ("children growuped.\n");
}


// Execute a number of non-disrupted iterations (at least iterations exceeds
// step_max_iters or the time exceeds step_max_time
static void evol_step (evol_t *self) {
    self->step_iters_cnt = 0;
    timer_restart (self->step_timer);
    while (self->step_iters_cnt < self->step_max_iters &&
           timer_total (self->step_timer, 0) < self->step_max_time) {
        evol_crossover (self);
        evol_mutate (self);
        evol_children_growup (self);
        evol_join (self);
        self->step_iters_cnt++;
    }
}


// Due to variations of the context, (all) individuals in the population should
// be updated (modified or transformed to new individuals, or abandoned)
// accordingly. This operation transforms current livings group to a new one,
// and the ancestors as well as the children group are cleared (because old
// ancestors are meaningless in the new context). This operation is also
// equivalent to initialize a new population from current one.
static void evol_renew_population (evol_t *self) {
    assert (self->renewer);

    list4x_t *old_livings_rank_fit = self->livings_rank_fit;
    list4x_t *old_livings_rank_div = self->livings_rank_div;
    list4x_t *old_livings_rank_score = self->livings_rank_score;

    // Create new population: new livings group, empty ancestors and children
    evol_new_livings_group (self);
    list4x_purge (self->ancestors);
    list4x_purge (self->children);

    // Use score list for iteration, and pop item during iteration
    s_indiv_t *living = NULL;
    list4x_iterator_t iter = list4x_iter_init (old_livings_rank_score, true);
    while ((living = (s_indiv_t *) list4x_iter (old_livings_rank_score, &iter))
           != NULL) {
        list4x_iter_pop (old_livings_rank_score, &iter);
        int result = self->renewer (self->context, living->genome);

        // If individual is keeped, reset it and add it to new livings group
        if (result == 0) {
            s_indiv_reset (living);
            evol_add_living (self, living);
        }
        // If individual is dropped, destroy it
        else if (result < 0)
            s_indiv_free (&living);
        else
            assert (false);
    }

    // Destroy old livings group
    list4x_free (&old_livings_rank_fit);
    list4x_free (&old_livings_rank_div);
    list4x_free (&old_livings_rank_score); // score list is already empty
}


// Restart recorders before running
static void evol_restart_recorders (evol_t *self) {
    timer_restart (self->timer);
    self->iters_cnt = 0;
    self->last_best_fit = evol_best_fitness (self);
    self->improved_fit_percent = DOUBLE_MAX;
    self->improved_fit_iters_begin = self->iters_cnt;
    self->improved_fit_time_begin = timer_total (self->timer, 0);
}


// Stop recorders
static void evol_stop_recorders (evol_t *self) {
    timer_stop (self->timer, 0);
}


// Update recorders after each step
static void evol_update_recorders (evol_t *self) {
    // iterations
    self->iters_cnt += self->step_iters_cnt;

    // evolving time
    double evol_time = timer_total (self->timer, 0);

    // update improved_fit_percent conditionally
    if ((self->iters_cnt - self->improved_fit_iters_begin) >=
                                                    self->unimproved_iters ||
        (evol_time - self->improved_fit_time_begin) >=
                                                    self->unimproved_period) {
        double current_best_fit = evol_best_fitness (self);
        self->improved_fit_percent =
          double_equal (self->last_best_fit, 0.0) ?
          DOUBLE_MAX :
          (current_best_fit - self->last_best_fit) / fabs (self->last_best_fit);

        print_info ("fitness improved: %.3f%% during %zu iters and %.2f s\n",
                    100 * self->improved_fit_percent,
                    self->iters_cnt - self->improved_fit_iters_begin,
                    evol_time - self->improved_fit_time_begin);

        // update references
        self->last_best_fit = current_best_fit;
        self->improved_fit_iters_begin = self->iters_cnt;
        self->improved_fit_time_begin = evol_time;
    }
}


static void evol_reset_stats (evol_t *self) {
    self->initial_best_fit = evol_best_fitness (self);
    self->overall_improved_fit = 0.0;
}


// @todo rewrite
static void evol_update_stats (evol_t *self) {
    self->overall_improved_fit =
        evol_best_fitness (self) / self->initial_best_fit;
}


static void evol_report_stats (evol_t *self) {
    printf ("Overall fitness improvement: %.2f%%\n",
        (evol_best_fitness (self) - self->initial_best_fit) /
        self->initial_best_fit * 100);
    printf ("Total iters: %zu, time: %.2f s.\n",
            self->iters_cnt,
            timer_total (self->timer, 0));

    printf ("Best genome: \n");
    evol_print_indiv (self,
                      (s_indiv_t *) list4x_first (self->livings_rank_fit));
}


// ---------------------------------------------------------------------------

// @todo need to check every item
evol_t *evol_new (void) {
    evol_t *self = malloc (sizeof (evol_t));
    assert (self);

    // ------------------ Population ------------------ //

    evol_new_livings_group (self);
    evol_new_ancestors_group (self);
    evol_new_children_group (self);

    // ------------------ Parameters ------------------ //

    // Individual context
    self->context = NULL;

    // Group size
    self->max_livings   = EVOL_DEFAULT_MAX_LIVINGS;
    self->max_ancestors = EVOL_DEFAULT_MAX_ANCESTORS;
    self->max_children  = EVOL_DEFAULT_MAX_CHILDREN;

    // Computational and performance limitations
    self->max_iters         = SIZE_MAX;
    self->max_time          = DOUBLE_MAX;
    self->unimproved_iters  = EVOL_DEFAULT_UNIMPROVED_ITERS;
    self->unimproved_period = EVOL_DEFAULT_UNIMPROVED_PERIOD;
    self->min_improved_fit  = EVOL_DEFAULT_MIN_IMPROVED_FITNESS;

    // Step computitional limitations
    self->step_max_iters = EVOL_DEFAULT_STEP_MAX_ITERS;
    self->step_max_time  = EVOL_DEFAULT_STEP_MAX_TIME;

    // Callbacks
    self->genome_destructor   = NULL;
    self->genome_duplicator   = NULL;
    self->genome_printer      = NULL;
    self->feasiblity_assessor = NULL;
    self->fitness_assessor    = NULL;
    self->distance_assessor   = NULL;
    self->should_renew        = NULL;
    self->renewer             = NULL;
    self->stopper             = NULL;

    self->heuristics      = list4x_new ();
    list4x_set_destructor (self->heuristics, (free_func_t) s_heuristic_free);
    self->crossovers      = list4x_new ();
    self->mutators        = list4x_new ();
    self->local_improvers = list4x_new ();

    self->num_dicings_parent1  = EVOL_DEFAULT_NUM_DICINGS_FOR_PARENT1;
    self->num_dicings_parent2  = EVOL_DEFAULT_NUM_DICINGS_FOR_PARENT2;
    self->num_dicings_mutation = EVOL_DEFAULT_NUM_DICINGS_FOR_MUTATION;
    self->max_neighbors        = EVOL_DEFAULT_MAX_NEIGHBORS;
    self->weight_fit           = EVOL_DEFAULT_WEIGHT_FITNESS;

    // ------------------ Recorders ------------------ //

    self->timer      = timer_new ("EVOL_RUN");
    self->step_timer = timer_new ("EVOL_STEP");

    // ------------------ Statistics ------------------ //

    self->overall_improved_fit = 0;

    // ------------------ Auxiliaries ------------------ //

    self->rng = rng_new ();

    print_info ("evol created.\n");
    return self;
}


// @todo need to check every item again
void evol_free (evol_t **self_p) {
    assert (self_p);
    if (*self_p) {
        evol_t *self = *self_p;

        list4x_free (&self->livings_rank_fit);
        list4x_free (&self->livings_rank_div);
        list4x_free (&self->livings_rank_score);
        list4x_free (&self->ancestors);
        list4x_free (&self->children);

        list4x_free (&self->heuristics);
        list4x_free (&self->crossovers);
        list4x_free (&self->mutators);
        list4x_free (&self->local_improvers);

        timer_free (&self->timer);
        timer_free (&self->step_timer);

        rng_free (&self->rng);

        free (self);
        *self_p = NULL;
    }
}


void evol_set_context (evol_t *self, void *context) {
    assert (self);
    self->context = context;
}


void evol_set_livings_group_size (evol_t *self, size_t max_livings) {
    assert (self);
    assert (max_livings > 0);
    self->max_livings = max_livings;
}


void evol_set_ancestors_group_size (evol_t *self, size_t max_ancestors) {
    assert (self);
    assert (max_ancestors > 0);
    self->max_ancestors = max_ancestors;
}


void evol_set_children_group_size (evol_t *self, size_t max_children) {
    assert (self);
    assert (max_children > 0);
    self->max_children = max_children;
}


void evol_set_genome_destructor (evol_t *self, free_func_t fn) {
    assert (self);
    assert (fn);
    self->genome_destructor = fn;
}


void evol_set_genome_duplicator (evol_t *self, duplicate_func_t fn) {
    assert (self);
    assert (fn);
    self->genome_duplicator = fn;
}


void evol_set_genome_printer (evol_t *self, print_func_t fn) {
    assert (self);
    assert (fn);
    self->genome_printer = fn;
}


void evol_set_feasibility_assessor (evol_t *self, evol_feasiblity_assessor_t fn) {
    assert (self);
    assert (fn);
    self->feasiblity_assessor = fn;
}


void evol_set_fitness_assessor (evol_t *self, evol_fitness_assessor_t fn) {
    assert (self);
    assert (fn);
    self->fitness_assessor = fn;
}


void evol_set_distance_assessor (evol_t *self, evol_distance_assessor_t fn) {
    assert (self);
    assert (fn);
    self->distance_assessor = fn;
}


void evol_set_renewer (evol_t *self,
                       evol_should_renew_t fn,
                       evol_renewer_t gn) {
    assert (self);
    assert (fn);
    assert (gn);
    self->should_renew = fn;
    self->renewer = gn;
}


void evol_set_stopper (evol_t *self, evol_stopper_t fn) {
    assert (self);
    assert (fn);
    self->stopper = fn;
}


void evol_register_heuristic (evol_t *self,
                              evol_heuristic_t fn,
                              bool is_random,
                              size_t max_expected) {
    assert (self);
    assert (fn);
    s_heuristic_t *h = s_heuristic_new (fn, is_random, max_expected);
    list4x_append (self->heuristics, h);
}


void evol_register_crossover (evol_t *self, evol_crossover_t fn) {
    assert (self);
    assert (fn);
    list4x_append (self->crossovers, fn);
}


void evol_register_mutator (evol_t *self, evol_mutator_t fn) {
    assert (self);
    assert (fn);
    list4x_append (self->mutators, fn);
}


void evol_register_local_improver (evol_t *self, evol_local_improver_t fn) {
    assert (self);
    assert (fn);
    list4x_append (self->local_improvers, fn);
}


void evol_init (evol_t *self) {
    assert (self);

    size_t num_livings = evol_num_livings (self);
    assert (num_livings == 0);

    size_t num_h = list4x_size (self->heuristics);
    if (num_h == 0) {
        print_warning ("No heuristics registered. "
                       "Population not initialized.\n");
        return;
    }

    print_info ("evol initilizing...\n");

    size_t max_expected;
    size_t random_h_cnt = 0;
    s_heuristic_t *h = NULL;
    size_t num_expected;
    list4x_t *h_genomes = NULL;
    genome_t genome = NULL;
    s_indiv_t *indiv = NULL;

    // Step 1: non-random heuristics
    print_debug ("non-random heuristics\n");

    max_expected = self->max_livings / num_h;
    if (max_expected == 0) {
        max_expected = 1;
        print_warning (
            "Livings group is smaller than number of nonrandom heuristics.\n");
    }

    list4x_iterator_t iter_h = list4x_iter_init (self->heuristics, true);
    while ((h = (s_heuristic_t *) list4x_iter (self->heuristics, &iter_h)) != NULL) {

        if (h->is_random) {
            random_h_cnt++;
            continue;
        }

        num_expected = max_expected < h->max_expected ?
                       max_expected :
                       h->max_expected;
        if (num_expected == 0)
            continue;

        h_genomes = h->fn (self->context, num_expected);
        list4x_iterator_t iter_g = list4x_iter_init (h_genomes, true);
        while ((genome = list4x_iter (h_genomes, &iter_g)) != NULL) {
            indiv = evol_new_indiv (self, genome, self->genome_destructor);
            evol_add_living (self, indiv);
        }

        if (self->genome_duplicator)
            list4x_set_destructor (h_genomes, self->genome_destructor);
        else
            list4x_set_destructor (h_genomes, NULL);
        list4x_free (&h_genomes);
    }

    // Step 2: random heuristics
    print_debug ("random heuristics. num: %zu\n", random_h_cnt);

    if (random_h_cnt == 0) // no random heuristics
        return;

    num_livings = evol_num_livings (self);
    max_expected = (self->max_livings - num_livings) / random_h_cnt;
    if (max_expected == 0) {
        max_expected = 1;
        print_warning ("Livings group idle size is smaller than "
                       "number of random heuristics.\n");
    }

    iter_h = list4x_iter_init (self->heuristics, true);
    while ((h = (s_heuristic_t *) list4x_iter (self->heuristics, &iter_h)) != NULL) {

        if (!h->is_random)
            continue;

        num_expected = max_expected < h->max_expected ?
                       max_expected :
                       h->max_expected;
        if (num_expected <= 0)
            continue;

        h_genomes = h->fn (self->context, num_expected);
        print_debug ("num_expected: %zu, generated by heuristic: %zu\n",
                    num_expected, list4x_size (h_genomes));
        size_t cnt = 0;

        list4x_iterator_t iter_g = list4x_iter_init (h_genomes, true);
        while ((genome = list4x_iter (h_genomes, &iter_g)) != NULL) {
            cnt++;
            indiv = evol_new_indiv (self, genome, self->genome_destructor);
            evol_add_living (self, indiv);
        }

        print_debug ("");
        if (self->genome_duplicator)
            list4x_set_destructor (h_genomes, self->genome_destructor);
        else
            list4x_set_destructor (h_genomes, NULL);
        list4x_free (&h_genomes);
    }

    print_debug ("");
    assert (list4x_size (self->ancestors) == 0);
    assert (list4x_size (self->children) == 0);
    evol_assert_population (self);
}


void evol_run (evol_t *self) {
    assert (self);

    evol_reset_stats (self);
    evol_restart_recorders (self);

    while (!evol_should_stop (self)) {

        if (self->should_renew && self->should_renew (self->context)) {
            print_info ("Renew population.\n");
            evol_renew_population (self);
            evol_reset_stats (self);
            evol_restart_recorders (self);
        }
        else if (evol_slowdown_happens (self)) {
            print_info ("Improvement slows down.\n");
            // @todo when slowdown happens, diversity rather than simply quit
            // evol_diversity_population (self);
            break;
        }

        evol_step (self);

        evol_update_recorders (self);
    }

    evol_stop_recorders (self);
    evol_update_stats (self);
    evol_report_stats (self);
}


// @todo finish
genome_t evol_best_genome (evol_t *self) {
    assert (self);

    if (evol_num_livings (self) == 0)
        return NULL;

    s_indiv_t *indiv_best_fitness = list4x_first (self->livings_rank_fit);
    return indiv_best_fitness->genome;
}


// ---------------------------------------------------------------------------
// Self test using string

typedef struct {
    rng_t *rng;
} str_evol_context_t;


static double string_fitness (const void *context, char *str) {
    // size_t len = strlen (str);
    // return len == 0 ? (double) len : (double)(len / (int)(str[0]));
    return strlen (str);
}


static double string_distance (const void *context, char *str1, char *str2) {
    return (double) string_levenshtein_distance (str1, str2);
}


static list4x_t *string_crossover (const str_evol_context_t *context,
                                   const char *str1,
                                   const char *str2) {
    return string_cut_and_splice (str1, str2, context->rng);
}


static list4x_t *string_heuristic (const str_evol_context_t *context,
                                   size_t max_expected) {
    list4x_t *list = list4x_new ();
    for (size_t cnt = 0; cnt < max_expected; cnt++)
        list4x_append (list, string_random_alphanum (0, 16, context->rng));
    return list;
}


static bool string_should_renew (const str_evol_context_t *context) {
    return rng_random (context->rng) > 0.9;
}


static int string_renewer (const str_evol_context_t *context, char *str) {
    size_t len = strlen (str);
    if (len < 6)
        return -1;
    else if (rng_random (context->rng) > 0.9)
        str[0] = 'A';
    return 0;
}


void evol_test (bool verbose) {
    print_info (" * evol: \n");
    evol_t *evol = evol_new ();

    // context
    rng_t *rng = rng_new ();
    str_evol_context_t context = {rng};

    evol_set_context (evol, &context);
    evol_set_genome_destructor (evol, (free_func_t) string_free);
    evol_set_genome_printer (evol, (print_func_t) string_print);
    evol_set_fitness_assessor (evol, (evol_fitness_assessor_t) string_fitness);
    evol_set_distance_assessor (evol, (evol_distance_assessor_t) string_distance);
    evol_register_heuristic (evol,
                             (evol_heuristic_t) string_heuristic,
                             true,
                             SIZE_MAX);
    evol_register_crossover (evol, (evol_crossover_t) string_crossover);
    // evol_set_renewer (evol,
    //                   (evol_should_renew_t) string_should_renew,
    //                   (evol_renewer_t) string_renewer);

    evol_init (evol);

    evol_run (evol);

    evol_free (&evol);
    rng_free (&rng);
    print_info ("OK\n");
}
