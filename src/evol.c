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

- update_by_forgotten() 补充新neighbor可以优化
- score computation: best fit and div may change, should update?
- add diversification
    - 当slow down条件满足后report结果，可以选择停止，或者进行diversify后继续。
    - a method: adjust weight_fit
- 消息机制（放在evol还是solver?）

- [x]init() and renew()需要在最后保证livings group已经sort
- [x]renew之后livings size可能小于2，cx无法进行，用heuristics fill
- [x]sort livings when enough livings exist rather than at the very beginning.
- [x]rewrite evol_renew_population ()
- [x]diversity的计算及更新
    - 精确算法
    - 近似算法
- [x]把individual放在arrayset里似乎没有必要。

*/

#include "classes.h"

#define EVOL_DEFAULT_MAX_LIVINGS   100
#define EVOL_DEFAULT_MAX_ANCESTORS 20
#define EVOL_DEFAULT_MAX_CHILDREN  30
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


// Individual structure
typedef struct {
    // Core representation
    genome_t genome;

    free_func_t genome_destructor;
    print_func_t genome_printer;

    // Evaluations
    bool   feasible;
    double fitness;
    double diversity;
    double score;

    // Handles in livings group
    void *handle_livings_fit;
    void *handle_livings_div;
    void *handle_livings_score;

    // Handle in ancestors group and children group
    // @todo these two handles are redundant, could be removed after test
    void *handle_ancestors;
    void *handle_children;

    // Records of neighbors. list of s_neighbor_t sorted by distance.
    // max size: evol_t.max_neighbors
    list4x_t *neighbors;

    // Records of as other individuals' neighbors. list of s_asneighbor_t.
    list4x_t *as_neighbors;
} s_indiv_t;


// Record of neighbor
typedef struct {
    s_indiv_t *indiv; // neighbor individual
    void *handle_in_as_neighbors; // handle of item in indiv->as_neighbors
    double distance;
} s_neighbor_t;


// Record of as other's neighbor
typedef struct {
    s_indiv_t *indiv; // individual who view this as neighbor
    void *handle_in_neighbors; // handle of item in indiv->neighbors
} s_asneighbor_t;


// Heuristic structure
typedef struct {
    evol_heuristic_t fn;
    bool is_random;
    size_t max_expected;
} s_heuristic_t;


// Evolution structure
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

    // Group maximal size
    size_t max_livings;
    size_t max_ancestors;
    size_t max_children;

    // Computational limitaitons for every run
    size_t max_iters;
    double max_time; // second

    // Improvement limitations
    // In unimproved_iters or unimproved_period, fitness should at least be
    // improved min_improved_fit, otherwise slowdown happens.
    size_t unimproved_iters;
    double unimproved_period;
    double min_improved_fit; // percentage

    // Computational limitaitions for every step
    // (one step is a number of non-disrupted iterations)
    size_t step_max_iters;
    double step_max_time; // second

    // Callbacks
    free_func_t                genome_destructor;
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

    // Parent picking dicings for crossover and mutation
    size_t num_dicings_parent1;
    size_t num_dicings_parent2;
    size_t num_dicings_mutation;

    // Maximal number of closest individuals (defined as neighbors) for
    // individual's diversity assessment.
    size_t max_neighbors;

    // Weight of fitness for score computation.
    // Weight of diversity then is (1 - weight_fit)
    double weight_fit;

    // ------------------ Recorders ------------------- //

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

    // --------------- Stats (for run) ----------------- //

    double initial_best_fit;
    double overall_improved_fit; // percentage

    // ------------------ Auxiliaries ------------------ //

    rng_t *rng;
    s_indiv_t *parent1, *parent2;
};


// Create a new neighbor
static s_neighbor_t *s_neighbor_new (s_indiv_t *indiv, double distance) {
    s_neighbor_t *self = (s_neighbor_t *) malloc (sizeof (s_neighbor_t));
    assert (self);
    self->indiv = indiv;
    self->handle_in_as_neighbors = NULL;
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


// Create a new as_neighbor
static s_asneighbor_t *s_asneighbor_new (s_indiv_t *indiv, void *handle) {
    s_asneighbor_t *self = (s_asneighbor_t *) malloc (sizeof (s_asneighbor_t));
    assert (self);
    self->indiv = indiv;
    self->handle_in_neighbors = handle;
    return self;
}


// // Destroy a as_neighbor
static void s_asneighbor_free (s_asneighbor_t **self_p) {
    assert (self_p);
    if (*self_p) {
        free (*self_p);
        *self_p = NULL;
    }
}


// Create a new individual (use evol_new_indiv() rather than this one to create
// an individual for evolution)
static s_indiv_t *s_indiv_new (genome_t genome,
                               free_func_t genome_destructor) {
    s_indiv_t *self = (s_indiv_t *) malloc (sizeof (s_indiv_t));
    assert (self);

    self->genome = genome;
    if (genome_destructor)
        self->genome_destructor = genome_destructor;
    else
        self->genome_destructor = NULL;

    self->feasible  = false;
    self->fitness   = DOUBLE_NONE;
    self->diversity = DOUBLE_NONE;
    self->score     = DOUBLE_NONE;

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


// Reset individual to the state after creation
static void s_indiv_reset (s_indiv_t *self) {
    self->feasible  = false;
    self->fitness   = DOUBLE_NONE;
    self->diversity = DOUBLE_NONE;
    self->score     = DOUBLE_NONE;

    self->handle_livings_fit   = NULL;
    self->handle_livings_div   = NULL;
    self->handle_livings_score = NULL;
    self->handle_ancestors     = NULL;
    self->handle_children      = NULL;

    list4x_purge (self->neighbors);
    list4x_purge (self->as_neighbors);
}


// Individual fitness comparator
static int s_indiv_compare_fitness (const s_indiv_t *indiv1,
                                    const s_indiv_t *indiv2) {
    return (indiv1->fitness < indiv2->fitness) ?
           -1 :
           ((indiv1->fitness > indiv2->fitness) ? 1 : 0);
}


// Individual diversity comparator
static int s_indiv_compare_diversity (const s_indiv_t *indiv1,
                                      const s_indiv_t *indiv2) {
    return (indiv1->diversity < indiv2->diversity) ?
           -1 :
           ((indiv1->diversity > indiv2->diversity) ? 1 : 0);
}


// Individual score comparator
static int s_indiv_compare_score (const s_indiv_t *indiv1,
                                  const s_indiv_t *indiv2) {
    return (indiv1->score < indiv2->score) ?
           -1 :
           ((indiv1->score > indiv2->score) ? 1 : 0);
}


// Check if someone is in neighbors list
static bool s_indiv_has_neighbor (s_indiv_t *self, s_indiv_t *someone) {
    if (list4x_size (self->neighbors) == 0)
        return false;
    s_neighbor_t *nb;
    list4x_iterator_t iter = list4x_iter_init (self->neighbors, true);
    while ((nb = (s_neighbor_t *) list4x_iter (self->neighbors, &iter))
           != NULL) {
        if (nb->indiv == someone)
            return true;
    }
    return false;
}


// Check if someone is in as_neighbors list
static bool s_indiv_has_as_neighbor (s_indiv_t *self, s_indiv_t *someone) {
    if (list4x_size (self->as_neighbors) == 0)
        return false;
    s_asneighbor_t *as_nb = NULL;
    list4x_iterator_t iter = list4x_iter_init (self->as_neighbors, true);
    while ((as_nb = (s_asneighbor_t *) list4x_iter (self->as_neighbors, &iter))
           != NULL) {
        if (as_nb->indiv == someone)
            return true;
    }
    return false;
}


// Add someone to indiv's neighbors list. But do not add as_neighbor record in
// someone (pairing not completed).
// Return handle in neighors list if added, NULL if failed.
static void *s_indiv_add_neighbor_unilaterally (s_indiv_t *self,
                                                s_indiv_t *someone,
                                                double distance,
                                                size_t max_neighbors) {
    // @todo remove after fully test
    assert (!s_indiv_has_neighbor (self, someone));

    if (max_neighbors == 0)
        return NULL;

    s_neighbor_t *farmost = NULL;
    bool overflow = false;
    if (list4x_size (self->neighbors) == max_neighbors) {
        farmost = (s_neighbor_t *) list4x_last (self->neighbors);
        if (farmost->distance <= distance)
            return NULL;
        overflow = true;
    }

    void *handle = list4x_insert_sorted (self->neighbors,
                                         s_neighbor_new (someone, distance));
    if (overflow) {
        // The farmost must not be someone
        if (farmost->handle_in_as_neighbors)
            list4x_remove (farmost->indiv->as_neighbors,
                           farmost->handle_in_as_neighbors);
        list4x_remove_last (self->neighbors);
    }
    return handle;
}


// Add as_neighbor record to indiv if it has become someone's neighbor, then
// add back item handle in someone's neighbor item to finish pairing
static void s_indiv_add_as_neighbor (s_indiv_t *self,
                                     s_indiv_t *someone,
                                     void *handle_in_neighbors) {
    // @todo remove the following assertions after fully test
    assert (self ==
        ((s_neighbor_t *) list4x_item (someone->neighbors, handle_in_neighbors))
            ->indiv);
    assert (!s_indiv_has_as_neighbor (self, someone));

    void *handle_in_as_neighbors =
        list4x_append (self->as_neighbors,
                       s_asneighbor_new (someone, handle_in_neighbors));
    s_neighbor_t *neighbor =
        (s_neighbor_t *) list4x_item (someone->neighbors, handle_in_neighbors);
    assert (neighbor->handle_in_as_neighbors == NULL);
    neighbor->handle_in_as_neighbors = handle_in_as_neighbors;
}


// Try to add someone as indiv's neighbor (pairing is done if added).
// Return 0 if added, -1 if not.
static int s_indiv_add_neighbor (s_indiv_t *self,
                                 s_indiv_t *someone,
                                 double distance,
                                 size_t max_neighbors) {

    void *handle = s_indiv_add_neighbor_unilaterally (self,
                                                      someone,
                                                      distance,
                                                      max_neighbors);
    if (handle)
        s_indiv_add_as_neighbor (someone, self, handle);
    return handle ? 0 : -1;
}


static bool s_indiv_is_living (s_indiv_t *self) {
    return self->handle_livings_fit != NULL;
}


// Check if individual is out of any group
static bool s_indiv_is_out_of_groups (s_indiv_t *self) {
    return (self->handle_livings_fit   == NULL &&
            self->handle_livings_div   == NULL &&
            self->handle_livings_score == NULL &&
            self->handle_ancestors     == NULL &&
            self->handle_children      == NULL);
}


// Assert neighbor pairing. (neighbor_indiv is self's neighbor)
static void s_indiv_assert_neighbor_pairing (s_indiv_t *self,
                                             s_indiv_t *neighbor_indiv) {
    bool found = false;
    s_neighbor_t *nb;
    list4x_iterator_t iter_nb = list4x_iter_init (self->neighbors, true);
    while ((nb = (s_neighbor_t *) list4x_iter (self->neighbors, &iter_nb))
           != NULL) {
        if (nb->indiv == neighbor_indiv) {
            found = true;
            assert (nb->handle_in_as_neighbors);
            s_asneighbor_t *as_nb =
                (s_asneighbor_t *) list4x_item (neighbor_indiv->as_neighbors,
                                                nb->handle_in_as_neighbors);
            assert (as_nb->indiv == self);
            s_neighbor_t *as_nb_nb =
                (s_neighbor_t *) list4x_item (self->neighbors,
                                              as_nb->handle_in_neighbors);
            assert (as_nb_nb->indiv == neighbor_indiv);
        }
    }
    assert (found);
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
        free (*self_p);
        *self_p = NULL;
    }
}


// ---------------------------------------------------------------------------

// Create new livings group
static void evol_new_livings_group (evol_t *self) {
    // List sorted by fitness descending order. This list is always sorted.
    self->livings_rank_fit = list4x_new ();
    list4x_set_comparator (self->livings_rank_fit,
                           (compare_func_t) s_indiv_compare_fitness);
    list4x_sort (self->livings_rank_fit, false);

    // List sorted by diversity descending order.
    // This list is sorted conditionally by evol_regularize_population().
    self->livings_rank_div = list4x_new ();
    list4x_set_comparator (self->livings_rank_div,
                           (compare_func_t) s_indiv_compare_diversity);

    // List sorted by score descending order.
    // This list is sorted conditionally by evol_regularize_population().
    // Let this list be responsible for destroying items (individuals).
    self->livings_rank_score = list4x_new ();
    list4x_set_destructor (self->livings_rank_score,
                           (free_func_t) s_indiv_free);
    list4x_set_comparator (self->livings_rank_score,
                           (compare_func_t) s_indiv_compare_score);
}


// Create new ancestors group
static void evol_new_ancestors_group (evol_t *self) {
    self->ancestors = list4x_new ();
    list4x_set_destructor (self->ancestors, (free_func_t) s_indiv_free);
}


// Create new children group
static void evol_new_children_group (evol_t *self) {
    self->children = list4x_new ();
    list4x_set_destructor (self->children, (free_func_t) s_indiv_free);
    list4x_set_comparator (self->children,
                           (compare_func_t) s_indiv_compare_fitness);
}


// Get number of individuals in livings group
static size_t evol_num_livings (evol_t *self) {
    return list4x_size (self->livings_rank_fit);
}


// Get number of individuals in children group
static size_t evol_num_children (evol_t *self) {
    return list4x_size (self->children);
}


// Print an individual
static void evol_print_indiv (evol_t *self, s_indiv_t *indiv) {
    printf ("\nIndividual:\n");
    if (self->genome_printer)
        self->genome_printer (indiv->genome);
    printf ("feasible: %s, fitness: %8.3f, diversity: %8.3f, score: %8.3f\n",
            indiv->feasible ? "true" : "false",
            indiv->fitness, indiv->diversity, indiv->score);
    printf ("role: %s, #neighbors: %zu, #as_neighbors: %zu\n",
            indiv->handle_livings_fit ? "living" :
            (indiv->handle_ancestors ? "ancestor" : "child"),
            list4x_size (indiv->neighbors),
            list4x_size (indiv->as_neighbors));
}


// Print a group
static void evol_print_group (evol_t *self, const char *group_name) {
    list4x_t *group = NULL;
    if (streq (group_name, "livings_rank_fit"))
        group = self->livings_rank_fit;
    else if (streq (group_name, "livings_rank_div"))
        group = self->livings_rank_div;
    else if (streq (group_name, "livings_rank_score"))
        group = self->livings_rank_score;
    else if (streq (group_name, "ancestors"))
        group = self->ancestors;
    else if (streq (group_name, "children"))
        group = self->children;
    else
        assert (false);

    printf ("\ngroup %s, size: %zu, sorted: %s\n",
        group_name,
        list4x_size (group),
        list4x_is_sorted (group) ?
        (list4x_is_sorted_ascending (group) ? "ascending" : "descending") :
        "no");
    printf ("-------------------------------------------------------\n");
    s_indiv_t *indiv = NULL;
    list4x_iterator_t iter = list4x_iter_init (group, true);
    while ((indiv = (s_indiv_t *) list4x_iter (group, &iter)) != NULL)
        evol_print_indiv (self, indiv);
    printf ("-------------------------------------------------------\n");
}


// Check if population is regularized, which means neighborhood relations are
// established, and livings group are sorted by diversity and score.
static bool evol_population_is_regularized (evol_t *self) {
    return (list4x_is_sorted (self->livings_rank_div) &&
            list4x_is_sorted (self->livings_rank_score));
}


// Check correctness of an individual
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
        if (evol_population_is_regularized (self)) {
            assert (!double_is_none (indiv->diversity));
            assert (!double_is_none (indiv->score));
        }
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
        assert (list4x_size (indiv->neighbors) == 0);
        assert (list4x_size (indiv->as_neighbors) == 0);
    }

    assert (indiv->neighbors);
    assert (indiv->as_neighbors);
    list4x_assert_sort (indiv->neighbors, "ascending");

    if (indiv->handle_livings_fit || indiv->handle_ancestors) {

        s_neighbor_t *nb = NULL;
        list4x_iterator_t iter = list4x_iter_init (indiv->neighbors, true);
        while ((nb = (s_neighbor_t *) list4x_iter (indiv->neighbors, &iter))
               != NULL) {
            assert (nb->indiv->handle_livings_fit || nb->indiv->handle_ancestors);
            assert (nb->indiv->handle_children == NULL);
            s_asneighbor_t *nb_as_nb =
                (s_asneighbor_t *)(list4x_item (nb->indiv->as_neighbors,
                                                nb->handle_in_as_neighbors));
            assert (nb_as_nb->indiv == indiv);
        }

        s_asneighbor_t *as_nb = NULL;
        iter = list4x_iter_init (indiv->as_neighbors, true);
        while ((as_nb =
                    (s_asneighbor_t *) list4x_iter (indiv->as_neighbors, &iter))
               != NULL) {
            assert (as_nb->indiv->handle_livings_fit ||
                    as_nb->indiv->handle_ancestors);
            assert (as_nb->indiv->handle_children == NULL);
            s_neighbor_t *as_nb_nb =
                (s_neighbor_t *) list4x_item (as_nb->indiv->neighbors,
                                              as_nb->handle_in_neighbors);
            assert (as_nb_nb->indiv == indiv);
        }
    }
}


// Check correctness of population
static void evol_assert_population (evol_t *self) {
    assert (list4x_size (self->livings_rank_fit) ==
            list4x_size (self->livings_rank_div));
    assert (list4x_size (self->livings_rank_fit) ==
            list4x_size (self->livings_rank_score));
    assert (list4x_size (self->livings_rank_fit) <= self->max_livings + 1);
    assert (list4x_size (self->ancestors) <= self->max_ancestors + 1);
    assert (list4x_size (self->children) <= self->max_children + 1);

    list4x_assert_sort (self->livings_rank_fit, "descending");
    if (evol_population_is_regularized (self)) {
        list4x_assert_sort (self->livings_rank_div, "descending");
        list4x_assert_sort (self->livings_rank_score, "descending");
    }
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


// Get best fitness value of all current livings
static double evol_best_fitness (evol_t *self) {
    if (evol_num_livings (self) == 0)
        return DOUBLE_NONE;
    return ((s_indiv_t *) list4x_first (self->livings_rank_fit))->fitness;
}


// Get best diversity value of all current livings
static double evol_best_diversity (evol_t *self) {
    assert (list4x_is_sorted_descending (self->livings_rank_div));
    if (evol_num_livings (self) == 0)
        return DOUBLE_NONE;
    return ((s_indiv_t *) list4x_first (self->livings_rank_div))->diversity;
}


// Compute individual's feasibility and fitness
static void evol_assess_feasibility_and_fitness (evol_t *self,
                                                 s_indiv_t *indiv) {
    indiv->feasible = self->feasiblity_assessor ?
        self->feasiblity_assessor (self->context, indiv->genome) :
        true;

    indiv->fitness = self->fitness_assessor (self->context, indiv->genome);
}


// Assess individual's diversity by its neighbors
static void evol_assess_diversity_locally (evol_t *self, s_indiv_t *indiv) {
    size_t num_neighbors = list4x_size (indiv->neighbors);
    if (num_neighbors == 0) {
        indiv->diversity = 0.0;
        return;
    }

    indiv->diversity = 0.0;
    s_neighbor_t *neighbor = NULL;
    list4x_iterator_t iter = list4x_iter_init (indiv->neighbors, true);
    while ((neighbor = (s_neighbor_t *) list4x_iter (indiv->neighbors, &iter))
            != NULL)
        indiv->diversity += neighbor->distance;

    indiv->diversity = indiv->diversity / num_neighbors;
}


// Compute individual's score
static void evol_assess_score (evol_t *self, s_indiv_t *indiv) {
    // Ensure that fitness, and diversity are already assessed
    assert (!double_is_none (indiv->fitness));
    assert (!double_is_none (indiv->diversity));

    double norm_fit = indiv->fitness;
    double norm_div = indiv->diversity;

    // Normalize fitness and diversity if possible
    if (evol_num_livings (self) > 0) {
        double best_fit = evol_best_fitness (self);
        if (!double_is_none (best_fit) && !double_equal (best_fit, 0))
            norm_fit = norm_fit / fabs (best_fit);
        double best_div = evol_best_diversity (self);
        if (!double_is_none (best_div) && !double_equal (best_div, 0))
            norm_div = norm_div / fabs (best_div);
    }

    // Compute score
    indiv->score =
        self->weight_fit * norm_fit + (1 - self->weight_fit) * norm_div;
}


// Re-assess a living's diversity and score, and reorder it in group if needed.
static void evol_update_diversity_and_score_for_living (evol_t *self,
                                                        s_indiv_t *living) {
    assert (living->handle_livings_fit);
    assert (evol_population_is_regularized (self));

    evol_assess_diversity_locally (self, living);
    list4x_reorder (self->livings_rank_div, living->handle_livings_div);
    evol_assess_score (self, living);
    list4x_reorder (self->livings_rank_score, living->handle_livings_score);
}


// Regularize livings group (assume that ancestors group is empty).
// Establish neighborhood relation, assess diversity and score for livings,
// and sort livings according to diversity and score.
static void evol_regularize_population (evol_t *self) {
    assert (!evol_population_is_regularized (self));
    assert (list4x_size (self->ancestors) == 0);

    s_indiv_t *indiv1;
    list4x_iterator_t iter1 = list4x_iter_init (self->livings_rank_fit, true);
    while ((indiv1 = (s_indiv_t *) list4x_iter (self->livings_rank_fit, &iter1))
           != NULL) {

        s_indiv_t *indiv2;
        list4x_iterator_t iter2 =
            list4x_iter_init_from (self->livings_rank_fit, iter1.handle, true);
        while ((indiv2 =
                    (s_indiv_t *) list4x_iter (self->livings_rank_fit, &iter2))
               != NULL) {
            assert (indiv1 != indiv2);
            double dist = self->distance_assessor (self->context,
                                                   indiv1->genome,
                                                   indiv2->genome);
            s_indiv_add_neighbor (indiv1, indiv2, dist, self->max_neighbors);
            s_indiv_add_neighbor (indiv2, indiv1, dist, self->max_neighbors);
        }

        assert (list4x_size (indiv1->neighbors) == self->max_neighbors);
        evol_assess_diversity_locally (self, indiv1);
    }

    list4x_sort (self->livings_rank_div, false);

    iter1 = list4x_iter_init (self->livings_rank_fit, true);
    while ((indiv1 = (s_indiv_t *) list4x_iter (self->livings_rank_fit, &iter1))
           != NULL)
        evol_assess_score (self, indiv1);

    list4x_sort (self->livings_rank_score, false);

    // print_debug ("");
    // evol_assert_population (self);
    // print_debug ("");
}


// Assess a newcomer who has no conection with the population (livings and
// ancestors group) yet.
// After this, the newcomer will have its attributes assessed, and neighbors
// defined. Its as_neighbors will remain empty.
// This function will not change the population at all.
static void evol_assess_newcomer (evol_t *self, s_indiv_t *newcomer) {
    assert (s_indiv_is_out_of_groups (newcomer));
    assert (list4x_size (newcomer->neighbors) == 0);
    assert (list4x_size (newcomer->as_neighbors) == 0);

    if (double_is_none (newcomer->fitness))
        evol_assess_feasibility_and_fitness (self, newcomer);

    // Find and add neighbors from livings group
    s_indiv_t *indiv;
    list4x_iterator_t iter = list4x_iter_init (self->livings_rank_fit, true);
    while ((indiv = (s_indiv_t *) list4x_iter (self->livings_rank_fit, &iter))
           != NULL) {
        double distance = self->distance_assessor (self->context,
                                                   newcomer->genome,
                                                   indiv->genome);
        s_indiv_add_neighbor_unilaterally (newcomer,
                                           indiv,
                                           distance,
                                           self->max_neighbors);
    }

    // Find and add neighbors from ancestors group
    iter = list4x_iter_init (self->ancestors, true);
    while ((indiv = (s_indiv_t *) list4x_iter (self->ancestors, &iter))
           != NULL) {
        double distance = self->distance_assessor (self->context,
                                                   newcomer->genome,
                                                   indiv->genome);
        s_indiv_add_neighbor_unilaterally (newcomer,
                                           indiv,
                                           distance,
                                           self->max_neighbors);
    }

    evol_assess_diversity_locally (self, newcomer);
    evol_assess_score (self, newcomer);
}


// Update attributes of livings and ancestors after acceptation of newcomer.
// For efficiency, only newcomer's neighbors are updated.
static void evol_update_population_by_newcomer (evol_t *self,
                                                s_indiv_t *newcomer) {
    assert (list4x_size (newcomer->as_neighbors) == 0);
    assert (newcomer->handle_livings_fit);

    // For each newcomer's neighbor
    s_neighbor_t *neighbor;
    list4x_iterator_t iter = list4x_iter_init (newcomer->neighbors, true);
    while ((neighbor = (s_neighbor_t *)list4x_iter (newcomer->neighbors, &iter))
            != NULL) {
        s_indiv_t *indiv = neighbor->indiv;
        assert (indiv != newcomer);

        // Add newcomer to indiv's as_neighbors list (pairing completed)
        s_indiv_add_as_neighbor (indiv, newcomer, iter.handle);

        // print_debug ("");
        // s_indiv_assert_neighbor_pairing (newcomer, indiv);
        // print_debug ("");

        // To see if newcomer could become indiv's neighbor
        int result = s_indiv_add_neighbor (indiv,
                                           newcomer,
                                           neighbor->distance,
                                           self->max_neighbors);

        // If newcomer becomes indiv's neighbor, and indiv is in livings group,
        // re-assess indiv's diversity and score, and reorder it.
        if (result == 0 && indiv->handle_livings_fit) {
            evol_update_diversity_and_score_for_living (self, indiv);

            // print_debug ("");
            // s_indiv_assert_neighbor_pairing (indiv, newcomer);
            // print_debug ("");
        }
    }

    // print_debug ("");
    // evol_assert_population (self);
    // print_debug ("");
}


// Update attributes of livings and ancestors taking acount of a forgotten one.
// Remove all neighbor relation from other individuals to forgotten. Then Try
// to add new neighbors for forgotten's as_neighbors.
static void evol_update_population_by_forgotten (evol_t *self,
                                                 s_indiv_t *forgotten) {
    // print_debug ("");
    // evol_print_indiv (self, forgotten);

    // Remove forgotten from neighbors' as_neighbors list
    s_neighbor_t *nb = NULL;
    list4x_iterator_t iter_nb =
        list4x_iter_init (forgotten->neighbors, true);
    while ((nb = (s_neighbor_t *) list4x_iter (forgotten->neighbors, &iter_nb))
           != NULL) {
        assert (nb->handle_in_as_neighbors);
        list4x_remove (nb->indiv->as_neighbors, nb->handle_in_as_neighbors);
    }

    // Remove forgotten from as_neighbors' neighbors list, and try to add more
    // new neighbors for them.

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

        // Remove forgotten from as_neighbor's neighbors list
        s_indiv_t *indiv = as_nb->indiv;
        list4x_remove (indiv->neighbors, as_nb->handle_in_neighbors);

        // If indiv is in livings group, try to add new neighbor for indiv from
        // its neighbors' neighbors
        size_t max_new_nbs =
            self->max_neighbors - list4x_size (indiv->neighbors);
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

                // Check if nb_nb->indiv is already added in new_nbs list
                bool in_new_nbs = false;
                s_neighbor_t *new_nb = NULL;
                list4x_iterator_t iter_new_nb = list4x_iter_init (new_nbs, true);
                while ((new_nb =
                        (s_neighbor_t *) list4x_iter (new_nbs, &iter_new_nb))
                       != NULL) {
                    if (new_nb->indiv == nb_nb->indiv) {
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
        // neighbor's as_neighbors list (pairing)
        if (list4x_size (new_nbs) > 0) {
            s_neighbor_t *new_nb = NULL;
            list4x_iterator_t iter_new_nbs = list4x_iter_init (new_nbs, true);
            while ((new_nb =
                    (s_neighbor_t *) list4x_iter (new_nbs, &iter_new_nbs))
                   != NULL) {
                int re = s_indiv_add_neighbor (indiv,
                                               new_nb->indiv,
                                               new_nb->distance,
                                               self->max_neighbors);
                assert (re == 0);
                list4x_iter_remove (new_nbs, &iter_new_nbs);
            }
        }

        // If indiv is in livings group, as its neighbors changed, re-assess
        // its diversity and score, and reorder it.
        if (s_indiv_is_living (indiv))
            evol_update_diversity_and_score_for_living (self, indiv);

    }
    list4x_free (&new_nbs);
}


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
    assert (s_indiv_is_out_of_groups (dead));

    // Append the new dead individual (ancestors group is a FIFO queue)
    dead->handle_ancestors = list4x_append (self->ancestors, dead);
    dead->fitness = DOUBLE_NONE;
    dead->diversity = DOUBLE_NONE;
    dead->score = DOUBLE_NONE;

    // If ancestors group overflows, forget the oldest, and update population
    if (list4x_size (self->ancestors) > self->max_ancestors) {
        s_indiv_t *oldest = (s_indiv_t *) list4x_pop_first (self->ancestors);
        evol_update_population_by_forgotten (self, oldest);
        s_indiv_free (&oldest);
    }
}


// Add an individual (newcomer) to livings group.
// - Assess newcomer first.
// - If livings group is full, and the newcomer's score is worse than the worst,
//   it is rejected and destroyed (no chance to go into ancestors group).
// - Otherwise it is accepted. If livings group overflows, the worst one is
//   killed and moved to ancestors group.
static void evol_add_living (evol_t *self, s_indiv_t *newcomer) {
    // If livings group is not regularized yet, simply assess newcomer's fitness
    // and drop it in.
    if (!evol_population_is_regularized (self)) {
        if (double_is_none (newcomer->fitness))
            evol_assess_feasibility_and_fitness (self, newcomer);
        newcomer->handle_livings_fit =
            list4x_insert_sorted (self->livings_rank_fit, newcomer);
        newcomer->handle_livings_div =
            list4x_append (self->livings_rank_div, newcomer);
        newcomer->handle_livings_score =
            list4x_append (self->livings_rank_score, newcomer);

        // Regularize livings group if enough livings are gathered
        if (evol_num_livings (self) > self->max_livings / 2)
            evol_regularize_population (self);

        return;
    }

    // Livings group is regularized

    // Assess newcomer's attributes
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

    // Newcomer accepted. Add it to livings group. The livings group is sorted.
    newcomer->handle_livings_fit =
        list4x_insert_sorted (self->livings_rank_fit, newcomer);
    newcomer->handle_livings_div =
        list4x_insert_sorted (self->livings_rank_div, newcomer);
    newcomer->handle_livings_score =
        list4x_insert_sorted (self->livings_rank_score, newcomer);

    // Update population (approximate algorithm employed)
    evol_update_population_by_newcomer (self, newcomer);

    // print_debug ("");
    // evol_assert_population (self);
    // print_debug ("");

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

        // Remove the dead from livings group
        list4x_pop (self->livings_rank_fit, dead->handle_livings_fit);
        dead->handle_livings_fit = NULL;
        list4x_pop (self->livings_rank_div, dead->handle_livings_div);
        dead->handle_livings_div = NULL;
        list4x_pop (self->livings_rank_score, dead->handle_livings_score);
        dead->handle_livings_score = NULL;

        // Add the dead to ancestors group
        evol_add_ancestor (self, dead);

        // print_debug ("");
        // evol_assert_population (self);
        // print_debug ("");
    }

    // print_debug ("");
    // evol_assert_population (self);
    // print_debug ("");
}


static void evol_fill_livings_with_heuristics (evol_t *self) {
    size_t num_h = list4x_size (self->heuristics);
    assert (num_h > 0);

    if (evol_num_livings (self) == self->max_livings)
        return;

    size_t max_expected;
    size_t random_h_cnt = 0;
    s_heuristic_t *h = NULL;
    size_t num_expected;
    list4x_t *h_genomes = NULL;
    genome_t genome = NULL;
    s_indiv_t *indiv = NULL;

    // Step 1: non-random heuristics

    max_expected = (self->max_livings - evol_num_livings (self)) / num_h;
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

    if (random_h_cnt == 0) // no random heuristics
        return;

    max_expected = (self->max_livings - evol_num_livings (self)) / random_h_cnt;
    if (max_expected == 0) {
        max_expected = 1;
        print_warning ("Livings group idle size is smaller than "
                       "number of random heuristics.\n");
    }

    iter_h = list4x_iter_init (self->heuristics, true);
    while ((h = (s_heuristic_t *) list4x_iter (self->heuristics, &iter_h))
           != NULL) {
        if (!h->is_random)
            continue;

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

    // For each registered crossover operators
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


// Due to variations of the context, (all) individuals in the population should
// be updated (modified or transformed to new individuals, or abandoned)
// accordingly. This operation transforms current livings group to a new one,
// and the ancestors as well as the children group are cleared (because old
// ancestors are meaningless in the new context). This operation is also
// equivalent to initialize a new population from current one.
static void evol_renew_population (evol_t *self) {
    assert (self->renewer);
    print_info ("renew population.\n");

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
    list4x_free (&old_livings_rank_score); // score list should be already empty

    // If new livings group are too small, use heuristics to fill it
    if (evol_num_livings (self) < self->max_livings * 2/3) {
        print_info ("Livings are too few, use heuristics to add more.\n");
        evol_fill_livings_with_heuristics (self);
    }

    // Make sure that the population is regularized
    if (!evol_population_is_regularized (self))
        evol_regularize_population (self);

    print_info ("new livings size: %zu\n", evol_num_livings (self));
}


// Merge children group into livings group.
// The whole process is:
// for each child in children group in fitness descending order:
// - Remove it from children group (do not destroy it)
// - Throw the child into livings group. It may be accepted or rejected, which
// is processed in evol_add_living ()
static void evol_join (evol_t *self) {
    // print_info ("join\n");
    // print_debug ("");
    // evol_assert_population (self);
    // print_debug ("");

    if (evol_num_children (self) == 0)
        return;

    // Make sure children group is sorted in fitness descending order
    if (!list4x_is_sorted_descending (self->children))
        list4x_sort (self->children, false);

    // For every child, remove it from children group,
    // and add it to livings group. After the loop, the children group should
    // be cleared.
    s_indiv_t *child = NULL;
    list4x_iterator_t iter = list4x_iter_init (self->children, true);
    while ((child = (s_indiv_t *) list4x_iter (self->children, &iter))
           != NULL) {
        // Pop child from children group
        list4x_iter_pop (self->children, &iter);
        child->handle_children = NULL;
        // Add child to livings group
        evol_add_living (self, child);
    }
    assert (evol_num_children (self) == 0);
}


// Does fitness improve too slow?
static bool evol_slowdown_happens (evol_t *self) {
    return self->improved_fit_percent < self->min_improved_fit;
}


// Does the evolution reach iteration or time limitations
static bool evol_should_stop_itself (evol_t *self) {
    return (self->iters_cnt >= self->max_iters) ||
           (timer_total (self->timer, 0) >= self->max_time);
}


// Should the evolution stop?
static bool evol_should_stop (evol_t *self) {
    if (self->stopper && self->stopper (self->context))
        return true;
    else
        return evol_should_stop_itself (self);
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


static void evol_update_stats (evol_t *self) {
    self->overall_improved_fit =
        evol_best_fitness (self) / self->initial_best_fit;
}


static void evol_report_stats (evol_t *self) {
    printf ("\n--------------------------------------\n");
    printf ("Stats: \n");
    printf ("Overall fitness improvement: %.2f%%\n",
        (evol_best_fitness (self) - self->initial_best_fit) /
        self->initial_best_fit * 100);
    printf ("Total iters: %zu, time: %.2f s.\n",
            self->iters_cnt,
            timer_total (self->timer, 0));

    printf ("Best fitness individual: \n");
    evol_print_indiv (self,
                      (s_indiv_t *) list4x_first (self->livings_rank_fit));
    printf ("\n");
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

    // ------------------ Recorders ------------------- //

    self->timer      = timer_new ("EVOL_RUN");
    self->step_timer = timer_new ("EVOL_STEP");

    // ------------------ Stats ----------------------- //

    // ------------------ Auxiliaries ----------------- //

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
    print_info ("evol freed.\n");
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


void evol_run (evol_t *self) {
    assert (self);

    print_info ("initilizing...\n");
    // Fill livings group with heuristics
    evol_fill_livings_with_heuristics (self);

    // Make sure that the population is regularized
    if (!evol_population_is_regularized (self))
        evol_regularize_population (self);

    print_info ("population initilized. #livings in population: %zu\n",
                evol_num_livings (self));

    evol_reset_stats (self);
    evol_restart_recorders (self);

    while (!evol_should_stop (self)) {

        if (self->should_renew && self->should_renew (self->context)) {
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


genome_t evol_best_genome (evol_t *self) {
    assert (self);

    if (evol_num_livings (self) == 0)
        return NULL;

    s_indiv_t *indiv_best_fitness = list4x_first (self->livings_rank_fit);
    return indiv_best_fitness->genome;
}


// ---------------------------------------------------------------------------
// Self test using string

// A demo context for string evolution
typedef struct {
    rng_t *rng;
} str_evol_context_t;


// String fitness: length
static double string_fitness1 (const void *context, char *str) {
    return strlen (str);
}


// String fitness: average ASCII
static double string_fitness2 (const void *context, char *str) {
    size_t len = strlen (str);
    if (len == 0)
        return 0.0;
    else {
        int s = 0;
        char *p = str;
        while (*p != '\0') {
            s += (int)(*p);
            p++;
        }
        return s/((double)len);
    }
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
        list4x_append (list, string_random_alphanum (2, 10, context->rng));
    return list;
}


static bool string_should_renew (const str_evol_context_t *context) {
    return rng_random (context->rng) < 0.01;
}


static int string_renewer (const str_evol_context_t *context, char *str) {
    if (strlen (str) > 100)
        return -1;
    else if (rng_random (context->rng) < 0.03)
        str[0] = 'x';
    return 0;
}


void evol_test (bool verbose) {
    print_info (" * evol: \n");

    // Create a context
    rng_t *rng_context = rng_new ();
    str_evol_context_t context = {rng_context};

    // Create evolution object
    evol_t *evol = evol_new ();

    // Set context
    evol_set_context (evol, &context);

    // Set all necessary callbacks
    evol_set_genome_destructor (evol, (free_func_t) string_free);
    evol_set_genome_printer (evol, (print_func_t) string_print);
    evol_set_fitness_assessor (evol, (evol_fitness_assessor_t) string_fitness2);
    evol_set_distance_assessor (evol, (evol_distance_assessor_t) string_distance);
    evol_register_heuristic (evol,
                             (evol_heuristic_t) string_heuristic,
                             true,
                             SIZE_MAX);
    evol_register_crossover (evol, (evol_crossover_t) string_crossover);
    evol_set_renewer (evol,
                      (evol_should_renew_t) string_should_renew,
                      (evol_renewer_t) string_renewer);

    // Run evolution
    evol_run (evol);

    // Get results
    // @todo to add

    // Destroy evolution object
    evol_free (&evol);

    rng_free (&rng_context);
    print_info ("OK\n");
}
