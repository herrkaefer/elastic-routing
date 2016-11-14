/*  =========================================================================
    numeric_ext - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


size_t factorial (size_t n) {
    if (n == 0)
        return 1;
    size_t f = 1;
    for (size_t i = 1; i <= n; i++) {
        f = f * i;
        if (f == 0)
            return SIZE_MAX;
    }
    return f;
}
