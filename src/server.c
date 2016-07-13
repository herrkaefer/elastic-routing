/*  =========================================================================
    server.c - elastic routing server

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"


struct _server_t {

    hash_t *commands;
    vrp_t *model; // Pointer to currently selected model.
    size_t max_clients;
    listx_t clients;
    client_t *current_client;

    // Networking
    int port;

};


typedef struct _client_t {
    size_t id;            /* Client incremental unique ID. */
    int fd;                 /* Client socket. */

    // char *name;             /* As set by CLIENT SETNAME. */

    // Networking
    zsock_t *socket;
    int port;


    int argc;               /* Num of arguments of current command. */
    char **argv;            /* Arguments of current command. */
    s_command_t *cmd,
    s_command_t *last_cmd;  /* Last command executed. */

    listx_t *reply;            /* List of reply objects to send to the client. */
};


typedef void (*s_command_proc_t) (client_t *c);

typedef struct {
    char *name;
    s_command_proc_t proc;

    int argc;
    char *sflags; /* Flags as string representation, one char per flag. */

} s_ommand_t;


static s_command_t command_table[] = {
    {"add_node", add_command, -2, "w"},
    {"set_node_type", set_command, 3, "w"},
    {"add_vehicle", add_command, 3, "w"},
    {"get_num_vehicles", get_command, 2, "r"}
};


static void create_commands_dict (server_t *self) {
    self->commands = hash_new ((hashfunc_t) string_hash, (matcher_t) string_equal);
    assert (self->commands);

    size_t num_commands = sizeof (command_table) / sizeof (command_table[0]);
    for (size_t idx = 0; idx < num_commands; idx++)
        hash_insert_nq (self->commands, command_table[idx].name, &command_table[idx]);
}


static void server_process_command (server_t *self,
                                    client_t client,
                                    const char *cmd_name) {
    s_command_t *cmd = hash_lookup (self->commands, cmd_name);
    if (cmd == NULL) {
        print_info ("unknown command %s\n", cmd_name);
        return;
    }

    cmd->proc (client);
}


// ---------------------------------------------------------------------------
server_t *server_new () {

    server_t *self = (server_t *) malloc (sizeof (server_t));
    assert (self);

    // self->commands
    create_commands_dict (self);
    assert (self->commands);

    return self;
}


void server_free (server_t **self_p) {
    assert (self_p);
    if (*self_p) {
        server_t *self = *self_p;

        hash_free (&self->commands);

        free (self);
        *self_p = NULL;
    }
    print_info ("server freed.\n");
}



void server_test (bool verbose) {
    print_info (" * server: \n");

    print_info ("OK\n");
}
