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
 * const char *houselights_schedule_refresh (void);
 *
 *    Activate the last saved set of schedules from the configuration.
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
#include "housealmanac.h"

#include "houselights_plugs.h"
#include "houselights_schedule.h"

#define DEBUG if (echttp_isdebug()) printf

typedef struct {
    int hour;
    int minutes;
    char base;
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

static int LightsRandom = 0; // Random adjustment to make it realistic.


static void houselights_schedule_import (const char *ascii, LightTime *t) {
    if (!ascii) {
        t->hour = -2;
        return;
    }
    const char *p = strchr(ascii, ':');
    if (p) {
        t->minutes = atoi(p+1);
        if (t->minutes < -30) t->minutes = -30;
        if (t->minutes >= 60) t->minutes = 59;
    } else {
        t->minutes = 0;
    }
    if (ascii[0] == '+') {
        t->base = '+'; // Delta after sunset.
        ascii += 1;
    } else if (ascii[0] == '-') {
        t->base = '-'; // Delta before sunrise.
        ascii += 1;
    } else {
        t->base = 0; // Time of day.
    }
    t->hour = atoi(ascii);
    if (t->hour < 0 || t->hour > 23) t->hour = 0;
}

static time_t houselights_schedule_adjust (time_t base, LightTime *t) {

    // It is OK if minutes < 0. For example 12:-20 is "20mn before 12".
    int delta = (t->hour * 3600) + (t->minutes * 60) + LightsRandom;

    if (t->base == '-') {
        return housealmanac_sunrise() - delta;
    } else if (t->base == '+') {
        return housealmanac_sunset() + delta;
    }
    return base + delta;
}

const char *houselights_schedule_refresh (void) {

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
    int i;

    if (ScheduleDisabled) return;
    if (!housealmanac_ready()) return; // Start scheduling only when ready.

    if (now < LastCall + 30) return; // Re-evaluate twice a minute.
    LastCall = now;

    struct tm t = *localtime (&now);
    int hour = t.tm_hour;
    int today = t.tm_wday;
    int yesterday = (today <= 0) ? 6 : today - 1;
    t.tm_hour = t.tm_min = t.tm_sec = 0;
    time_t base = mktime (&t);

    if ((now % 300) <= 30) { // Re-evaluate every 5 minutes.
        struct timeval tv;
        gettimeofday (&tv, 0);
        LightsRandom = (tv.tv_usec % 600) - 300; // Range -5 to 5 minutes.
    }

    for (i = 0 ; i < SchedulesCount; ++i) {

        if (!Schedules[i].id) continue;
        if (!Schedules[i].plug) continue;

        time_t on = houselights_schedule_adjust (base, &(Schedules[i].on));
        time_t off = houselights_schedule_adjust (base, &(Schedules[i].off));

        int duration = 0;

        if (on < off) {
            // Active from on to off today.
            //
            if (now >= on && now < off) {
                if (Schedules[i].days & (1 << today)) {
                    duration = (int)(off - now);
                }
            }
        } else  if (on > off) {
            // his can happen if the interval crosses midnight. Two cases:
            // - evening: active from on to midnight (real off is tomorrow).
            // - morning: active from midnight to off (real on was yesterday).
            // Restriction: durations longer than 12h are not supported.
            //
            if ((hour > 12) && (now >= on)) {
                // Current time is between on and midnight.
                if (Schedules[i].days & (1 << today)) {
                    duration = (int)(off + (24*60*60) - now);
                }
            } else if ((hour < 12) && (now < off)) {
                // Current time is between midnight and off.
                // This schedule started yesterday, so this is the reference.
                if (Schedules[i].days & (1 << yesterday)) {
                    duration = (int)(off - now);
                }
            }
        }

        // If the schedule is active, maintain the plugs on until the next
        // evaluation (plus a 10 seconds grace period to avoid flickering).
        // If no schedule is active for this plug, it will just switch
        // off on its own, when the last pulse expires.
        // If this service stops for any reason, the lights will just go off
        // on their own after less than a minute.
        //
        if (duration > 0) {
            houselights_plugs_on (Schedules[i].plug, 40, 0);
            if (Schedules[i].state != 'a') {
                houselog_event ("PLUG", Schedules[i].plug, "SCHEDULED",
                                "ON FOR %d MINUTES", (duration+30)/60);
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

