/*  =========================================================================
    numeric_ext - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
