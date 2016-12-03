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

    ctypedef struct coord2d_t:
        double v1
        double v2

    ctypedef enum node_type_t:
        NT_NONE = 0
        NT_DEPOT = 1
        NT_CUSTOMER = 2

    ctypedef enum node_role_t:
        NR_NONE = 0
        NR_SENDER = 1
        NR_RECEIVER = 2


    ctypedef struct solution_t
    ctypedef struct vrp_t

    # Model
    vrp_t *vrp_new ()
    void vrp_free (vrp_t **obj_p)
    void vrp_test (bool verbose)
    void vrp_set_coord_sys (vrp_t *self, coord2d_sys_t coord_sys)
    size_t vrp_add_node (vrp_t *self, const char *ext_id, node_type_t type)
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
                              size_t requset_id,
                              node_role_t node_role,
                              size_t earliest,
                              size_t latest)

    void vrp_set_service_duration (vrp_t *self,
                                   size_t request_id,
                                   size_t sender_or_receiver_id,
                                   size_t service_duration)

    solution_t *vrp_solve (vrp_t *self)


    # Solution
    solution_t *solution_new ()
    void solution_free (solution_t **obj_p)
    solution_t *solution_dup (const solution_t *self)


cdef class VRPModel:
    cdef vrp_t *model
    cdef solution_t *sol

    def __init__(self):
        self.model = vrp_new ()
        if self.model == NULL:
            raise MemoryError("Insuffient memory.")
        self.sol = NULL

    def __dealloc__(self):
        if self.model is not NULL:
            vrp_free (&self.model)
        if self.sol is not NULL:
            solution_free (&self.sol)

    cpdef void test (self):
        vrp_test (True)

    cpdef void set_coord_sys (self, coord2d_sys):
        vrp_set_coord_sys (self.model, coord2d_sys)

    cpdef size_t add_node (self, ext_id, node_type_t type):
        return vrp_add_node (self.model, ext_id, type)

    cpdef void set_node_coord (self, node_id, coord):
        vrp_set_node_coord (self.model, node_id, coord)

    cpdef void set_arc_distance (self, from_node_id, to_node_id, distance):
        vrp_set_arc_distance (self.model, from_node_id, to_node_id, distance)

    cpdef void set_arc_duration (self, from_node_id, to_node_id, duration):
        vrp_set_arc_duration (self.model, from_node_id, to_node_id, duration)

    cpdef void generate_beeline_distances (self):
        vrp_generate_beeline_distances (self.model)

    cpdef void generate_durations (self, speed):
        vrp_generate_durations (self.model, speed)


    cpdef size_t add_vehicle (self, ext_id, max_capacity,
                              start_node_id, end_node_id):
        return vrp_add_vehicle (self.model, ext_id, max_capacity,
                                start_node_id, end_node_id)

    cpdef size_t add_request (self, ext_id, sender, receiver, quantity):
        return vrp_add_request (self.model,
                                ext_id, sender, receiver, quantity)



    cpdef void add_time_window (self, requset_id, node_role, earliest, latest):
        vrp_add_time_window (self.model,
                             requset_id, node_role, earliest, latest)

    cpdef void set_service_duration (self, request_id,
                                     sender_or_receiver_id, service_duration):
        vrp_set_service_duration (self.model, request_id,
                                  sender_or_receiver_id, service_duration)

    cpdef void solve (self):
        self.sol = vrp_solve (self.model)

