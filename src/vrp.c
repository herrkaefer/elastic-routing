/*  =========================================================================
    vrp - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/
/*
@todo

- 去掉requests, fleet, roadgraph类，直接都扁平放到model里面。

*/

#include "classes.h"


// ---------------------------------------------------------------------------
// Node

typedef struct {
    size_t id; // internal id, for fast query
    char ext_id[UUID_STR_LEN]; // external id connecting to outside

    node_role_t role; // NR_DEPOT or NR_CUSTOMER
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
    self->role = NR_NONE;
    coord2d_set_none (&self->coord);

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
}


// ---------------------------------------------------------------------------
// Vehicle

typedef struct {
    size_t id;
    char ext_id[UUID_STR_LEN];

    double max_capacity; // maximal capacity
    double capacity; // current capacity

    // @todo use id or s_node_t?
    size_t start_node_id;
    size_t end_node_id;

    // coord2d_t coord; // current position

    // attached route
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
}


// ---------------------------------------------------------------------------
// Request

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
    RT_PD, // pickup-and-delivery, two nodes task (quantity >= 0)
    RT_VISIT, // single node visiting
              // quantity > 0: pickup (goods are always on vehicle after pickup);
              // quantity < 0: delivery (goods are already on vehicle);
              // quantity == 0: visiting
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

    // Task
    size_t pickup_node_id;
    size_t delivery_node_id;
    double quantity;

    // Multiple time windows are supported.
    // For pickup or delivery, TWs are saved in respective list.
    // The value is unsigned int which represents the number of time units since
    // a common reference point.
    // The number of TW is size/2. The earliest time for TW_i is at index 2*i,
    // and the latest time for TW_i is at index (2*i+1).
    listu_t *pickup_time_windows;
    listu_t *delivery_time_windows;

    // service duration
    size_t pickup_duration; // service time for pickup. e.g. min
    size_t delivery_duration; // service time for delivery. e.g. min

    // Solution related
    // vehicle attached to. One request is only attached to one vehicle.
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

    self->pickup_time_windows = NULL;
    self->delivery_time_windows = NULL;

    self->pickup_duration = 0;
    self->delivery_duration = 0;

    self->vehicle_id = ID_NONE;
    return self;
}


// Destroy request object
static void s_request_free (s_request_t **self_p) {
    assert (self_p);
    if (*self_p) {
        s_request_t *self = *self_p;
        if (self->pickup_time_windows != NULL)
            listu_free (&self->pickup_time_windows);
        if (self->delivery_time_windows != NULL)
            listu_free (&self->delivery_time_windows);

        free (self);
        *self_p = NULL;
    }
}


// ---------------------------------------------------------------------------
// VRP Model

struct _vrp_t {

    // Roadgraph
    arrayset_t *nodes; // vertices of road graph
    matrixd_t *distances; // arc distance matrix
    matrixu_t *durations; // arc duration matrix
    coord2d_sys_t coord_sys; // coordinate system

    // Fleet
    arrayset_t *vehicles;

    // Transportation requests
    arrayset_t *requests;

    // Constraints
    double max_route_distance;
    double max_route_duration;
    // ...

    // Auxiliaries
    listu_t *node_ids;
    listu_t *depot_ids;
    listu_t *customer_ids;
    listu_t *vehicle_ids;
    listu_t *pending_request_ids;
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
    arrayset_set_data_free_func (self->nodes, (destructor_t) s_node_free);
    arrayset_set_hash_funcs (self->nodes,
                             (hashfunc_t) string_hash,
                             (matcher_t) string_equal,
                             NULL);

    self->distances = NULL; // lazy creation
    self->durations = NULL;
    self->coord_sys = CS_NONE;

    // Fleet
    self->vehicles = arrayset_new (1);
    arrayset_set_data_free_func (self->vehicles, (destructor_t) s_vehicle_free);
    arrayset_set_hash_funcs (self->vehicles,
                             (hashfunc_t) string_hash,
                             (matcher_t) string_equal,
                             NULL);

    // Requests
    self->requests = arrayset_new (1);
    arrayset_set_data_free_func (self->requests, (destructor_t) s_request_free);
    arrayset_set_hash_funcs (self->requests,
                             (hashfunc_t) string_hash,
                             (matcher_t) string_equal,
                             NULL);

    // Constraints
    self->max_route_distance = DOUBLE_MAX;
    self->max_route_duration = SIZE_MAX;

    // Auxiliaries
    self->node_ids = listu_new (0);
    self->depot_ids = listu_new (0);
    self->customer_ids = listu_new (0);
    self->vehicle_ids = listu_new (0);
    self->pending_request_ids = listu_new (0);

    print_info ("vrp created.\n");
    return self;
}


void vrp_free (vrp_t **self_p) {
    assert (self_p);
    if (*self_p) {
        vrp_t *self = *self_p;

        arrayset_free (&self->nodes);
        matrixd_free (&self->distances);
        matrixu_free (&self->durations);
        arrayset_free (&self->vehicles);
        arrayset_free (&self->requests);

        // auxiliaries
        listu_free (&self->node_ids);
        listu_free (&self->depot_ids);
        listu_free (&self->customer_ids);
        listu_free (&self->vehicle_ids);
        listu_free (&self->pending_request_ids);

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
    size_t num_vehicles = 1;
    size_t num_nodes;
    double capacity = DOUBLE_MAX;
    double *demands = NULL;
    matrixd_t *costs = NULL;
    coord2d_t *coords = NULL;

    const int LINE_LEN = 128;
    char line[LINE_LEN];
    const char *p = NULL;

    int ind1 = 0, ind2 = 0;
    int value1, value2; // double?
    double value_d;
    int value_i;

    FILE *vrpfile;
    int error_flag = 0;

    // Open file
    vrpfile = fopen (filename, "r");
    if (vrpfile == NULL) {
        print_error ("Open VRP file %s failed.\n", filename);
        return NULL;
    }

    // Recognize file format and try to read vehicle number K from filename
    p = strrchr (filename, '.');
    // VRP
    if (p != NULL && (strcmp (p + 1, "vrp") == 0 || strcmp (p + 1, "VRP") == 0)) {
        // try to get K from filename
        p = strrchr (filename, 'k');
        if (p == NULL)
            p = strrchr (filename, 'K');
        if (p != NULL)
            num_vehicles = (size_t) strtol (p + 1, NULL, 10);
        else
            print_warning ("No vehicle number specified in filename.\n");
    }
    // TSP
    else if (p != NULL &&
             (strcmp (p + 1, "tsp") == 0 ||
              strcmp (p + 1, "TSP") == 0 ||
              strcmp (p + 1, "atsp") == 0 ||
              strcmp (p + 1, "ATSP") == 0)) {
        num_vehicles = 1;
    }
    else {
        print_error ("Unsupported input file.\n");
        return NULL;
    }

    // Read file content
    while (error_flag == 0 && fgets (line, LINE_LEN, vrpfile) != NULL) {

        p = strchr (line, ':');

        if (p != NULL) { // specification part

            data_section = DATA_SECTION_NONE;

            if (strncmp (line, "DIMENSION", strlen ("DIMENSION")) == 0) {
                num_nodes = strtol (p + 1, NULL, 10); // number of all nodes
                costs = matrixd_new (num_nodes, num_nodes);
                assert (costs);
            }

            else if (strncmp (line, "CAPACITY", strlen ("CAPACITY")) == 0)
                capacity = strtod (p + 1, NULL);

            else if (strncmp (line, "EDGE_WEIGHT_TYPE",
                              strlen ("EDGE_WEIGHT_TYPE")) == 0) {
                if (strstr (p + 1, "EUC_2D") != NULL)
                    edge_weight_type = EUC_2D;
                else if (strstr (p + 1, "EXPLICIT") != NULL)
                    edge_weight_type = EXPLICIT;
                else {
                    print_error ("unsupported EDGE_WEIGHT_TYPE: %s.\n", p + 1);
                    error_flag = 1;
                }
            }

            else if (strncmp (line, "EDGE_WEIGHT_FORMAT",
                              strlen ("EDGE_WEIGHT_FORMAT")) == 0) {
                if (strstr (p + 1, "LOWROW") != NULL)
                    edge_weight_format = LOWROW;
                else {
                    print_error ("unsupported EDGE_WEIGHT_FORMAT: %s.\n", p + 1);
                    error_flag = 1;
                }
            }
        }

        // Start of data part
        else if (strstr (line, "SECTION") != NULL) {
            if (strncmp (line, "NODE_COORD_SECTION",
                         strlen ("NODE_COORD_SECTION")) == 0) {
                data_section = NODE_COORD_SECTION;
                coords = (coord2d_t *) malloc (num_nodes * sizeof (coord2d_t));
                assert (coords);
            }
            else if (strncmp (line, "DEMAND_SECTION",
                              strlen ("DEMAND_SECTION")) == 0) {
                data_section = DEMAND_SECTION;
                demands = (double *) malloc (num_nodes * sizeof (double));
                assert (demands);
            }
            else if (strncmp (line, "EDGE_WEIGHT_SECTION",
                              strlen ("EDGE_WEIGHT_SECTION")) == 0) {
                data_section = EDGE_WEIGHT_SECTION;
            }
            else
                data_section = DATA_SECTION_NONE;
        }

        // Data
        else {
            switch (data_section) {
                case NODE_COORD_SECTION:
                    sscanf (line, "%d %d %d", &ind1, &value1, &value2);
                    coords[ind1-1].v1 = (double) value1;
                    coords[ind1-1].v2 = (double) value2;
                    break;

                case DEMAND_SECTION:
                    sscanf (line, "%d %d", &ind1, &value1);
                    demands[ind1-1] = (double) value1;
                    break;

                case EDGE_WEIGHT_SECTION:
                    if (edge_weight_type == EXPLICIT) {
                        switch (edge_weight_format) {
                            case LOWROW:

                                if (ind1 == 0 && ind2 == 0) {
                                    // c[0] = 0;
                                    matrixd_set (costs, 0, 0, 0);
                                    ind1 = 1; // start at position (1, 0)
                                }

                                // PRINT("Splitting string \"%s\" into tokens:\n", line);
                                p = strtok (line, " ");
                                while (p != NULL) {
                                    // c[(N+1)*ind1+ind2] = strtod (p, NULL);
                                    // c[(N+1)*ind2+ind1] = c [(N+1)*ind1+ind2];
                                    value_d = strtod (p, NULL);
                                    matrixd_set (costs, ind1, ind2, value_d);
                                    matrixd_set (costs, ind2, ind1, value_d);

                                    ind2 += 1;
                                    if (ind2 == ind1) {
                                        // c[(N+1)*ind1+ind2] = 0; // asign 0 to diagonal
                                        matrixd_set (costs, ind1, ind2, 0);
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

    // No error while reading file
    if (error_flag == 0) {
        // compute cost as Euclidian distance
        if (edge_weight_type == EUC_2D) {
            for (ind1 = 0; ind1 < num_nodes; ind1++) {
                matrixd_set (costs, ind1, ind1, 0);
                for (ind2 = ind1 + 1; ind2 < num_nodes; ind2++) {
                    // @note: rounding the distance by
                    // nint(sqrt(xd*xd+yd*yd)). see TSPLIB spec.
                    // for nint see
                    // https://www.iwr.uni-heidelberg.de/groups/comopt/software/TSPLIB95/TSPFAQ.html
                    value_i = (int)(
                      coord2d_distance (&coords[ind1], &coords[ind2],
                                        CS_CARTESIAN2D)
                      + 0.5);
                    matrixd_set (costs, ind1, ind2, (double) value_i);
                    matrixd_set (costs, ind2, ind1, (double) value_i);
                }
            }
        }

        // create model
        self = vrp_new ();
        char ext_id[UUID_STR_LEN];
        size_t depot_id, node_id;

        // Add nodes, arc distances, and requests
        for (size_t cnt = 0; cnt < num_nodes; cnt++) {
            sprintf (ext_id, "node-%04zu", cnt + 1);
            if (demands != NULL) // CVRP
                node_id = vrp_add_node (self, ext_id,
                                        (cnt == 0) ? NR_DEPOT : NR_CUSTOMER);
            else // TSP
                node_id = vrp_add_node (self, ext_id, NR_CUSTOMER);

            if (coords != NULL)
                vrp_set_node_coord (self, node_id, coords[cnt]);

            if (demands != NULL) { // CVRP
                if (cnt == 0) // depot
                    depot_id = node_id;
                else // add request: depot->customer
                    vrp_add_request (self, ext_id, depot_id, node_id, demands[cnt]);
            }
            else // TSP
                vrp_add_request (self, ext_id, ID_NONE, node_id, 0);

            for (size_t cnt1 = 0; cnt1 <= cnt; cnt1++) {
                vrp_set_arc_distance (self, cnt1, cnt,
                                      matrixd_get (costs, cnt1, cnt));
                vrp_set_arc_distance (self, cnt, cnt1,
                                      matrixd_get (costs, cnt, cnt1));
            }
        }

        // Add vehicles
        if (demands != NULL) { // CVRP
            for (size_t cnt = 0; cnt < num_vehicles; cnt++) {
                sprintf (ext_id, "vehicle-%04zu", cnt + 1);
                vrp_add_vehicle (self, ext_id, capacity, depot_id, depot_id);
            }
        }
        else {
            sprintf (ext_id, "vehicle-%04d", 1);
            vrp_add_vehicle (self, ext_id, DOUBLE_MAX, ID_NONE, ID_NONE);
        }
    }

    if (demands != NULL)
        free (demands);
    if (costs != NULL)
        matrixd_free (&costs);
    if (coords != NULL)
        free (coords);

    print_info ("vrp created from file.\n");
    return self;
}


// ---------------------------------------------------------------------------
// Roadgraph
// ---------------------------------------------------------------------------


// Get node by id
static s_node_t *vrp_node (vrp_t *self, size_t node_id) {
    s_node_t *node = (s_node_t *) arrayset_data (self->nodes, node_id);
    return node;
}


void vrp_set_coord_sys (vrp_t *self, coord2d_sys_t coord_sys) {
    assert (self);
    assert (self->coord_sys == CS_NONE); // only set once
    self->coord_sys = coord_sys;
}


size_t vrp_add_node (vrp_t *self, const char *ext_id, node_role_t role) {
    assert (self);

    s_node_t *node = s_node_new (ext_id);
    size_t id = arrayset_add (self->nodes, node, node->ext_id);

    if (id == ID_NONE) {
        print_error ("Node with external ID %s already exists.\n", ext_id);
        s_node_free (&node);
        return ID_NONE;
    }

    node->id = id;
    node->role = role;
    listu_append (self->node_ids, id);
    if (role == NR_DEPOT)
        listu_append (self->depot_ids, id);
    else if (role == NR_CUSTOMER)
        listu_append (self->customer_ids, id);

    return id;
}


void vrp_set_node_coord (vrp_t *self, size_t node_id, coord2d_t coord) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    node->coord.v1 = coord.v1;
    node->coord.v2 = coord.v2;
}


void vrp_set_arc_distance (vrp_t *self,
                           size_t from_node_id,
                           size_t to_node_id,
                           double distance) {
    assert (self);
    assert (distance >= 0);
    if (self->distances == NULL)
        self->distances = matrixd_new (2, 2);
    matrixd_set (self->distances, from_node_id, to_node_id, distance);
}


void vrp_set_arc_duration (vrp_t *self,
                           size_t from_node_id,
                           size_t to_node_id,
                           size_t duration) {
    assert (self);
    assert (duration >= 0);
    if (self->durations == NULL)
        self->durations = matrixu_new (2, 2);
    matrixu_set (self->durations, from_node_id, to_node_id, duration);
}


void vrp_generate_beeline_distances (vrp_t *self) {
    assert (self);

    size_t num_nodes = vrp_num_nodes (self);
    assert (num_nodes == listu_size (self->node_ids));
    size_t cnt1, cnt2;
    size_t id1, id2;
    double dist;

    for (cnt1 = 0; cnt1 < num_nodes; cnt1++) {
        for (cnt2 = 0; cnt2 < num_nodes; cnt2++) {
            id1 = listu_get (self->node_ids, cnt1);
            id2 = listu_get (self->node_ids, cnt2);
            if (cnt1 != cnt2)
                dist = coord2d_distance (vrp_node_coord (self, id1),
                                         vrp_node_coord (self, id2),
                                         self->coord_sys);
            else
                dist = 0;
            vrp_set_arc_distance (self, id1, id2, dist);
        }
    }
}


void vrp_generate_durations (vrp_t *self, double speed) {
    assert (self);
    assert (speed > 0);

    size_t num_nodes = vrp_num_nodes (self);
    size_t cnt1, cnt2;
    size_t id1, id2;
    double dist;

    for (cnt1 = 0; cnt1 < num_nodes; cnt1++) {
        for (cnt2 = 0; cnt2 < num_nodes; cnt2++) {
            id1 = listu_get (self->node_ids, cnt1);
            id2 = listu_get (self->node_ids, cnt2);
            dist = matrixd_get (self->distances, id1, id2);
            vrp_set_arc_duration (self, id1, id2, (int)(dist / speed));
        }
    }
}


coord2d_sys_t vrp_coord_sys (vrp_t *self) {
    assert (self);
    return self->coord_sys;
}


const char *vrp_node_ext_id (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    assert (node != NULL);
    return node->ext_id;
}


bool vrp_node_is_depot (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    assert (node != NULL);
    return node->role == NR_DEPOT;
}


bool vrp_node_is_customer (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    return node->role == NR_CUSTOMER;
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
    return &(node->coord);
}


size_t vrp_num_nodes (vrp_t *self) {
    assert (self);
    return arrayset_size (self->nodes);
}


size_t vrp_num_depots (vrp_t *self) {
    assert (self);
    return listu_size (self->depot_ids);
}


size_t vrp_num_customers (vrp_t *self) {
    assert (self);
    return listu_size (self->customer_ids);
}


const listu_t *vrp_nodes (vrp_t *self) {
    assert (self);
    return self->node_ids;
}


const listu_t *vrp_depots (vrp_t *self) {
    assert (self);
    return self->depot_ids;
}


const listu_t *vrp_customers (vrp_t *self) {
    assert (self);
    return self->customer_ids;
}


double vrp_arc_distance (vrp_t *self, size_t from_node_id, size_t to_node_id) {
    assert (self);
    return matrixd_get (self->distances, from_node_id, to_node_id);
}


size_t vrp_arc_duration (vrp_t *self, size_t from_node_id, size_t to_node_id) {
    assert (self);
    return matrixu_get (self->durations, from_node_id, to_node_id);
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


static bool vrp_vehicle_exists (vrp_t *self, size_t vehicle_id) {
    return arrayset_data (self->vehicles, vehicle_id) != NULL;
}


size_t vrp_add_vehicle (vrp_t *self,
                        const char *vehicle_ext_id,
                        double max_capacity,
                        size_t start_node_id,
                        size_t end_node_id) {
    assert (self);
    assert (vehicle_ext_id);
    assert (max_capacity > 0);
    assert (start_node_id == ID_NONE || vrp_node_exists (self, start_node_id));
    assert (end_node_id == ID_NONE || vrp_node_exists (self, end_node_id));

    s_vehicle_t *vehicle = s_vehicle_new (vehicle_ext_id);
    assert (vehicle);

    size_t id = arrayset_add (self->vehicles, vehicle, vehicle->ext_id);
    if (id == ID_NONE) {
        print_error ("vehicle with external ID %s already exists.\n", vehicle_ext_id);
        s_vehicle_free (&vehicle);
        return ID_NONE;
    }

    vehicle->id = id;
    vehicle->max_capacity = max_capacity;
    vehicle->start_node_id = start_node_id;
    vehicle->end_node_id = end_node_id;

    listu_append (self->vehicle_ids, id);
    return id;
}


size_t vrp_num_vehicles (vrp_t *self) {
    assert (self);
    return arrayset_size (self->vehicles);
}


const listu_t *vrp_vehicles (vrp_t *self) {
    assert (self);
    return self->vehicle_ids;
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


size_t vrp_vehicle_end_node_id (vrp_t *self, size_t vehicle_id) {
    assert (self);
    s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);
    return vehicle->end_node_id;
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


size_t vrp_add_request (vrp_t *self,
                        const char *request_ext_id,
                        size_t pickup_node,
                        size_t delivery_node,
                        double quantity) {
    assert (self);
    assert (request_ext_id); // @todo should ext_id be required?
    assert (pickup_node == ID_NONE ||
            vrp_node_exists (self, pickup_node));
    assert (delivery_node == ID_NONE ||
            vrp_node_exists (self, delivery_node));
    assert (pickup_node == ID_NONE ||
            delivery_node == ID_NONE ||
            pickup_node != delivery_node);
    assert (quantity >= 0);

    // Create a new request
    s_request_t *request = s_request_new (request_ext_id);
    size_t id = arrayset_add (self->requests, request, request->ext_id);
    request->id = id;

    // Set task
    request->pickup_node_id = pickup_node;
    request->delivery_node_id = delivery_node;
    if (pickup_node == ID_NONE || delivery_node == ID_NONE)
        request->type = RT_VISIT;
    else
        request->type = RT_PD;
    request->quantity = quantity;

    listu_append (self->pending_request_ids, id);
    return id;
}


void vrp_add_time_window_for_request (vrp_t *self,
                                      size_t request_id,
                                      bool is_pickup,
                                      size_t earliest,
                                      size_t latest) {
    assert (self);
    assert (earliest <= latest);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);

    if (is_pickup) {
        if (request->pickup_time_windows == NULL) {
            request->pickup_time_windows = listu_new (2);
            assert (request->pickup_time_windows);
        }
        listu_append (request->pickup_time_windows, earliest);
        listu_append (request->pickup_time_windows, latest);
    }
    else {
        if (request->delivery_time_windows == NULL) {
            request->delivery_time_windows = listu_new (2);
            assert (request->delivery_time_windows);
        }
        listu_append (request->delivery_time_windows, earliest);
        listu_append (request->delivery_time_windows, latest);
    }
}


void vrp_set_service_duration_for_request (vrp_t *self,
                                           size_t request_id,
                                           bool is_pickup,
                                           size_t service_duration) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    if (is_pickup)
        request->pickup_duration = service_duration;
    else
        request->delivery_duration = service_duration;
}


size_t vrp_num_pickup_time_windows_of_request (vrp_t *self, size_t request_id) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    if (request->pickup_time_windows != NULL)
        return listu_size (request->pickup_time_windows) / 2;
    else
        return 0;
}


size_t vrp_earliest_pickup_time_of_request (vrp_t *self,
                                             size_t request_id,
                                             size_t tw_idx) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request->pickup_time_windows);
    return listu_get (request->pickup_time_windows, tw_idx * 2);
}


size_t vrp_latest_pickup_time_of_request (vrp_t *self,
                                           size_t request_id,
                                           size_t tw_idx) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request->pickup_time_windows);
    return listu_get (request->pickup_time_windows, tw_idx * 2 + 1);
}


size_t vrp_num_delivery_time_windows_of_request (vrp_t *self, size_t request_id) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    if (request->delivery_time_windows != NULL)
        return listu_size (request->delivery_time_windows) /2;
    else
        return 0;
}


size_t vrp_earliest_delivery_time_of_request (vrp_t *self,
                                               size_t request_id,
                                               size_t tw_idx) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request->delivery_time_windows);
    return listu_get (request->delivery_time_windows, tw_idx * 2);
}


size_t vrp_latest_delivery_time_of_request (vrp_t *self,
                                             size_t request_id,
                                             size_t tw_idx) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request->delivery_time_windows);
    return listu_get (request->delivery_time_windows, tw_idx * 2 + 1);
}


size_t vrp_pickup_duration_of_request (vrp_t *self, size_t request_id) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    return request->pickup_duration;
}


size_t vrp_delivery_duration_of_request (vrp_t *self, size_t request_id) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    return request->delivery_duration;
}


size_t vrp_query_request (vrp_t *self, const char *request_ext_id) {
    assert (self);
    return arrayset_query (self->requests, request_ext_id);
}


size_t vrp_num_requests (vrp_t* self) {
    assert (self);
    return arrayset_size (self->requests);
}


const listu_t *vrp_pending_request_ids (vrp_t *self) {
    assert (self);
    return self->pending_request_ids;
}


size_t vrp_request_pickup_node (vrp_t *self, size_t request_id) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    return request->pickup_node_id;
}


size_t vrp_request_delivery_node (vrp_t *self, size_t request_id) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    return request->delivery_node_id;
}


double vrp_request_quantity (vrp_t *self, size_t request_id) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    return request->quantity;
}


// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Submodel dispatch and solve
// ---------------------------------------------------------------------------

// Validate that roadgraph are well defined
static bool vrp_validate_roadgraph (vrp_t *self) {
    size_t num_nodes = vrp_num_nodes (self);
    if (num_nodes == 0) {
        print_error ("No node exist.\n");
        return false;
    }

    bool coord_sys_is_defined = (vrp_coord_sys (self) != CS_NONE);
    for (size_t idx1 = 0; idx1 < num_nodes; idx1++) {
        size_t node_id1 = listu_get (self->node_ids, idx1);

        // All or none of node coordinates should be set
        if (coord_sys_is_defined &&
            coord2d_is_none (vrp_node_coord (self, idx1))) {
            print_error ("Coordinate of node (external ID %s) is not set.\n",
                         vrp_node_ext_id (self, node_id1));
            return false;
        }

        for (size_t idx2 = 0; idx2 < num_nodes; idx2++) {
            size_t node_id2 = listu_get (self->node_ids, idx2);

            // All or none of arc distances should be set
            if (self->distances != NULL) {
                double dist = vrp_arc_distance (self, node_id1, node_id2);
                if (dist == DOUBLE_NONE) {
                    print_error ("Distance from node %s to node %s is not set.\n",
                                 vrp_node_ext_id (self, node_id1),
                                 vrp_node_ext_id (self, node_id2));
                    return false;
                }
            }

            // All or none of arc durations should be set
            if (self->durations != NULL) {
                double duration = vrp_arc_duration (self, node_id1, node_id2);
                if (duration == DOUBLE_NONE) {
                    print_error ("Duration from node %s to node %s is not set.\n",
                                 vrp_node_ext_id (self, node_id1),
                                 vrp_node_ext_id (self, node_id2));
                    return false;
                }
            }
        }
    }
    return true;
}


// Validate that fleet are well defined
static bool vrp_validate_fleet (vrp_t *self) {
    // At least one vehicle is added
    if (vrp_num_vehicles (self) == 0) {
        print_info ("No vehicle exists.\n");
        return false;
    }

    return true;
}


// Validate that model is well defined
static bool vrp_validate (vrp_t *self) {
    return vrp_validate_roadgraph (self) &&
           vrp_validate_fleet (self);
}


// static bool vrp_fleet_is_homogeneous (vrp_t *self);


// static bool vrp_is_cvrp (vrp_t *self);


static bool vrp_is_tsp (vrp_t *self) {
    assert (self);

    // One vehicle
    if (vrp_num_vehicles (self) != 1)
        return false;

    // Check all requests
    for (s_request_t *request = arrayset_first (self->requests);
         request != NULL;
         request = arrayset_next (self->requests)) {

        // request type: visiting with zero quantity
        if (request->type != RT_VISIT || !double_equal (request->quantity, 0))
            return false;
        assert (!(request->pickup_node_id == ID_NONE &&
                  request->delivery_node_id == ID_NONE));

        // No time window constraints
        if (request->pickup_time_windows != NULL &&
            listu_size (request->pickup_time_windows) > 0)
            return false;
        if (request->delivery_time_windows != NULL &&
            listu_size (request->delivery_time_windows) > 0)
            return false;
    }

    return true;
}


solution_t *vrp_solve (vrp_t *self) {
    assert (self);

    if (!vrp_validate (self))
        return NULL;


    // @todo Shink the roadgraph according to requests?


    // Submodel inference
    // @todo improve this process: submodel inference by attributes

    solution_t *sol = NULL;

    if (vrp_is_tsp (self)) {
        print_info ("Submodel detected: TSP\n");
        tsp_t *model = tsp_new_from (self);
        assert (model);
        sol = tsp_solve (model);
        tsp_free (&model);
    }
    else
        print_error ("Unsupported model. Not solved.\n");
    return sol;
}


// ---------------------------------------------------------------------------
void vrp_test (bool verbose) {
    print_info (" * vrp: \n");

    char filename[] =
        // "benchmark/cvrp/A-n32-k5.vrp";
        "benchmark/tsplib/tsp/berlin52.tsp";

    vrp_t *vrp = vrp_new_from_file (filename);
    assert (vrp);

    printf ("#nodes: %zu\n", vrp_num_nodes (vrp));
    printf ("#vehicles: %zu\n", vrp_num_vehicles (vrp));

    vrp_solve (vrp);

    vrp_free (&vrp);
    print_info ("OK\n");
}
