/*  =========================================================================
    types - global types definations

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __TYPEDEFS_H_INCLUDED__
#define __TYPEDEFS_H_INCLUDED__

typedef size_t (*hashfunc_t) (const void *obj);

typedef void (*destructor_t) (void **obj_p);

typedef int (*comparator_t) (const void *obj1, const void *obj2);

typedef bool (*matcher_t) (const void *obj1, const void *obj2);

typedef void *(*duplicator_t) (const void *obj);

typedef void (*copier_t) (void *dest, const void *src);

typedef void (*printer_t) (const void *obj);

#endif
