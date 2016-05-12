/*  =========================================================================
    typedefs - global definations

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/


typedef size_t (*hashfunc_t) (const void *obj);

typedef void (*destructor_t) (void **obj_p);

typedef int (*comparator_t) (const void *obj1, const void *obj2);

typedef bool (*matcher_t) (const void *obj1, const void *obj2);

typedef void *(*duplicator_t) (const void *obj);

typedef void (*copier_t) (void *dest, const void *src);

typedef void (*printer_t) (const void *obj);

#define streq(s1,s2)    (!strcmp ((s1), (s2)))
#define strneq(s1,s2)   (strcmp ((s1), (s2)))

#define UUID_STR_LEN 32
#define PI 3.1415926

#ifdef SIZE_MAX
#define ID_NONE   SIZE_MAX
#define SIZE_NONE SIZE_MAX
#else
#define ID_NONE   ((size_t)-1)
#define SIZE_NONE ((size_t)-1)
#endif

#define TIME_NONE (time_t)(0)

#define DOUBLE_MAX       DBL_MAX
#define DOUBLE_NONE      NAN //DBL_MAX
#define DOUBLE_THRESHOLD (2*DBL_MIN)

