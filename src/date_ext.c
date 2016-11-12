/*  =========================================================================
    date_ext - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#include "classes.h"


time_t date_to_time (int year, int mon, int day, int hour, int min, int sec) {
    struct tm t = {0};
    t.tm_sec  = sec;
    t.tm_min  = min;
    t.tm_hour = hour;
    t.tm_mday = day;
    t.tm_mon  = mon - 1;
    t.tm_year = year - 1900;
    return mktime(&t);
}


bool out_of_date (time_t expiry_t) {
    return (time(NULL) > expiry_t);
}


bool out_of_date_by_date (int expiry_year, int expiry_month, int expiry_day) {
    return out_of_date (
            date_to_time (expiry_year, expiry_month, expiry_day, 0, 0, 0));
}
