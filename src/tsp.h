/*  =========================================================================
    tsp - TSP

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __TSP_H_INCLUDED__
#define __TSP_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _tsp_t tsp_t;

// Create a new tsp object
tsp_t *tsp_new_from (vrp_t *vrp);

// Destroy tsp object
void tsp_free (tsp_t **self_p);

void tsp_solve (tsp_t *self);

// Self test
void tsp_test (bool verbose);


#ifdef __cplusplus
}
#endif

#endif
