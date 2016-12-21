/*  =========================================================================
    classes - project private header

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __CLASSES_H_INCLUDED__
#define __CLASSES_H_INCLUDED__

// Standard ANSI include files. Not necessary if czmq.h is included.
// #include <ctype.h>
// #include <limits.h>
// #include <stdarg.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <stddef.h>
// #include <string.h>
// #include <time.h>
// #include <errno.h>
// #include <float.h>
// #include <math.h>
// #include <signal.h>
// #include <setjmp.h>
// #include <assert.h>
// #include <stdbool.h>


// External APIs
#include "../include/liber.h"

// Private class structures
typedef struct _tspi_t tspi_t;
typedef struct _tsp_t tsp_t;
typedef struct _cvrp_t cvrp_t;
typedef struct _vrptw_t vrptw_t;

// Internal API headers
#include "util.h"
#include "tspi.h"
#include "tsp.h"
#include "cvrp.h"
#include "vrptw.h"

#endif
