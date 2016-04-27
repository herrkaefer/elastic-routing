/*  =========================================================================
    gdom_selftest.c - run selftests

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"

typedef struct {
    const char *testname;
    void (*test) (bool);
} test_item_t;

static test_item_t
all_tests [] = {
// #ifdef WITH_DRAFTS
    // { "queue", queue_test },
    // { "matrix4d", matrix4d_test },
    // { "hash", hash_test },
    // { "arrayset", arrayset_test },
    // { "vrp", vrp_test },
    // { "list4u", list4u_test },
    // { "list4x", list4x_test },
    // { "rng", rng_test },
    // { "timer", timer_test },
    // { "evol", evol_test },
// #endif // WITH_DRAFTS
    {0, 0}          //  Sentinel
};


//  -------------------------------------------------------------------------
//  Run all tests.
//

static void test_runall (bool verbose) {
    test_item_t *item;
    printf ("Running selftests...\n");
    for (item = all_tests; item->test; item++)
        item->test (verbose);
    printf ("Tests passed OK.\n");
}


int main (int argc, char **argv) {
    test_runall (true);
    return 0;
}
