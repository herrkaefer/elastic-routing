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

    // node_type_t type; // NT_DEPOT or NT_CUSTOMER
    coord2d_t coord; // 2D coordinates

    listu_t *pending_request_ids; // associated pending requests
    // ... other renferences

} s_node_t;


// Create a node
static s_node_t *s_node_new (const char *ext_id) {
    assert (ext_id);
    assert (strlen (ext_id) <= UUID_STR_LEN);

    s_node_t *self = (s_node_t *) malloc (sizeof (s_node_t));
    assert (self);

    self->id = ID_NONE;

    strcpy (self->ext_id, ext_id);
    coord2d_set_none (&self->coord);

    self->pending_request_ids = listu_new (1);
    assert (self->pending_request_ids);

    return self;
}


// Destroy a node
static void s_node_free (s_node_t **self_p) {
    assert (self_p);
    if (*self_p) {
        s_node_t *self = *self_p;
        listu_free (&self->pending_request_ids);
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

// request state is not deliverately considered yet
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


// RT_PD: pair-nodes task (quantity >= 0):
//     pickup at sender and delivery to receiver.
// RT_VISIT: single-node task (quantity >= 0).
//     One of sender and receiver is set, and the other is ID_NONE.
//     sender_id != ID_NONE and quantity > 0:
//     pickup (goods are always on vehicle after pickup);
//     receiver_id != ID_NONE and quantity > 0:
//     delivery (goods are already on vehicle)
//     quantity == 0: visiting node (sender or receiver) with no goods
typedef enum {
    RT_NONE,
    RT_PD,
    RT_VISIT
} request_type_t;


typedef struct {
    size_t id;
    char ext_id[UUID_STR_LEN]; // external ID connecting request to outside task

    request_type_t type;

    request_state_t state;

    // Task: pickup at sender, deliver at receiver
    size_t sender_id;
    size_t receiver_id;
    double quantity; // >= 0

    // Note that time windows and service durations are associated with
    // request rather than node, which is more natural in practice.
    // Multiple time windows are supported and sorted in ascending order:
    // (etw1, ltw1, etw2, ltw2, ...)
    // The value represents number of time units since a common reference point.
    listu_t *pickup_time_windows;
    listu_t *delivery_time_windows;

    // Service duration
    size_t pickup_duration; // service duration for pickup (num of time units)
    size_t delivery_duration; // service duration for delivery (num of time units)

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

    self->sender_id = ID_NONE;
    self->receiver_id = ID_NONE;
    self->quantity = 0;

    self->pickup_time_windows = listu_new (2);
    assert (self->pickup_time_windows);
    listu_sort (self->pickup_time_windows, true);
    self->delivery_time_windows = listu_new (2);
    assert (self->delivery_time_windows);
    listu_sort (self->delivery_time_windows, true);

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
        listu_free (&self->pickup_time_windows);
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

    rng_t *rng;

    // Node IDs in roadgraph, sorted ascendingly
    listu_t *node_ids;
    // Vehicle IDs, sorted ascendingly
    listu_t *vehicle_ids;
    // IDs of nodes which play a role of sender in request, sorted ascendingly
    listu_t *sender_ids;
    // IDs of nodes which play a role of receiver in request, sorted ascendingly
    listu_t *receiver_ids;
    // IDs of requests which state is pending, sorted ascendingly
    listu_t *pending_request_ids;
};


// ---------------------------------------------------------------------------
// Constructor and destructor
// ---------------------------------------------------------------------------


vrp_t *vrp_new (void) {
    vrp_t *self = (vrp_t *) malloc (sizeof (vrp_t));
    assert (self);

    // Roadgraph
    self->nodes = arrayset_new (0);
    arrayset_set_data_destructor (self->nodes, (destructor_t) s_node_free);
    arrayset_set_hash (self->nodes,
                       (hashfunc_t) string_hash,
                       (matcher_t) string_equal,
                       NULL);

    self->distances = NULL; // lazy creation
    self->durations = NULL;
    self->coord_sys = CS_NONE;

    // Fleet
    self->vehicles = arrayset_new (1);
    arrayset_set_data_destructor (self->vehicles, (destructor_t) s_vehicle_free);
    arrayset_set_hash (self->vehicles,
                       (hashfunc_t) string_hash,
                       (matcher_t) string_equal,
                       NULL);

    // Requests
    self->requests = arrayset_new (1);
    arrayset_set_data_destructor (self->requests, (destructor_t) s_request_free);
    arrayset_set_hash (self->requests,
                       (hashfunc_t) string_hash,
                       (matcher_t) string_equal,
                       NULL);

    // Constraints
    self->max_route_distance = DOUBLE_MAX;
    self->max_route_duration = SIZE_MAX;

    // Auxiliaries
    self->rng = rng_new ();
    self->node_ids = listu_new (0);
    listu_sort (self->node_ids, true);
    self->vehicle_ids = listu_new (0);
    listu_sort (self->vehicle_ids, true);
    self->sender_ids = listu_new (0);
    listu_sort (self->sender_ids, true);
    self->receiver_ids = listu_new (0);
    listu_sort (self->receiver_ids, true);
    self->pending_request_ids = listu_new (0);
    listu_sort (self->pending_request_ids, true);

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
        rng_free (&self->rng);
        listu_free (&self->node_ids);
        listu_free (&self->vehicle_ids);
        listu_free (&self->sender_ids);
        listu_free (&self->receiver_ids);
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

        if (edge_weight_type == EUC_2D && coords != NULL)
            vrp_set_coord_sys (self, CS_CARTESIAN2D);

        // Add nodes, arc distances, and requests
        for (size_t cnt = 0; cnt < num_nodes; cnt++) {
            sprintf (ext_id, "node-%04zu", cnt + 1);
            node_id = vrp_add_node (self, ext_id);
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
                if (cnt1 == cnt)
                    vrp_set_arc_distance (self, cnt1, cnt, 0);
                else {
                    vrp_set_arc_distance (self, cnt1, cnt,
                                          matrixd_get (costs, cnt1, cnt));
                    vrp_set_arc_distance (self, cnt, cnt1,
                                          matrixd_get (costs, cnt, cnt1));
                }
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
static s_node_t *vrp_node (const vrp_t *self, size_t node_id) {
    s_node_t *node = (s_node_t *) arrayset_data (self->nodes, node_id);
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

    if (id == ID_NONE) {
        print_error ("Node with external ID %s already exists.\n", ext_id);
        s_node_free (&node);
        return ID_NONE;
    }

    node->id = id;
    assert (!listu_includes (self->node_ids, id));
    listu_insert_sorted (self->node_ids, id);
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


size_t vrp_query_node (vrp_t *self, const char *node_ext_id) {
    assert (self);
    return arrayset_query (self->nodes, node_ext_id);
}


bool vrp_node_exists (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    return node != NULL;
}


const char *vrp_node_ext_id (const vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    assert (node != NULL);
    return node->ext_id;
}


const coord2d_t *vrp_node_coord (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    return &(node->coord);
}


const listu_t *vrp_node_pending_request_ids (vrp_t *self, size_t node_id) {
    assert (self);
    s_node_t *node = vrp_node (self, node_id);
    assert (node);
    return node->pending_request_ids;
}


size_t vrp_num_nodes (vrp_t *self) {
    assert (self);
    return arrayset_size (self->nodes);
}


const listu_t *vrp_nodes (vrp_t *self) {
    assert (self);
    return self->node_ids;
}


double vrp_arc_distance (const vrp_t *self,
                         size_t from_node_id, size_t to_node_id) {
    assert (self);
    assert (self->distances != NULL);
    return matrixd_get (self->distances, from_node_id, to_node_id);
}


size_t vrp_arc_duration (const vrp_t *self,
                         size_t from_node_id, size_t to_node_id) {
    assert (self);
    assert (self->durations != NULL);
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
    vehicle->capacity = max_capacity;
    vehicle->start_node_id = start_node_id;
    vehicle->end_node_id = end_node_id;

    assert (!listu_includes (self->vehicle_ids, id));
    listu_insert_sorted (self->vehicle_ids, id);
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


static void vrp_associate_node_with_new_request (vrp_t *self,
                                                 size_t node_id,
                                                 size_t request_id) {
    s_node_t *node = vrp_node (self, node_id);
    assert (node);
    assert (!listu_includes (node->pending_request_ids, request_id));
    listu_append (node->pending_request_ids, request_id);
}


size_t vrp_add_request (vrp_t *self,
                        const char *request_ext_id,
                        size_t sender,
                        size_t receiver,
                        double quantity) {
    assert (self);
    assert (request_ext_id); // @todo should ext_id be required?
    assert (sender == ID_NONE ||
            vrp_node_exists (self, sender));
    assert (receiver == ID_NONE ||
            vrp_node_exists (self, receiver));

    // At least one of sender and reciver should be specified
    assert (sender != ID_NONE || receiver != ID_NONE);
    assert (sender != receiver);
    assert (quantity >= 0);

    // Create a new request
    s_request_t *request = s_request_new (request_ext_id);
    size_t id = arrayset_add (self->requests, request, request->ext_id);
    request->id = id;

    // Set task
    request->sender_id = sender;
    request->receiver_id = receiver;
    if (sender == ID_NONE || receiver == ID_NONE)
        request->type = RT_VISIT;
    else
        request->type = RT_PD;
    request->quantity = quantity;

    // Associate nodes with request
    if (sender != ID_NONE) {
        vrp_associate_node_with_new_request (self, sender, id);
        if (!listu_includes (self->sender_ids, sender))
            listu_insert_sorted (self->sender_ids, sender);
    }
    if (receiver != ID_NONE) {
        vrp_associate_node_with_new_request (self, receiver, id);
        if (!listu_includes (self->receiver_ids, receiver))
            listu_insert_sorted (self->receiver_ids, receiver);
    }

    assert (!listu_includes (self->pending_request_ids, id));
    listu_insert_sorted (self->pending_request_ids, id);
    return id;
}


int vrp_add_time_window (vrp_t *self,
                          size_t request_id,
                          node_role_t node_role,
                          size_t earliest,
                          size_t latest) {
    assert (self);
    assert (earliest <= latest);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    assert (node_role == NR_SENDER || node_role == NR_RECEIVER);

    listu_t *tws = (node_role == NR_SENDER) ?
                   request->pickup_time_windows :
                   request->delivery_time_windows;

    size_t idx_e = listu_insert_sorted (tws, earliest);
    size_t idx_l = listu_insert_sorted (tws, latest);
    if (idx_e % 2 != 0 || (idx_e + 1 != idx_l)) {
        listu_remove_at (tws, idx_l);
        listu_remove_at (tws, idx_e);
        return -1;
    }

    return 0;
}


void vrp_set_service_duration (vrp_t *self,
                               size_t request_id,
                               node_role_t node_role,
                               size_t service_duration) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    assert (node_role == NR_SENDER || node_role == NR_RECEIVER);
    if (node_role == NR_SENDER)
        request->pickup_duration = service_duration;
    else
        request->delivery_duration = service_duration;
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


size_t vrp_num_senders (vrp_t *self) {
    assert (self);
    return listu_size (self->sender_ids);
}


size_t vrp_num_receivers (vrp_t *self) {
    assert (self);
    return listu_size (self->receiver_ids);
}


const listu_t *vrp_senders (vrp_t *self) {
    assert (self);
    return self->sender_ids;
}


const listu_t *vrp_receivers (vrp_t *self) {
    assert (self);
    return self->receiver_ids;
}


size_t vrp_request_sender (vrp_t *self, size_t request_id) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    return request->sender_id;
}


size_t vrp_request_receiver (vrp_t *self, size_t request_id) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    return request->receiver_id;
}


double vrp_request_quantity (vrp_t *self, size_t request_id) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    return request->quantity;
}


size_t vrp_num_time_windows (vrp_t *self,
                             size_t request_id,
                             node_role_t node_role) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    assert (node_role == NR_SENDER || node_role == NR_RECEIVER);
    listu_t *tws = (node_role == NR_SENDER) ?
                   request->pickup_time_windows :
                   request->delivery_time_windows;
    return (tws != NULL) ? listu_size (tws) / 2 : 0;
}


size_t vrp_earliest_of_time_window (vrp_t *self,
                                    size_t request_id,
                                    node_role_t node_role,
                                    size_t tw_idx) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    assert (node_role == NR_SENDER || node_role == NR_RECEIVER);
    assert (tw_idx < vrp_num_time_windows (self, request_id, node_role));

    listu_t *tws = (node_role == NR_SENDER) ?
                   request->pickup_time_windows :
                   request->delivery_time_windows;
    assert (tws);
    return listu_get (tws, tw_idx * 2);
}


size_t vrp_latest_of_time_window (vrp_t *self,
                                  size_t request_id,
                                  node_role_t node_role,
                                  size_t tw_idx) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    assert (node_role == NR_SENDER || node_role == NR_RECEIVER);
    assert (tw_idx < vrp_num_time_windows (self, request_id, node_role));

    listu_t *tws = (node_role == NR_SENDER) ?
                   request->pickup_time_windows :
                   request->delivery_time_windows;
    assert (tws);
    return listu_get (tws, tw_idx * 2 + 1);
}


size_t vrp_earliest_service_time (vrp_t *self,
                                  size_t request_id,
                                  node_role_t node_role) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    assert (node_role == NR_SENDER || node_role == NR_RECEIVER);

    listu_t *tws = (node_role == NR_SENDER) ?
                   request->pickup_time_windows :
                   request->delivery_time_windows;
    assert (tws);

    size_t num_tws = vrp_num_time_windows (self, request_id, node_role);
    if (num_tws == 0)
        return 0;

    size_t earliest_st = SIZE_MAX;
    for (size_t idx_tw = 0; idx_tw < num_tws; idx_tw++) {
        size_t earliest_of_tw =
            vrp_earliest_of_time_window (self, request_id, node_role, idx_tw);
        if (earliest_of_tw < earliest_st)
            earliest_st = earliest_of_tw;
    }
    return earliest_st;
}


size_t vrp_latest_service_time (vrp_t *self,
                                size_t request_id,
                                node_role_t node_role) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    assert (node_role == NR_SENDER || node_role == NR_RECEIVER);

    listu_t *tws = (node_role == NR_SENDER) ?
                   request->pickup_time_windows :
                   request->delivery_time_windows;
    assert (tws);

    size_t num_tws = vrp_num_time_windows (self, request_id, node_role);
    if (num_tws == 0)
        return SIZE_MAX;

    size_t latest_st = 0;
    for (size_t idx_tw = 0; idx_tw < num_tws; idx_tw++) {
        size_t latest_of_tw =
            vrp_latest_of_time_window (self, request_id, node_role, idx_tw);
        if (latest_of_tw > latest_st)
            latest_st = latest_of_tw;
    }
    return latest_st;
}


size_t vrp_service_duration (vrp_t *self,
                             size_t request_id,
                             node_role_t node_role) {
    assert (self);
    s_request_t *request = vrp_request (self, request_id);
    assert (request);
    assert (node_role == NR_SENDER || node_role == NR_RECEIVER);

    return (node_role == NR_SENDER) ?
           request->pickup_duration :
           request->delivery_duration;
}


bool vrp_time_windows_are_equal (vrp_t *self,
                                 size_t request_id1, node_role_t node_role1,
                                 size_t request_id2, node_role_t node_role2) {
    assert (self);
    s_request_t *request1 = vrp_request (self, request_id1);
    assert (request1);
    assert (node_role1 == NR_SENDER || node_role1 == NR_RECEIVER);
    s_request_t *request2 = vrp_request (self, request_id2);
    assert (request2);
    assert (node_role2 == NR_SENDER || node_role2 == NR_RECEIVER);

    listu_t *tws1 = (node_role1 == NR_SENDER) ?
                    request1->pickup_time_windows :
                    request1->delivery_time_windows;
    listu_t *tws2 = (node_role2 == NR_SENDER) ?
                    request2->pickup_time_windows :
                    request2->delivery_time_windows;
    return listu_equal (tws1, tws2);
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
    assert (num_nodes == listu_size (self->node_ids));

    bool coord_sys_is_defined = (vrp_coord_sys (self) != CS_NONE);

    for (size_t idx1 = 0; idx1 < num_nodes; idx1++) {
        size_t node_id1 = listu_get (self->node_ids, idx1);

        // All or none of node coordinates should be set
        if (coord_sys_is_defined &&
            coord2d_is_none (vrp_node_coord (self, node_id1))) {
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

    // @todo others ...

    return true;
}


// @todo not good, to rewrite or drop !!!
static bool vrp_validate_requests (vrp_t *self) {
    size_t num_requests = listu_size (self->pending_request_ids);
    if (num_requests == 0)
        return false;

    // Check pending requests
    for (size_t idx = 0; idx < num_requests; idx++) {

        size_t request_id = listu_get (self->pending_request_ids, idx);
        s_request_t *request = vrp_request (self, request_id);

        size_t num_ptw =
            vrp_num_time_windows (self, request_id, NR_SENDER);
        size_t num_dtw =
            vrp_num_time_windows (self, request_id, NR_RECEIVER);

        if ((num_ptw > 0 || num_dtw > 0) && self->durations == NULL) {
            print_error ("Arc duration of roadgraph is not set.\n");
            return false;
        }

        if (num_ptw > 0 && num_dtw > 0) {
            size_t earliest_pickup_time =
                vrp_earliest_of_time_window (self, request_id, NR_SENDER, 0);
            size_t latest_delivery_time =
                vrp_latest_of_time_window (self, request_id, NR_RECEIVER, num_dtw-1);
            size_t arc_duration =
                vrp_arc_duration (self,
                                  vrp_request_sender (self, request_id),
                                  vrp_request_receiver (self, request_id));
            size_t pickup_duration =
                vrp_service_duration (self, request_id, NR_SENDER);

            if (earliest_pickup_time + pickup_duration + arc_duration >
                latest_delivery_time) {
                print_error (
                    "Time windows constraints cannot be satisfied for request %s.\n",
                    request->ext_id);
                return false;
            }
        }
    }

    return true;
}


// Validate that model is well defined
static bool vrp_validate (vrp_t *self) {
    return vrp_validate_roadgraph (self) &&
           vrp_validate_fleet (self) &&
           vrp_validate_requests (self);
}


typedef struct {
    bool arc_distances_defined;
    bool arc_durations_defined;

    bool single_sender;
    bool single_receiver;
    bool requests_are_all_pickup_and_delivery;
    bool requests_are_all_visiting_without_goods;
    bool time_windows_defined;
    bool time_windows_are_same_for_single_sender; // relevant if single_sender is true
    bool time_windows_are_same_for_single_receiver; // relevant if single_receiver is true

    bool single_vehicle;
    bool vehicles_start_at_same_node;
    bool vehicles_end_at_same_node;
    bool vehicles_have_same_capacity;
    bool vehicles_start_at_single_sender; // relevant if single_sender is true
    bool vehicles_end_at_single_sender; // relevant if single_sender is true
    bool vehicles_start_at_single_receiver; // relevant if single_receiver is true
    bool vehicles_end_at_single_receiver; // relevant if single_receiver is true
} s_attributes_t;


static s_attributes_t vrp_collect_attributes (vrp_t *self) {
    s_attributes_t attr;

    // Roadgraph

    attr.arc_distances_defined = (self->distances != NULL);
    attr.arc_durations_defined = (self->durations != NULL);

    // Requests

    attr.single_sender = true;
    attr.single_receiver = true;
    attr.requests_are_all_pickup_and_delivery = true;
    attr.requests_are_all_visiting_without_goods = true;
    attr.time_windows_defined = false;
    attr.time_windows_are_same_for_single_sender = true;
    attr.time_windows_are_same_for_single_receiver = true;

    size_t num_requests = listu_size (self->pending_request_ids);
    assert (num_requests > 0);

    size_t single_sender_id = ID_NONE;
    size_t single_receiver_id = ID_NONE;
    size_t first_request_id = ID_NONE;

    for (size_t idx = 0; idx < num_requests; idx++) {
        size_t request_id = listu_get (self->pending_request_ids, idx);
        if (first_request_id == ID_NONE)
            first_request_id = request_id;
        s_request_t *request = vrp_request (self, request_id);

        // request type
        if (request->type != RT_PD)
            attr.requests_are_all_pickup_and_delivery = false;
        if (request->type != RT_VISIT || request->quantity > 0)
            attr.requests_are_all_visiting_without_goods = false;

        // sender and receiver node
        if (single_sender_id == ID_NONE)
            single_sender_id = request->sender_id;
        if (single_sender_id != request->sender_id)
            attr.single_sender = false;

        if (single_receiver_id == ID_NONE)
            single_receiver_id = request->receiver_id;
        if (single_receiver_id != request->receiver_id)
            attr.single_receiver = false;

        // time windows
        if (vrp_num_time_windows (self, request_id, NR_SENDER) > 0 ||
            vrp_num_time_windows (self, request_id, NR_RECEIVER) > 0)
            attr.time_windows_defined = true;

        if (attr.single_sender &&
            attr.time_windows_defined &&
            request_id != first_request_id) {
            if (!vrp_time_windows_are_equal (self,
                                             first_request_id, NR_SENDER,
                                             request_id, NR_SENDER))
                attr.time_windows_are_same_for_single_sender = false;
        }

        if (attr.single_receiver &&
            attr.time_windows_defined &&
            request_id != first_request_id) {
            if (!vrp_time_windows_are_equal (self,
                                             first_request_id, NR_RECEIVER,
                                             request_id, NR_RECEIVER))
                attr.time_windows_are_same_for_single_receiver = false;
        }
    }

    // Vehicles

    size_t num_vehicles = listu_size (self->vehicle_ids);
    assert (num_vehicles > 0);
    assert (num_vehicles == vrp_num_vehicles (self));
    attr.single_vehicle = (num_vehicles == 1);
    attr.vehicles_have_same_capacity = true;
    attr.vehicles_start_at_same_node = true;
    attr.vehicles_end_at_same_node = true;
    attr.vehicles_start_at_single_sender = false;
    attr.vehicles_end_at_single_sender = false;
    attr.vehicles_start_at_single_receiver = false;
    attr.vehicles_end_at_single_receiver = false;

    double capacity = DOUBLE_NONE;
    size_t start_node_id = ID_NONE;
    size_t end_node_id = ID_NONE;

    for (size_t idx = 0; idx < num_vehicles; idx++) {
        size_t vehicle_id = listu_get (self->vehicle_ids, idx);
        s_vehicle_t *vehicle = vrp_vehicle (self, vehicle_id);

        // capacity
        if (double_is_none (capacity))
            capacity = vehicle->capacity;
        if (!double_equal (capacity, vehicle->capacity))
            attr.vehicles_have_same_capacity = false;

        // start and end node
        if (start_node_id == ID_NONE && vehicle->start_node_id != ID_NONE)
            start_node_id = vehicle->start_node_id;
        if (start_node_id != ID_NONE && vehicle->start_node_id != start_node_id)
            attr.vehicles_start_at_same_node = false;
        if (end_node_id == ID_NONE && vehicle->end_node_id != ID_NONE)
            end_node_id = vehicle->end_node_id;
        if (end_node_id != ID_NONE && vehicle->end_node_id != end_node_id)
            attr.vehicles_end_at_same_node = false;
    }

    // for single linehaul depot case
    if (attr.single_sender) {
        if (attr.vehicles_start_at_same_node &&
            (start_node_id == ID_NONE || start_node_id == single_sender_id))
            attr.vehicles_start_at_single_sender = true;
        if (attr.vehicles_end_at_same_node && end_node_id == single_sender_id)
            attr.vehicles_end_at_single_sender = true;
    }

    // for single backhaul depot case
    if (attr.single_receiver) {
        if (attr.vehicles_start_at_same_node &&
            start_node_id == single_receiver_id)
            attr.vehicles_start_at_single_receiver = true;
        if (attr.vehicles_end_at_same_node &&
            (end_node_id == ID_NONE || end_node_id == single_receiver_id))
            attr.vehicles_end_at_single_receiver = true;
    }

    return attr;
}


static bool vrp_is_tsp (const vrp_t *self, const s_attributes_t *attr) {
    if (attr->arc_distances_defined &&
        attr->single_vehicle &&
        attr->requests_are_all_visiting_without_goods &&
        !attr->time_windows_defined)
        return true;
    return false;
}


static bool vrp_is_cvrp (const vrp_t *self, const s_attributes_t *attr) {
    if (attr->arc_distances_defined &&
        attr->requests_are_all_pickup_and_delivery &&
        !attr->time_windows_defined &&
        attr->single_sender &&
        attr->vehicles_start_at_single_sender &&
        attr->vehicles_end_at_single_sender)
        return true;
    return false;
}


static bool vrp_is_vrptw (const vrp_t *self, const s_attributes_t *attr) {
    if (attr->arc_distances_defined &&
        attr->arc_durations_defined &&
        attr->requests_are_all_pickup_and_delivery &&
        attr->time_windows_defined &&
        attr->single_sender &&
        attr->vehicles_start_at_single_sender &&
        attr->vehicles_end_at_single_sender)
        return true;
    return false;
}


solution_t *vrp_solve (vrp_t *self) {
    assert (self);

    if (!vrp_validate (self)) {
        print_error ("Model validation failed.\n");
        return NULL;
    }

    s_attributes_t attr = vrp_collect_attributes (self);

    // Dispatch submodel based on attributes
    solution_t *sol = NULL;
    if (vrp_is_tsp (self, &attr)) {
        print_info ("Submodel detected: TSP\n");
        tsp_t *model = tsp_new_from_generic (self);
        assert (model);
        sol = tsp_solve (model);
        tsp_free (&model);
    }
    else if (vrp_is_cvrp (self, &attr)) {
        print_info ("Submodel detected: CVRP\n");
        cvrp_t *model = cvrp_new_from_generic (self);
        assert (model);
        sol = cvrp_solve (model);
        cvrp_free (&model);
    }
    else if (vrp_is_vrptw (self, &attr)) {
        print_info ("Submodel detected: VRPTW\n");
        vrptw_t *model = vrptw_new_from_generic (self);
        assert (model);
        sol = vrptw_solve (model);
        vrptw_free (&model);
    }
    else
        print_error ("Unsupported model. Not solved.\n");

    return sol;
}


// ---------------------------------------------------------------------------
void vrp_test (bool verbose) {
    print_info (" * vrp: \n");

    char filename[] =
        "benchmark/cvrp/A-n32-k5.vrp";
        // "benchmark/tsplib/tsp/berlin52.tsp";
    // "benchmark/tsplib/tsp/a280.tsp";

    vrp_t *vrp = vrp_new_from_file (filename);
    assert (vrp);

    printf ("#nodes: %zu\n", vrp_num_nodes (vrp));
    printf ("#vehicles: %zu\n", vrp_num_vehicles (vrp));

    solution_t *sol = vrp_solve (vrp);
    if (sol != NULL)
        solution_print (sol);

    vrp_free (&vrp);
    print_info ("OK\n");
}
