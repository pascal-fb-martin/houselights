/* HouseLights - a simple web server to control lights.
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
 * houselights.c - Main loop of the HouseLights program.
 *
 * SYNOPSYS:
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "echttp.h"
#include "echttp_cors.h"
#include "echttp_static.h"

#include "houseportalclient.h"
#include "houselog.h"
#include "houseconfig.h"
#include "housediscover.h"

#include "houselights_plugs.h"
#include "houselights_schedule.h"

static int use_houseportal = 0;


static const char *lights_status (const char *method, const char *uri,
                                  const char *data, int length) {
    static char buffer[65537];
    int cursor = 0;

    cursor += snprintf (buffer, sizeof(buffer),
                        "{\"host\":\"%s\",\"proxy\":\"%s\",\"timestamp\":%d,"
                            "\"lights\":{",
                        houselog_host(), houseportal_server(), (long)time(0));

    cursor += houselights_plugs_status (buffer+cursor, sizeof(buffer)-cursor);
    cursor += snprintf (buffer+cursor, sizeof(buffer)-cursor, "}}");
    echttp_content_type_json ();
    return buffer;
}

static const char *lights_schedule (const char *method, const char *uri,
                                    const char *data, int length) {
    static char buffer[65537];
    int cursor = 0;

    cursor += snprintf (buffer, sizeof(buffer),
                        "{\"host\":\"%s\",\"proxy\":\"%s\",\"timestamp\":%d,"
                            "\"lights\":{",
                        houselog_host(), houseportal_server(), (long)time(0));

    cursor += houselights_schedule_status (buffer+cursor, sizeof(buffer)-cursor);
    cursor += snprintf (buffer+cursor, sizeof(buffer)-cursor, "}}");
    echttp_content_type_json ();
    return buffer;
}

static const char *lights_set (const char *method, const char *uri,
                               const char *data, int length) {

    const char *name = echttp_parameter_get("device");
    const char *state = echttp_parameter_get("state");
    const char *pulsep = echttp_parameter_get("pulse");

    if (!name) {
        echttp_error (404, "missing device name");
        return "";
    }
    if (!state) {
        echttp_error (400, "missing state value");
        return "";
    }

    if (!strcmp(state, "on")) {
        int pulse = pulsep ? atoi(pulsep) : 0;
        if (pulse < 0) {
            echttp_error (400, "invalid pulse value");
            return "";
        }
        houselights_plugs_on (name, pulse, 1);
    } else if (!strcmp(state, "off")) {
        houselights_plugs_off (name, 1);
    } else {
        echttp_error (400, "invalid state value");
        return "";
    }
    return lights_status (method, uri, data, length);
}

static const char *lights_save (const char *method, const char *uri,
                                  const char *data, int length) {
    const char *text = lights_schedule (method, uri, data, length);
    houseconfig_update (text);
    return text;
}

static const char *lights_enable (const char *method, const char *uri,
                                  const char *data, int length) {

    houselights_schedule_enable();
    return lights_save (method, uri, data, length);
}

static const char *lights_disable (const char *method, const char *uri,
                                   const char *data, int length) {

    houselights_schedule_disable();
    return lights_save (method, uri, data, length);
}

static const char *lights_add (const char *method, const char *uri,
                                   const char *data, int length) {

    const char *device = echttp_parameter_get("device");
    const char *on = echttp_parameter_get("on");
    const char *off = echttp_parameter_get("off");
    const char *days = echttp_parameter_get("days");

    houselights_schedule_add (device, on, off, atoi(days));
    housediscover (0);

    return lights_save (method, uri, data, length);
}

static const char *lights_delete (const char *method, const char *uri,
                                  const char *data, int length) {

    const char *id = echttp_parameter_get("id");
    if (!id) {
        echttp_error (400, "missing id");
        return "";
    }
    houselights_schedule_delete (id);
    return lights_save (method, uri, data, length);
}

static void lights_background (int fd, int mode) {

    static time_t LastRenewal = 0;
    time_t now = time(0);

    if (use_houseportal) {
        static const char *path[] = {"lights:/lights"};
        if (now >= LastRenewal + 60) {
            if (LastRenewal > 0)
                houseportal_renew();
            else
                houseportal_register (echttp_port(4), path, 1);
            LastRenewal = now;
        }
    }
    houselights_plugs_periodic(now);
    houselights_schedule_periodic(now);
    houselog_background (now);
    housediscover (now);
}

static void lights_protect (const char *method, const char *uri) {
    echttp_cors_protect(method, uri);
}

int main (int argc, const char **argv) {

    const char *error;

    // These strange statements are to make sure that fds 0 to 2 are
    // reserved, since this application might output some errors.
    // 3 descriptors are wasted if 0, 1 and 2 are already open. No big deal.
    //
    open ("/dev/null", O_RDONLY);
    dup(open ("/dev/null", O_WRONLY));

    signal(SIGPIPE, SIG_IGN);

    echttp_default ("-http-service=dynamic");

    echttp_open (argc, argv);
    if (echttp_dynamic_port()) {
        houseportal_initialize (argc, argv);
        use_houseportal = 1;
    }
    houselog_initialize ("lights", argc, argv);

    houseconfig_default ("--config=lights");
    error = houseconfig_load (argc, argv);
    if (error) {
        houselog_trace
            (HOUSE_FAILURE, "CONFIG", "Cannot load configuration: %s\n", error);
    }
    error = houselights_schedule_load (argc, argv);
    if (error) {
        houselog_trace
            (HOUSE_FAILURE, "PLUG", "Cannot initialize: %s\n", error);
        exit(1);
    }

    echttp_cors_allow_method("GET");
    echttp_protect (0, lights_protect);

    echttp_route_uri ("/lights/schedule", lights_schedule);
    echttp_route_uri ("/lights/status", lights_status);
    echttp_route_uri ("/lights/set",    lights_set);
    echttp_route_uri ("/lights/enable", lights_enable);
    echttp_route_uri ("/lights/disable",lights_disable);
    echttp_route_uri ("/lights/add",    lights_add);
    echttp_route_uri ("/lights/delete", lights_delete);

    echttp_static_route ("/", "/usr/local/share/house/public");
    echttp_background (&lights_background);
    housediscover_initialize (argc, argv);

    houselog_event ("SERVICE", "lights", "STARTED", "ON %s", houselog_host());
    echttp_loop();
}

