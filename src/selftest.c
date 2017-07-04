/*  =========================================================================
    selftest.c - selftests

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
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
    // { "route", route_test },
    // { "solution", solution_test },
    // { "tspi", tspi_test },
    { "tsp", tsp_test },
    // { "cvrp", cvrp_test },
    // { "vrptw", vrptw_test },
    // { "vrp", vrp_test },
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
