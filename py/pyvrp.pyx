# pyer.pyx - Cython wrapping of C public APIs
#
# Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

# cimport cython
# from libc.stdlib cimport malloc, free
from libcpp cimport bool


cdef extern from "../src/classes.h":

    ctypedef enum coord2d_sys_t:
        CS_NONE = 0
        CS_CARTESIAN2D = 1
        CS_POLAR2D = 2
        CS_WGS84 = 3
        CS_GCJ02 = 4

    ctypedef struct _coord2d_t:
        double v1
        double v2
    ctypedef _coord2d_t coord2d_t

    ctypedef enum node_role_t:
        NR_NONE = 0
        NR_SENDER = 1
        NR_RECEIVER = 2

    ctypedef struct solution_t
    ctypedef struct vrp_t

    # Model
    vrp_t *vrp_new ()
    void vrp_free (vrp_t **self_p)
    void vrp_test (bool verbose)
    void vrp_set_coord_sys (vrp_t *self, coord2d_sys_t coord_sys)
    size_t vrp_add_node (vrp_t *self, const char *ext_id)
    void vrp_set_node_coord (vrp_t *self, size_t node_id, coord2d_t coord)
    void vrp_set_arc_distance (vrp_t *self,
                               size_t from_node_id, size_t to_node_id,
                               double distance)
    void vrp_set_arc_duration (vrp_t *self,
                               size_t from_node_id, size_t to_node_id,
                               size_t duration)
    void vrp_generate_beeline_distances (vrp_t *self)
    void vrp_generate_durations (vrp_t *self, double speed)

    size_t vrp_add_vehicle (vrp_t *self,
                            const char *vehicle_ext_id,
                            double max_capacity,
                            size_t start_node_id,
                            size_t end_node_id)

    size_t vrp_add_request (vrp_t *self,
                            const char *request_ext_id,
                            size_t sender,
                            size_t receiver,
                            double quantity)

    void vrp_add_time_window (vrp_t *self,
                              size_t request_id,
                              node_role_t node_role,
                              size_t earliest,
                              size_t latest)

    void vrp_set_service_duration (vrp_t *self,
                                   size_t request_id,
                                   size_t sender_or_receiver_id,
                                   size_t service_duration)

    solution_t *vrp_solve (vrp_t *self)


    # Solution
    void solution_free (solution_t **self_p)
    void solution_print (const solution_t *self)


cdef class VRPSolver:

    cdef vrp_t *_model
    cdef solution_t *_sol

    def __init__(self):
        self._model = vrp_new ()
        if self._model == NULL:
            raise MemoryError("Insuffient memory.")
        self._sol = NULL


    def __dealloc__(self):
        if self._model is not NULL:
            vrp_free (&self._model)
        if self._sol is not NULL:
            solution_free (&self._sol)


    cpdef void test (self):
        vrp_test (True)


    cpdef void set_coord_sys (self, coord_sys):
        if coord_sys.upper() == 'CARTESIAN2D':
            c_sys = CS_CARTESIAN2D
        elif coord_sys.upper() == 'POLAR2D':
            c_sys = CS_POLAR2D
        elif coord_sys.upper() == 'WGS84':
            c_sys = CS_WGS84
        elif coord_sys.upper() == 'GCJ02':
            c_sys = CS_GCJ02
        else:
            print("invalid coordiante system")
            c_sys = CS_NONE
        vrp_set_coord_sys (self._model, c_sys)


    cpdef size_t add_node (self, ext_id):
        return vrp_add_node (self._model, ext_id)


    cpdef void set_node_coord (self, node_id, coord):
        cdef coord2d_t c
        c.v1 = coord[0]
        c.v2 = coord[1]
        vrp_set_node_coord (self._model, node_id, c)


    cpdef void set_arc_distance (self, from_node_id, to_node_id, distance):
        vrp_set_arc_distance (self._model, from_node_id, to_node_id, distance)


    cpdef void set_arc_duration (self, from_node_id, to_node_id, duration):
        vrp_set_arc_duration (self._model, from_node_id, to_node_id, duration)


    cpdef void generate_beeline_distances (self):
        vrp_generate_beeline_distances (self._model)


    cpdef void generate_durations (self, speed):
        vrp_generate_durations (self._model, speed)


    cpdef size_t add_vehicle (self, ext_id, max_capacity, start_node, end_node):
        return vrp_add_vehicle (self._model, ext_id, max_capacity,
                                start_node, end_node)


    cpdef size_t add_request (self, ext_id, sender, receiver, quantity):
        return vrp_add_request (self._model,
                                ext_id, sender, receiver, quantity)


    cpdef void add_time_window (self, request_id, node_role, earliest, latest):
        if node_role.lower() == 'sender':
            nr = NR_SENDER
        elif node_role.lower() == 'receiver':
            nr = NR_RECEIVER
        else:
            print("invalid node role")
            return

        vrp_add_time_window (self._model, request_id, nr, earliest, latest)


    cpdef void set_service_duration (self, request_id,
                                     node_role, service_duration):
        if node_role.lower() == 'sender':
            nr = NR_SENDER
        elif node_role.lower() == 'receiver':
            nr = NR_RECEIVER
        else:
            print("invalid node role")
            return

        vrp_set_service_duration (self._model, request_id, nr, service_duration)


    cpdef void solve (self):
        self._sol = vrp_solve (self._model)


    cpdef void print_solution (self):
        solution_print (self._sol);


