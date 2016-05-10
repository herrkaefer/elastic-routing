/*  =========================================================================
    vrp - generic VRP model

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/
/*
@todo

- 去掉requests, fleet, roadgraph类，直接都扁平放到model里面。

*/

#include "classes.h"


typedef enum {
    NT_NONE,
    NT_DEPOT,
    NT_CUSTOMER
} node_type_t;


typedef struct {
    size_t id; // internal id, for fast query
    char ext_id[UUID_STR_LEN]; // external id connecting to outside

    node_type_t type; // NT_DEPOT or NT_CUSTOMER
    coord2d_t coord; // 2D coordinates
} s_node_t;


// Create a node
static s_node_t *s_node_new (const char *ext_id) {
    assert (ext_id);
    assert (strlen (ext_id) <= UUID_STR_LEN);

    s_node_t *self = (s_node_t *) malloc (sizeof (s_node_t));
    assert (self);

    self->id = ID_NONE;

    strcpy (self->ext_id, ext_id);
    self->type = NT_NONE;
    coord2d_set_none (&self->coord);

    print_info ("node created.\n");
    return self;
}


// Destroy a node
static void s_node_free (s_node_t **self_p) {
    assert (self_p);
    if (*self_p) {
        s_node_t *self = *self_p;

        // free properties here

        free (self);
        *self_p = NULL;
    }
    print_info ("node freed.\n");
}


// ---------------------------------------------------------------------------
typedef struct {
    size_t id;
    char ext_id[UUID_STR_LEN];

    double max_capacity; // maximal capacity
    double capacity; // actual capacity

    // @todo use id or s_node_t?
    size_t start_node_id;
    size_t end_node_id;

    // coord2d_t coord; // current position

    // route attached.
    size_t route_id;
} s_vehicle_t;


// Create a vehicle
static s_vehicle_t *s_vehicle_new (const char *ext_id) {
    assert (ext_id);
    assert (strlen (ext_id) <= UUID_STR_LEN);

    s_vehicle_t *self = (s_vehicle_t *) malloc (sizeof (s_vehicle_t));
    assert (self);

    self->id = ID_NONE;
    strcpy (self->ext_id, ext_id);
    self->max_capacity = DOUBLE_MAX;
    self->capacity = self->max_capacity;
    self->start_node_id = ID_NONE;
    self->end_node_id = ID_NONE;
    self->route_id = ID_NONE;

    return self;
}


// Destroy a vehicle
static void s_vehicle_free (s_vehicle_t **self_p) {
    assert (self_p);
    if (*self_p) {
        s_vehicle_t *self = *self_p;

        // free properties

        free (self);
        *self_p = NULL;
    }
    print_info ("vehicle freed.\n");
}


// ---------------------------------------------------------------------------
typedef enum {
    RS_PENDING, // added, not planned
    RS_PLANNED, // planned, waiting for execution
    RS_BEFORE_PICKUP,
    RS_PICKINGUP,
    RS_BEFORE_DELIVERY,
    RS_DELIVERING,
    RS_BEFORE_VISIT,
    RS_VISITING,
    RS_COMPLETED, // exeuted
} request_state_t;


// @todo about request type, specify node type or not?
typedef enum {
    RT_NONE,
    RT_PICKUP_AND_DELIVERY, // two nodes task (quantity could be zero or not)
    RT_VISIT, // single node task (quantity could be zero or not)
    // RT_DEPOT2CUSTOMER,
    // RT_CUSTOMER2DEPOT,
    // RT_CUSTOMER2CUSTOMER,
    // RT_VISIT2CUSTOMER,
    // RT_VISIT2DEPOT,
} request_type_t;


typedef struct {
    size_t id;
    char ext_id[UUID_STR_LEN]; // external ID connecting request to outside task

    request_type_t type;

    request_state_t state;

    size_t pickup_node_id;
    size_t delivery_node_id;
    double quantity;

    // time windows
    // @todo: need a TW object? time windows and service duration
    time_t pickup_earliest;
    time_t pickup_latest;

    time_t delivery_earliest;
    time_t delivery_latest;

    // service durations
    size_t pickup_duration; // service time for pickup. e.g. min
    size_t delivery_duration; // service time for delivery. e.g. min

    // vehicle attached to. One request is attached to one vehicle.
    size_t vehicle_id;
} s_request_t;


// Create a request object
static s_request_t *s_request_new (const char *ext_id) {
    assert (ext_id && strlen (ext_id) <= UUID_STR_LEN);

    s_request_t *self = (s_request_t *) malloc (sizeof (s_request_t));
    assert (self);

    self->id = ID_NONE;

    strcpy (self->ext_id, ext_id);

    self->type = RT_NONE;
    self->state = RS_PENDING;

    self->pickup_node_id = ID_NONE;
    self->delivery_node_id = ID_NONE;
    self->quantity = 0;

    self->pickup_earliest = TIME_NONE;
    self->pickup_latest = TIME_NONE;

    self->delivery_earliest = TIME_NONE;
    self->delivery_latest = TIME_NONE;

    self->pickup_duration = 0;
    self->delivery_duration = 0;

    self->vehicle_id = ID_NONE;

    print_info ("request created.\n");
    return self;
}


// Destroy request object
static void s_request_free (s_request_t **self_p) {
    assert (self_p);
    if (*self_p) {
        s_request_t *self = *self_p;
        // free properties here

        free (self);
        *self_p = NULL;
    }
    print_info ("request freed.\n");
}


// ---------------------------------------------------------------------------
typedef struct {
    size_t id;

    list4u_t *node_ids;
    list4u_t *node_request_ids;

    // vehicle attached to. One route is attached to one vehicle.
    size_t vehicle_id;

    // requests attached
    // list4u_t request_ids;
} s_route_t;


// Create a route object
static s_route_t *s_route_new (const char *ext_id) {
    assert (ext_id && strlen (ext_id) <= UUID_STR_LEN);

    s_route_t *self = (s_route_t *) malloc (sizeof (s_route_t));
    assert (self);

    self->id = ID_NONE;

    self->node_ids = list4u_new (0);
    self->node_request_ids = list4u_new (0);

    self->vehicle_id = ID_NONE;
    // self->request_ids = list4u_new ();

    print_info ("route created.\n");
    return self;
}


// Destroy request object
static void s_route_free (s_route_t **self_p) {
    assert (self_p);
    if (*self_p) {
        s_route_t *self = *self_p;

        list4u_free (&self->node_ids);
        list4u_free (&self->node_request_ids);

        // list4u_free (&self->request_ids);

        free (self);
        *self_p = NULL;
    }
    print_info ("route freed.\n");
}


// ---------------------------------------------------------------------------
struct _vrp_t {
    // Roadgraph
    arrayset_t *nodes; // vertices of road graph
    matrix4d_t *distances; // arc distance matrix
    matrix4d_t *durations; // arc duration matrix
    coord2d_sys_t coord_sys; // coordinate system

    // Fleet
    arrayset_t *vehicles;

    // Transportation requests
    arrayset_t *requests;

    // Solution
    arrayset_t *plan;

    // Constraints on plan
    double max_route_distance;
    double max_route_duration;
    // ...

    // Auxiliaries
    list4u_t *depot_ids;
    list4u_t *customer_ids;
    list4u_t *pending_requests;
    // bool vehicles_are_homogeneous; // @todo what is the meaning related to state?
};


// ---------------------------------------------------------------------------
// Constructor and destructor
// ---------------------------------------------------------------------------


vrp_t *vrp_new (void) {
    vrp_t *self = (vrp_t *) malloc (sizeof (vrp_t));
    assert (self);

    // Roadgraph
    self->nodes = arrayset_new (0);
    arrayset_set_data_free_func (self->nodes, (free_func_t) s_node_free);
    arrayset_set_hash_funcs (self->nodes,
                             (hash_func_t) string_hash,
                             (equal_func_t) string_equal,
                             NULL);

    self->distances = matrix4d_new (0, 0);
    self->durations = matrix4d_new (0, 0);
    self->coord_sys = CS_NONE;

    // Fleet
    self->vehicles = arrayset_new (0);
    arrayset_set_data_free_func (self->vehicles, (free_func_t) s_vehicle_free);
    arrayset_set_hash_funcs (self->vehicles,
                             (hash_func_t) string_hash,
                             (equal_func_t) string_equal,
                             NULL);

    // Requests
    self->requests = arrayset_new (0);
    arrayset_set_data_free_func (self->requests, (free_func_t) s_request_free);
    arrayset_set_hash_funcs (self->requests,
                             (hash_func_t) string_hash,
                             (equal_func_t) string_equal,
                             NULL);

    // Plan
    self->plan = arrayset_new (0);
    arrayset_set_data_free_func (self->plan, (free_func_t) route_free);
    // @todo do plans need hash?
    // arrayset_set_hash_funcs (self->plans,
    //                          (hash_func_t) string_hash,
    //                          (equal_func_t) string_equal,
    //                          (free_func_t) string_free);

    // Constraints
    self->max_route_distance = DOUBLE_NONE;
    self->max_route_duration = DOUBLE_NONE;

    // Auxiliaries

    self->depot_ids = list4u_new (0);
    self->customer_ids = list4u_new (0);
    self->pending_requests = list4u_new (0);

    print_info ("vrp created.\n");
    return self;
}


void vrp_free (vrp_t **self_p) {
    assert (self_p);
    if (*self_p) {
        vrp_t *self = *self_p;

        arrayset_free (&self->nodes);
        matrix4d_free (&self->distances);
        matrix4d_free (&self->durations);
        arrayset_free (&self->vehicles);
        arrayset_free (&self->requests);
        arrayset_free (&self->plan);

        // auxiliaries
        list4u_free (&self->depot_ids);
        list4u_free (&self->customer_ids);
        list4u_free (&self->pending_requests);

        free (self);
        *self_p = NULL;
    }
    print_info ("vrp freed.\n");
}


vrp_t *vrp_new_from_file (const char *filename) {
    assert (filename);

    enum {
        DATA_SECTION_NONE,
        EDGE_WEIGHT_SECTION,
        NODE_COORD_SECTION,
        DEMAND_SECTION,
        DEPOT_SECTION
    } data_section = DATA_SECTION_NONE;

    enum {
        EDGE_WEIGHT_TYPE_NONE,
        EUC_2D,
        EXPLICIT
    } edge_weight_type = EDGE_WEIGHT_TYPE_NONE;

    enum {
        EDGE_WEIGHT_FORMAT_NONE,
        LOWROW
    } edge_weight_format = EDGE_WEIGHT_FORMAT_NONE;

    vrp_t *self = NULL;
    int K; // num_vehicles
    int N; // num_nodes
    double Q; // capacity
    double *q = NULL; // demands
    matrix4d_t *c = NULL;
    coord2d_t *coords = NULL; // corrds

    const int LINE_LEN = 128;
    char line[LINE_LEN];
    const char *p = NULL;

    int ind1 = 0, ind2 = 0;
    int value1, value2; // double?
    double value_d;
    int value_i;

    FILE *vrpfile;
    int error_flag = 0;

    // open file
    vrpfile = fopen (filename, "r");
    if (vrpfile == NULL) {
        print_error ("Open VRP file %s failed.\n", filename);
        return NULL;
    }

    // recognize file format and determine K
    p = strrchr (filename, '.');
    if (p != NULL && (strcmp (p+1, "vrp") == 0 || strcmp (p+1, "VRP") == 0)) {
        // try to get K from filename
        p = strchr (filename, 'k');
        if (p == NULL)
            p = strchr (filename, 'K');
        if (p != NULL)
            K = (int) strtol (p+1, NULL, 10); // note: long int to int!
        else {
            print_warning ("No vehicle number specified in the file.\n");
            K = 1;
        }
    }
    else if (p != NULL && (strcmp (p+1, "txt") == 0 || strcmp (p+1, "TXT") == 0)) {
        print_error ("File format .txt is not supported yet.\n");
        return NULL;
    }
    else {
        print_error ("Unrecognized input file.\n");
        return NULL;
    }

    // read file content
    while (error_flag == 0 && fgets (line, LINE_LEN, vrpfile) != NULL) {

        p = strchr (line, ':');

        if (p != NULL) { // specification part

            data_section = DATA_SECTION_NONE;

            if (strncmp (line, "DIMENSION", strlen ("DIMENSION")) == 0) {
                N = strtol (p+1, NULL, 10) - 1;
                q = (double *) malloc ((N+1) * sizeof (double));
                assert (q);
                // c = (double *) malloc ((N+1) * (N+1) * sizeof (double));
                c = matrix4d_new (N+1, N+1);
                assert (c);
            }

            else if (strncmp (line, "CAPACITY", strlen ("CAPACITY")) == 0)
                Q = strtod (p+1, NULL);

            else if (strncmp (line, "EDGE_WEIGHT_TYPE", strlen ("EDGE_WEIGHT_TYPE")) == 0) {
                if (strstr (p+1, "EUC_2D") != NULL)
                    edge_weight_type = EUC_2D;
                else if (strstr(p+1, "EXPLICIT") != NULL)
                    edge_weight_type = EXPLICIT;
                else {
                    puts ("ERROR: cvrp_new_from_file: unsupported EDGE_WEIGHT_TYPE.");
                    error_flag = 1;
                }
            }

            else if (strncmp (line, "EDGE_WEIGHT_FORMAT", strlen ("EDGE_WEIGHT_FORMAT")) == 0) {
                if (strstr (p+1, "LOWROW") != NULL)
                    edge_weight_format = LOWROW;
                else {
                    puts ("ERROR: cvrp_new_from_file: unsupported EDGE_WEIGHT_FORMAT.");
                    error_flag = 1;
                }
            }
        }

        else if (strstr (line, "SECTION") != NULL) { // start of data part
            if (strncmp (line, "NODE_COORD_SECTION", strlen ("NODE_COORD_SECTION")) == 0) {
                data_section = NODE_COORD_SECTION;

                // malloc memory of coordinates
                coords = (coord2d_t *) malloc ((N+1) * sizeof (coord2d_t));
                assert (coords);
            }
            else if (strncmp (line, "DEMAND_SECTION", strlen ("DEMAND_SECTION")) == 0)
                data_section = DEMAND_SECTION;
            else if (strncmp (line, "EDGE_WEIGHT_SECTION", strlen ("EDGE_WEIGHT_SECTION")) == 0)
                data_section = EDGE_WEIGHT_SECTION;
            else
                data_section = DATA_SECTION_NONE;
        }

        else { // data
            switch (data_section) {
                case NODE_COORD_SECTION:
                    sscanf (line, "%d %d %d", &ind1, &value1, &value2);
                    coords[ind1-1].v1 = (double)value1;
                    coords[ind1-1].v2 = (double)value2;
                    break;

                case DEMAND_SECTION:
                    sscanf (line, "%d %d", &ind1, &value1);
                    q[ind1-1] = (double)value1;
                    // PRINT("index: %d value1: %d\n", ind1, value1);
                    break;

                case EDGE_WEIGHT_SECTION:
                    if (edge_weight_type == EXPLICIT) {
                        switch (edge_weight_format) {
                            case LOWROW:

                                if (ind1 == 0 && ind2 == 0) {
                                    // c[0] = 0;
                                    matrix4d_set (c, 0, 0, 0);
                                    ind1 = 1; // start at position (1, 0)
                                }

                                // PRINT("Splitting string \"%s\" into tokens:\n", line);
                                p = strtok(line," ");
                                while (p != NULL) {
                                    // c[(N+1)*ind1+ind2] = strtod (p, NULL);
                                    // c[(N+1)*ind2+ind1] = c [(N+1)*ind1+ind2];
                                    value_d = strtod (p, NULL);
                                    matrix4d_set (c, ind1, ind2, value_d);
                                    matrix4d_set (c, ind2, ind1, value_d);

                                    ind2 += 1;
                                    if (ind2 == ind1) {
                                        // c[(N+1)*ind1+ind2] = 0; // asign 0 to diagonal
                                        matrix4d_set (c, ind1, ind2, 0);
                                        ind1 += 1; // new line
                                        ind2 = 0;
                                    }

                                    p = strtok (NULL, " ");
                                }
                                break;

                            default:
                                break;
                        }
                    }
                    break;

                case DEPOT_SECTION:
                    break;

                default:
                    print_warning ("unprocessed data section in file.\n");
            }
        }
    } // end of while

    fclose (vrpfile);

    // no error while reading file
    if (error_flag == 0) {
        // compute cost as Euclidian distance
        if (edge_weight_type == EUC_2D) {
            for (ind1 = 0; ind1 <= N; ind1++) {
                matrix4d_set (c, ind1, ind1, 0);
                for (ind2 = ind1+1; ind2 <= N; ind2++) {
                    // NOTE: rounding the distance by
                    // nint(sqrt(xd*xd+yd*yd)). see TSPLIB spec.
                    // for nint see
                    // https://www.iwr.uni-heidelberg.de/groups/comopt/software/TSPLIB95/TSPFAQ.html
                    value_i = (int)(
                      coord2d_distance (&coords[ind1], &coords[ind2],
                                        CS_CARTESIAN2D)
                      + 0.5);
                    matrix4d_set (c, ind1, ind2, (double) value_i);
                    matrix4d_set (c, ind2, ind1, (double) value_i);
                }
            }
        }

        // create model
        self = vrp_new ();
        char ext_id[UUID_STR_LEN];
        // s_node_t *depot = NULL;
        size_t depot_id;

        // add nodes, arc distances, and requests
        for (size_t cnt = 0; cnt <= N; cnt++) {
            sprintf (ext_id, "%zu", cnt);
            // s_node_t *node = vrp_add_node (self, ext_id);
            size_t node_id;

            node_id = vrp_add_node (self, ext_id);
            if (cnt == 0)
                vrp_set_node_as_depot (self, node_id);
            else
                vrp_set_node_as_customer (self, node_id);

            if (cnt == 0)
                depot_id = node_id;

            if (coords != NULL)
                vrp_set_node_coord (self, node_id, coords[cnt]);

            if (cnt > 0) { // add request: depot->customer
                size_t r_id = vrp_add_request (self, ext_id);
                vrp_set_request_task (self, r_id, depot_id, node_id, q[cnt]);
            }

            for (size_t cnt1 = 0; cnt1 <= cnt; cnt1++) {
                vrp_set_distance (self, cnt1, cnt, matrix4d_get (c, cnt1, cnt));
                vrp_set_distance (self, cnt, cnt1, matrix4d_get (c, cnt, cnt1));
            }
        }

        // add vehicles
        for (size_t cnt = 0; cnt < K; cnt++) {
            sprintf (ext_id, "%zu", cnt);
            size_t v_id = vrp_add_vehicle (self, ext_id);
            vrp_set_vehicle_max_capacity (self, v_id, Q);
        }
    }

    // free memory
    free(q);
    matrix4d_free (&c);
    free(coords);

    return self;
}


// ---------------------------------------------------------------------------
// Roadgraph
// ---------------------------------------------------------------------------


// Get node by id
static s_node_t *vrp_node (vrp_t *self, size_t node_id) {
    s_node_t *node = (s_node_t *) arrayset_data (self->nodes, node_id);
    assert (node);
    return node;
}


void vrp_set_coord_sys (vrp_t *self, coord2d_sys_t coord_sys) {
    assert (self);
    assert (self->coord_sys == CS_NONE); // only set once
    self->coord_sys = coord_sys;
}


size_t vrp_add_node (vrp_t *self, const char *ext_id) {
    assert (self);

    s_node_t *node = s_node_new (ext_id);
    size_t id = arrayset_add (self->nodes, node, node->ext_id);
    node->id = id;
    return id;
}


void vrp_set_node_as_depot (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    assert (node->type == NT_NONE);
    node->type = NT_DEPOT;
    list4u_append (self->depot_ids, node_id);
}


void vrp_set_node_as_customer (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    assert (node->type == NT_NONE);
    node->type = NT_CUSTOMER;
    list4u_append (self->customer_ids, node_id);
}


bool vrp_node_is_depot (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    return node->type == NT_DEPOT;
}


bool vrp_node_is_customer (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    return node->type == NT_CUSTOMER;
}


size_t vrp_query_node (vrp_t *self, const char *node_ext_id) {
    assert (self);
    return arrayset_query (self->nodes, node_ext_id);
}


bool vrp_node_exists (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    return node != NULL;
}


const coord2d_t *vrp_node_coord (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    return &node->coord;
}


void vrp_set_node_coord (vrp_t *self, size_t node_id, coord2d_t coord) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    node->coord.v1 = coord.v1;
    node->coord.v2 = coord.v2;
}


size_t vrp_num_nodes (vrp_t *self) {
    assert (self);
    return arrayset_size (self->nodes);
}


size_t vrp_num_depots (vrp_t *self) {
    assert (self);
    return list4u_size (self->depot_ids);
}


size_t vrp_num_customers (vrp_t *self) {
    assert (self);
    return list4u_size (self->customer_ids);
}


size_t *vrp_node_ids (vrp_t *self) {
    assert (self);
    return arrayset_id_array (self->nodes);
}


size_t *vrp_depot_ids (vrp_t *self) {
    assert (self);
    return list4u_dup_array (self->depot_ids);
}


size_t *vrp_customer_ids (vrp_t *self) {
    assert (self);
    return list4u_dup_array (self->customer_ids);
}


void vrp_set_distance (vrp_t *self,
                       size_t from_node_id,
                       size_t to_node_id,
                       double distance) {
    assert (self);
    assert (distance >= 0);
    matrix4d_set (self->distances, from_node_id, to_node_id, distance);
}


int vrp_generate_straight_distances (vrp_t *self) {
    assert (self);

    size_t num_nodes = vrp_num_nodes (self);
    size_t *ids = vrp_node_ids (self);
    size_t cnt1, cnt2;
    double dist;

    for (cnt1 = 0; cnt1 < num_nodes; cnt1++) {
        for (cnt2 = 0; cnt2 < num_nodes; cnt2++) {
            dist = coord2d_distance (vrp_node_coord (self, ids[cnt1]),
                                     vrp_node_coord (self, ids[cnt1]),
                                     self->coord_sys);
            vrp_set_distance (self, ids[cnt1], ids[cnt2], dist);
        }
    }

    free (ids);
    return 0;
}


double vrp_distance (vrp_t *self, size_t from_node_id, size_t to_node_id) {
    assert (self);
    return matrix4d_get (self->distances, from_node_id, to_node_id);
}


void vrp_set_duration (vrp_t *self,
                       size_t from_node_id,
                       size_t to_node_id,
                       double duration) {
    assert (self);
    assert (duration >= 0);
    matrix4d_set (self->durations, from_node_id, to_node_id, duration);
}


int vrp_generate_durations (vrp_t *self, double speed) {
    assert (self);
    assert (speed >= 0);

    int result = 0;
    size_t num_nodes = vrp_num_nodes (self);
    size_t *ids = vrp_node_ids (self);
    size_t cnt1, cnt2;
    double dist;

    for (cnt1 = 0; cnt1 < num_nodes; cnt1++) {
        for (cnt2 = 0; cnt2 < num_nodes; cnt2++) {
            dist = matrix4d_get (self->distances, ids[cnt1], ids[cnt2]);
            assert (!double_is_none (dist));
            matrix4d_set (self->durations, ids[cnt1], ids[cnt2], dist / speed);
        }
    }

    free (ids);
    return result;
}



double vrp_duration (vrp_t *self, size_t from_node_id, size_t to_node_id) {
    assert (self);
    return matrix4d_get (self->durations, from_node_id, to_node_id);
}


// ---------------------------------------------------------------------------
// Fleet
// ---------------------------------------------------------------------------


// Get vehicle by id
static s_vehicle_t *vrp_vehicle (vrp_t *self, size_t vehicle_id) {
    s_vehicle_t *vehicle =
        (s_vehicle_t *) arrayset_data (self->vehicles, vehicle_id);
    assert (vehicle);
    return vehicle;
}


size_t vrp_add_vehicle (vrp_t *self, const char *vehicle_ext_id) {
    assert (self);
    s_vehicle_t *vehicle = s_vehicle_new (vehicle_ext_id);
    size_t id = arrayset_add (self->vehicles, vehicle, vehicle->ext_id);
    vehicle->id = id;
    return id;
}


size_t vrp_num_vehicles (vrp_t *self) {
    assert (self);
    return arrayset_size (self->vehicles);
}


const char *vrp_vehicle_ext_id (vrp_t *self, size_t vehicle_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    return vehicle->ext_id;
}


double vrp_vehicle_max_capacity (vrp_t *self, size_t vehicle_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    return vehicle->max_capacity;
}


void vrp_set_vehicle_max_capacity (vrp_t *self,
                                   size_t vehicle_id,
                                   double max_capacity) {
    assert (self);
    assert (max_capacity > 0);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    vehicle->max_capacity = max_capacity;
}


double vrp_vehicle_capacity (vrp_t *self, size_t vehicle_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    return vehicle->capacity;
}


void vrp_reset_vehicle_capacity (vrp_t *self, size_t vehicle_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    vehicle->capacity = vehicle->max_capacity;
}


void vrp_reset_all_vehicles_capacities (vrp_t *self) {
    assert (self);
    for (s_vehicle_t *vehicle = (s_vehicle_t *) arrayset_first (self->vehicles);
         vehicle != NULL;
         vehicle = (s_vehicle_t *) arrayset_next (self->vehicles)) {
        vehicle->capacity = vehicle->max_capacity;
    }
}


double vrp_vehicle_load (vrp_t *self, size_t vehicle_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    return vehicle->max_capacity - vehicle->capacity;
}


void vrp_vehicle_do_pickup (vrp_t *self, size_t vehicle_id, double quantity) {
    assert (self);
    assert (quantity >= 0);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    assert (quantity <= vehicle->capacity);
    vehicle->capacity -= quantity;
}


void vrp_vehicle_do_delivery (vrp_t *self, size_t vehicle_id, double quantity) {
    assert (self);
    assert (quantity >= 0);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    assert (vehicle->capacity + quantity <= vehicle->max_capacity);
    vehicle->capacity += quantity;
}


size_t vrp_vehicle_start_node_id (vrp_t *self, size_t vehicle_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    return vehicle->start_node_id;
}


void vrp_vehicle_set_start_node_id (vrp_t *self,
                                    size_t vehicle_id,
                                    size_t node_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    vehicle->start_node_id = node_id;
}


size_t vrp_vehicle_end_node_id (vrp_t *self, size_t vehicle_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    return vehicle->end_node_id;
}


void vrp_set_vehicle_end_node_id (vrp_t *self,
                                  size_t vehicle_id,
                                  size_t node_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    vehicle->end_node_id = node_id;
}


size_t vrp_vehicle_route_id (vrp_t *self, size_t vehicle_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    return vehicle->route_id;
}


void vrp_attach_route_to_vehicle (vrp_t *self,
                                  size_t vehicle_id,
                                  size_t route_id) {
    assert (self);
    // assert (self->route_id != ID_NONE);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    vehicle->route_id = route_id;
}


void vrp_detach_route_from_vehicle (vrp_t *self, size_t vehicle_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    vehicle->route_id = ID_NONE;
}


// ---------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------


// Get request by id
static s_request_t *vrp_request (vrp_t *self, size_t request_id) {
    s_request_t *request =
        (s_request_t *) arrayset_data (self->requests, request_id);
    assert (request);
    return request;
}


size_t vrp_add_request (vrp_t *self, const char *request_ext_id) {
    assert (self);

    s_request_t *request = s_request_new (request_ext_id);
    size_t id = arrayset_add (self->requests, request, request->ext_id);
    request->id = id;

    // @todo add to pending list?
    // ...

    return id;
}


size_t vrp_query_request (vrp_t *self, const char *request_ext_id) {
    assert (self);
    return arrayset_query (self->requests, request_ext_id);
}


void vrp_set_request_task (vrp_t *self,
                           size_t request_id,
                           size_t pickup_node_id,
                           size_t delivery_node_id,
                           double quantity) {
    assert (self);
    assert (quantity >= 0);

    s_request_t *request = vrp_request (self, request_id);

    request->pickup_node_id = pickup_node_id;
    request->delivery_node_id = delivery_node_id;
    if (pickup_node_id == ID_NONE || delivery_node_id == ID_NONE)
        request->type = RT_VISIT;
    else
        request->type = RT_PICKUP_AND_DELIVERY;
    request->quantity = quantity;
}


void vrp_set_request_pickup_time (vrp_t *self,
                                  size_t request_id,
                                  time_t pickup_earliest,
                                  time_t pickup_latest,
                                  size_t pickup_duration) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    request->pickup_earliest = pickup_earliest;
    request->pickup_latest = pickup_latest;
    request->pickup_duration = pickup_duration;
}


void vrp_set_request_delivery_time (vrp_t *self,
                                    size_t request_id,
                                    time_t delivery_earliest,
                                    time_t delivery_latest,
                                    size_t delivery_duration) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    request->delivery_earliest = delivery_earliest;
    request->delivery_latest = delivery_latest;
    request->delivery_duration = delivery_duration;
}


// ---------------------------------------------------------------------------
// Plan
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Submodels verifications
// ---------------------------------------------------------------------------

// Current state of fleet is homogeneous
bool vrp_fleet_is_homogeneous (vrp_t *self);

// CVRP verification
bool vrp_is_cvrp (vrp_t *self);

// ---------------------------------------------------------------------------
void vrp_test (bool verbose) {
    print_info (" * vrp: \n");

    // vrp_t *vrp = vrp_new ();

    vrp_t *vrp = vrp_new_from_file ("testdata/A-n32-k5.vrp");
    assert (vrp);

    // size_t num_nodes = vrp_num_nodes (vrp);
    // print_info ("num_nodes: %zu\n", num_nodes);
    assert (vrp_num_nodes (vrp) == 32);

    // size_t num_vehicles = vrp_num_vehicles (vrp);
    // print_info ("num_vehicles: %zu\n", num_vehicles);
    assert (vrp_num_vehicles (vrp) == 5);

    //
    vrp_free (&vrp);
    print_info ("OK\n");
}
