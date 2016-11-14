/*  =========================================================================
    main.c - CLI implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"

int main (int argc, char *argv []) {
    puts ("er - Elastic Routing");
    bool verbose = false;
    int argn;
    for (argn = 1; argn < argc; argn++) {
        if (strcmp (argv [argn], "--help") == 0
        ||  strcmp (argv [argn], "-h") == 0) {
            puts ("er [options] ...");
            puts ("  --verbose / -v         verbose test output");
            puts ("  --help / -h            this information");
            return 0;
        }
        else
        if (strcmp (argv [argn], "--verbose") == 0
        ||  strcmp (argv [argn], "-v") == 0)
            verbose = true;
        else {
            printf ("Unknown option: %s\n", argv [argn]);
            return 1;
        }
    }
    //  Insert main code here
    if (verbose)
        // zsys_info ("er - Elastic Routing");
        printf ("version: %d\n", GLOBDOM_VERSION);

    solvt *solver = solvnew ();
    solvrun (solver);
    solvfree (&solver);

    return 0;
}
