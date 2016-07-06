/*  =========================================================================
    evol - evolution framework

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __EVOL_H_INCLUDED__
#define __EVOL_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _evol_t evol_t;

// Genome representation
typedef void *genome_t;

// Heuristic callback for generating genomes from context
// Return a list of genome_t
// This function is used to initialize population before evolution
typedef list4x_t *(*evol_heuristic_t) (void *context, size_t max_expected);

// Fitness assessment callback
typedef double (*evol_fitness_assessor_t) (void *context,
                                           genome_t genome);

// Feasibility assessment callback
typedef bool (*evol_feasiblity_assessor_t) (void *context,
                                            genome_t genome);

// Function of computing distance between individuals
typedef double (*evol_distance_assessor_t) (void *context,
                                            genome_t genome1,
                                            genome_t genome2);

// Crossover callback: generating child from parents
// Return a list of children's genomes
// Note that if you do not set genome duplicator for evol, leave the list's
// free function as NULL, so that evol can use the genomes and release them.
typedef list4x_t *(*evol_crossover_t) (void *context,
                                       genome_t parent1,
                                       genome_t parent2);

// Mutation callback
typedef genome_t (*evol_mutator_t) (void *context, genome_t genome);

// Local improving callback (genome is improved in place)
typedef void (*evol_local_improver_t) (void *context, genome_t genome);

// Callback to check whether renewer should be called to update the population.
// This is mostly due to variation of context
typedef bool (*evol_should_renew_t) (void *context);

// Genome renew callback (genome is updated in place)
// When context varies during evolution, you can use this callback to update
// current individuals accordingly.
// Return 0 if genome should be keeped (modified or tansformed into a new one.)
// Return -1 if genome should be dropped.
typedef int (*evol_renewer_t) (void *context, genome_t genome);

// Outside stopper callback
typedef bool (*evol_stopper_t) (void *context);

// ---------------------------------------------------------------------------

// Create evolution object
evol_t *evol_new (void);

// Destroy evolution object
void evol_free (evol_t **self_p);

// Set the context to which individuals are refered (e.g. a model)
void evol_set_context (evol_t *self, void *context);

// Set livings group size
// If this is not set, default value is used.
void evol_set_livings_group_size (evol_t *self, size_t max_livings);

// Set ancestors group size
// If this is not set, default value is used.
void evol_set_ancestors_group_size (evol_t *self, size_t max_ancestors);

// Set children group size
// If this is not set, default value is used.
void evol_set_children_group_size (evol_t *self, size_t max_children);

// Set genome free function
void evol_set_genome_destructor (evol_t *self, destructor_t fn);

// Set genome duplicate function
// If genome duplicator is set, genome destructor should be set as well.
void evol_set_genome_duplicator (evol_t *self, duplicator_t fn);

// Set genome print function
void evol_set_genome_printer (evol_t *self, printer_t fn);

// Set genome feasiblity assessment function
void evol_set_feasibility_assessor (evol_t *self,
                                    evol_feasiblity_assessor_t fn);

// Set genome fitness assessment function
void evol_set_fitness_assessor (evol_t *self, evol_fitness_assessor_t fn);

// Set genome distance computing function
void evol_set_distance_assessor (evol_t *self, evol_distance_assessor_t fn);

// Set should_renew callback and genome renewer callback
void evol_set_renewer (evol_t *self, evol_should_renew_t fn, evol_renewer_t gn);

// Set outside stopper callback
void evol_set_stopper (evol_t *self, evol_stopper_t fn);

// Register heuristic function.
// Call this multiple times to add multiple heuristics.
// is_random: if the function generates random genomes with different calling.
// max_num_expected: max number of genomes expected to get for every calling.
void evol_register_heuristic (evol_t *self,
                              evol_heuristic_t fn,
                              bool is_random,
                              size_t max_expected);

// Register crossover function
void evol_register_crossover (evol_t *self, evol_crossover_t fn);

// Set genome mutation function
void evol_register_mutator (evol_t *self, evol_mutator_t fn);

// Set genome local search function
void evol_register_local_improver (evol_t *self, evol_local_improver_t fn);

// Run evolution
void evol_run (evol_t *self);

// Get genome with best fitness
genome_t evol_best_genome (evol_t *self);

void evol_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
