/*  =========================================================================
    liber - project public header

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

// Public API classes
#include "types.h"
#include "rng.h"
#include "timer.h"
#include "hash.h"
#include "queue.h"
#include "arrayset.h"
#include "matrixd.h"
#include "listu.h"
#include "listx.h"
#include "evol.h"
#include "solver.h"

#endif
