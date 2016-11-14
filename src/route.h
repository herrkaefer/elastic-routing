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

typedef struct _route_t route_t;

route_t *route_new (void);

void route_free (route_t **self_p);

void route_print (route_t *self);

void route_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
