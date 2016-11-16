/*  =========================================================================
    route - node sequence

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __ROUTE_H_INCLUDED__
#define __ROUTE_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

// typedef struct _route_t route_t;
typedef listu_t route_t;

#define route_new listu_new
#define route_free listu_free

// route_t *route_new_from_array (size_t *node_ids, size_t num_ndoes);

// route_t *route_new_from_list (listu_t *node_ids);

// void route_free (route_t **self_p);

// void route_print (route_t *self);

// void route_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
