/*  =========================================================================
    timer - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"

struct _timer_t {
    double start_time;
    double cum_time;
    char   name[32];
    bool   is_running;
    size_t count;
};


#if defined (__linux__) || defined (__APPLE__) || defined (__MINGW32__)

#include <unistd.h>
#include <sys/times.h>

static double timer_cpu_time (void) {
    struct tms now;
    times (&now);
    return ((double) now.tms_utime) / ((double) sysconf (_SC_CLK_TCK));
}

#else

static double timer_cpu_time (void) {
    return ((double) clock()) / ((double) CLOCKS_PER_SEC);
}

#endif


timer_t *timer_new (const char *name) {
    timer_t *self = (timer_t *) malloc (sizeof (timer_t));
    assert (self);

    self->is_running = false;
    self->cum_time = 0.0;
    self->count = 0;
    if (name == (char *)NULL || name[0] == '\0')
        strncpy (self->name, "ANONYMOUS", sizeof(self->name)-1);
    else
        strncpy (self->name, name, sizeof(self->name)-1);
    self->name[sizeof(self->name)-1] = '\0';

    print_info ("timer %s created.\n", self->name);
    return self;
}


void timer_free (timer_t **self_p) {
    assert (self_p);
    if (*self_p) {
        free (*self_p);
        *self_p = NULL;
    }
    print_info ("timer freed.\n");
}


void timer_start (timer_t *self) {
    assert (self);
    assert (!self->is_running);
    self->start_time = timer_cpu_time ();
    self->is_running = true;
}


void timer_suspend (timer_t *self) {
    assert (self);
    assert (self->is_running);
    self->cum_time += timer_cpu_time () - self->start_time;
    self->is_running = false;
}


void timer_resume (timer_t *self) {
    assert (self);
    assert (!self->is_running);
    self->start_time = timer_cpu_time ();
    self->is_running = true;
}


double timer_stop (timer_t *self, int printit) {
    assert (self);
    assert (self->is_running);

    double z = timer_cpu_time () - self->start_time;
    self->is_running = false;
    self->cum_time += z;
    self->count++;

    if (printit == 1 || (printit == 2 && z > 0.0)) {
        print_info ("Time for %s: %.3f seconds (%.3f total in %zu calls)\n",
                self->name, z, self->cum_time, self->count);
        fflush (stdout);
    }
    else if (printit == 3 || (printit == 4 && z > 0.0)) {
        print_info ("T %-34.34s %9.2f %9.3f %zu\n",
                self->name, z, self->cum_time, self->count);
        fflush(stdout);
    }

    return z;
}


double timer_total (timer_t *self, int printit) {
    assert (self);

    double z = self->cum_time;

    if (self->is_running)
        z += timer_cpu_time () - self->start_time;

    if (printit == 1 || (printit == 2 && z > 0.0)) {
        print_info ("Total time for %-34.34s %.3f seconds in %zu%s calls\n",
            self->name, z, self->count,
            self->is_running ? "+1" : "");
        fflush (stdout);
    }
    else if (printit == 3 || (printit == 4 && z > 0.0)) {
        print_info ("CT %-34.34s %9.3f %10zu%s\n",
            self->name, z, self->count,
            self->is_running ? "+1" : "");
        fflush (stdout);
    }

    return z;
}


void timer_reset (timer_t *self) {
    assert (self);
    self->is_running = false;
    self->cum_time = 0.0;
    self->count = 0;
}


void timer_restart (timer_t *self) {
    assert (self);
    self->cum_time = 0.0;
    self->count = 0;
    self->start_time = timer_cpu_time ();
    self->is_running = true;
}


void timer_test (bool verbose)
{
    print_info (" * timer: \n");
    timer_t *timer = timer_new ("TEST");

    timer_start (timer);
    while (1) if (timer_total (timer, 0) > 1.0) break;
    timer_suspend (timer);
    timer_resume (timer);
    while (1) if (timer_total (timer, 0) > 1.8) break;
    timer_stop (timer, 0);

    timer_total (timer, 1);

    timer_start (timer);
    while (1) if (timer_total (timer, 0) > 2.4) break;
    timer_suspend (timer);
    timer_resume (timer);
    while (1) if (timer_total (timer, 0) > 3.0) break;
    timer_stop (timer, 0);

    timer_total (timer, 1);

    timer_reset (timer);

    timer_start (timer);
    while (1) if (timer_total (timer, 0) > 3.4) break;
    timer_suspend (timer);
    timer_resume (timer);
    while (1) if (timer_total (timer, 0) > 5.0) break;
    timer_stop (timer, 0);

    timer_total (timer, 1);


    timer_free (&timer);
    print_info ("OK\n");
}

