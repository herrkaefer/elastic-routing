/*  =========================================================================
    cvrp - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


struct _cvrp_t {
    size_t num_vehicles;  // number of vehicles expected to be used (K)
    size_t num_customers; // number of customers (N)
    double capacity;  // vehicle capacity (C)
    size_t *node_ids; // nodes id array. 0: depot; 1~num_customers: customer
    double *demands; // customer demands array (q).
                     // size: (N+1). q[i], i=0,1,...,N. q[0]=0.
    // matrixd *costs; // cost matrix (c).
    // coord2d_t *coords; // coordinates of depot and customers.
    roadgraph_t *roadgraph;
};


//  --------------------------------------------------------------------------
// Model parameters validation
// static bool cvrp_validate_params (int num_customers, const double *demands,
//                     double capacity, int num_vehicles, const double *costs,
//                     const coord2d_t *coords) {
//     if (num_customers <= 0) {
//         print_error ("Number of customers should be positive.\n");
//         return false;
//     }

//     if (capacity <= 0) {
//         print_error ("Vehicle capacity should be positive.\n");
//         return false;
//     }

//     if (num_vehicles < 0) {
//         print_error ("Number of vehicles is negative.\n");
//         return false;
//     }

//     if (demands == NULL) {
//         print_error ("Customer demands not provided.\n");
//         return false;
//     }

//     if (costs == NULL) {
//         print_error ("road costs not provided.\n");
//         return false;
//     }

//     int node1, node2;
//     double cost;
//     for (node1 = 0; node1 <= num_customers; node1++) {
//         if (node1 == 0) {
//             if (!double_equal (demands[0], 0)) {
//                 print_error ("Demand of depot should be zero.\n");
//                 return false;
//             }
//         }
//         else if (demands[node1] <= 0) {
//             print_error ("Demand of customer should be positive.\n");
//             return false;
//         }
//         for (node2 = 0; node2 <= num_customers; node2++) {
//             cost = costs[(num_customers+1) *node1+node2];
//             if (node1 == node2) {
//                 if (!double_equal (cost, 0)) {
//                     print_error ("Cost of node %d to itself is not zero.\n", node1);
//                     return false;
//                 }
//             }
//             else if (cost < 0) {
//                 print_error ("Cost from node %d to node %d is negative.\n", node1, node2);
//                 return false;
//             }
//         }
//     }

//     return true;
// }


// ---------------------------------------------------------------------------
cvrp_t *cvrp_new () {
    cvrp_t *self = (cvrp_t *) malloc (sizeof (cvrp_t));
    assert (self);
    self->num_vehicles = 0;
    self->num_customers = 0;
    self->capacity = 0;
    self->node_ids = NULL;
    self->demands = NULL;
    self->roadgraph = NULL;
    print_info ("cvrp created.\n");
    return self;
}


void cvrp_set_from_generic (cvrp_t *self, const vrp_t *vrp) {
    assert (self);

    self->num_vehicles = fleet_size (vrp->fleet);
    self->capacity = fleet_vehicle_capacity (vrp->fleet);
    assert (self->capacity >= 0);

    self->num_customers = requests_num_customers (vrp->requests);

    self->node_ids =
        (size_t *) malloc (sizeof (size_t) * (self->num_customers + 1));
    assert (self->node_ids);

    size_t *depot_ids = requests_depot_ids (vrp->requests);
    assert (request_num_depots (vrp->requests) == 1);
    self->node_ids[0] = depot_ids[0];
    free (depot_ids);

    size_t *customer_ids = requests_customer_ids (vrp->requests);
    assert (request_num_customers (vrp->requests) == self->num_customers);
    memcpy (self->node_ids + 1, customer_ids,
            sizeof (size_t) * self->num_customers);
    free (customer_ids);

    self->demands =
        (double *) malloc (sizeof (double) * (self->num_customers + 1));
    assert (self->demands);
    self->demands[0] = 0;

    assert (requests_customers_are_all_deliveries (vrp->requests));
    double *customer_demands = requests_customer_deliveries (vrp->requests);
    assert (customer_demands != NULL);
    memcpy (self->demands + 1, customer_demands,
            sizeof (double) * self->num_customers);
    free (customer_demands);

    self->roadgraph = roadgraph;
}


//////////////////////////////////////////////////////////////////////////////

// cvrp_t *cvrp_new (int num_customers, const double *demands,
//                     double capacity, int num_vehicles, const double *costs,
//                     const coord2d_t *coords) {
//     cvrp_t *self = NULL;
//     unsigned char *p = NULL;
//     int sz[4];

//     if (!cvrp_validate_params (num_customers, demands,
//             capacity, num_vehicles, costs, coords)) {
//         puts ("ERROR: cvrp_new: invalid input found.\n");
//         return NULL;
//     }

//     sz[0] = sizeof (cvrp_t);
//     sz[1] = sz[0] + (num_customers+1) * sizeof (double); // q
//     sz[2] = sz[1] + (num_customers+1) * (num_customers+1) * sizeof (double); // c
//     sz[3] = (coords != NULL) ? (sz[2] + (num_customers+1) * sizeof (coord2d_t)) : sz[2]; // coordinates

//     self = (cvrp_t *) malloc (sz[3]);
//     assert (self);

//     p = (unsigned char *) self;

//     self->K = num_vehicles;
//     self->N = num_customers;
//     self->Q = capacity;
//     self->q = (double *)(p + sz[0]);
//     memcpy (self->q, demands, sz[1]-sz[0]);
//     self->c = (double *)(p + sz[1]);
//     memcpy (self->c, costs, sz[2]-sz[1]);
//     if (coords != NULL) {
//         self->coords = (coord2d_t *)(p + sz[2]);
//         memcpy (self->coords, coords, sz[3]-sz[2]);
//     }
//     else
//         self->coords = NULL;

//     puts ("cvrp created.");
//     return self;
// }



//  --------------------------------------------------------------------------
//  Create and initialize cvrp object from .vrp file
cvrp_t *cvrp_new_from_file (const char *filename)
{
    enum {DATA_SECTION_NONE, EDGE_WEIGHT_SECTION, NODE_COORD_SECTION, DEMAND_SECTION, DEPOT_SECTION} data_section = DATA_SECTION_NONE;
    enum {EDGE_WEIGHT_TYPE_NONE, EUC_2D, EXPLICIT} edge_weight_type = EDGE_WEIGHT_TYPE_NONE;
    enum {EDGE_WEIGHT_FORMAT_NONE, LOWROW} edge_weight_format = EDGE_WEIGHT_FORMAT_NONE;

    cvrp_t *self = NULL;
    int K;
    int N;
    double Q;
    double *q = NULL;
    double *c = NULL;
    coord2d_t *coords = NULL;

    const int LINE_LEN = 128;
    char line[LINE_LEN];
    const char *p = NULL;

    int node_index1 = 0, node_index2 = 0;
    int value1, value2; // double?

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
            puts ("WARNING: cvrp_new_from_file: no vehicle number specified in the filename. K is set to 0.");
            K = 0;
        }
    }
    else if (p != NULL && (strcmp (p+1, "txt") == 0 || strcmp (p+1, "TXT") == 0)) {
        puts ("ERROR: cvrp_new_from_file: file format .txt is not supported yet.");
        return NULL;
    }
    else {
        puts ("ERROR: cvrp_new_from_file: unrecognized input file.");
        return NULL;
    }

    // read file content
    while (error_flag == 0 && fgets (line, LINE_LEN, vrpfile) != NULL) {

        p = strchr (line, ':');

        if (p != NULL) { // specification part

            data_section = DATA_SECTION_NONE;

            if (strncmp (line, "DIMENSION", sizeof ("DIMENSION")-1) == 0) {
                N = strtol (p+1, NULL, 10) - 1;
                q = (double *) malloc ((N+1) * sizeof (double));
                assert (q);
                c = (double *) malloc ((N+1) * (N+1) * sizeof (double));
                assert (c);
            }

            else if (strncmp (line, "CAPACITY", sizeof ("CAPACITY")-1) == 0)
                Q = strtod (p+1, NULL);

            else if (strncmp (line, "EDGE_WEIGHT_TYPE", sizeof ("EDGE_WEIGHT_TYPE")-1) == 0) {
                if (strstr (p+1, "EUC_2D") != NULL)
                    edge_weight_type = EUC_2D;
                else if (strstr(p+1, "EXPLICIT") != NULL)
                    edge_weight_type = EXPLICIT;
                else {
                    puts ("ERROR: cvrp_new_from_file: unsupported EDGE_WEIGHT_TYPE.");
                    error_flag = 1;
                }
            }

            else if (strncmp (line, "EDGE_WEIGHT_FORMAT", sizeof ("EDGE_WEIGHT_FORMAT")-1) == 0) {
                if (strstr (p+1, "LOWROW") != NULL)
                    edge_weight_format = LOWROW;
                else {
                    puts ("ERROR: cvrp_new_from_file: unsupported EDGE_WEIGHT_FORMAT.");
                    error_flag = 1;
                }
            }
        }

        else if (strstr (line, "SECTION") != NULL) { // start of data part
            if (strncmp (line, "NODE_COORD_SECTION", sizeof ("NODE_COORD_SECTION")-1) == 0) {
                data_section = NODE_COORD_SECTION;

                // malloc memory of coordinates
                coords = (coord2d_t *) malloc ((N+1) * sizeof (coord2d_t));
                assert (coords);
            }
            else if (strncmp (line, "DEMAND_SECTION", sizeof ("DEMAND_SECTION")-1) == 0)
                data_section = DEMAND_SECTION;
            else if (strncmp (line, "EDGE_WEIGHT_SECTION", sizeof ("EDGE_WEIGHT_SECTION")-1) == 0)
                data_section = EDGE_WEIGHT_SECTION;
            else
                data_section = DATA_SECTION_NONE;
        }

        else { // data
            switch (data_section) {
                case NODE_COORD_SECTION:
                    sscanf (line, "%d %d %d", &node_index1, &value1, &value2);
                    coords[node_index1-1].x = (double)value1;
                    coords[node_index1-1].y = (double)value2;
                    break;

                case DEMAND_SECTION:
                    sscanf (line, "%d %d", &node_index1, &value1);
                    q[node_index1-1] = (double)value1;
                    // PRINT("index: %d value1: %d\n", node_index1, value1);
                    break;

                case EDGE_WEIGHT_SECTION:
                    if (edge_weight_type == EXPLICIT) {
                        switch (edge_weight_format) {
                            case LOWROW:

                                if (node_index1 == 0 && node_index2 == 0) {
                                    c[0] = 0;
                                    node_index1 = 1; // start at position (1, 0)
                                }

                                // PRINT("Splitting string \"%s\" into tokens:\n", line);
                                p = strtok(line," ");
                                while (p != NULL) {
                                    c[(N+1)*node_index1+node_index2] = strtod (p, NULL);
                                    c[(N+1)*node_index2+node_index1] = c [(N+1)*node_index1+node_index2];

                                    // PRINT("(%d %d): %.2f, \n", node_index1, node_index2, c[(N+1)*node_index1+node_index2]);

                                    node_index2 += 1;
                                    if (node_index2 == node_index1) {
                                        c[(N+1)*node_index1+node_index2] = 0; // asign 0 to diagonal
                                        node_index1 += 1; // new line
                                        node_index2 = 0;
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
                    // PRINT("ERROR: cvrp_new_from_file: data section reading error.\n");
                    // error_flag = 1;
                    break;
            }
        }
    } // end of while

    fclose (vrpfile);

    if (error_flag == 0) // no error while reading file
    {
        // compute cost as Euclidian distance
        if (edge_weight_type == EUC_2D)
        {
            for (node_index1 = 0; node_index1 <= N; node_index1++)
            {
                c[(N+2)*node_index1] = 0;
                for (node_index2 = node_index1+1; node_index2 <= N; node_index2++)
                {
                    // NOTE: rounding the distance. nint(sqrt(xd*xd+yd*yd)). see TSPLIB spec.
                    // for nint: https://www.iwr.uni-heidelberg.de/groups/comopt/software/TSPLIB95/TSPFAQ.html
                    c[(N+1)*node_index1+node_index2] =
                        (int)(sqrt ((coords[node_index1].x - coords[node_index2].x) * (coords[node_index1].x - coords[node_index2].x)
                            + (coords[node_index1].y - coords[node_index2].y) * (coords[node_index1].y - coords[node_index2].y))
                            + 0.5);
                    c[(N+1)*node_index2+node_index1] = c[(N+1)*node_index1+node_index2];
                }
            }
        }

        // create CVRP problem
        self = cvrp_new (N, q, Q, K, c, coords);
    }

    // free memory
    if (q != NULL) {free(q); q = NULL;}
    if (c != NULL) {free(c); c = NULL;}
    if (coords != NULL) {free(coords); coords = NULL;}

    return self;
}


void cvrp_free (cvrp_t **self_p) {
    assert (self_p);
    if (*self_p) {
        cvrp_t *self = *self_p;

        free (self->node_ids);
        free (self->demands);

        free (self);
        *self_p = NULL;
    }
    print_info ("cvrp freed.\n");
}


cvrp_t *cvrp_dup (cvrp_t *self) {
    assert (self);

    cvrp_t *new_cvrp = cvrp_new ();
    assert (new_cvrp);

    new_cvrp->num_vehicles = self->num_vehicles;
    new_cvrp->num_customers = self->num_customers;
    new_cvrp->capacity = self->capacity;

    size_t num;
    if (self->node_ids != NULL) {
        num = sizeof (size_t) * (self->num_customers + 1);
        new_cvrp->node_ids = (size_t *) malloc (num);
        assert (new_cvrp->node_ids);
        memcpy (new_cvrp->node_ids, self->node_ids, num);
    }

    if (self->demands != NULL) {
        num = sizeof (double) * (self->num_customers + 1);
        new_cvrp->demands = (double *) malloc (num);
        assert (new_cvrp->demands);
        memcpy (new_cvrp->demands, self->demands, num);
    }

    new_cvrp->roadgraph = self->roadgraph;

    return new_cvrp;
}


// Get customer number
size_t cvrp_num_customers (cvrp_t *self) {
    assert (self);
    return self->num_customers;
}


// Get vehicle number
size_t cvrp_num_vehicles (cvrp_t *self) {
    assert (self);
    return self->num_vehicles;
}


// Get vehicle capacity
double cvrp_capacity (cvrp_t *self) {
    assert (self);
    return self->capacity;
}


// Get customer demands
const double *cvrp_demands (cvrp_t *self) {
    assert (self);
    return self->demands;
}


// Get customer demand
double cvrp_demand (cvrp_t *self, size_t node_index) {
    assert (self);
    return self->demands[node_index];
}


// Get arc cost
double cvrp_cost (cvrp_t *self, size_t from_node_index, size_t to_node_index) {
    assert (self);
    assert (from_node_index >= 0 &&
            from_node_index <= self->self->num_customers &&
            to_node_index >= 0 &&
            to_node_index <= self->num_customers);
    return roadgraph_distance (self->roadgraph,
                               node_ids[from_node_index],
                               node_ids[to_node_index]);
}


// // Get coordinates
// const coord2d_t *cvrp_coords (cvrp_t *self) {
//     assert (self);
//     return self->coords;
// }


// Get node coordinate
const coord2d_t *cvrp_coord (cvrp_t *self, size_t node_index) {
    assert (self);
    assert (node_index >= 0 && node_index <= self->num_customers);
    return roadgraph_node_coord (self->roadgraph, node_ids[node_index]);
}


// void cvrp_print_route(const CVRP_Problem *vrp, const int *route, int start, int end)
// {
//     // printf("route[0] %d\n", route[end]);
//     PRINT("\nroute[%d:%d]: ", start, end);
//     // printf("print route: \n");
//     // printf("%d\n", route[2]);
//     for (int i = start; i <= end; i++) PRINT("%3d ", route[i]);
//     if (vrp != NULL)
//     {
//         PRINT(" (cost: %.2f, demand: %.2f)",
//               compute_route_cost(route, start, end, vrp->c, vrp->N),
//               compute_route_demand(route, start, end, vrp->q, vrp->N));
//     }
//     PRINT("\n");
// }

void cvrp_test (bool verbose) {
    print_info (" * cvrp: \n");
    // cvrp_t *cvrp = cvrp_new_from_file ("testdata/A-n32-k5.vrp");
    // assert (cvrp);
    // print_info ("N: %d, K: %d, Q: %f\n",
    //     cvrp_num_customers (cvrp),
    //     cvrp_num_vehicles (cvrp),
    //     cvrp_capacity (cvrp));

    // // ...
    // cvrp_free (&cvrp);
    print_info ("OK\n");
}
