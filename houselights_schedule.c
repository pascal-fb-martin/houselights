/* houseslights - A simple home web server for lighting control
 *
 * Copyright 2020, Pascal Martin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 *
 * houselights_schedule.c - Control the light schedule.
 *
 * SYNOPSYS:
 *
 * This module handles scheduling lighting plugs at specific intervals.
 *
 * const char *houselights_schedule_load (int argc, const char *argv[]);
 *
 *    Load the last saved set of schedules from the configuration.
 *
 * void houselights_schedule_enable  (void);
 * void houselights_schedule_disable (void);
 *
 *    Turn the complete schedule function on and off.
 *
 * void houselights_schedule_add (const char *plug,
 *                                const char *on, const char *off, int days);
 *
 *    A schedule defines when to switch a light on. The light is identified
 *    by its plug name, the on and off parameter defined the time interval.
 *    Each time follows the syntax:
 *        hh | hh:mm | hh:-mm | sunrise:[-]mm | sunset:[-]mm
 *    If the on time is less than the off time, then both are for the same day.
 *    If the off time is less than the on time, then the off time is for the
 *    next day,
 *    The days value is a bit map: Sunday is bit 0 and Saturday is bit 6.
 *
 * void houselights_schedule_delete (const char *id);
 *
 * void houselights_schedule_periodic (time_t now);
 *
 * int houselights_schedule_status (char *buffer, int size);
 *
 *    A function that populates a complete status in JSON.
 *
 */

#include <sys/time.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <echttp.h>
#include <echttp_json.h>

#include "houselog.h"
#include "houseconfig.h"
#include "housediscover.h"

#include "houselights_plugs.h"
#include "houselights_schedule.h"

#define DEBUG if (echttp_isdebug()) printf

typedef struct {
    int hour;
    int minutes;
} LightTime;

typedef struct {
    int id;
    char *plug;
    LightTime on;
    LightTime off;
    int days;
    char state; // i: idle, a: active.
} LightSchedule;

#define MAX_SCHEDULES 256

static int ScheduleDisabled = 1;

static LightSchedule Schedules[MAX_SCHEDULES];
static int           SchedulesCount = 0;

// TBD: these default hours to be adjusted daily through an almanac service.
//
static LightTime LightsSunrise = {5, 0};
static LightTime LightsSunset  = {20, 0};

static int LightsRandom = 0; // Changed hourly.


static void houselights_schedule_import (const char *ascii, LightTime *t) {
    if (!ascii) {
        t->hour = -2;
        return;
    }
    const char *p = strchr(ascii, ':');
    if (p) {
        t->minutes = atoi(p+1);
    } else {
        t->minutes = 0;
    }
    if (strstr (ascii, "sunrise") == ascii) {
        t->hour = -1; 
        return;
    } else if (strstr (ascii, "sunset") == ascii) {
        t->hour = 25;
        return;
    } else {
        t->hour = atoi(ascii);
        if (t->hour < 0 || t->hour > 23) t->hour = 0;
    }
    if (t->minutes < -30) t->minutes = -30;
    if (t->minutes >= 60) t->minutes = 59;
}

static int houselights_schedule_adjust (LightTime *t) {

    if (t->hour == -2)  return 0;

    int hour, minutes;

    minutes = t->minutes;
    if (t->hour == -1) {
        hour = LightsSunrise.hour;
        minutes += LightsSunrise.minutes;
    } else if (t->hour == 25) {
        hour = LightsSunset.hour;
        minutes += LightsSunset.minutes;
    } else {
        hour = t->hour;
    }
    // It is OK if minutes < 0. For example 12:-20 is "20mn before 12".
    return hour * 60 + minutes + LightsRandom;
}

const char *houselights_schedule_load (int argc, const char *argv[]) {

    int i;
    const char *mode = houseconfig_string (0, ".lights.mode");
    int schedules = houseconfig_array(0, ".lights.schedules");

    if (mode && strcmp (mode, "auto")) {
        ScheduleDisabled = 1;
    } else {
        ScheduleDisabled = 0;
    }
    if (echttp_isdebug()) printf ("Schedule disabled: %s (%s)\n", ScheduleDisabled?"true":"false", mode?"configured":"default");

    for (i = 0; i < SchedulesCount; ++i) {
        if (Schedules[i].plug) free (Schedules[i].plug);
        Schedules[i].plug = 0;
    }

    if (schedules > 0) {
        int count = houseconfig_array_length (schedules);

        if (echttp_isdebug()) printf ("Schedule: %d entries\n", count);

        if (count > MAX_SCHEDULES) count = MAX_SCHEDULES;
        SchedulesCount = 0;

        for (i = 0; i < count; ++i) {
            char path[128];
            snprintf (path, sizeof(path), "[%d]", i);
            int item = houseconfig_object (schedules, path);
            if (item <= 0) continue;
            const char *device = houseconfig_string (item, ".device");
            const char *on = houseconfig_string (item, ".on");
            const char *off = houseconfig_string (item, ".off");
            if (!device || !on || !off) continue;
            int days = houseconfig_integer (item, ".days");
            if (!days) days = 0x7f;
            houselights_schedule_add (device, on, off, days);
            if (echttp_isdebug()) printf ("  %s\n", device);
        }
    }
    return 0;
}

void houselights_schedule_enable (void) {
    ScheduleDisabled = 0;
}

void houselights_schedule_disable (void) {
    ScheduleDisabled = 1;
}


void houselights_schedule_add (const char *plug,
                               const char *on, const char *off, int days) {
    if (SchedulesCount < MAX_SCHEDULES) {
        Schedules[SchedulesCount].id =
            0x1000000 + (time(0) & 0xffff00) + SchedulesCount;
        Schedules[SchedulesCount].plug = strdup(plug);
        houselights_schedule_import (on, &(Schedules[SchedulesCount].on));
        houselights_schedule_import (off, &(Schedules[SchedulesCount].off));
        Schedules[SchedulesCount].days = days;
        Schedules[SchedulesCount].state = 'i';
        SchedulesCount += 1;
    }
}

void houselights_schedule_delete (const char *identifier) {

    int i;
    int id = atoi (identifier);
    for (i = 0; i < SchedulesCount; ++i) {
        if (Schedules[i].id != id) continue;
        if (Schedules[i].plug) free(Schedules[i].plug);
        Schedules[i].plug = 0;
        Schedules[i].id = 0;
    }
    for (i = SchedulesCount - 1; i >= 0; --i) {
        if (Schedules[i].id) break;
        SchedulesCount = i;
    }
}

void houselights_schedule_periodic (time_t now) {

    static time_t LastCall = 0;
    struct tm *t;
    int i;

    if (ScheduleDisabled) return;

    if (now < LastCall + 60) return;
    LastCall = now;

    t = localtime (&LastCall);
    int daytime = t->tm_hour * 60 + t->tm_min;
    int today = t->tm_wday;
    int yesterday = (today <= 0) ? 6 : today - 1;

    if (daytime % 60 == 0) {
        struct timeval tv;
        gettimeofday (&tv, 0);
        LightsRandom = tv.tv_usec % 20 - 10; // Range -10 .. 10.
    }

    for (i = 0 ; i < SchedulesCount; ++i) {

        if (!Schedules[i].id) continue;
        if (!Schedules[i].plug) continue;

        int on = houselights_schedule_adjust (&(Schedules[i].on));
        int off = houselights_schedule_adjust (&(Schedules[i].off));

        int active = 0;
        int duration = 0;

        if (on < off) {
            // Active from on to off today.
            //
            if (daytime >= on && daytime < off) {
                if (Schedules[i].days & (1 << today)) {
                    active = 1;
                    duration = off - daytime;
                }
            }
        } else {
            // Active from on to midnight today, from midnight to off next day
            // (In the later case, the check is made the next day..)
            //
            if (daytime >= on) {
                if (Schedules[i].days & (1 << today)) {
                    active = 1;
                    duration = 24*60 - daytime;
                }
            } else if (daytime < off) {
                if (Schedules[i].days & (1 << yesterday)) {
                    active = 1;
                    duration = off - daytime;
                }
            }
        }

        // If the schedule is active, maintain the plugs on until the next
        // evaluation (plus a 5 seconds grace period to avoid flickering).
        // If no schedule is active for this plug, it will just switch
        // off on its own, when the last pulse expires.
        // If this service crashes, the lights will just go off on their
        // own after a minute.
        //
        if (active) {
            houselights_plugs_on (Schedules[i].plug, 65, 0);
            if (Schedules[i].state != 'a') {
                houselog_event ("PLUG", Schedules[i].plug, "SCHEDULED",
                                "ON FOR %d MINUTES", duration);
                Schedules[i].state = 'a';
            }
        } else {
            if (Schedules[i].state != 'i') {
                houselog_event ("PLUG", Schedules[i].plug, "SCHEDULED", "OFF");
                Schedules[i].state = 'i';
            }
        }
    }
}

int houselights_schedule_status (char *buffer, int size) {

    int i;
    int cursor = 0;
    const char *prefix = "";

    cursor += snprintf (buffer+cursor, size-cursor,
                        "\"mode\":\"%s\",\"schedules\":[",
                        ScheduleDisabled?"manual":"auto");
    if (cursor >= size) goto overflow;

    for (i = 0; i < SchedulesCount; ++i) {

        if (!Schedules[i].id) continue;
        if (!Schedules[i].plug) continue;

        cursor += snprintf (buffer+cursor, size-cursor,
                            "%s{\"id\":%d,\"device\":\"%s\",\"state\":\"%c\""
                                ",\"on\":\"%02d:%02d\",\"off\":\"%02d:%02d\""
                                ",\"days\":%d}",
                            prefix, Schedules[i].id,
                            Schedules[i].plug, Schedules[i].state,
                            Schedules[i].on.hour, Schedules[i].on.minutes,
                            Schedules[i].off.hour, Schedules[i].off.minutes,
                            Schedules[i].days);
        if (cursor >= size) goto overflow;
        prefix = ",";
    }

    cursor += snprintf (buffer+cursor, size-cursor, "]");
    if (cursor >= size) goto overflow;

    return cursor;

overflow:
    houselog_trace (HOUSE_FAILURE, "BUFFER", "overflow");
    buffer[0] = 0;
    return 0;
}

