/*  =========================================================================
    timer - timer

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __TIMER_H_INCLUDED__
#define __TIMER_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _timer_t timer_t;

// Create a new timer
timer_t *timer_new (const char *name);

// Destroy timer
void timer_free (timer_t **self_p);

// Start timer
void timer_start (timer_t *self);

// Suspend timer
void timer_suspend (timer_t *self);

// Resume timer
void timer_resume (timer_t *self);

// Stop timer
// Return time passed from last start
// printit: 0 - no report; 1~4 - report
double timer_stop (timer_t *self, int printit);

// Get cumulated time of timer.
// The timer could be either stopped or running.
// printit: 0 - no report; 1~4 - report
double timer_total (timer_t *self, int printit);

// Reset timer
void timer_reset (timer_t *self);

// Restart timer
void timer_restart (timer_t *self);

// Self test
void timer_test (bool verbose);

#ifdef __cplusplus
}
#endif

#endif
