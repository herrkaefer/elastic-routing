/*  =========================================================================
    heuristic - Elastic Routing: heuristics
    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __HEURISTIC_H_INCLUDED__
#define __HEURISTIC_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

int h_clarke_wright (const cvrp_t *cvrp, Individual *individual, int num_indivs_expected);

int h_sweep (const cvrp_t *cvrp, Individual *individual, int num_indivs_expected);

int h_tsp_split (const cvrp_t *cvrp, Individual *individual, int num_indivs_expected);

int h_random_permute_split (const cvrp_t *cvrp, Individual *individual, int num_indivs_expected);

int h_random_set_partition (const cvrp_t *cvrp, Individual *individual, int num_indivs_expected);

int h_greedy (const cvrp_t *cvrp, Individual *individual, int num_indivs_expected);

void h_split (const cvrp_t *cvrp, const int *route, Individual *individual);

#ifdef __cplusplus
}
#endif

#endif
