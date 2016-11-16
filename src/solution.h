/*  =========================================================================
    solution - generic VRP solution representation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __SOLUTION_H_INCLUDED__
#define __SOLUTION_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _solution_t solution_t;

solution_t *solution_new (vrp_t *vrp);

void solution_free (solution_t **self_p);

// Add a route from node ids array
void solution_add_route_from_array (solution_t *self,
                                    const size_t *node_ids,
                                    size_t num_nodes);

// Add a route from a list. node_ids is then taken over by the solution.
void solution_add_route_from_list (solution_t *self, listu_t *node_ids);

size_t solution_num_routes (solution_t *self);

route_t *solution_route (solution_t *self, size_t route_idx);

size_t solution_route_length (solution_t *self, size_t route_idx);

void solution_print (solution_t *self);

void solution_test (bool verbose);


#ifdef __cplusplus
}
#endif

#endif
