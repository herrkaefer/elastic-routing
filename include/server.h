/*  =========================================================================
    server - elastic routing server

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __SERVER_H_INCLUDED__
#define __SERVER_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _server_t server_t;
typedef struct _client_t client_t;

// Create a new server object
server_t *server_new (void);

// Destroy server object
void server_free (server_t **self_p);

// Solve
listu_t *server_solve (server_t *self);

// Self test
void server_test (bool verbose);


#ifdef __cplusplus
}
#endif

#endif
