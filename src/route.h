/*  =========================================================================
    route - node sequence

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

void route_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
