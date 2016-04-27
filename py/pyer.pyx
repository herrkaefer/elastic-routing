# pynbs.pyx - Cython wrapping of C public APIs
# Copyright (c) the Contributors as noted in the AUTHORS file.
# This file is part of the N-Body Simulation Project.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cdef extern from "../include/er.h":
    ctypedef struct er_solver_t
    er_solver_t *er_solver_new ()
    void er_solver_destroy (er_solver_t **obj_p)
    void er_solver_run (er_solver_t *obj)

cdef class Solver:
    cdef er_solver_t *_thisptr

    def __init__(self):
        self._thisptr = er_solver_new()
        if self._thisptr == NULL:
            raise MemoryError("Insuffient memory.")

    def __dealloc__(self):
        if self._thisptr != NULL:
            er_solver_destroy(&self._thisptr)

    cpdef void run(self):
        er_solver_run(self._thisptr)



