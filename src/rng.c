/*  =========================================================================
    rng - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"
#include "deps/pcg/pcg_variants.h"
#include "deps/pcg/entropy.h"

struct _rng_t {
    pcg32_random_t rng;
};


rng_t *rng_new (void) {
    rng_t *self = (rng_t *) malloc (sizeof (rng_t));
    assert (self);

    uint64_t seeds[2];
    entropy_getbytes ((void *) seeds, sizeof (seeds));
    pcg32_srandom_r (&self->rng, seeds[0], seeds[1]);

    // "quick and dirty way to do the initialization"
    // pcg32_srandom_r(&self->rng, time(NULL), (intptr_t)&self->rng);

    print_info ("rng created.\n");
    return self;
}


void rng_free (rng_t **self_p) {
    assert (self_p);
    if (*self_p) {
        rng_t *self = *self_p;

        free (self);
        *self_p = NULL;
    }
    print_info ("rng freed.\n");
}


double rng_random (rng_t *self) {
    assert (self);
    return ldexp (pcg32_random_r (&self->rng), -32);
}


int rng_random_int (rng_t *self, int a, int b) {
    assert (self);
    uint32_t bound = (a <= b) ? (uint32_t)(b - a) : (uint32_t)(a - b);
    if (bound == 0) return a;
    int start = (a <= b) ? a : b;
    uint32_t r = pcg32_boundedrand_r (&self->rng, bound); // [0, bound)
    return start + (int) r;
}


double rng_random_double (rng_t *self, double a, double b) {
    assert (self);
    double v = rng_random (self);
    return (a <= b) ? (b-a)*v+a : (a-b)*v+b;
}


void rng_test (bool verbose) {
    print_info (" * rng: \n");

    timer_t *timer = timer_new ("RNG");
    rng_t *rng = rng_new ();

    timer_start (timer);

    for (size_t i = 0; i < 1e8; i++) {
        double r = rng_random (rng);
        assert (r >= 0);
        assert (r < 1);
    }

    timer_stop (timer, 0);
    timer_start (timer);

    for (size_t i = 0; i < 1e8; i++) {
        int a = rng_random_int (rng, 0, 1001);
        assert (a >= 0);
        assert (a <= 1000);
        int b = rng_random_int (rng, 2000, 3001);
        assert (b >= 2000);
        assert (b <= 3000);
        int r = rng_random_int (rng, a, b);
        assert (r >= a);
        assert (r < b);
    }

    timer_stop (timer, 0);
    timer_total (timer, 1);

    rng_free (&rng);
    timer_free (&timer);

    print_info ("OK\n");
}


