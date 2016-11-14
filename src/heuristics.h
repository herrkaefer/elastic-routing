/*  =========================================================================
    heuristic - heuristics

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
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
