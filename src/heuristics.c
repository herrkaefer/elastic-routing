/*  =========================================================================
    heuristic.c - Elastic Routing: heuristic algorithms
    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"

// to write
int h_clarke_wright (const cvrp_t *cvrp, Individual *individual, int num_indivs_expected) {
    return 0;
}

/* split a TSP route into several CVRP routes, and construct an individual (Prins2004)
   @param vrp: CVRP problem
   @param route: TSP route. format: 0->route[1]->route[2]->...->route[vrp->N]->...
   @param individual (output): CVRP individual
*/
void h_split(const cvrp_t *vrp, const int *route, Individual *individual)
{
    assert (vrp);
    assert (route);
    assert (individual);

    int num_customers = cvrp_num_customers (vrp);
    double capacity = cvrp_capacity (vrp);

    double *sp_cost = NULL; // cost of the shortest path from node 0 to node j in auxiliary graph H
    int *predecessor = NULL; // predecessor of node j on the shortest path
    double route_demand, route_cost;
    int i, j, k;

    sp_cost = (double *) malloc ((num_customers+1) * sizeof (double));
    predecessor = (int *) malloc ((num_customers+1) * sizeof(int));
    if (sp_cost == NULL || predecessor == NULL) {
        print_error ("out of memory.\n");
        goto LABEL_SPLIT;
    }

    // initialize
    for (i = 0; i <= num_customers; i++) {
        sp_cost[i] = DOUBLE_MAX;
        predecessor[i] = -1; // NIL
    }
    sp_cost[0] = 0;

    // relax. NOTE: only capacity constraint is considered (i.e. CVRP).
    for (i = 1; i <= num_customers; i++) {
        route_demand = 0;
        route_cost = 0;

        for (j = i; j <= num_customers; j++) {
            route_demand += cvrp_demand (vrp, route[j]);

            if (route_demand <= capacity) { // arc (i-1,j) exists in auxiliary graph H
                if (i == j)
                    route_cost = cvrp_cost (vrp, 0, route[j]) + cvrp_cost (vrp, route[j], 0);
                    // route_cost = (vrp->c)[route[j]] + (vrp->c)[(N+1)*route[j]]; // c(0,route[j])+c(route[j],0)
                else
                    route_cost  = route_cost
                                - cvrp_cost (vrp, route[j-1], 0)
                                + cvrp_cost (vrp, route[j-1], route[j])
                                + cvrp_cost (vrp, route[j], 0);

                if (sp_cost[i-1] + route_cost < sp_cost[j]) {
                    sp_cost[j] = sp_cost[i-1] + route_cost;
                    predecessor[j] = i - 1;
                }
            }
            else
                break; // to next i
        }
    }

    // PRINT("\nroute:");
    // for (i = 0; i <= N; i++) PRINT(" %d", route[i]);
    // PRINT("\n");

    // PRINT("predecessor:");
    // for (i = 0; i <= N; i++) PRINT(" %d", predecessor[i]);
    // PRINT("\n");
    // PRINT("sp_cost:");
    // for (i = 0; i <= N; i++) PRINT(" %.2f", sp_cost[i]);
    // PRINT("\n");

    // generate individual from predecessor
    individual->num_routes = 0;
    j = num_customers;
    i = predecessor[num_customers];
    while (i >= 0) {
        (individual->first_customer)[individual->num_routes] = route[i+1];
        for (k = i+1; k < j; k++)
            individual->successor[route[k]-1] = route[k+1];

        individual->successor[route[j]-1] = 0;
        individual->num_routes += 1;
        j = i;
        i = predecessor[i];
    }

    if (sp_cost[N] > 0) {
        individual->feasible = 1;
        individual->cost = sp_cost[N];
        // individual->cost = cvrp_compute_cost(vrp, individual); // for comparison
    }

    LABEL_SPLIT:

        free(sp_cost);
        free(predecessor);
}


/* Sweep algorithm for CVRP (classic version): 路径不允许跨越节点包含新的节点
   @status: untested
*/
// int sweep_classic(const cvrp_t *vrp, Individual *individual, int num_indivs_expected)
// {
//     int num_indivs = 0, num_indivs_limit;
//     int N = vrp->N;
//     Polar *polar = NULL;
//     double x0, y0, x, y;
//     int *startlist = NULL;
//     int current_node, last_node; // range: [1,N]
//     int *route = NULL; // storing single route. format: 0->route[1]->route[2]->...->route[n]->0
//     int route_cnt, visited_customcnt, route_customcnt;
//     double route_demand;
//     double cost;
//     int index_node, last_index_polar, index_polar;
//     int i, j;

//     // 如果节点坐标没有提供，则此算法无法执行
//     if (vrp->point == NULL)
//     {
//         PRINT("INFO: sweep: no individual is generated because the coordinates of nodes are not available.\n");
//         return 0;
//     }

//     num_indivs_limit = (num_indivs_expected < N) ? num_indivs_expected : N;

//     /* for test -> */
//     // num_indivs_limit = 1;
//     /* <- for test */

//     polar = (Polar *)malloc(N*sizeof(Polar));
//     route = (int *)malloc((N+2)*sizeof(int));
//     startlist = (int *)malloc(num_indivs_limit*sizeof(int));
//     if (polar == NULL || route == NULL || startlist == NULL)
//     {
//         PRINT("ERROR: sweep: polar or route or startlist malloc failed.\n");
//         goto LABEL_SWEEP_NOJUMP;
//     }

//     // depot cartesian coordinate
//     x0 = (vrp->point)[0].x;
//     y0 = (vrp->point)[0].y;
//     // polar[0].index = 0;
//     // polar[0].theta = 0;

//     // compute angles of customers
//     for (i = 1; i <= N; i++)
//     {
//         x = (vrp->point)[i].x;
//         y = (vrp->point)[i].y;

//         polar[i-1].index = i;
//         polar[i-1].theta = cartesian_to_polar(x-x0, y-y0, NULL);
//         // PRINT("%d: (%.2f, %.2f) -> (%d, %.2f)\n", i, x-x0, y-y0, polar[i-1].index, polar[i-1].theta);
//     }

//     // sort customers in increasing order of angle
//     qsort(polar, N, sizeof(Polar), compare_polar);

//     PRINT("\npolar sorted by theta:\n\n");
//     for (i = 0; i < N; i++)
//     {
//         PRINT("%5d ", i);
//     }
//     PRINT("\n");
//     for (i = 0; i < N; i++)
//     {
//         PRINT("%5d ", polar[i].index);
//     }
//     PRINT("\n");
//     for (i = 0; i < N; i++)
//     {
//         PRINT("%5.2f ", polar[i].theta);
//     }
//     PRINT("\n\n");


//     /* 产生num_indivs_limit个扫描起点
//     当前方案：随机产生num_indivs_expected个初始customer
//     可考虑的改进方案：使用离depot较远或demand较大的customer作为扫描起点
//     */
//     generate_random_mi(startlist, num_indivs_limit, 1, N);

//     PRINT("\nstarter list:\n\n");
//     for (i = 0; i < num_indivs_limit; i++)
//     {
//         PRINT("%5d ", startlist[i]);
//     }
//     PRINT("\n\n");


//     /*** main loop ***/

//     num_indivs = 0;

//     // from each starter customer, construct a solution
//     for (i = 0; i < num_indivs_limit; i++)
//     {

//         last_node = startlist[i]; // first customer

//         // locate the first customer in sorted polar
//         last_index_polar = 0;
//         while (polar[last_index_polar].index != last_node) last_index_polar++;

//         // start the first route
//         route_cnt = 1;
//         route[0] = 0;
//         route[1] = last_node;
//         route_customcnt = 1;
//         route_demand = (vrp->q)[last_node];
//         cost = (vrp->c)[last_node]; // i.e. c(0, current_node)

//         // solution construction
//         for (visited_customcnt = 2; visited_customcnt <= N; visited_customcnt++)
//         {

//             // determine the next customer included
//             // 当前方案：按angle连续添加node，不允许跨越(可考虑改进)
//             index_polar = (last_index_polar+1) % N;
//             current_node = polar[index_polar].index;

//             // PRINT("#last node: %d, current_node: %d\n", last_node, current_node);

//             if (route_demand + (vrp->q)[current_node] > vrp->Q) // close the route and start a new route
//             {
//                 route[route_customcnt+1] = 0; // end with depot
//                 cost += (vrp->c)[(N+1)*last_node]; // add c(last_node, 0)

//                 // post optimization of the route
//                 PRINT("route before post optimization: ");
//                 for (j = 0; j <= route_customcnt+1; j++) PRINT("%d ", route[j]);
//                 PRINT("\n");
//                 tsp_2_opt(route, 0, route_customcnt+1, vrp->c, N);
//                 PRINT("route after post optimization:  ");
//                 for (j = 0; j <= route_customcnt+1; j++) PRINT("%d ", route[j]);
//                 PRINT("\n");

//                 // put the route into individual list
//                 // PRINT("route_cnt: %d, route[1]: %d, route_customcnt: %d\n", route_cnt, route[1], route_customcnt);
//                 (individual[num_indivs].first_customer)[route_cnt-1] = route[1];
//                 for (index_node = 2; index_node <= route_customcnt+1; index_node++)
//                 {
//                     (individual[num_indivs].successor)[route[index_node-1]-1] = route[index_node];
//                 }

//                 individual[num_indivs].num_routes = route_cnt;
//                 individual[num_indivs].feasible = 1;
//                 individual[num_indivs].cost = cost;

//                 // start a new route
//                 route_cnt++;
//                 route_customcnt = 1;
//                 route[1] = current_node;
//                 route_demand = (vrp->q)[current_node];
//                 cost += (vrp->c)[current_node]; // add c(0, current_node)
//                 // PRINT("new route:\n");
//             }

//             else // current route continues
//             {
//                 route_customcnt++;
//                 route[route_customcnt] = current_node;
//                 route_demand += (vrp->q)[current_node];
//                 cost += (vrp->c)[(N+1)*last_node+current_node]; // add c(current_node, current_node)
//             }

//             last_node = current_node;
//             last_index_polar = index_polar;

//         }

//         // close the last route
//         route[route_customcnt+1] = 0; // end with depot
//         cost += (vrp->c)[(N+1)*last_node]; // add c(last_node, 0)

//         // post optimization of the last route
//         PRINT("route before post optimization: ");
//         for (j = 0; j <= route_customcnt+1; j++) PRINT("%d ", route[j]);
//         PRINT("\n");
//         tsp_2_opt(route, 0, route_customcnt+1, vrp->c, N);
//         PRINT("route after post optimization:  ");
//         for (j = 0; j <= route_customcnt+1; j++) PRINT("%d ", route[j]);
//         PRINT("\n");

//         // put the last route into individual list
//         (individual[num_indivs].first_customer)[route_cnt-1] = route[1];
//         for (index_node = 2; index_node <= route_customcnt+1; index_node++)
//         {
//             (individual[num_indivs].successor)[route[index_node-1]-1] = route[index_node];
//         }

//         individual[num_indivs].num_routes = route_cnt;
//         individual[num_indivs].feasible = 1;
//         individual[num_indivs].cost = cost;

//         // print solution
//         // PRINT("\n\nindividual #%d:\n\n", num_indivs+1);
//         // print_individual(vrp, &individual[num_indivs], 1);
//         // PRINT("cost: %f\n\n", cost);

//         num_indivs++;
//     }

//     LABEL_SWEEP_NOJUMP:

//         if (polar != NULL)
//         {
//             free(polar);
//             polar = NULL;
//         }

//         if (route != NULL)
//         {
//             free(route);
//             route = NULL;
//         }

//         if (startlist != NULL)
//         {
//             free(startlist);
//             startlist = NULL;
//         }

//         return num_indivs;
// }


/* Sweep algorithm for CVRP */
int h_sweep(const cvrp_t *vrp, Individual *individual, int num_indivs_expected)
{
    int individual_cnt, num_indivs = 0;
    int N = vrp->N;
    Polar *polar = NULL;
    int *flag_visited = NULL; // 0: not visited, 1: visited
    double x0, y0, x, y;
    int *starters = NULL;
    int *route = NULL; // single route. format: 0->route[1]->route[2]->...->route[N]->0
    int route_cnt, visited_customcnt, route_customcnt;
    double route_demand;
    int index_polar_starter, index_polar_individual_starter, index_polar, index_node, node;
    int is_first_route;
    int i;

    if (num_indivs_expected < 1) return 0;

    if (vrp->point == NULL)
    {
        PRINT("INFO: sweep: no individual is generated because the coordinates of nodes are not available.\n");
        return 0;
    }

    polar        = (Polar *)malloc(N     * sizeof(Polar));
    flag_visited = (int *)  malloc(N     * sizeof(int));
    route        = (int *)  malloc((N+2) * sizeof(int));
    starters     = (int *)  malloc(N     * sizeof(int));
    if (polar == NULL || flag_visited == NULL || route == NULL || starters == NULL)
    {
        PRINT("ERROR: sweep: out of memory.\n");
        goto LABEL_SWEEP;
    }

    // compute angles of customer nodes (depot as origin)
    x0 = (vrp->point)[0].x;
    y0 = (vrp->point)[0].y;
    for (i = 0; i < N; i++)
    {
        x = (vrp->point)[i+1].x;
        y = (vrp->point)[i+1].y;
        polar[i].index = i + 1;
        polar[i].theta = cartesian_to_polar(x-x0, y-y0, NULL);
    }

    // sort customers in ascending order of angle
    qsort(polar, N, sizeof(Polar), compare_polar);

    num_indivs = (num_indivs_expected < N) ? num_indivs_expected : N;

    // random permute [1, N], and use its first num_indivs elements below
    shuffle_range(starters, 1, N+1);

    // from each starter, construct a solution
    for (individual_cnt = 0; individual_cnt < num_indivs; individual_cnt++)
    {
        visited_customcnt = 0;
        route_cnt = 0;
        // cost = 0;
        memset(flag_visited, 0, N*sizeof(int));
        is_first_route = 1;

        // find the first node in polar
        index_polar_individual_starter = 0;
        while (polar[index_polar_individual_starter].index != starters[individual_cnt])
            index_polar_individual_starter++;
        // PRINT("\nindividual: #%d starts at: %d or node: %d\n",
        //      individual_cnt, index_polar_individual_starter, polar[index_polar_individual_starter].index);
        index_polar_starter = index_polar_individual_starter;

        while (visited_customcnt < N)
        {
            // start a new route
            // PRINT("\nstart a new route.\n");
            route[0] = 0;
            route_customcnt = 0;
            route_demand = 0;

            /* 确定新route的扫描起点：
               如果在扫描上个route时有节点被越过而没有加入，则将index_polar指向它
               (第一个越过的节点，也即没被访问的相对起始点角度最小的节点).
               否则接着上一个route之后的节点开始.
            */
            if (index_polar_starter >= 0)
            {
                index_polar = index_polar_starter;
                index_polar_starter = -1;
            }

            while (route_demand < vrp->Q && visited_customcnt < N)
            {
                if (index_polar == index_polar_individual_starter)
                {
                    if (is_first_route == 1) // allowed only for the beginning of the first route
                    {
                        is_first_route = 0;
                    }
                    else
                    {
                        break;
                    }
                }

                node = polar[index_polar].index;

                // PRINT("index_polar: %d, node: %d, visited_customcnt: %d, flag_visited: %d\n",
                //      index_polar, node, visited_customcnt, flag_visited[index_polar]);

                if (flag_visited[index_polar] == 0)
                {
                    if ((route_demand + (vrp->q)[node]) <= vrp->Q)
                    {
                        // add node to route
                        route_customcnt++;
                        visited_customcnt++;
                        flag_visited[index_polar] = 1;
                        route[route_customcnt] = node;
                        route_demand += (vrp->q)[node];
                        // PRINT("----> add node %d to route. route_demand: %.2f, capacity: %.2f\n", node, route_demand, vrp->Q);
                    }
                    else if (index_polar_starter < 0)
                    {
                        // PRINT("----> set as starting node of next route.\n");
                        index_polar_starter = index_polar; // set as start node of next route
                    }

                    // PRINT("route_demand: %.2f\n\n", route_demand);
                }

                index_polar = (index_polar + 1) % N;

            } // end while

            // close route
            route[route_customcnt+1] = 0;

            // add route to individual
            (individual[individual_cnt].first_customer)[route_cnt] = route[1];
            for (index_node = 2; index_node <= route_customcnt+1; index_node++)
            {
                (individual[individual_cnt].successor)[route[index_node-1]-1] = route[index_node];
            }

            route_cnt++;

        } // end while

        individual[individual_cnt].num_routes = route_cnt;
        individual[individual_cnt].feasible = 1;
        individual[individual_cnt].cost = cvrp_compute_cost(vrp, individual+individual_cnt);

        // improve individual by local search
        local_search(vrp, &individual[individual_cnt]);

    } // end for

    LABEL_SWEEP:

        free(polar);
        free(flag_visited);
        free(route);
        free(starters);
        return num_indivs;
}


/* TSP+Split algorithm for CVRP */
int h_tsp_split(const cvrp_t *vrp, Individual *individual, int num_indivs_expected)
{
    int *route = NULL; // TSP route. format: 0->route[1]->...->route[N]->0
    int N = vrp->N;

    if (vrp == NULL || individual == NULL || num_indivs_expected < 1)
    {
        PRINT("ERROR: tsp_split: invalid argument.\n");
        return 0;
    }

    route = (int *)malloc((N+2)*sizeof(int));
    if (route == NULL)
    {
        PRINT("ERROR: tsp_split: out of memory.\n");
        return 0;
    }

    /* construct a TSP route
       method: sweep first, random permutation as candidate
    */
    if (tsp_sweep(route, vrp->point, N, 0, 0) == 0)
    {
        PRINT("INFO: tsp_split: simple construction is used.\n");
        route[0] = 0;
        shuffle_range(route+1, 1, N+1);
        route[N+1] = 0;
    }

    // PRINT("\nroute before local search:\n");
    // print_route(vrp, route, 0, N+1);

    /* TSP route improvement by local search */
    tsp_2_opt(route, 0, N+1, vrp->c, N); // 2-opt
    // tsp_3_opt(route, 0, N+1, vrp->c, N, NULL, 0); // 3-opt

    // PRINT("\nroute after local search:\n");
    // print_route(vrp, route, 0, N+1);

    // Split TSP route into VRP routes, and put them into individual
    split(vrp, route, individual);

    free(route);
    return 1;
}


int h_random_permute_split(const cvrp_t *vrp, Individual *individual, int num_indivs_expected)
{
    if (num_indivs_expected < 1) return 0;

    int N = vrp->N, num_indivs, i;
    int *route = (int *)malloc((N+1)*sizeof(int));
    if (route == NULL)
    {
        PRINT("ERROR: random_permute_split: out of memory.\n");
        return 0;
    }

    route[0] = 0;
    for (i = 1; i <= N; i++) route[i] = i;

    num_indivs = (num_indivs_expected < N) ? num_indivs_expected : N;
    for (i = 0; i < num_indivs; i++)
    {
        shuffle(route+1, N);
        split(vrp, route, individual+i);
    }

    free(route);
    return num_indivs;
}


// set partitioning problem for cvrp
// generate an new individual by greed method
bool h_greedy(CVRP_Problem *vrp, Individual *individual)
{
    int* buf = (int *) malloc(sizeof(int) * vrp->N * 3);  // size ???
    int* index_rank_q = &buf[0];// size = sizeof(int) * vrp->N
    int  num_rank_q = vrp->N;
    int* state = &buf[vrp->N];  // indicate whether the requirement of the customer is met or not
    int* seq = &buf[2*vrp->N];  // the sequence of the customer met
    int  num_seq = 0;
    double residue = vrp->Q;
    int  curr_select_cnt;
    int i, j, k;

    // sort the customers due to q
    // the quick sort method can improve the performance of the following code
    for (i = 0; i < vrp->N; i ++)
    {
        index_rank_q[i] = i+1;
        state[i] = 0;
    }
    quick_sort_a_ref(vrp->q, index_rank_q, vrp->N);

    // there is a customer whose requirement is beyond the capacity of the vehicle
    // then return false
    if (vrp->q[index_rank_q[vrp->N-1]] > vrp->Q)
    {
        free(buf);
        return false;
    }

    k = 0; // the k-th bucket
    while ((num_rank_q > 0) && (k < vrp->K))
    {
        // assemble the k-th bucket
        residue = vrp->Q;
        curr_select_cnt = 0;
        for (i = num_rank_q - 1; i >= 0 && residue > 0; i --)
        {
            if (vrp->q[index_rank_q[i]] <= residue)
            {
                seq[num_seq] = index_rank_q[i];
                num_seq ++;

                if (curr_select_cnt == 0)
                {
                    individual->first_customer[k] = index_rank_q[i];
                }

                residue -= vrp->q[index_rank_q[i]];
                state[i] = 1;

                curr_select_cnt ++;
            }
        }

        // adjust the state
        // if (curr_select_cnt < num_rank_q)
        {
            i = 0;
            while (state[i] == 0) i ++;

            for (j = i + 1; j < num_rank_q; j ++)
            {
                if (state[j] == 0)
                {
                    index_rank_q[i] = index_rank_q[j];
                    state[i] = 0;
                    i ++;
                }
            }
        }

        num_rank_q -= curr_select_cnt;
        k ++;
    }

    // K vehicles does not meet the requirement of all customers
    // then return false
    if (num_rank_q > 0)
    {
        free(buf);
        return false;
    }

    // assemble the individual
    // sort the first_customer
    individual->num_routes = k;
    quick_sort_a(individual->first_customer, individual->num_routes);

    for (i = 0; i < vrp->N - 1; i ++)
    {
        individual->successor[seq[i]-1] = (binary_search(individual->first_customer, individual->num_routes, seq[i+1]) == -1) ? seq[i+1] : 0;
    }
    individual->successor[seq[i]-1] = 0;

    // ....
    individual->cost = cvrp_compute_cost(vrp, individual);
    individual->feasible = 1;

    // optimized
    local_search(vrp, individual);

    free(buf);
    return true;
}


// generate one individual randomly by means of spp
bool generate_individual_spp(CVRP_Problem *vrp, Individual *individual)
{
    int selected, i, k = 0;

    int* buf = (int *) malloc((vrp->N * 3) * sizeof(int));
    int* seq = buf;
    int* bucket = & buf[vrp->N];
    int* free_list = & bucket[vrp->N];
    int  num_free = vrp->N;
    for (i = 0; i < vrp->N; i ++)
    {
        free_list[i] = i+1;
    }

    double* residue = (double *) malloc(sizeof(double) * vrp->K);
    for (i = 0; i < vrp->K; i ++)
    {
        residue[i] = vrp->Q;
    }

    // assemble the bucket randomly
    while (num_free > 0)
    {
        selected = generate_random_i(0, num_free);

        i = 0;
        while ((i < vrp->K) && (vrp->q[free_list[selected]] > residue[i])) i ++;

        if (i == vrp->K)
        {
            // failure
            free(buf);
            free(residue);
            return false;
        }

        seq[k] = free_list[selected];
        bucket[k] = i;
        k ++;

        residue[i] -= vrp->q[free_list[selected]];
        num_free --;
        free_list[selected] = free_list[num_free];
    }

    // reconstruct the individual
    for (i = 0; i < vrp->K; i ++)
    {
        individual->first_customer[i] = -1;
    }

    for (i = 0; i < vrp->N; i ++)
    {
        if (individual->first_customer[bucket[i]] == -1)
        {
            individual->successor[seq[i]-1] = 0;
            individual->first_customer[bucket[i]] = seq[i];
        }
        else
        {
            individual->successor[seq[i]-1] = individual->first_customer[bucket[i]];
            individual->first_customer[bucket[i]] = seq[i];
        }
    }

    i = vrp->K - 1;
    while (individual->first_customer[i] == -1) i --;
    individual->num_routes = i + 1;
    quick_sort_a(individual->first_customer, individual->num_routes);

    // ....
    individual->cost = cvrp_compute_cost(vrp, individual);
    individual->feasible = 1;

    // optimized
    local_search(vrp, individual);

    free(buf);
    free(residue);
    return true;
}


int h_random_set_partition(CVRP_Problem *vrp, Individual *individual, int num_indivs)
{
    int  total_test = num_indivs * 10;
    int  num_test = 0, i = 0;
    while ((i < num_indivs) && (num_test < total_test))
    {
        while (!generate_individual_spp(vrp, &individual[i]) && (num_test < total_test)) num_test ++;

        if (num_test < total_test)
        {
            i ++;
        }
    }

    return i;
}
