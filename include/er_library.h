/*  =========================================================================
    liber - project public header

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __ER_H_INCLUDED__
#define __ER_H_INCLUDED__

// External dependencies
#include <czmq.h>

// version macros for compile-time API detection
#define ER_VERSION_MAJOR 0
#define ER_VERSION_MINOR 1
#define ER_VERSION_PATCH 0

#define ER_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define ER_VERSION \
    ER_MAKE_VERSION(ER_VERSION_MAJOR, \
    ER_VERSION_MINOR, \
    ER_VERSION_PATCH)

// Public class structures
typedef struct _arrayset_t arrayset_t;
typedef struct _hash_t hash_t;
typedef struct _listu_t listu_t;
typedef struct _listx_t listx_t;
typedef struct _matrixd_t matrixd_t;
typedef struct _matrixu_t matrixu_t;
typedef struct _queue_t queue_t;
typedef struct _rng_t rng_t;
typedef struct _timer_t timer_t;
typedef struct _evol_t evol_t;


// Public API headers
#include "types.h"
#include "arrayset.h"
#include "hash.h"
#include "listu.h"
#include "listx.h"
#include "matrixd.h"
#include "matrixu.h"
#include "queue.h"
#include "rng.h"
#include "timer.h"
#include "evol.h"

#endif
